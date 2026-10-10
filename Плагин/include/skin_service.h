#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include <endstone/player.h>
#include <endstone/server.h>

#include "models.h"
#include "packets/skin_packet.h"

namespace zombie {

class SkinService {
public:
    explicit SkinService(endstone::Server &server);

    bool loadSkins(const std::filesystem::path &skins_dir);

    bool applySkin(endstone::Player &target, const std::string &skin_key);

    void reapplySkin(endstone::Player &target);

    void sendAllCustomSkinsTo(endstone::Player &recipient);

    [[nodiscard]] bool hasSkin(const std::string &skin_key) const;

    [[nodiscard]] std::string getPlayerSkin(const std::string &uuid) const;

    void removePlayerSkin(const std::string &uuid);

private:
    endstone::Server &server_;
    std::unordered_map<std::string, RawSkinData> loaded_skins_;
    std::unordered_map<std::string, std::string> player_skins_; // uuid -> skin_key
};

}  // namespace zombie
