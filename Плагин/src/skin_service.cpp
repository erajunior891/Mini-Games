#include "skin_service.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <iostream>

namespace zombie {

SkinService::SkinService(endstone::Server &server) : server_(server) {}

bool SkinService::loadSkins(const std::filesystem::path &skins_dir)
{
    loaded_skins_.clear();

    if (!std::filesystem::exists(skins_dir)) {
        return false;
    }

    for (const auto &entry : std::filesystem::directory_iterator(skins_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        auto path = entry.path();
        if (path.extension() != ".png") {
            continue;
        }

        std::string stem = path.stem().string();
        std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) { return std::tolower(c); });

        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char *data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);

        if (data) {
            RawSkinData skin;
            skin.width = width;
            skin.height = height;
            skin.rgba_bytes = std::string(reinterpret_cast<char *>(data), width * height * 4);
            stbi_image_free(data);

            loaded_skins_[stem] = std::move(skin);
        }
    }

    return !loaded_skins_.empty();
}

bool SkinService::hasSkin(const std::string &skin_key) const
{
    std::string key = skin_key;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
    return loaded_skins_.contains(key);
}

bool SkinService::applySkin(endstone::Player &target, const std::string &skin_key)
{
    std::string key = skin_key;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });

    auto it = loaded_skins_.find(key);
    if (it == loaded_skins_.end()) {
        return false;
    }

    std::string uuid_str = target.getUniqueId().str();
    player_skins_[uuid_str] = key;

    std::string skin_id = "zombie_" + key + "_" + uuid_str;
    std::string packet_payload = SkinPacketBuilder::buildSkinPacket(target.getUniqueId(), skin_id, it->second);

    // Send to all online players (including the target player)
    for (auto *p : server_.getOnlinePlayers()) {
        if (p) {
            p->sendPacket(93, packet_payload);
        }
    }

    return true;
}

void SkinService::reapplySkin(endstone::Player &target)
{
    std::string uuid_str = target.getUniqueId().str();
    auto it = player_skins_.find(uuid_str);
    if (it != player_skins_.end()) {
        applySkin(target, it->second);
    }
}

void SkinService::sendAllCustomSkinsTo(endstone::Player &recipient)
{
    for (auto *p : server_.getOnlinePlayers()) {
        if (!p || p == &recipient) {
            continue;
        }

        std::string uuid_str = p->getUniqueId().str();
        auto it = player_skins_.find(uuid_str);
        if (it != player_skins_.end()) {
            auto skin_it = loaded_skins_.find(it->second);
            if (skin_it != loaded_skins_.end()) {
                std::string skin_id = "zombie_" + it->second + "_" + uuid_str;
                std::string packet_payload =
                    SkinPacketBuilder::buildSkinPacket(p->getUniqueId(), skin_id, skin_it->second);
                recipient.sendPacket(93, packet_payload);
            }
        }
    }
}

std::string SkinService::getPlayerSkin(const std::string &uuid) const
{
    auto it = player_skins_.find(uuid);
    if (it != player_skins_.end()) {
        return it->second;
    }
    return "";
}

void SkinService::removePlayerSkin(const std::string &uuid)
{
    player_skins_.erase(uuid);
}

}  // namespace zombie
