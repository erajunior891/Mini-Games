#include "fire_service.h"

#include <cmath>

#include <endstone/color_format.h>
#include <endstone/level/dimension.h>
#include <endstone/level/level.h>

namespace zombie {

FireService::FireService(endstone::Server &server) : server_(server) {}

bool FireService::shouldBurn(endstone::Player &player)
{
    // 1. Sun only burns in the Overworld
    auto &dim = player.getDimension();
    if (dim.getType() != endstone::Dimension::Type::Overworld) {
        return false;
    }

    // 2. Daytime check (ticks 0 to 12000 is day)
    int time = dim.getLevel().getTime() % 24000;
    if (time >= 12000) {
        return false;
    }

    // 3. Water extinguishes fire (but NOT lava — zombies ignore lava)
    if (player.isInWater()) {
        return false;
    }

    // 4. Sky exposure check (must not be beneath a solid roof = shadow)
    const auto &loc = player.getLocation();
    int highest_y = dim.getHighestBlockYAt(static_cast<int>(std::floor(loc.getX())),
                                          static_cast<int>(std::floor(loc.getZ())));
    if (loc.getY() < highest_y - 1) {
        return false;  // In shadow — safe
    }

    // 5. Helmet protection check
    auto &inv = player.getInventory();
    auto helmet = inv.getHelmet();
    if (helmet.has_value()) {
        return false;  // Wearing any helmet protects from sun
    }

    return true;
}

void FireService::setVisualFire(endstone::Player &player, bool on_fire)
{
    std::string payload = ActorDataPacketBuilder::buildOnFirePacket(player.getRuntimeId(), on_fire);

    // Пакет отсылается только самому горящему игроку (Зомби), чтобы у других не отображался огонь
    player.sendPacket(39, payload);
}

void FireService::tickPlayer(endstone::Player &player)
{
    bool burn = shouldBurn(player);
    std::string uuid = player.getUniqueId().str();
    bool was_burning = burning_state_[uuid];

    // Send packet ONLY when burning state changes (not every tick)
    if (burn != was_burning) {
        setVisualFire(player, burn);
        burning_state_[uuid] = burn;
    }

    // Apply 1 HP fire damage per tick when actively burning from the sun
    if (burn) {
        player.performCommand("damage @s 1 fire");
        player.sendTip(endstone::ColorFormat::Red + "☣ Солнце сжигает вас!");
    }
}

void FireService::onPlayerQuit(const endstone::Player &player)
{
    std::string uuid = player.getUniqueId().str();
    burning_state_.erase(uuid);
}

void FireService::syncAllBurningTo(endstone::Player &/*recipient*/)
{
    // Не рассылаем состояние горения другим игрокам, чтобы огонь видел только сам горящий
}

bool FireService::isBurning(const std::string &uuid) const
{
    auto it = burning_state_.find(uuid);
    return it != burning_state_.end() && it->second;
}

}  // namespace zombie

