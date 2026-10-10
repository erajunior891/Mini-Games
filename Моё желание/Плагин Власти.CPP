#include <endstone/plugin/plugin.h>
#include <endstone/event/player/player_join_event.h>
#include <endstone/event/player/player_interact_event.h>
#include <endstone/event/player/player_respawn_event.h>
#include <endstone/player.h>
#include <endstone/level/dimension.h>
#include <endstone/color_format.h>

#include <algorithm>
#include <string>
#include <vector>

class ZombieMagePlugin : public endstone::Plugin {
public:
    static constexpr const char* OWNER_XUID = "2535455670442871";
    static constexpr int OWNER_MAX_HP = 3;  // 1.5 hearts

    // Имена артефактов
    inline static const std::string ITEM_SKULL  = "§cЧереп Вестника";
    inline static const std::string ITEM_BLAZE  = "§6Жезл Всполоха";
    inline static const std::string ITEM_DRAGON = "§5Флакон Дракона";
    inline static const std::string ITEM_WIND   = "§7Сфера Ветров";
    inline static const std::string ITEM_SONIC  = "§1Резонатор Тьмы";
    inline static const std::string ITEM_SHIELD = "§bПечать Бессмертия";

    void onEnable() override {
        registerEvent(&ZombieMagePlugin::onPlayerJoin, *this);
        registerEvent(&ZombieMagePlugin::onPlayerRespawn, *this);
        registerEvent(&ZombieMagePlugin::onPlayerInteract, *this);
        getLogger().info("ZombieMage C++ Plugin активирован!");
    }

    bool checkAndApplyProfile(endstone::Player& player) {
        if (player.getXuid() == OWNER_XUID) {
            // Устанавливаем 3 HP (1.5 сердца) Владельцу
            player.setMaxHealth(OWNER_MAX_HP);
            if (player.getHealth() > OWNER_MAX_HP) {
                player.setHealth(OWNER_MAX_HP);
            }
            return true;
        }

        // Проверка чужаков
        bool has_illegal_items = false;
        auto& inventory = player.getInventory();

        for (int i = 0; i < inventory.getSize(); ++i) {
            auto item = inventory.getItem(i);
            if (item.has_value() && item->hasItemMeta()) {
                auto meta = item->getItemMeta();
                if (meta && meta->hasDisplayName()) {
                    if (isArtifact(meta->getDisplayName())) {
                        has_illegal_items = true;
                        break;
                    }
                }
            }
        }

        const auto& tags = player.getScoreboardTags();
        bool has_mage_tag = std::find(tags.begin(), tags.end(), "ZombieMage") != tags.end();

        if (has_illegal_items || has_mage_tag) {
            (void)player.removeScoreboardTag("ZombieMage");
            player.setMaxHealth(20);
            player.setHealth(20);
            inventory.clear();

            player.sendMessage(endstone::ColorFormat::Red +
                               "[СИСТЕМА] У вас нет прав на класс Зомби-Мага!");
            getLogger().warning("Игрок " + player.getName() + " (" + player.getXuid() +
                                ") пытался использовать артефакты Зомби-Мага!");
            return false;
        }

        return false;
    }

    void onPlayerJoin(endstone::PlayerJoinEvent& event) {
        checkAndApplyProfile(event.getPlayer());
    }

    void onPlayerRespawn(endstone::PlayerRespawnEvent& event) {
        auto& player = event.getPlayer();
        if (player.getXuid() == OWNER_XUID) {
            player.setMaxHealth(OWNER_MAX_HP);
            player.setHealth(OWNER_MAX_HP);
        }
    }

    void onPlayerInteract(endstone::PlayerInteractEvent& event) {
        auto& player = event.getPlayer();
        const auto& item = event.getItem();

        if (!item.has_value() || !item->hasItemMeta()) {
            return;
        }

        auto meta = item->getItemMeta();
        if (!meta || !meta->hasDisplayName()) {
            return;
        }

        std::string item_name = meta->getDisplayName();

        if (isArtifact(item_name)) {
            if (!checkAndApplyProfile(player)) {
                event.cancel();
                return;
            }

            auto loc = player.getLocation();
            auto& dim = player.getDimension();

            // 1. Череп визер-скелета
            if (item_name == ITEM_SKULL) {
                (void)dim.spawnActor(loc, "minecraft:wither_skull");
            }
            // 2. Огненный шар блейза
            else if (item_name == ITEM_BLAZE) {
                (void)dim.spawnActor(loc, "minecraft:small_fireball");
            }
            // 3. Дыхание дракона
            else if (item_name == ITEM_DRAGON) {
                (void)dim.spawnActor(loc, "minecraft:area_effect_cloud");
            }
            // 4. Ветряной залп
            else if (item_name == ITEM_WIND) {
                (void)dim.spawnActor(loc, "minecraft:wind_charge");
            }
            // 5. Звуковой выстрел Вардена
            else if (item_name == ITEM_SONIC) {
                player.performCommand("particle minecraft:sonic_explosion ~ ~1 ~");
                player.performCommand("playsound mob.warden.sonic_boom @a ~ ~ ~");
            }
            // 6. Печать Бессмертия (255 Сопротивления на 10 сек)
            else if (item_name == ITEM_SHIELD) {
                player.performCommand("effect @s resistance 10 255");
                player.sendMessage(endstone::ColorFormat::Aqua +
                                   "[ЩИТ] Абсолютная защита активирована на 10 секунд!");
            }
        }
    }

private:
    bool isArtifact(const std::string& name) const {
        return name == ITEM_SKULL || name == ITEM_BLAZE ||
               name == ITEM_DRAGON || name == ITEM_WIND ||
               name == ITEM_SONIC || name == ITEM_SHIELD;
    }
};

ENDSTONE_PLUGIN("zombie_mage", "1.0.0", ZombieMagePlugin)
{
    description = "Персональный класс Зомби-Мага с привязкой по XUID";
    prefix = "ZombieMage";
}