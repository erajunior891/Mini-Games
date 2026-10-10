from pathlib import Path
from typing import List

from endstone import Player
from endstone.command import Command, CommandSender
from endstone.plugin import Plugin

from endstone_zombie_infection.commands import ZombieCommandHandler
from endstone_zombie_infection.listeners import ZombieEventListener
from endstone_zombie_infection.manager import InfectionManager
from endstone_zombie_infection.storage import StorageManager


class ZombieInfectionPlugin(Plugin):
    api_version = "0.11"
    version = "1.0.0"

    commands = {
        "zombie": {
            "description": "Zombie Infection System commands",
            "usages": [
                "/zombie",
                "/zombie info [player: string]",
                "/zombie infect <player: string> [class: string]",
                "/zombie reset <player: string>",
                "/zombie class <player: string> <class: string>",
                "/zombie setspawn [x: float] [y: float] [z: float]",
                "/zombie spawn [player: string]",
                "/zombie list",
                "/zombie reload",
            ],
            "permissions": ["zombie.command"],
        },
        "zombiespawn": {
            "description": "Quickly set or teleport to zombie spawn",
            "usages": [
                "/zombiespawn",
                "/zombiespawn set",
                "/zombiespawn <player: string>",
            ],
            "permissions": ["zombie.admin"],
        },
    }

    permissions = {
        "zombie.command": {
            "description": "Allows players to use /zombie info",
            "default": True,
        },
        "zombie.admin": {
            "description": "Allows admins to manage infections and classes",
            "default": "op",
        },
        "zombie.immune": {
            "description": "Grants immunity from zombie infection",
            "default": "op",
        },
    }

    def on_enable(self) -> None:
        self.save_default_config()

        data_dir = Path(self.data_folder)
        self.storage = StorageManager(data_dir)
        self.manager = InfectionManager(self.server, self.storage, self.config)

        self.listener = ZombieEventListener(self.manager)
        self.register_events(self.listener)

        self.command_handler = ZombieCommandHandler(self.manager, self)

        # 1-second (20 ticks) tick task for incubation, sun burning, cooldowns
        self.server.scheduler.run_task(self, self.manager.on_tick, delay=20, period=20)

        # 60-second periodic atomic save
        self.server.scheduler.run_task(self, self.storage.save, delay=1200, period=1200)

        self.logger.info("Zombie Infection System v1.0.0 enabled successfully!")

    def on_disable(self) -> None:
        if hasattr(self, "storage"):
            self.storage.save()
        self.logger.info("Zombie Infection System disabled.")

    def on_command(self, sender: CommandSender, command: Command, args: List[str]) -> bool:
        if command.name == "zombie":
            return self.command_handler.execute(sender, command, args)
        elif command.name == "zombiespawn":
            if args and args[0].lower() == "set":
                return self.command_handler._handle_setspawn(sender, args[1:])
            elif args:
                return self.command_handler._handle_spawn(sender, args)
            else:
                return self.command_handler._handle_spawn(sender, [])
        return False

    # Public API for external plugins
    def is_infected(self, player: Player) -> bool:
        return self.manager.is_infected(player)

    def is_incubating(self, player: Player) -> bool:
        return self.manager.is_incubating(player)

    def get_player_class(self, player: Player) -> str:
        data = self.manager.get_player_data(player)
        return data.zombie_class or "none"

    def reset_player(self, player: Player) -> None:
        self.manager.reset_player(player)
