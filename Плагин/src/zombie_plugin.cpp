#include "zombie_plugin.h"

#include <algorithm>

#include <endstone/color_format.h>
#include <endstone/scheduler/scheduler.h>
#include <endstone/server.h>

void ZombieInfectionPlugin::onLoad()
{
    getLogger().info("ZombieInfection C++ loaded");
}

void ZombieInfectionPlugin::onEnable()
{
    skin_service_ = std::make_unique<zombie::SkinService>(getServer());
    fire_service_ = std::make_unique<zombie::FireService>(getServer());
    infection_manager_ =
        std::make_unique<zombie::InfectionManager>(getServer(), *skin_service_, *fire_service_);
    mage_service_ = std::make_unique<zombie::MageService>(getServer());

    // Attempt to load skins from plugin's data folder or default skins directory
    auto skins_path = getDataFolder() / "skins";
    if (!skin_service_->loadSkins(skins_path)) {
        // Fallback to relative skins directory if running from dev directory
        skin_service_->loadSkins("plugins/zombie_infection/skins");
    }

    // Register all event handlers
    registerEvent(&ZombieInfectionPlugin::onPlayerSkinChange, *this, endstone::EventPriority::Highest, true);
    registerEvent(&ZombieInfectionPlugin::onActorDamage, *this, endstone::EventPriority::Highest, true);
    registerEvent(&ZombieInfectionPlugin::onPlayerDeath, *this);
    registerEvent(&ZombieInfectionPlugin::onPlayerRespawn, *this);
    registerEvent(&ZombieInfectionPlugin::onPlayerJoin, *this);
    registerEvent(&ZombieInfectionPlugin::onPlayerQuit, *this);
    registerEvent(&ZombieInfectionPlugin::onPlayerItemConsume, *this);
    registerEvent(&ZombieInfectionPlugin::onPlayerInteract, *this);

    // Register 1-second (20 ticks) timer for infection and sun burning logic
    getServer().getScheduler().runTaskTimer(*this, [this]() { infection_manager_->onTick(); }, 20, 20);

    getLogger().info("ZombieInfection C++ enabled successfully!");
}

void ZombieInfectionPlugin::onDisable()
{
    getLogger().info("ZombieInfection C++ disabled");
}

void ZombieInfectionPlugin::onPlayerSkinChange(endstone::PlayerSkinChangeEvent &event)
{
    auto &player = event.getPlayer();

    // If player is infected as a zombie, prevent client from resetting the skin
    if (infection_manager_->isZombie(player)) {
        event.cancel();
        player.sendErrorMessage(endstone::ColorFormat::Red +
                                "Вы не можете изменить свой скин, пока находитесь в облике зомби!");
    }
}

void ZombieInfectionPlugin::onActorDamage(endstone::ActorDamageEvent &event)
{
    auto &victim_mob = event.getActor();
    auto *damager_actor = event.getDamageSource().getDamagingActor();

    // Check if damager and victim are both players
    auto *damager_player = dynamic_cast<endstone::Player *>(damager_actor);
    auto *victim_player = dynamic_cast<endstone::Player *>(&victim_mob);

    if (damager_player && victim_player) {
        bool damager_is_zombie = infection_manager_->isZombie(*damager_player);
        bool victim_is_zombie = infection_manager_->isZombie(*victim_player);

        // Friendly fire between zombies is disabled
        if (damager_is_zombie && victim_is_zombie) {
            event.cancel();
            return;
        }

        // Transmission: zombie damages human
        if (damager_is_zombie && !victim_is_zombie) {
            infection_manager_->startIncubation(*victim_player, damager_player->getName());
        }
    }
}

void ZombieInfectionPlugin::onPlayerDeath(endstone::PlayerDeathEvent &event)
{
    auto &player = event.getPlayer();
    if (infection_manager_->isIncubating(player)) {
        // Instant turn on death while incubating
        infection_manager_->transformToZombie(player);
    }
}

void ZombieInfectionPlugin::onPlayerRespawn(endstone::PlayerRespawnEvent &event)
{
    auto &player = event.getPlayer();
    infection_manager_->onPlayerRespawn(player);
    mage_service_->onPlayerRespawn(player);
}

void ZombieInfectionPlugin::onPlayerJoin(endstone::PlayerJoinEvent &event)
{
    auto &player = event.getPlayer();

    // Check ZombieMage profile (owner HP / illegal artifact detection)
    mage_service_->checkAndApplyProfile(player);

    auto player_id = player.getUniqueId();
    // Delayed task (10 ticks / 0.5s) to allow client chunk and actor loading
    getServer().getScheduler().runTaskLater(
        *this,
        [this, player_id]() {
            auto *p = getServer().getPlayer(player_id);
            if (!p || !p->isValid()) {
                return;
            }
            // Send existing custom skins and burning state to the joining player
            skin_service_->sendAllCustomSkinsTo(*p);
            fire_service_->syncAllBurningTo(*p);

            // Reapply state if the joining player was already a zombie
            infection_manager_->onPlayerJoin(*p);
        },
        10);
}

void ZombieInfectionPlugin::onPlayerQuit(endstone::PlayerQuitEvent &event)
{
    infection_manager_->onPlayerQuit(event.getPlayer());
}

void ZombieInfectionPlugin::onPlayerItemConsume(endstone::PlayerItemConsumeEvent &event)
{
    auto &player = event.getPlayer();
    if (!infection_manager_->isZombie(player)) {
        return;
    }

    std::string item_type = static_cast<std::string>(event.getItem().getType().getId());
    if (item_type.find("rotten_flesh") != std::string::npos) {
        int max_hp = player.getMaxHealth();
        int new_hp = std::min(max_hp, player.getHealth() + 4);
        player.setHealth(new_hp);

        player.sendTip(endstone::ColorFormat::Green + "+4 HP (Гнилая плоть)");
        try {
            player.playSound(player.getLocation(), "random.burp", 1.0f, 1.0f);
            player.spawnParticle("minecraft:villager_happy", player.getLocation());
        }
        catch (...) {
        }
    }
}

void ZombieInfectionPlugin::onPlayerInteract(endstone::PlayerInteractEvent &event)
{
    mage_service_->onPlayerInteract(event);
}

bool ZombieInfectionPlugin::onCommand(endstone::CommandSender &sender, const endstone::Command &command,
                                      const std::vector<std::string> &args)
{
    if (command.getName() != "zombie") {
        return false;
    }

    if (args.empty()) {
        sender.sendMessage(endstone::ColorFormat::Yellow + "--- Zombie Infection C++ Команды ---");
        sender.sendMessage(endstone::ColorFormat::Green + "/zombie info [игрок] " + endstone::ColorFormat::Gray +
                           "- Статус и класс");
        if (sender.hasPermission("zombie.admin")) {
            sender.sendMessage(endstone::ColorFormat::Red + "/zombie infect <игрок> [класс] " +
                               endstone::ColorFormat::Gray + "- Заразить");
            sender.sendMessage(endstone::ColorFormat::Red + "/zombie reset <игрок> " +
                               endstone::ColorFormat::Gray + "- Вылечить");
            sender.sendMessage(endstone::ColorFormat::Red + "/zombie class <игрок> <класс> " +
                               endstone::ColorFormat::Gray + "- Сменить класс");
            sender.sendMessage(endstone::ColorFormat::Red + "/zombie list " + endstone::ColorFormat::Gray +
                               "- Список заражённых");
        }
        return true;
    }

    std::string sub = args[0];
    std::transform(sub.begin(), sub.end(), sub.begin(), [](unsigned char c) { return std::tolower(c); });

    if (sub == "info") {
        std::string target_name = (args.size() > 1) ? args[1] : sender.getName();
        endstone::Player *target = nullptr;
        for (auto *p : getServer().getOnlinePlayers()) {
            if (p && p->getName() == target_name) {
                target = p;
                break;
            }
        }
        if (!target) {
            sender.sendErrorMessage("Игрок '" + target_name + "' не найден в сети.");
            return true;
        }

        const auto &data = infection_manager_->getPlayerData(*target);
        if (data.stage == zombie::InfectionStage::Human) {
            sender.sendMessage(endstone::ColorFormat::Green + "Статус: Человек");
        }
        else if (data.stage == zombie::InfectionStage::Incubating) {
            sender.sendMessage(endstone::ColorFormat::Yellow + "Статус: Инкубация (" +
                               std::to_string(static_cast<int>(data.incubation_remaining)) + "с)");
        }
        else {
            sender.sendMessage(endstone::ColorFormat::DarkRed + "Статус: Зомби | Класс: " +
                               endstone::ColorFormat::Yellow + data.zombie_class);
        }
        return true;
    }

    if (sub == "infect") {
        if (!sender.hasPermission("zombie.admin")) {
            sender.sendErrorMessage("У вас нет прав для выполнения этой команды.");
            return true;
        }
        if (args.size() < 2) {
            sender.sendErrorMessage("Использование: /zombie infect <игрок> [класс]");
            return true;
        }
        std::string target_name = args[1];
        endstone::Player *target = nullptr;
        for (auto *p : getServer().getOnlinePlayers()) {
            if (p && p->getName() == target_name) {
                target = p;
                break;
            }
        }
        if (!target) {
            sender.sendErrorMessage("Игрок '" + target_name + "' не найден в сети.");
            return true;
        }
        std::string cls = (args.size() > 2) ? args[2] : "";
        infection_manager_->transformToZombie(*target, cls);
        sender.sendMessage(endstone::ColorFormat::Green + "[☣] Игрок " + target->getName() + " успешно заражён!");
        return true;
    }

    if (sub == "reset") {
        if (!sender.hasPermission("zombie.admin")) {
            sender.sendErrorMessage("У вас нет прав для выполнения этой команды.");
            return true;
        }
        if (args.size() < 2) {
            sender.sendErrorMessage("Использование: /zombie reset <игрок>");
            return true;
        }
        std::string target_name = args[1];
        endstone::Player *target = nullptr;
        for (auto *p : getServer().getOnlinePlayers()) {
            if (p && p->getName() == target_name) {
                target = p;
                break;
            }
        }
        if (!target) {
            sender.sendErrorMessage("Игрок '" + target_name + "' не найден в сети.");
            return true;
        }
        infection_manager_->resetPlayer(*target);
        sender.sendMessage(endstone::ColorFormat::Green + "[☣] Игрок " + target->getName() + " успешно вылечен!");
        return true;
    }

    if (sub == "class") {
        if (!sender.hasPermission("zombie.admin")) {
            sender.sendErrorMessage("У вас нет прав для выполнения этой команды.");
            return true;
        }
        if (args.size() < 3) {
            sender.sendErrorMessage("Использование: /zombie class <игрок> <класс>");
            return true;
        }
        std::string target_name = args[1];
        std::string cls = args[2];
        endstone::Player *target = nullptr;
        for (auto *p : getServer().getOnlinePlayers()) {
            if (p && p->getName() == target_name) {
                target = p;
                break;
            }
        }
        if (!target) {
            sender.sendErrorMessage("Игрок '" + target_name + "' не найден в сети.");
            return true;
        }
        if (infection_manager_->setPlayerClass(*target, cls)) {
            sender.sendMessage(endstone::ColorFormat::Green + "[☣] Класс изменён на: " + cls);
        }
        else {
            sender.sendErrorMessage("Не удалось изменить класс. Проверьте правильность класса.");
        }
        return true;
    }

    if (sub == "list") {
        if (!sender.hasPermission("zombie.admin")) {
            sender.sendErrorMessage("У вас нет прав для выполнения этой команды.");
            return true;
        }
        int zombies_count = 0;
        int inc_count = 0;
        for (auto *p : getServer().getOnlinePlayers()) {
            if (p) {
                if (infection_manager_->isZombie(*p)) {
                    zombies_count++;
                }
                else if (infection_manager_->isIncubating(*p)) {
                    inc_count++;
                }
            }
        }
        sender.sendMessage(endstone::ColorFormat::Yellow + "--- Список заражённых ---");
        sender.sendMessage(endstone::ColorFormat::Red + "Зомби в сети: " + std::to_string(zombies_count));
        sender.sendMessage(endstone::ColorFormat::Yellow + "На стадии инкубации: " + std::to_string(inc_count));
        return true;
    }

    sender.sendErrorMessage("Неизвестная подкоманда. Используйте /zombie для справки.");
    return true;
}

ENDSTONE_PLUGIN("zombie_infection", "1.0.0", ZombieInfectionPlugin)
{
    description = "High-performance C++ Zombie Infection System with native skins and sun burning";
    website = "https://github.com/EndstoneMC/endstone";
    authors = {"Endstone Developer"};
    prefix = "ZombieInfection";

    permission("zombie.command")
        .description("Allows players to use /zombie info")
        .default_(endstone::PermissionDefault::True);

    permission("zombie.admin")
        .description("Allows admins to manage infections and classes")
        .default_(endstone::PermissionDefault::Operator);

    command("zombie")
        .description("Zombie Infection System commands")
        .usages("/zombie",
                "/zombie info [player: string]",
                "/zombie infect <player: string> [class: string]",
                "/zombie reset <player: string>",
                "/zombie class <player: string> <class: string>",
                "/zombie list")
        .permissions("zombie.command");
}
