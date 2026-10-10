#include "world_border_plugin.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <endstone/color_format.h>
#include <endstone/scheduler/scheduler.h>
#include <endstone/server.h>

void WorldBorderPlugin::onLoad()
{
    getLogger().info("WorldBorder C++ loaded");
}

void WorldBorderPlugin::onEnable()
{
    loadConfigValues();

    // Register event listener
    registerEvent(&WorldBorderPlugin::onPlayerMove, *this, endstone::EventPriority::Highest, true);

    // Register 1-second (20 ticks) timer for enforcement outside border
    getServer().getScheduler().runTaskTimer(*this, [this]() { tickBorderEnforcement(); }, 20, 20);

    getLogger().info("WorldBorder C++ enabled successfully!");
}

void WorldBorderPlugin::onDisable()
{
    getLogger().info("WorldBorder C++ disabled");
}

void WorldBorderPlugin::loadConfigValues()
{
    // Default values if no custom config file parsed yet
    enabled_ = true;
    shape_ = "square";
    center_x_ = 0.0;
    center_z_ = 0.0;
    radius_ = 500.0;
    warning_distance_ = 15.0;
    prevent_move_ = true;
    knockback_ = true;
    knockback_strength_ = 1.2;
    teleport_to_safe_ = true;
    damage_outside_ = true;
    damage_amount_ = 2.0;
    particles_enabled_ = true;
    particle_name_ = "minecraft:redstone_ore_dust_particle";
}

std::pair<double, bool> WorldBorderPlugin::getDistanceToBorder(double x, double z) const
{
    double dx = std::abs(x - center_x_);
    double dz = std::abs(z - center_z_);

    if (shape_ == "circle") {
        double dist = std::sqrt(dx * dx + dz * dz);
        return {dist, dist > radius_};
    }

    // Default square
    double dist = std::max(dx, dz);
    return {dist, dist > radius_};
}

endstone::Location WorldBorderPlugin::getSafeLocation(const endstone::Location &loc) const
{
    double margin = 2.0;
    double dx = loc.getX() - center_x_;
    double dz = loc.getZ() - center_z_;

    double new_x = center_x_;
    double new_z = center_z_;

    if (shape_ == "circle") {
        double dist = std::sqrt(dx * dx + dz * dz);
        if (dist > 0.0) {
            double safe_r = std::max(0.0, radius_ - margin);
            double factor = safe_r / dist;
            new_x = center_x_ + dx * factor;
            new_z = center_z_ + dz * factor;
        }
    }
    else {
        double max_coord = std::max(0.0, radius_ - margin);
        new_x = center_x_ + std::max(-max_coord, std::min(max_coord, dx));
        new_z = center_z_ + std::max(-max_coord, std::min(max_coord, dz));
    }

    return {loc.getDimension(), static_cast<float>(new_x), loc.getY(), static_cast<float>(new_z), loc.getPitch(),
            loc.getYaw()};
}

bool WorldBorderPlugin::isPlayerBypassed(const endstone::Player &player) const
{
    std::string name = player.getName();
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
    if (bypassed_players_.contains(name)) {
        return true;
    }
    return player.hasPermission("worldborder.bypass");
}

void WorldBorderPlugin::onPlayerMove(endstone::PlayerMoveEvent &event)
{
    if (!enabled_) {
        return;
    }

    auto &player = event.getPlayer();
    if (isPlayerBypassed(player)) {
        return;
    }

    const auto &to_loc = event.getTo();
    auto [dist, is_outside] = getDistanceToBorder(to_loc.getX(), to_loc.getZ());

    if (is_outside) {
        if (prevent_move_) {
            event.cancel();
            player.sendTip(endstone::ColorFormat::Red +
                           "⛔ Вы достигли границы мира! Дальше пройти нельзя.");
            try {
                player.playSound(player.getLocation(), "note.bass", 1.0f, 0.5f);
                if (particles_enabled_) {
                    player.spawnParticle(particle_name_, player.getLocation());
                }
            }
            catch (...) {
            }
            return;
        }
    }

    // Warning distance check
    if (radius_ - warning_distance_ <= dist && dist <= radius_) {
        int remaining = std::max(0, static_cast<int>(radius_ - dist));
        player.sendTip(endstone::ColorFormat::Yellow + "⚠ Вы приближаетесь к границе мира! (до границы: " +
                       std::to_string(remaining) + "м)");
    }
}

void WorldBorderPlugin::tickBorderEnforcement()
{
    if (!enabled_) {
        return;
    }

    for (auto *player : getServer().getOnlinePlayers()) {
        if (!player || isPlayerBypassed(*player)) {
            continue;
        }

        const auto &loc = player->getLocation();
        auto [dist, is_outside] = getDistanceToBorder(loc.getX(), loc.getZ());

        if (is_outside) {
            if (teleport_to_safe_) {
                auto safe_loc = getSafeLocation(loc);
                player->teleport(safe_loc);
            }

            if (damage_outside_ && player->getGameMode() == endstone::GameMode::Survival) {
                int new_health = std::max(0, static_cast<int>(player->getHealth()) - static_cast<int>(damage_amount_));
                player->setHealth(new_health);
                player->sendTip(endstone::ColorFormat::Red +
                                "⛔ Вы находитесь за границей мира и получаете урон!");
                try {
                    player->playSound(player->getLocation(), "game.player.hurt", 1.0f, 1.0f);
                }
                catch (...) {
                }
            }
        }
    }
}

bool WorldBorderPlugin::onCommand(endstone::CommandSender &sender, const endstone::Command &command,
                                  const std::vector<std::string> &args)
{
    if (command.getName() != "worldborder" && command.getName() != "wb") {
        return false;
    }

    if (args.empty() || args[0] == "help" || args[0] == "info" || args[0] == "get") {
        sender.sendMessage(endstone::ColorFormat::Gold + "[Граница мира] " + endstone::ColorFormat::White +
                           "Центр: X=" + std::to_string(center_x_) + ", Z=" + std::to_string(center_z_) +
                           " | Радиус: " + std::to_string(radius_) + "м | Форма: " + shape_);
        if (sender.hasPermission("worldborder.admin")) {
            sender.sendMessage(endstone::ColorFormat::Gray + "Использование: /wb set <радиус> [center_x] [center_z]");
            sender.sendMessage(endstone::ColorFormat::Gray + "Использование: /wb center <center_x> <center_z>");
            sender.sendMessage(endstone::ColorFormat::Gray + "Использование: /wb bypass [игрок]");
        }
        return true;
    }

    std::string sub = args[0];
    std::transform(sub.begin(), sub.end(), sub.begin(), [](unsigned char c) { return std::tolower(c); });

    if (sub == "set") {
        if (!sender.hasPermission("worldborder.admin")) {
            sender.sendErrorMessage("У вас нет прав для выполнения этой команды.");
            return true;
        }
        if (args.size() < 2) {
            sender.sendErrorMessage("Использование: /wb set <радиус> [center_x] [center_z]");
            return true;
        }
        try {
            double new_radius = std::stod(args[1]);
            if (new_radius <= 0) {
                sender.sendErrorMessage("Радиус должен быть положительным числом.");
                return true;
            }
            radius_ = new_radius;
            if (args.size() >= 4) {
                center_x_ = std::stod(args[2]);
                center_z_ = std::stod(args[3]);
            }
            sender.sendMessage(endstone::ColorFormat::Green + "[Граница мира] Новый радиус: " +
                               std::to_string(radius_) + "м (Центр: X=" + std::to_string(center_x_) +
                               ", Z=" + std::to_string(center_z_) + ")");
        }
        catch (...) {
            sender.sendErrorMessage("Пожалуйста, введите корректные числа.");
        }
        return true;
    }
    else if (sub == "center") {
        if (!sender.hasPermission("worldborder.admin")) {
            sender.sendErrorMessage("У вас нет прав для выполнения этой команды.");
            return true;
        }
        if (args.size() < 3) {
            sender.sendErrorMessage("Использование: /wb center <center_x> <center_z>");
            return true;
        }
        try {
            center_x_ = std::stod(args[1]);
            center_z_ = std::stod(args[2]);
            sender.sendMessage(endstone::ColorFormat::Green + "[Граница мира] Новый центр: X=" +
                               std::to_string(center_x_) + ", Z=" + std::to_string(center_z_));
        }
        catch (...) {
            sender.sendErrorMessage("Пожалуйста, введите корректные числа.");
        }
        return true;
    }
    else if (sub == "bypass") {
        if (!sender.hasPermission("worldborder.admin")) {
            sender.sendErrorMessage("У вас нет прав для выполнения этой команды.");
            return true;
        }
        std::string target_name = (args.size() > 1) ? args[1] : sender.getName();
        std::transform(target_name.begin(), target_name.end(), target_name.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (bypassed_players_.contains(target_name)) {
            bypassed_players_.erase(target_name);
            sender.sendMessage(endstone::ColorFormat::Yellow + "[Граница мира] Игнорирование границы для " +
                               target_name + " отключено.");
        }
        else {
            bypassed_players_.insert(target_name);
            sender.sendMessage(endstone::ColorFormat::Green + "[Граница мира] Игнорирование границы для " +
                               target_name + " включено.");
        }
        return true;
    }

    sender.sendErrorMessage("Неизвестное действие. Используйте /wb для справки.");
    return true;
}

ENDSTONE_PLUGIN("world_border", "1.0.0", WorldBorderPlugin)
{
    description = "High-performance C++ World Border plugin";
    website = "https://github.com/EndstoneMC/endstone";
    authors = {"Endstone Developer"};
    prefix = "WorldBorder";

    permission("worldborder.command")
        .description("Allows viewing world border info")
        .default_(endstone::PermissionDefault::True);

    permission("worldborder.admin")
        .description("Allows setting and configuring world border")
        .default_(endstone::PermissionDefault::Operator);

    permission("worldborder.bypass")
        .description("Allows bypassing the world border limits")
        .default_(endstone::PermissionDefault::Operator);

    command("worldborder")
        .description("World border management commands")
        .aliases("wb")
        .usages("/worldborder",
                "/worldborder get",
                "/worldborder set <radius: float> [center_x: float] [center_z: float]",
                "/worldborder center <center_x: float> <center_z: float>",
                "/worldborder bypass [player: string]")
        .permissions("worldborder.command");
}
