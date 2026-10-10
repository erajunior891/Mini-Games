#pragma once

#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

#include <endstone/command/command.h>
#include <endstone/command/command_sender.h>
#include <endstone/event/player/player_move_event.h>
#include <endstone/game_mode.h>
#include <endstone/level/location.h>
#include <endstone/player.h>
#include <endstone/plugin/plugin.h>

class WorldBorderPlugin : public endstone::Plugin {
public:
    void onLoad() override;
    void onEnable() override;
    void onDisable() override;

    bool onCommand(endstone::CommandSender &sender, const endstone::Command &command,
                   const std::vector<std::string> &args) override;

    void onPlayerMove(endstone::PlayerMoveEvent &event);

    void tickBorderEnforcement();

    [[nodiscard]] std::pair<double, bool> getDistanceToBorder(double x, double z) const;
    [[nodiscard]] endstone::Location getSafeLocation(const endstone::Location &loc) const;
    [[nodiscard]] bool isPlayerBypassed(const endstone::Player &player) const;

private:
    void loadConfigValues();

    bool enabled_{true};
    std::string shape_{"square"};
    double center_x_{0.0};
    double center_z_{0.0};
    double radius_{500.0};
    double warning_distance_{15.0};

    bool prevent_move_{true};
    bool knockback_{true};
    double knockback_strength_{1.2};
    bool teleport_to_safe_{true};
    bool damage_outside_{true};
    double damage_amount_{2.0};

    bool particles_enabled_{true};
    std::string particle_name_{"minecraft:redstone_ore_dust_particle"};

    std::unordered_set<std::string> bypassed_players_;
};
