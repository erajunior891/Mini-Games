#include "mage_service.h"

#include <algorithm>

#include <endstone/color_format.h>
#include <endstone/level/dimension.h>

namespace zombie {

// Artifact item display names (using § section sign for Minecraft color codes)
const std::string MageService::ITEM_SKULL  = "§cЧереп Вестника";     // §cЧереп Вестника
const std::string MageService::ITEM_BLAZE  = "§6Жезл Всполоха";     // §6Жезл Всполоха
const std::string MageService::ITEM_DRAGON = "§5Флакон Дракона";  // §5Флакон Дракона
const std::string MageService::ITEM_WIND   = "§7Сфера Ветров";       // §7Сфера Ветров
const std::string MageService::ITEM_SONIC  = "§1Резонатор Тьмы";  // §1Резонатор Тьмы
const std::string MageService::ITEM_SHIELD = "§bПечать Бессмертия";  // §bПечать Бессмертия

MageService::MageService(endstone::Server &server) : server_(server) {}

bool MageService::isArtifact(const std::string &display_name) const
{
    return display_name == ITEM_SKULL  || display_name == ITEM_BLAZE ||
           display_name == ITEM_DRAGON || display_name == ITEM_WIND  ||
           display_name == ITEM_SONIC  || display_name == ITEM_SHIELD;
}

bool MageService::checkAndApplyProfile(endstone::Player &player)
{
    if (player.getXuid() == OWNER_XUID) {
        // Set owner's max HP to 3 (1.5 hearts)
        player.setMaxHealth(OWNER_MAX_HP);
        if (player.getHealth() > OWNER_MAX_HP) {
            player.setHealth(OWNER_MAX_HP);
        }
        return true;
    }

    // Check if non-owner has artifact items
    auto &inventory = player.getInventory();
    bool has_illegal_items = false;

    for (int i = 0; i < inventory.getSize(); ++i) {
        auto item = inventory.getItem(i);
        if (item.has_value() && item->hasItemMeta()) {
            auto meta = item->getItemMeta();
            if (meta && meta->hasDisplayName()) {
                std::string name = meta->getDisplayName();
                if (isArtifact(name)) {
                    has_illegal_items = true;
                    break;
                }
            }
        }
    }

    const auto &tags = player.getScoreboardTags();
    bool has_mage_tag = std::find(tags.begin(), tags.end(), "ZombieMage") != tags.end();

    if (has_illegal_items || has_mage_tag) {
        (void)player.removeScoreboardTag("ZombieMage");
        player.setMaxHealth(20);
        player.setHealth(20);
        inventory.clear();

        player.sendMessage(endstone::ColorFormat::Red +
                           "[СИСТЕМА] У вас нет прав на класс Зомби-Мага!");
        return false;
    }

    return false;
}

void MageService::onPlayerInteract(endstone::PlayerInteractEvent &event)
{
    auto &player = event.getPlayer();
    const auto &item = event.getItem();

    if (!item.has_value() || !item->hasItemMeta()) {
        return;
    }

    auto meta = item->getItemMeta();
    if (!meta || !meta->hasDisplayName()) {
        return;
    }

    std::string item_name = meta->getDisplayName();

    if (!isArtifact(item_name)) {
        return;
    }

    // Only the owner can use artifacts
    if (!checkAndApplyProfile(player)) {
        event.cancel();
        return;
    }

    auto loc = player.getLocation();
    auto &dim = player.getDimension();

    // 1. Wither skull
    if (item_name == ITEM_SKULL) {
        (void)dim.spawnActor(loc, "minecraft:wither_skull");
    }
    // 2. Blaze fireball
    else if (item_name == ITEM_BLAZE) {
        (void)dim.spawnActor(loc, "minecraft:small_fireball");
    }
    // 3. Dragon breath area effect
    else if (item_name == ITEM_DRAGON) {
        (void)dim.spawnActor(loc, "minecraft:area_effect_cloud");
    }
    // 4. Wind charge
    else if (item_name == ITEM_WIND) {
        (void)dim.spawnActor(loc, "minecraft:wind_charge");
    }
    // 5. Sonic boom (Warden)
    else if (item_name == ITEM_SONIC) {
        player.performCommand("particle minecraft:sonic_explosion ~ ~1 ~");
        player.performCommand("playsound mob.warden.sonic_boom @a ~ ~ ~");
    }
    // 6. Shield of Immortality (Resistance 255 for 10 sec)
    else if (item_name == ITEM_SHIELD) {
        // Use /effect command since addPotionEffect is not in the C++ API
        player.performCommand("effect @s resistance 10 255");
        player.sendMessage(endstone::ColorFormat::Aqua +
                           "[ЩИТ] Абсолютная защита активирована на 10 секунд!");
    }
}

void MageService::onPlayerRespawn(endstone::Player &player)
{
    if (player.getXuid() == OWNER_XUID) {
        player.setMaxHealth(OWNER_MAX_HP);
        player.setHealth(OWNER_MAX_HP);
    }
}

}  // namespace zombie
