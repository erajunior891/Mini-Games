#pragma once

#include <string>
#include <unordered_set>

#include <endstone/player.h>
#include <endstone/server.h>
#include <endstone/event/player/player_interact_event.h>
#include <endstone/event/player/player_join_event.h>
#include <endstone/event/player/player_respawn_event.h>
#include <endstone/inventory/item_stack.h>
#include <endstone/inventory/meta/item_meta.h>
#include <endstone/inventory/player_inventory.h>

namespace zombie {

/**
 * @brief Manages the ZombieMage class — a special owner-exclusive class
 *        with unique artifact items and reduced max HP.
 *
 * Owner XUID: 2535455670442871
 * Only the owner can hold/use artifact items. If any other player has them,
 * their inventory is cleared and they're reset to normal state.
 */
class MageService {
public:
    explicit MageService(endstone::Server &server);

    /// Owner XUID constant
    static constexpr const char *OWNER_XUID = "2535455670442871";

    /// Owner max HP (1.5 hearts = 3 HP)
    static constexpr int OWNER_MAX_HP = 3;

    // Artifact display names (§-colored)
    static const std::string ITEM_SKULL;   // §cЧереп Вестника
    static const std::string ITEM_BLAZE;   // §6Жезл Всполоха
    static const std::string ITEM_DRAGON;  // §5Флакон Дракона
    static const std::string ITEM_WIND;    // §7Сфера Ветров
    static const std::string ITEM_SONIC;   // §1Резонатор Тьмы
    static const std::string ITEM_SHIELD;  // §bПечать Бессмертия

    /**
     * @brief Check if a display name matches any artifact item.
     */
    [[nodiscard]] bool isArtifact(const std::string &display_name) const;

    /**
     * @brief Checks a player on join — applies owner profile or strips
     *        illegal artifacts from non-owners.
     * @return true if the player IS the owner.
     */
    bool checkAndApplyProfile(endstone::Player &player);

    /**
     * @brief Handle interact event — process artifact usage.
     *        Only the owner can use artifacts; others get cancelled + stripped.
     */
    void onPlayerInteract(endstone::PlayerInteractEvent &event);

    /**
     * @brief Re-apply owner HP on respawn.
     */
    void onPlayerRespawn(endstone::Player &player);

private:
    endstone::Server &server_;
};

}  // namespace zombie
