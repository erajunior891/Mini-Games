#include "packets/actor_data_packet.h"
#include "packets/binary_stream_writer.h"

namespace zombie {

std::string ActorDataPacketBuilder::buildOnFirePacket(uint64_t runtime_id, bool on_fire)
{
    BinaryStreamWriter writer;

    // 1. Target entity runtime ID (VarUInt64)
    writer.writeVarUInt64(runtime_id);

    // 2. Metadata dictionary: count = 1 item
    writer.writeVarUInt32(1);

    // Data item key: ID 0 is ActorDataIDs::FLAGS (or Reserved0)
    writer.writeVarUInt32(0);
    // Data item type: 7 is TypeInt64 (Long)
    writer.writeVarUInt32(7);

    // Value: 64-bit flags bitmask where bit 0 is ActorFlags::ONFIRE
    int64_t flags = on_fire ? 1LL : 0LL;
    writer.writeVarInt64(flags);

    // 3. Synched properties: count = 0
    writer.writeVarUInt32(0); // int properties count
    writer.writeVarUInt32(0); // float properties count

    // 4. Tick (VarUInt64) = 0
    writer.writeVarUInt64(0);

    return writer.getString();
}

}  // namespace zombie
