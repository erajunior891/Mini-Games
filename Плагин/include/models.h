#pragma once

#include <string>

namespace zombie {

enum class InfectionStage {
    Human = 0,
    Incubating = 1,
    Zombie = 2
};

struct ZombieClassInfo {
    std::string name;
    std::string display_name;
    float hp{20.0f};
    float speed{1.0f};
    float damage{1.0f};
    int weight{10};
    float bonus_infect_chance{0.0f};
    std::string skin_name{"normal"};
};

struct PlayerData {
    std::string uuid;
    std::string name;
    InfectionStage stage{InfectionStage::Human};
    std::string zombie_class;
    float incubation_remaining{0.0f};
    float respawn_remaining{0.0f};
    bool in_respawn_cooldown{false};
    int infect_count{0};
    std::string current_applied_skin;
};

}  // namespace zombie
