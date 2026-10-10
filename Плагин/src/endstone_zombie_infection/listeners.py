import random
from typing import Optional

from endstone import Player, GameMode
from endstone.event import (
    ActorDamageEvent,
    PlayerDeathEvent,
    PlayerRespawnEvent,
    PlayerItemConsumeEvent,
    PlayerJoinEvent,
    PlayerQuitEvent,
    event_handler,
    EventPriority,
)

from endstone_zombie_infection.manager import InfectionManager
from endstone_zombie_infection.models import InfectionStage


class ZombieEventListener:
    def __init__(self, manager: InfectionManager):
        self.manager = manager
        self.server = manager.server
        self.config = manager.config

    @event_handler(priority=EventPriority.HIGHEST, ignore_cancelled=True)
    def on_actor_damage(self, event: ActorDamageEvent) -> None:
        victim = event.actor
        damager = event.damage_source.damaging_actor or event.damage_source.actor

        # Check friendly fire between zombie players
        if isinstance(victim, Player) and isinstance(damager, Player):
            victim_is_zombie = self.manager.is_infected(victim)
            damager_is_zombie = self.manager.is_infected(damager)

            if victim_is_zombie and damager_is_zombie:
                friendly_fire = self.config.get("infection", {}).get("zombie-friendly-fire", False)
                if not friendly_fire:
                    event.cancel()
                    return

        # Check zombie damaging victim
        if isinstance(damager, Player) and self.manager.is_infected(damager):
            # Apply class damage multiplier
            damager_data = self.manager.get_player_data(damager)
            cls_info = self.manager.classes.get(damager_data.zombie_class)
            if cls_info:
                event.damage = event.damage * cls_info.damage

        # Check infection transmission to human player
        if isinstance(victim, Player):
            # Check if damager is a valid infect source
            is_valid_source = False
            attacker_name = "Зомби"
            attacker_class: Optional[str] = None

            if isinstance(damager, Player) and self.manager.is_infected(damager):
                is_valid_source = True
                attacker_name = damager.name
                attacker_class = self.manager.get_player_data(damager).zombie_class
            elif damager is not None:
                # Check mob type
                mob_type = getattr(damager, "type", "")
                infect_sources = self.config.get("infection", {}).get("infect-sources", [])
                if any(src.lower() in str(mob_type).lower() for src in infect_sources):
                    is_valid_source = True
                    attacker_name = str(mob_type).replace("minecraft:", "").capitalize()

            if is_valid_source:
                victim_data = self.manager.get_player_data(victim)
                # If already incubating and lethal hit, mark for instant turn
                if victim_data.stage == InfectionStage.HUMAN:
                    chance = self.manager.calculate_infection_chance(victim, attacker_class)
                    if random.random() <= chance:
                        self.manager.start_incubation(victim, attacker_name)

    @event_handler(priority=EventPriority.NORMAL)
    def on_player_death(self, event: PlayerDeathEvent) -> None:
        player = event.player
        data = self.manager.get_player_data(player)

        # 1. Human died while incubating -> instant zombie transformation
        if data.stage == InfectionStage.INCUBATING:
            data.stage = InfectionStage.ZOMBIE
            data.incubation_remaining = 0.0
            self.manager.storage.set_player(data)

        # 2. Zombie player died -> trigger respawn cooldown
        if data.stage == InfectionStage.ZOMBIE:
            delay = float(self.config.get("infection", {}).get("zombie-respawn-delay", 25.0))
            data.in_respawn_cooldown = True
            data.respawn_remaining = delay
            self.manager.storage.set_player(data)

    @event_handler(priority=EventPriority.NORMAL)
    def on_player_respawn(self, event: PlayerRespawnEvent) -> None:
        player = event.player
        data = self.manager.get_player_data(player)

        if data.stage == InfectionStage.ZOMBIE:
            if data.in_respawn_cooldown:
                # Put in spectator mode during cooldown
                player.game_mode = GameMode.SPECTATOR
                tip = self.config.get("messages", {}).get(
                    "respawn-countdown", "§c☣ Респаун через: §e{seconds} §cсек"
                ).replace("{seconds}", str(int(data.respawn_remaining)))
                player.send_tip(tip)
            else:
                self.manager.apply_zombie_state(player)
                self.manager.teleport_to_zombie_spawn(player)

    @event_handler(priority=EventPriority.NORMAL)
    def on_player_consume(self, event: PlayerItemConsumeEvent) -> None:
        player = event.player
        if not self.manager.is_infected(player):
            return

        item = event.item
        if item is None or item.type is None:
            return

        item_id = str(item.type.id).lower()
        if "rotten_flesh" in item_id:
            heal_amount = int(self.config.get("infection", {}).get("rotten-flesh-heal", 4))
            new_health = min(int(player.max_health), int(player.health) + heal_amount)
            player.health = new_health
            self.manager.last_health[str(player.unique_id)] = new_health

            # Clear hunger effect applied by rotten flesh
            try:
                self.server.dispatch_command(
                    self.server.command_sender,
                    f'effect clear "{player.name}" hunger'
                )
                player.play_sound(player.location, "random.burp", 1.0, 1.0)
                player.spawn_particle("minecraft:villager_happy", player.location)
                player.send_tip(f"§a+ {int(heal_amount)} HP")
            except Exception:
                pass

    @event_handler(priority=EventPriority.NORMAL)
    def on_player_join(self, event: PlayerJoinEvent) -> None:
        self.manager.on_player_join(event.player)

    @event_handler(priority=EventPriority.NORMAL)
    def on_player_quit(self, event: PlayerQuitEvent) -> None:
        self.manager.on_player_quit(event.player)
