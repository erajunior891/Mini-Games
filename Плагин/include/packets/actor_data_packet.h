#pragma once

#include <cstdint>
#include <string>

namespace zombie {

class ActorDataPacketBuilder {
public:
    /**
     * @brief Builds SetActorDataPacket (ID 39) payload setting the ONFIRE flag
     * @param runtime_id Actor runtime ID
     * @param on_fire True to show fire visual, false to remove
     * @return Raw packet payload bytes (excluding packet ID)
     */
    static std::string buildOnFirePacket(uint64_t runtime_id, bool on_fire);
};

}  // namespace zombie
