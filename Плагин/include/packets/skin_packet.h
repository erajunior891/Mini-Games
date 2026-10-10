#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <endstone/util/uuid.h>

namespace zombie {

struct RawSkinData {
    int width{64};
    int height{64};
    std::string rgba_bytes;
};

class SkinPacketBuilder {
public:
    /**
     * @brief Builds PlayerSkinPacket (ID 93) payload for changing a player's skin
     * @param target_uuid The target player's UUID
     * @param skin_id A unique skin identifier
     * @param skin The RGBA skin image data
     * @return Raw packet payload bytes (excluding packet ID)
     */
    static std::string buildSkinPacket(const endstone::UUID &target_uuid,
                                       const std::string &skin_id,
                                       const RawSkinData &skin);
};

}  // namespace zombie
