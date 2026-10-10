#pragma once

#include <memory>
#include <string>
#include <vector>

#include <endstone/command/command.h>
#include <endstone/command/command_sender.h>
#include <endstone/event/actor/actor_damage_event.h>
#include <endstone/event/actor/player_death_event.h>
#include <endstone/event/player/player_interact_event.h>
#include <endstone/event/player/player_item_consume_event.h>
#include <endstone/event/player/player_join_event.h>
#include <endstone/event/player/player_quit_event.h>
#include <endstone/event/player/player_respawn_event.h>
#include <endstone/event/player/player_skin_change_event.h>
#include <endstone/plugin/plugin.h>

#include "fire_service.h"
#include "infection_manager.h"
#include "mage_service.h"
#include "skin_service.h"

class ZombieInfectionPlugin : public endstone::Plugin {
public:
    void onLoad() override;
    void onEnable() override;
    void onDisable() override;

    bool onCommand(endstone::CommandSender &sender, const endstone::Command &command,
                   const std::vector<std::string> &args) override;

    // Event listeners
    void onPlayerSkinChange(endstone::PlayerSkinChangeEvent &event);
    void onActorDamage(endstone::ActorDamageEvent &event);
    void onPlayerDeath(endstone::PlayerDeathEvent &event);
    void onPlayerRespawn(endstone::PlayerRespawnEvent &event);
    void onPlayerJoin(endstone::PlayerJoinEvent &event);
    void onPlayerQuit(endstone::PlayerQuitEvent &event);
    void onPlayerItemConsume(endstone::PlayerItemConsumeEvent &event);
    void onPlayerInteract(endstone::PlayerInteractEvent &event);

private:
    std::unique_ptr<zombie::SkinService> skin_service_;
    std::unique_ptr<zombie::FireService> fire_service_;
    std::unique_ptr<zombie::InfectionManager> infection_manager_;
    std::unique_ptr<zombie::MageService> mage_service_;
};
