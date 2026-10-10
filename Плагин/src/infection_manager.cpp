#include "infection_manager.h"

#include <algorithm>

#include <endstone/color_format.h>

namespace zombie {

InfectionManager::InfectionManager(endstone::Server &server, SkinService &skin_service, FireService &fire_service)
    : server_(server), skin_service_(skin_service), fire_service_(fire_service)
{
    initDefaultClasses();
}

void InfectionManager::initDefaultClasses()
{
    classes_["normal"] = {"normal", "Обычный", 20.0f, 1.0f, 1.0f, 40, 0.0f, "normal"};
    classes_["tank"] = {"tank", "Танк", 40.0f, 0.8f, 1.1f, 20, 0.0f, "tank"};
    classes_["runner"] = {"runner", "Бегун", 16.0f, 1.2f, 1.0f, 20, 0.0f, "runner"};
    classes_["lider"] = {"lider", "Лидер", 24.0f, 1.05f, 1.2f, 10, 0.05f, "lider"};
    classes_["admin"] = {"admin", "Админ", 50.0f, 1.5f, 2.0f, 1, 0.5f, "admin"};
}

std::string InfectionManager::selectRandomClass()
{
    std::vector<std::string> names;
    std::vector<int> weights;
    for (const auto &[name, cls] : classes_) {
        names.push_back(name);
        weights.push_back(cls.weight);
    }
    if (names.empty()) {
        return "normal";
    }

    std::discrete_distribution<size_t> dist(weights.begin(), weights.end());
    return names[dist(rng_)];
}

PlayerData &InfectionManager::getPlayerData(const endstone::Player &player)
{
    std::string uuid = player.getUniqueId().str();
    auto it = players_.find(uuid);
    if (it == players_.end()) {
        PlayerData data;
        data.uuid = uuid;
        data.name = player.getName();
        players_[uuid] = data;
        return players_[uuid];
    }
    return it->second;
}

const PlayerData *InfectionManager::getPlayerData(const std::string &uuid) const
{
    auto it = players_.find(uuid);
    if (it != players_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool InfectionManager::isZombie(const endstone::Player &player) const
{
    auto *data = getPlayerData(player.getUniqueId().str());
    return data && data->stage == InfectionStage::Zombie;
}

bool InfectionManager::isIncubating(const endstone::Player &player) const
{
    auto *data = getPlayerData(player.getUniqueId().str());
    return data && data->stage == InfectionStage::Incubating;
}

bool InfectionManager::startIncubation(endstone::Player &victim, const std::string &attacker_name)
{
    auto &data = getPlayerData(victim);
    if (data.stage != InfectionStage::Human) {
        return false;
    }

    data.stage = InfectionStage::Incubating;
    data.incubation_remaining = 45.0f;
    data.zombie_class = selectRandomClass();

    victim.sendMessage(endstone::ColorFormat::DarkRed + "[☣] " + endstone::ColorFormat::Red +
                       "Вы были заражены " + attacker_name + "! Инкубация: 45 сек...");
    victim.sendTitle(endstone::ColorFormat::DarkRed + "[☣] ЗАРАЖЕНИЕ",
                     endstone::ColorFormat::Red + "Инкубация началась...", 10, 40, 10);
    try {
        victim.playSound(victim.getLocation(), "mob.zombie.say", 1.0f, 0.8f);
    }
    catch (...) {
    }

    return true;
}

void InfectionManager::transformToZombie(endstone::Player &player, const std::string &class_name)
{
    auto &data = getPlayerData(player);
    data.stage = InfectionStage::Zombie;
    data.incubation_remaining = 0.0f;

    if (!class_name.empty() && classes_.contains(class_name)) {
        data.zombie_class = class_name;
    }
    else if (data.zombie_class.empty() || !classes_.contains(data.zombie_class)) {
        data.zombie_class = selectRandomClass();
    }

    applyZombieState(player);

    const auto &cls = classes_[data.zombie_class];
    player.sendMessage(endstone::ColorFormat::DarkRed + "[☣] " + endstone::ColorFormat::Red +
                       "Вы стали зомби! Класс: " + endstone::ColorFormat::Yellow + cls.display_name);
    player.sendTitle(endstone::ColorFormat::DarkRed + "ВЫ ЗОМБИ!",
                     endstone::ColorFormat::Yellow + "Класс: " + cls.display_name, 10, 70, 20);

    try {
        player.playSound(player.getLocation(), "mob.zombie.death", 1.2f, 0.6f);
    }
    catch (...) {
    }

    server_.broadcastMessage(endstone::ColorFormat::DarkRed + "[☣] " + endstone::ColorFormat::Red +
                             player.getName() + " превратился в зомби!");
}

void InfectionManager::applyZombieState(endstone::Player &player)
{
    auto &data = getPlayerData(player);
    if (data.stage != InfectionStage::Zombie) {
        return;
    }

    auto cls_it = classes_.find(data.zombie_class);
    if (cls_it == classes_.end()) {
        cls_it = classes_.find("normal");
    }
    const auto &cls = cls_it->second;

    // Visual tags
    player.addScoreboardTag("zombie_infected");
    player.setNameTag(endstone::ColorFormat::DarkRed + "[☣] " + endstone::ColorFormat::Red + player.getName());
    player.setScoreTag(endstone::ColorFormat::Yellow + cls.display_name);

    // Health attributes
    player.setMaxHealth(static_cast<int>(cls.hp));
    player.setHealth(static_cast<int>(cls.hp));

    // Network Skin application
    skin_service_.applySkin(player, cls.skin_name);
}

void InfectionManager::resetPlayer(endstone::Player &player)
{
    auto &data = getPlayerData(player);
    data.stage = InfectionStage::Human;
    data.zombie_class = "";
    data.incubation_remaining = 0.0f;
    data.in_respawn_cooldown = false;

    player.removeScoreboardTag("zombie_infected");
    player.setNameTag(player.getName());
    player.setScoreTag("");

    player.setMaxHealth(20);
    player.setHealth(20);

    fire_service_.setVisualFire(player, false);
    skin_service_.removePlayerSkin(data.uuid);

    player.sendMessage(endstone::ColorFormat::Green + "[☣] Вы были полностью излечены!");
    player.sendTitle(endstone::ColorFormat::Green + "ИЗЛЕЧЕН", endstone::ColorFormat::Gray + "Вы снова человек",
                     10, 40, 10);
}

bool InfectionManager::setPlayerClass(endstone::Player &player, const std::string &class_name)
{
    if (!classes_.contains(class_name)) {
        return false;
    }
    auto &data = getPlayerData(player);
    if (data.stage != InfectionStage::Zombie) {
        return false;
    }

    data.zombie_class = class_name;
    applyZombieState(player);
    return true;
}

void InfectionManager::onPlayerJoin(endstone::Player &player)
{
    auto &data = getPlayerData(player);

    if (data.stage == InfectionStage::Zombie) {
        applyZombieState(player);
    }
}

void InfectionManager::onPlayerQuit(const endstone::Player &player)
{
    fire_service_.onPlayerQuit(player);
}

void InfectionManager::onPlayerRespawn(endstone::Player &player)
{
    auto &data = getPlayerData(player);
    if (data.stage == InfectionStage::Zombie) {
        applyZombieState(player);
    }
}

void InfectionManager::onTick()
{
    for (auto *player : server_.getOnlinePlayers()) {
        if (!player) {
            continue;
        }

        auto &data = getPlayerData(*player);

        if (data.stage == InfectionStage::Incubating) {
            data.incubation_remaining -= 1.0f;
            int sec = std::max(0, static_cast<int>(data.incubation_remaining));
            player->sendTip(endstone::ColorFormat::Red + "☣ Заражение: " + std::to_string(sec) + " сек осталось");

            if (sec <= 0) {
                transformToZombie(*player);
            }
        }
        else if (data.stage == InfectionStage::Zombie) {
            fire_service_.tickPlayer(*player);
        }
    }
}

}  // namespace zombie
