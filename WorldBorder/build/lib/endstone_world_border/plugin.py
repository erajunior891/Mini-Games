import math
from typing import List, Tuple, Optional

from endstone import Player, GameMode
from endstone.command import Command, CommandSender
from endstone.event import PlayerMoveEvent, event_handler, EventPriority
from endstone.level import Location
from endstone.plugin import Plugin


class WorldBorderPlugin(Plugin):
    api_version = "0.11"
    version = "1.0.0"

    commands = {
        "worldborder": {
            "description": "World border management commands",
            "aliases": ["wb"],
            "usages": [
                "/worldborder",
                "/worldborder get",
                "/worldborder set <radius: float> [center_x: float] [center_z: float]",
                "/worldborder center <center_x: float> <center_z: float>",
                "/worldborder reload",
                "/worldborder bypass [player: string]",
            ],
            "permissions": ["worldborder.command"],
        }
    }

    permissions = {
        "worldborder.command": {
            "description": "Allows viewing world border info",
            "default": True,
        },
        "worldborder.admin": {
            "description": "Allows setting and configuring the world border",
            "default": "op",
        },
        "worldborder.bypass": {
            "description": "Allows bypassing the world border limits",
            "default": "op",
        },
    }

    def on_enable(self) -> None:
        self.save_default_config()
        self.register_events(self)
        self.bypassed_players: set[str] = set()
        self.last_warning_time: dict[str, float] = {}

        # 1-second check task for players outside border
        self.server.scheduler.run_task(self, self._tick_border_enforcement, delay=20, period=20)
        self.logger.info("Endstone World Border v1.0.0 enabled successfully!")

    def on_disable(self) -> None:
        self.logger.info("Endstone World Border disabled.")

    def get_distance_to_border(self, x: float, z: float) -> Tuple[float, bool]:
        """Returns (current_dist_metric, is_outside)."""
        border_cfg = self.config.get("border", {})
        cx = float(border_cfg.get("center-x", 0.0))
        cz = float(border_cfg.get("center-z", 0.0))
        radius = float(border_cfg.get("radius", 500.0))
        shape = str(border_cfg.get("shape", "square")).lower()

        dx = abs(x - cx)
        dz = abs(z - cz)

        if shape == "circle":
            dist = math.sqrt(dx * dx + dz * dz)
            return dist, dist > radius
        else:
            # Square boundary
            dist = max(dx, dz)
            return dist, dist > radius

    def get_safe_location(self, loc: Location) -> Location:
        border_cfg = self.config.get("border", {})
        cx = float(border_cfg.get("center-x", 0.0))
        cz = float(border_cfg.get("center-z", 0.0))
        radius = float(border_cfg.get("radius", 500.0))
        shape = str(border_cfg.get("shape", "square")).lower()
        margin = 2.0

        dx = loc.x - cx
        dz = loc.z - cz

        if shape == "circle":
            dist = math.sqrt(dx * dx + dz * dz)
            if dist > 0:
                safe_r = max(0.0, radius - margin)
                factor = safe_r / dist
                new_x = cx + dx * factor
                new_z = cz + dz * factor
            else:
                new_x, new_z = cx, cz
        else:
            max_coord = max(0.0, radius - margin)
            new_x = cx + max(-max_coord, min(max_coord, dx))
            new_z = cz + max(-max_coord, min(max_coord, dz))

        return Location(loc.dimension, new_x, loc.y, new_z, loc.pitch, loc.yaw)

    def is_player_bypassed(self, player: Player) -> bool:
        if player.name.lower() in self.bypassed_players:
            return True
        if player.has_permission("worldborder.bypass"):
            return True
        return False

    @event_handler(priority=EventPriority.HIGHEST, ignore_cancelled=True)
    def on_player_move(self, event: PlayerMoveEvent) -> None:
        if not self.config.get("border", {}).get("enabled", True):
            return

        player = event.player
        if self.is_player_bypassed(player):
            return

        to_loc = event.to_location
        dist, is_outside = self.get_distance_to_border(to_loc.x, to_loc.z)

        if is_outside:
            prevent_move = self.config.get("action", {}).get("prevent-move", True)
            if prevent_move:
                event.cancel()

                # Visual & audio feedback
                msg = self.config.get("messages", {}).get(
                    "border-reached", "§c⛔ Вы достигли границы мира! Дальше пройти нельзя."
                )
                player.send_tip(msg)

                try:
                    player.play_sound(player.location, "note.bass", 1.0, 0.5)
                    particle_name = self.config.get("particles", {}).get(
                        "particle-name", "minecraft:redstone_ore_dust_particle"
                    )
                    player.spawn_particle(particle_name, player.location)
                except Exception:
                    pass
                return

        # Check warning distance
        radius = float(self.config.get("border", {}).get("radius", 500.0))
        warn_dist = float(self.config.get("border", {}).get("warning-distance", 15.0))
        if radius - warn_dist <= dist <= radius:
            remaining = max(0, int(radius - dist))
            msg_tmpl = self.config.get("messages", {}).get(
                "warning-approaching", "§e⚠ Вы приближаетесь к границе мира! (до границы: {distance}м)"
            )
            player.send_tip(msg_tmpl.replace("{distance}", str(remaining)))

    def _tick_border_enforcement(self) -> None:
        if not self.config.get("border", {}).get("enabled", True):
            return

        action_cfg = self.config.get("action", {})
        teleport_safe = action_cfg.get("teleport-to-safe", True)
        damage_outside = action_cfg.get("damage-outside", True)
        dmg_amount = float(action_cfg.get("damage-amount", 2.0))

        for player in self.server.online_players:
            if not player.is_valid:
                continue
            if self.is_player_bypassed(player):
                continue

            loc = player.location
            dist, is_outside = self.get_distance_to_border(loc.x, loc.z)
            if is_outside:
                if teleport_safe:
                    safe_loc = self.get_safe_location(loc)
                    player.teleport(safe_loc)

                if damage_outside and player.game_mode == GameMode.SURVIVAL:
                    player.health = max(0, int(player.health) - int(dmg_amount))
                    dmg_msg = self.config.get("messages", {}).get(
                        "border-damage", "§c⛔ Вы находитесь за границей мира и получаете урон!"
                    )
                    player.send_tip(dmg_msg)
                    try:
                        player.play_sound(player.location, "game.player.hurt", 1.0, 1.0)
                    except Exception:
                        pass

    def on_command(self, sender: CommandSender, command: Command, args: List[str]) -> bool:
        if command.name not in ("worldborder", "wb"):
            return False

        if not args or args[0].lower() in ("help", "info", "get"):
            border_cfg = self.config.get("border", {})
            cx = border_cfg.get("center-x", 0.0)
            cz = border_cfg.get("center-z", 0.0)
            r = border_cfg.get("radius", 500.0)
            shape = border_cfg.get("shape", "square")
            msg = self.config.get("messages", {}).get("border-info", "")
            msg = msg.replace("{center_x}", str(cx)).replace("{center_z}", str(cz))
            msg = msg.replace("{radius}", str(r)).replace("{shape}", str(shape))
            sender.send_message(msg)
            if sender.has_permission("worldborder.admin"):
                sender.send_message("§7Использование: §e/wb set <радиус> [center_x] [center_z]")
                sender.send_message("§7Использование: §e/wb center <center_x> <center_z>")
                sender.send_message("§7Использование: §e/wb reload")
                sender.send_message("§7Использование: §e/wb bypass [игрок]")
            return True

        sub = args[0].lower()

        if sub == "set":
            if not sender.has_permission("worldborder.admin"):
                sender.send_error_message("§cУ вас нет прав для выполнения этой команды.")
                return True
            if len(args) < 2:
                sender.send_error_message("§cИспользование: /wb set <радиус> [center_x] [center_z]")
                return True
            try:
                new_radius = float(args[1])
                if new_radius <= 0:
                    sender.send_error_message("§cРадиус должен быть положительным числом.")
                    return True
                self.config["border"]["radius"] = new_radius

                if len(args) >= 4:
                    self.config["border"]["center-x"] = float(args[2])
                    self.config["border"]["center-z"] = float(args[3])

                self.save_config()

                cx = self.config["border"]["center-x"]
                cz = self.config["border"]["center-z"]
                set_msg = self.config.get("messages", {}).get("border-set", "")
                set_msg = set_msg.replace("{radius}", str(new_radius)).replace("{center_x}", str(cx)).replace("{center_z}", str(cz))
                sender.send_message(set_msg)
            except ValueError:
                sender.send_error_message("§cПожалуйста, введите корректные числа.")
            return True

        elif sub == "center":
            if not sender.has_permission("worldborder.admin"):
                sender.send_error_message("§cУ вас нет прав для выполнения этой команды.")
                return True
            if len(args) < 3:
                sender.send_error_message("§cИспользование: /wb center <center_x> <center_z>")
                return True
            try:
                cx = float(args[1])
                cz = float(args[2])
                self.config["border"]["center-x"] = cx
                self.config["border"]["center-z"] = cz
                self.save_config()
                sender.send_message(f"§a[Граница мира] §fНовый центр границы: §eX={cx}, Z={cz}")
            except ValueError:
                sender.send_error_message("§cПожалуйста, введите корректные координаты.")
            return True

        elif sub == "reload":
            if not sender.has_permission("worldborder.admin"):
                sender.send_error_message("§cУ вас нет прав для выполнения этой команды.")
                return True
            self.reload_config()
            rel_msg = self.config.get("messages", {}).get("border-reloaded", "§aКонфигурация перезагружена!")
            sender.send_message(rel_msg)
            return True

        elif sub == "bypass":
            if not sender.has_permission("worldborder.admin"):
                sender.send_error_message("§cУ вас нет прав для выполнения этой команды.")
                return True
            target_name = args[1].lower() if len(args) > 1 else sender.name.lower()
            if target_name in self.bypassed_players:
                self.bypassed_players.remove(target_name)
                sender.send_message(f"§e[Граница мира] §fИгнорирование границы для §c{target_name} §fотключено.")
            else:
                self.bypassed_players.add(target_name)
                sender.send_message(f"§a[Граница мира] §fИгнорирование границы для §e{target_name} §aвключено.")
            return True

        sender.send_error_message(f"§cНеизвестное действие '{sub}'. Используйте /wb для справки.")
        return True
