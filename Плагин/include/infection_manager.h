#pragma once

#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include <endstone/player.h>
#include <endstone/server.h>

#include "fire_service.h"
#include "models.h"
#include "skin_service.h"

namespace zombie {

class InfectionManager {
public:
    InfectionManager(endstone::Server &server, SkinService &skin_service, FireService &fire_service);

    void initDefaultClasses();

    [[nodiscard]] PlayerData &getPlayerData(const endstone::Player &player);
    [[nodiscard]] const PlayerData *getPlayerData(const std::string &uuid) const;

    [[nodiscard]] bool isZombie(const endstone::Player &player) const;
    [[nodiscard]] bool isIncubating(const endstone::Player &player) const;

    bool startIncubation(endstone::Player &victim, const std::string &attacker_name);
    void transformToZombie(endstone::Player &player, const std::string &class_name = "");
    void resetPlayer(endstone::Player &player);
    bool setPlayerClass(endstone::Player &player, const std::string &class_name);

    void applyZombieState(endstone::Player &player);

    void onPlayerJoin(endstone::Player &player);
    void onPlayerQuit(const endstone::Player &player);
    void onPlayerRespawn(endstone::Player &player);

    void onTick();

    [[nodiscard]] const std::unordered_map<std::string, ZombieClassInfo> &getClasses() const
    {
        return classes_;
    }

private:
    std::string selectRandomClass();

    endstone::Server &server_;
    SkinService &skin_service_;
    FireService &fire_service_;

    std::unordered_map<std::string, PlayerData> players_; // uuid -> PlayerData
    std::unordered_map<std::string, ZombieClassInfo> classes_;
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace zombie
