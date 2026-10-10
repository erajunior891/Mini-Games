import math
import random
import time
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any

import endstone
from endstone import Player, Server, GameMode
from endstone.level import Location

from endstone_zombie_infection.models import InfectionStage, PlayerData, SafeZone, ZombieClassInfo
from endstone_zombie_infection.storage import StorageManager


class InfectionManager:
    def __init__(self, server: Server, storage: StorageManager, config: Dict[str, Any]):
        self.server = server
        self.storage = storage
        self.config = config
        self.classes: Dict[str, ZombieClassInfo] = {}
        self.safe_zones: List[SafeZone] = []
        self.sun_exposure_ticks: Dict[str, int] = {}
        self.last_health: Dict[str, int] = {}
        self.load_config(config)

    def load_config(self, config: Dict[str, Any]) -> None:
        self.config = config
        self._load_classes()
        self._load_safe_zones()

    def _load_classes(self) -> None:
        self.classes.clear()
        classes_cfg = self.config.get("classes", {})
        for name, data in classes_cfg.items():
            if not isinstance(data, dict):
                continue
            self.classes[name] = ZombieClassInfo(
                name=name,
                display_name=data.get("display-name", name.capitalize()),
                hp=float(data.get("hp", 20.0)),
                speed=float(data.get("speed", 1.0)),
                damage=float(data.get("damage", 1.0)),
                weight=int(data.get("weight", 10)),
                bonus_infect_chance=float(data.get("bonus-infect-chance", 0.0)),
            )

        if not self.classes:
            self.classes["normal"] = ZombieClassInfo(
                name="normal",
                display_name="Обычный",
                hp=20.0,
                speed=1.0,
                damage=1.0,
                weight=40,
            )

    def _load_safe_zones(self) -> None:
        self.safe_zones.clear()
        sz_cfg = self.config.get("safe-zones", {})
        if not sz_cfg.get("enabled", True):
            return

        for zone in sz_cfg.get("zones", []):
            if not isinstance(zone, dict):
                continue
            self.safe_zones.append(
                SafeZone(
                    name=zone.get("name", "zone"),
                    shape=zone.get("shape", "sphere"),
                    dimension=zone.get("dimension", "Overworld"),
                    center=zone.get("center", [0.0, 64.0, 0.0]),
                    radius=float(zone.get("radius", 20.0)),
                    min_point=zone.get("min", [0.0, 0.0, 0.0]),
                    max_point=zone.get("max", [0.0, 0.0, 0.0]),
                )
            )

    def get_player_data(self, player: Player) -> PlayerData:
        return self.storage.get_player(str(player.unique_id), player.name)

    def is_infected(self, player: Player) -> bool:
        data = self.get_player_data(player)
        return data.stage == InfectionStage.ZOMBIE

    def is_incubating(self, player: Player) -> bool:
        data = self.get_player_data(player)
        return data.stage == InfectionStage.INCUBATING

    def is_in_safe_zone(self, location: Location) -> bool:
        dim_name = location.dimension.name if location.dimension else "Overworld"
        x, y, z = location.x, location.y, location.z
        for zone in self.safe_zones:
            if zone.contains(x, y, z, dim_name):
                return True
        return False

    def is_immune(self, player: Player) -> bool:
        if player.game_mode in (GameMode.CREATIVE, GameMode.SPECTATOR):
            return True
        if player.has_permission("zombie.immune"):
            return True
        if self.is_in_safe_zone(player.location):
            return True
        return False

    def can_be_infected(self, player: Player) -> bool:
        data = self.get_player_data(player)
        if data.stage != InfectionStage.HUMAN:
            return False
        if self.is_immune(player):
            return False

        max_percent = self.config.get("infection", {}).get("max-zombie-percent", 100)
        if max_percent < 100:
            online = self.server.online_players
            total_count = len(online)
            if total_count > 0:
                current_zombies = sum(1 for p in online if self.is_infected(p))
                max_allowed = math.ceil(total_count * (max_percent / 100.0))
                if current_zombies >= max_allowed:
                    return False

        return True

    def calculate_infection_chance(self, victim: Player, attacker_class: Optional[str] = None) -> float:
        base_chance = float(self.config.get("infection", {}).get("infect-chance", 0.75))

        if attacker_class and attacker_class in self.classes:
            base_chance += self.classes[attacker_class].bonus_infect_chance

        armor_reduction = self.config.get("infection", {}).get("armor-reduction", {})
        total_reduction = 0.0

        inv = victim.inventory
        armor_pieces = [inv.helmet, inv.chestplate, inv.leggings, inv.boots]
        for piece in armor_pieces:
            if piece is None or piece.type is None:
                continue
            item_id = str(piece.type.id).lower()
            for mat, red in armor_reduction.items():
                if mat in item_id:
                    total_reduction += float(red)
                    break

        final_chance = max(0.05, min(0.95, base_chance - total_reduction))
        return final_chance

    def select_random_class(self) -> str:
        weights = [c.weight for c in self.classes.values()]
        class_names = list(self.classes.keys())
        if not class_names:
            return "normal"
        selected = random.choices(class_names, weights=weights, k=1)[0]
        return selected

    def start_incubation(self, victim: Player, attacker_name: str, preset_class: Optional[str] = None) -> bool:
        if not self.can_be_infected(victim):
            return False

        inc_sec = float(self.config.get("infection", {}).get("incubation-seconds", 45))
        data = self.get_player_data(victim)
        data.stage = InfectionStage.INCUBATING
        data.infected_at = time.time()
        data.infected_by = attacker_name
        data.incubation_remaining = inc_sec
        data.zombie_class = preset_class or self.select_random_class()
        self.storage.set_player(data)

        # Broadcast incubation start or notify victim
        msg_template = self.config.get("messages", {}).get(
            "incubation-start", "§4[☣] §cВы были заражены! Инкубация: {seconds} сек..."
        )
        victim.send_message(msg_template.replace("{seconds}", str(int(inc_sec))))
        victim.send_title("§4[☣] ЗАРАЖЕНИЕ", "§cИнкубация началась...", 10, 40, 10)
        try:
            victim.play_sound(victim.location, "mob.zombie.say", 1.0, 0.8)
        except Exception:
            pass

        return True

    def transform_to_zombie(self, player: Player, preset_class: Optional[str] = None) -> None:
        data = self.get_player_data(player)
        data.stage = InfectionStage.ZOMBIE
        data.incubation_remaining = 0.0

        if preset_class and preset_class in self.classes:
            data.zombie_class = preset_class
        elif not data.zombie_class or data.zombie_class not in self.classes:
            data.zombie_class = self.select_random_class()

        self.storage.set_player(data)
        self.apply_zombie_state(player)

        # Notify & broadcast
        cls_info = self.classes.get(data.zombie_class, self.classes.get("normal"))
        cls_display = cls_info.display_name if cls_info else "Обычный"

        trans_msg = self.config.get("messages", {}).get(
            "transformation", "§4[☣] §cВы стали зомби! Класс: §e{class}"
        )
        player.send_message(trans_msg.replace("{class}", cls_display))
        player.send_title("§4ВЫ ЗОМБИ!", f"§eКласс: {cls_display}", 10, 70, 20)

        try:
            player.play_sound(player.location, "mob.zombie.death", 1.2, 0.6)
            player.spawn_particle("minecraft:villager_angry", player.location)
        except Exception:
            pass

        # Broadcast infection killfeed
        bcast = self.config.get("messages", {}).get(
            "infection-broadcast", "§4[☣] §c{victim} §4был заражён §c{attacker}§4!"
        )
        attacker_name = data.infected_by or "Зомби"
        full_msg = bcast.replace("{victim}", player.name).replace("{attacker}", attacker_name)
        self.server.broadcast_message(full_msg)

        # Increment attacker infect count if attacker is an online player
        if data.infected_by:
            for p in self.server.online_players:
                if p.name.lower() == data.infected_by.lower():
                    att_data = self.get_player_data(p)
                    att_data.infect_count += 1
                    self.storage.set_player(att_data)
                    break

    def apply_zombie_state(self, player: Player) -> None:
        data = self.get_player_data(player)
        if data.stage != InfectionStage.ZOMBIE:
            return

        cls_info = self.classes.get(data.zombie_class, self.classes.get("normal"))
        if not cls_info:
            cls_info = ZombieClassInfo("normal", "Обычный", 20.0, 1.0, 1.0)

        # Tags
        player.add_scoreboard_tag("zombie_infected")
        player.name_tag = f"§4[☣] §c{player.name}"
        player.score_tag = f"§e{cls_info.display_name}"

        # Attributes
        # Base Minecraft walk_speed is 0.1
        player.walk_speed = 0.1 * cls_info.speed
        target_hp = int(cls_info.hp)
        player.max_health = target_hp
        if player.health > target_hp or player.health <= 0:
            player.health = target_hp

        self.last_health[data.uuid] = int(player.health)

    def reset_player(self, player: Player) -> None:
        data = self.get_player_data(player)
        data.stage = InfectionStage.HUMAN
        data.zombie_class = None
        data.incubation_remaining = 0.0
        data.in_respawn_cooldown = False
        data.respawn_remaining = 0.0
        self.storage.set_player(data)

        # Remove tag and reset name
        player.remove_scoreboard_tag("zombie_infected")
        player.name_tag = player.name
        player.score_tag = ""

        # Reset stats
        player.walk_speed = 0.1
        player.max_health = 20
        player.health = 20
        if player.game_mode == GameMode.SPECTATOR:
            player.game_mode = GameMode.SURVIVAL

        reset_msg = self.config.get("messages", {}).get("zombie-reset", "§a[☣] Вы были вылечены!")
        player.send_message(reset_msg)
        player.send_title("§aИЗЛЕЧЕН", "§7Вы снова человек", 10, 40, 10)

    def change_class(self, player: Player, new_class: str) -> bool:
        if new_class not in self.classes:
            return False
        data = self.get_player_data(player)
        if data.stage != InfectionStage.ZOMBIE:
            return False
        data.zombie_class = new_class
        self.storage.set_player(data)
        self.apply_zombie_state(player)
        cls_info = self.classes[new_class]
        player.send_message(f"§a[☣] Ваш класс изменён на §e{cls_info.display_name}")
        return True

    def on_player_join(self, player: Player) -> None:
        data = self.get_player_data(player)
        now = time.time()

        if data.stage == InfectionStage.INCUBATING:
            pause_offline = self.config.get("infection", {}).get("pause-incubation-offline", False)
            if not pause_offline and data.infected_at > 0:
                elapsed = now - data.infected_at
                inc_sec = float(self.config.get("infection", {}).get("incubation-seconds", 45))
                if elapsed >= inc_sec:
                    self.transform_to_zombie(player)
                    return
                else:
                    data.incubation_remaining = inc_sec - elapsed
                    self.storage.set_player(data)

        elif data.stage == InfectionStage.ZOMBIE:
            if data.in_respawn_cooldown:
                # Player left while in respawn cooldown
                player.game_mode = GameMode.SPECTATOR
            else:
                self.apply_zombie_state(player)

    def on_player_quit(self, player: Player) -> None:
        self.storage.save()
        uid = str(player.unique_id)
        self.sun_exposure_ticks.pop(uid, None)
        self.last_health.pop(uid, None)

    def tick_sun_burning(self, player: Player) -> None:
        sun_cfg = self.config.get("sun", {})
        if not sun_cfg.get("enabled", True):
            return

        dim = player.dimension
        if not dim:
            return
        # Sun burns only in Overworld
        if "overworld" not in dim.name.lower():
            return

        level = player.level
        if not level:
            return
        day_time = level.time % 24000
        # Daylight is approximately 0 to 12000
        if day_time >= 12000:
            self.sun_exposure_ticks[str(player.unique_id)] = 0
            return

        if player.is_in_water or player.is_in_lava:
            self.sun_exposure_ticks[str(player.unique_id)] = 0
            return

        # Check solid roof above player
        loc = player.location
        highest_y = dim.get_highest_block_y_at(int(loc.x), int(loc.z))
        if loc.y < highest_y - 1:
            self.sun_exposure_ticks[str(player.unique_id)] = 0
            return

        # Check helmet
        helmet_protects = sun_cfg.get("helmet-protects", True)
        inv = player.inventory
        if helmet_protects and inv.helmet and inv.helmet.type:
            # Protected by helmet
            durability_loss = sun_cfg.get("helmet-durability-loss", True)
            if durability_loss:
                try:
                    helmet = inv.helmet
                    meta = helmet.item_meta
                    meta.damage = meta.damage + 1
                    max_dur = getattr(helmet.type, "max_durability", 0)
                    if max_dur > 0 and meta.damage >= max_dur:
                        inv.helmet = None
                        player.play_sound(player.location, "random.break", 1.0, 1.0)
                        player.send_tip("§c☣ Ваш шлем сгорел от солнца!")
                    else:
                        helmet.set_item_meta(meta)
                        inv.helmet = helmet
                except Exception:
                    pass
            self.sun_exposure_ticks[str(player.unique_id)] = 0
            return

        uid = str(player.unique_id)
        current_ticks = self.sun_exposure_ticks.get(uid, 0) + 20
        self.sun_exposure_ticks[uid] = current_ticks

        delay_ticks = int(sun_cfg.get("delay-ticks", 30))
        if current_ticks >= delay_ticks:
            dmg = int(sun_cfg.get("damage", 1))

            # 1. Visually ignite player by briefly placing fire at player's feet
            bx = math.floor(loc.x)
            by = math.floor(loc.y)
            bz = math.floor(loc.z)

            # Keep ensures we ONLY place fire if the block is currently air
            self.server.dispatch_command(
                self.server.command_sender,
                f"setblock {bx} {by} {bz} fire [] keep"
            )
            self.server.dispatch_command(
                self.server.command_sender,
                f"setblock {bx} {by + 1} {bz} fire [] keep"
            )

            # Schedule cleanup of the temporary fire block after 2 ticks (100ms)
            plugin = self.server.plugin_manager.get_plugin("zombie_infection")
            if plugin:
                def make_cleanup(x: int, y: int, z: int):
                    return lambda: (
                        self.server.dispatch_command(
                            self.server.command_sender,
                            f"setblock {x} {y} {z} air [] replace fire"
                        ),
                        self.server.dispatch_command(
                            self.server.command_sender,
                            f"setblock {x} {y + 1} {z} air [] replace fire"
                        )
                    )
                self.server.scheduler.run_task(plugin, make_cleanup(bx, by, bz), delay=2)

            # 2. Deal fire damage using Bedrock's /damage command
            try:
                self.server.dispatch_command(
                    self.server.command_sender,
                    f'damage "{player.name}" {dmg} fire'
                )
            except Exception:
                new_health = max(0, int(player.health) - dmg)
                player.health = new_health

            self.last_health[uid] = int(player.health)

            try:
                player.spawn_particle("minecraft:basic_flame_particle", player.location)
                player.spawn_particle("minecraft:mobflame_single", player.location)
                player.send_tip("§c☣ Солнце сжигает вас!")
            except Exception:
                pass

    def tick_incubation(self, player: Player, data: PlayerData) -> None:
        data.incubation_remaining -= 1.0
        sec_left = max(0, int(data.incubation_remaining))
        total_sec = float(self.config.get("infection", {}).get("incubation-seconds", 45))
        progress = int(((total_sec - sec_left) / max(1.0, total_sec)) * 100)

        # Actionbar progress
        prog_msg = self.config.get("messages", {}).get(
            "incubation-progress", "§c☣ Заражение: {percent}% §7| §c{seconds} сек"
        )
        tip = prog_msg.replace("{percent}", str(progress)).replace("{seconds}", str(sec_left))
        player.send_tip(tip)

        # Escalating effects
        if sec_left <= 30 and sec_left % 5 == 0:
            # Dispatch subtle nausea / slowness
            try:
                self.server.dispatch_command(
                    self.server.command_sender,
                    f'effect give "{player.name}" slowness 5 0 true'
                )
                if sec_left <= 15:
                    self.server.dispatch_command(
                        self.server.command_sender,
                        f'effect give "{player.name}" nausea 6 0 true'
                    )
            except Exception:
                pass

        if sec_left <= 0:
            self.transform_to_zombie(player)

    def tick_respawn_cooldown(self, player: Player, data: PlayerData) -> None:
        data.respawn_remaining -= 1.0
        sec_left = max(0, int(data.respawn_remaining))

        countdown_msg = self.config.get("messages", {}).get(
            "respawn-countdown", "§c☣ Респаун через: §e{seconds} §cсек"
        )
        tip = countdown_msg.replace("{seconds}", str(sec_left))
        player.send_tip(tip)

        if sec_left <= 0:
            data.in_respawn_cooldown = False
            data.respawn_remaining = 0.0
            self.storage.set_player(data)

            # Return to survival and re-apply zombie stats
            player.game_mode = GameMode.SURVIVAL
            self.apply_zombie_state(player)

            # Teleport to zombie spawn (or fallback to safe zone/world spawn)
            if not self.teleport_to_zombie_spawn(player):
                if self.safe_zones:
                    sz = self.safe_zones[0]
                    level = self.server.level
                    dim = level.get_dimension(sz.dimension) if level else None
                    if dim:
                        player.teleport(Location(dim, sz.center[0], sz.center[1], sz.center[2]))

            player.send_title("§aВОЗРОЖДЕНИЕ", "§eВы вернулись в игру!", 10, 40, 10)

    def get_zombie_spawn_location(self) -> Optional[Location]:
        # 1. From storage (saved by /zombie setspawn)
        data = self.storage.get_zombie_spawn()
        if not data:
            # 2. From config.toml
            data = self.config.get("zombie-spawn")

        if not data or not data.get("enabled", True):
            return None

        dim_name = data.get("dimension", "Overworld")
        level = self.server.level
        if not level:
            return None

        dim = level.get_dimension(dim_name)
        if not dim:
            dim = level.get_dimension("Overworld")
        if not dim:
            return None

        x = float(data.get("x", 0.0))
        y = float(data.get("y", 64.0))
        z = float(data.get("z", 0.0))
        pitch = float(data.get("pitch", 0.0))
        yaw = float(data.get("yaw", 0.0))
        return Location(dim, x, y, z, pitch, yaw)

    def set_zombie_spawn(
        self, x: float, y: float, z: float, dimension: str = "Overworld", pitch: float = 0.0, yaw: float = 0.0
    ) -> None:
        data = {
            "enabled": True,
            "x": round(x, 2),
            "y": round(y, 2),
            "z": round(z, 2),
            "dimension": dimension,
            "pitch": round(pitch, 2),
            "yaw": round(yaw, 2),
        }
        self.storage.set_zombie_spawn(data)
        self.config["zombie-spawn"] = data

    def teleport_to_zombie_spawn(self, player: Player) -> bool:
        loc = self.get_zombie_spawn_location()
        if loc:
            player.teleport(loc)
            return True
        return False

    def on_tick(self) -> None:
        """Called every 20 ticks (1 second) by scheduler task."""
        for player in self.server.online_players:
            if not player.is_valid:
                continue

            data = self.get_player_data(player)

            # 1. Incubation tick
            if data.stage == InfectionStage.INCUBATING:
                self.tick_incubation(player, data)

            # 2. Respawn cooldown tick
            elif data.in_respawn_cooldown:
                self.tick_respawn_cooldown(player, data)

            # 3. Zombie mechanics
            elif data.stage == InfectionStage.ZOMBIE:
                if player.game_mode == GameMode.SURVIVAL:
                    self.tick_sun_burning(player)

                    # Suppress natural health regeneration
                    uid = str(player.unique_id)
                    prev_hp = int(self.last_health.get(uid, player.health))
                    if player.health > prev_hp:
                        # Natural regen detected without rotten flesh
                        player.health = prev_hp
                    else:
                        self.last_health[uid] = int(player.health)
