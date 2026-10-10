#include "packets/skin_packet.h"
#include "packets/binary_stream_writer.h"

namespace zombie {

std::string SkinPacketBuilder::buildSkinPacket(const endstone::UUID &target_uuid,
                                               const std::string &skin_id,
                                               const RawSkinData &skin)
{
    BinaryStreamWriter writer;

    // 1. Target Player UUID (16 bytes)
    writer.writeBytes(target_uuid.begin(), endstone::UUID::size());

    // 2. SerializedSkin:
    // Skin ID
    writer.writeString(skin_id);
    // PlayFab ID
    writer.writeString("");
    // Resource Patch (specifying classic humanoid geometry)
    writer.writeString("{\"geometry\":{\"default\":\"geometry.humanoid.custom\"}}");

    // Skin Image
    writer.writeLUInt32(static_cast<uint32_t>(skin.width));
    writer.writeLUInt32(static_cast<uint32_t>(skin.height));
    writer.writeVarUInt32(static_cast<uint32_t>(skin.rgba_bytes.size()));
    writer.writeBytes(skin.rgba_bytes.data(), skin.rgba_bytes.size());

    // Animations count = 0
    writer.writeLUInt32(0);

    // Cape Image (empty)
    writer.writeLUInt32(0);
    writer.writeLUInt32(0);
    writer.writeVarUInt32(0);

    // Geometry Data
    writer.writeString("");
    // Geometry Version
    writer.writeString("");
    // Animation Data
    writer.writeString("");
    // Cape ID
    writer.writeString("");
    // Full Skin ID
    writer.writeString(skin_id);
    // Arm Size ("wide" or "slim")
    writer.writeString("wide");
    // Skin Color
    writer.writeString("");

    // Persona pieces count = 0
    writer.writeLUInt32(0);
    // Piece tint colors count = 0
    writer.writeLUInt32(0);

    // Flags
    writer.writeBool(false); // is_premium
    writer.writeBool(false); // is_persona
    writer.writeBool(false); // is_persona_cape_on_classic
    writer.writeBool(true);  // is_primary_user
    writer.writeBool(true);  // is_override

    // 3. Localized skin names
    writer.writeString("");
    writer.writeString("");

    return writer.getString();
}

}  // namespace zombie
