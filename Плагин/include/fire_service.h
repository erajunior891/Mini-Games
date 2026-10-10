#pragma once

#include <string>
#include <unordered_map>

#include <endstone/player.h>
#include <endstone/server.h>

#include "packets/actor_data_packet.h"

namespace zombie {

class FireService {
public:
    explicit FireService(endstone::Server &server);

    [[nodiscard]] bool shouldBurn(endstone::Player &player);

    void tickPlayer(endstone::Player &player);

    void setVisualFire(endstone::Player &player, bool on_fire);

    void onPlayerQuit(const endstone::Player &player);

    void syncAllBurningTo(endstone::Player &recipient);

    [[nodiscard]] bool isBurning(const std::string &uuid) const;

private:
    endstone::Server &server_;
    std::unordered_map<std::string, bool> burning_state_; // uuid -> is_burning
};

}  // namespace zombie
