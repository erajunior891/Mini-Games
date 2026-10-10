from typing import Any, List, Optional

from endstone import Player
from endstone.command import Command, CommandSender

from endstone_zombie_infection.manager import InfectionManager
from endstone_zombie_infection.models import InfectionStage


class ZombieCommandHandler:
    def __init__(self, manager: InfectionManager, plugin: Any = None):
        self.manager = manager
        self.server = manager.server
        self.plugin = plugin

    def execute(self, sender: CommandSender, command: Command, args: List[str]) -> bool:
        if not args:
            self._send_help(sender)
            return True

        sub = args[0].lower()

        if sub == "infect":
            return self._handle_infect(sender, args[1:])
        elif sub == "reset":
            return self._handle_reset(sender, args[1:])
        elif sub == "class":
            return self._handle_class(sender, args[1:])
        elif sub == "setspawn":
            return self._handle_setspawn(sender, args[1:])
        elif sub == "spawn":
            return self._handle_spawn(sender, args[1:])
        elif sub == "info":
            return self._handle_info(sender, args[1:])
        elif sub == "list":
            return self._handle_list(sender)
        elif sub == "reload":
            return self._handle_reload(sender)
        else:
            self._send_help(sender)
            return True

    def _find_player(self, name: str) -> Optional[Player]:
        for p in self.server.online_players:
            if p.name.lower() == name.lower():
                return p
        return None

    def _send_help(self, sender: CommandSender) -> None:
        sender.send_message("§e--- Zombie Infection Команды ---")
        sender.send_message("§a/zombie info [игрок] §7- Статус и класс игрока")
        if sender.has_permission("zombie.admin"):
            sender.send_message("§c/zombie infect <игрок> [класс] §7- Заразить игрока")
            sender.send_message("§c/zombie reset <игрок> §7- Вылечить игрока")
            sender.send_message("§c/zombie class <игрок> <класс> §7- Сменить класс зомби")
            sender.send_message("§c/zombie setspawn [x y z] §7- Установить точку спавна зомби")
            sender.send_message("§c/zombie spawn [игрок] §7- Телепорт на спавн зомби")
            sender.send_message("§c/zombie list §7- Список заражённых")
            sender.send_message("§c/zombie reload §7- Перезагрузить конфиг")

    def _handle_infect(self, sender: CommandSender, args: List[str]) -> bool:
        if not sender.has_permission("zombie.admin"):
            sender.send_error_message("§cУ вас нет прав для выполнения этой команды.")
            return True

        if len(args) < 1:
            sender.send_error_message("§cИспользование: /zombie infect <игрок> [класс]")
            return True

        target_name = args[0]
        target = self._find_player(target_name)
        if not target:
            sender.send_error_message(f"§cИгрок '{target_name}' не найден в сети.")
            return True

        preset_class = args[1].lower() if len(args) > 1 else None
        if preset_class and preset_class not in self.manager.classes:
            classes_str = ", ".join(self.manager.classes.keys())
            sender.send_error_message(f"§cНеизвестный класс '{preset_class}'. Доступные: {classes_str}")
            return True

        self.manager.transform_to_zombie(target, preset_class=preset_class)
        cls_info = self.manager.classes.get(preset_class or self.manager.get_player_data(target).zombie_class)
        cls_name = cls_info.display_name if cls_info else ""
        sender.send_message(f"§a[☣] Игрок §e{target.name} §aбыл успешно заражён как §e{cls_name}§a!")
        return True

    def _handle_reset(self, sender: CommandSender, args: List[str]) -> bool:
        if not sender.has_permission("zombie.admin"):
            sender.send_error_message("§cУ вас нет прав для выполнения этой команды.")
            return True

        if len(args) < 1:
            sender.send_error_message("§cИспользование: /zombie reset <игрок>")
            return True

        target_name = args[0]
        target = self._find_player(target_name)
        if not target:
            sender.send_error_message(f"§cИгрок '{target_name}' не найден в сети.")
            return True

        self.manager.reset_player(target)
        sender.send_message(f"§a[☣] Игрок §e{target.name} §aбыл возвращён в состояние человека!")
        return True

    def _handle_class(self, sender: CommandSender, args: List[str]) -> bool:
        if not sender.has_permission("zombie.admin"):
            sender.send_error_message("§cУ вас нет прав для выполнения этой команды.")
            return True

        if len(args) < 2:
            sender.send_error_message("§cИспользование: /zombie class <игрок> <класс>")
            return True

        target_name = args[0]
        new_class = args[1].lower()
        target = self._find_player(target_name)
        if not target:
            sender.send_error_message(f"§cИгрок '{target_name}' не найден в сети.")
            return True

        if not self.manager.is_infected(target):
            sender.send_error_message(f"§cИгрок '{target.name}' не является зомби.")
            return True

        if not self.manager.change_class(target, new_class):
            classes_str = ", ".join(self.manager.classes.keys())
            sender.send_error_message(f"§cНеизвестный класс '{new_class}'. Доступные: {classes_str}")
            return True

        sender.send_message(f"§a[☣] Класс игрока §e{target.name} §aизменён на §e{new_class}§a!")
        return True

    def _handle_info(self, sender: CommandSender, args: List[str]) -> bool:
        target: Optional[Player] = None
        if args:
            target = self._find_player(args[0])
            if not target:
                sender.send_error_message(f"§cИгрок '{args[0]}' не найден в сети.")
                return True
        elif isinstance(sender, Player):
            target = sender
        else:
            sender.send_error_message("§cУкажите имя игрока: /zombie info <игрок>")
            return True

        data = self.manager.get_player_data(target)
        if data.stage == InfectionStage.HUMAN:
            sender.send_message(f"§e--- Информация о §a{target.name} §e---")
            sender.send_message("§7Статус: §aЧеловек")
        elif data.stage == InfectionStage.INCUBATING:
            sender.send_message(f"§e--- Информация о §e{target.name} §e---")
            sender.send_message("§7Статус: §eИнкубация")
            sender.send_message(f"§7Осталось: §c{int(data.incubation_remaining)} сек")
        else:
            cls_info = self.manager.classes.get(data.zombie_class)
            cls_display = cls_info.display_name if cls_info else "Неизвестно"
            sender.send_message(f"§e--- Информация о §c{target.name} §e---")
            sender.send_message("§7Статус: §cЗомби")
            sender.send_message(f"§7Класс: §e{cls_display}")
            sender.send_message(f"§7Заражений: §c{data.infect_count}")
        return True

    def _handle_list(self, sender: CommandSender) -> bool:
        if not sender.has_permission("zombie.admin"):
            sender.send_error_message("§cУ вас нет прав для выполнения этой команды.")
            return True

        zombies = []
        incubating = []
        for p in self.server.online_players:
            data = self.manager.get_player_data(p)
            if data.stage == InfectionStage.ZOMBIE:
                cls_info = self.manager.classes.get(data.zombie_class)
                cls_name = cls_info.display_name if cls_info else "Обычный"
                zombies.append(f"§c{p.name} §7(§e{cls_name}§7)")
            elif data.stage == InfectionStage.INCUBATING:
                incubating.append(f"§e{p.name} §7(§c{int(data.incubation_remaining)}с§7)")

        sender.send_message("§e--- Список заражённых ---")
        sender.send_message(f"§cЗомби ({len(zombies)}): " + (", ".join(zombies) if zombies else "§7нет"))
        sender.send_message(f"§eИнкубация ({len(incubating)}): " + (", ".join(incubating) if incubating else "§7нет"))
        return True

    def _handle_reload(self, sender: CommandSender) -> bool:
        if not sender.has_permission("zombie.admin"):
            sender.send_error_message("§cУ вас нет прав для выполнения этой команды.")
            return True

        if self.plugin:
            try:
                new_cfg = self.plugin.reload_config()
                self.manager.load_config(new_cfg)
                sender.send_message("§a[☣] Конфигурация успешно перезагружена!")
            except Exception as e:
                sender.send_error_message(f"§cОшибка при перезагрузке: {e}")
        return True

    def _handle_setspawn(self, sender: CommandSender, args: List[str]) -> bool:
        if not sender.has_permission("zombie.admin"):
            sender.send_error_message("§cУ вас нет прав для выполнения этой команды.")
            return True

        if len(args) >= 3:
            try:
                x = float(args[0])
                y = float(args[1])
                z = float(args[2])
                dim = (
                    args[3]
                    if len(args) > 3
                    else (sender.location.dimension.name if isinstance(sender, Player) else "Overworld")
                )
                pitch = 0.0
                yaw = 0.0
            except ValueError:
                sender.send_error_message("§cКоординаты должны быть числами: /zombie setspawn <x> <y> <z> [измерение]")
                return True
        elif isinstance(sender, Player):
            loc = sender.location
            x = loc.x
            y = loc.y
            z = loc.z
            dim = loc.dimension.name
            pitch = loc.pitch
            yaw = loc.yaw
        else:
            sender.send_error_message("§cИспользование из консоли: /zombie setspawn <x> <y> <z> [измерение]")
            return True

        self.manager.set_zombie_spawn(x, y, z, dim, pitch, yaw)
        sender.send_message(
            f"§a[☣] Точка спавна зомби успешно установлена: §e{x:.1f}, {y:.1f}, {z:.1f} §7({dim})"
        )
        return True

    def _handle_spawn(self, sender: CommandSender, args: List[str]) -> bool:
        if not sender.has_permission("zombie.admin"):
            sender.send_error_message("§cУ вас нет прав для выполнения этой команды.")
            return True

        target: Optional[Player] = None
        if args:
            target = self._find_player(args[0])
            if not target:
                sender.send_error_message(f"§cИгрок '{args[0]}' не найден.")
                return True
        elif isinstance(sender, Player):
            target = sender
        else:
            sender.send_error_message("§cИспользование из консоли: /zombie spawn <игрок>")
            return True

        if self.manager.teleport_to_zombie_spawn(target):
            target.send_message("§a[☣] Вы телепортированы на точку спавна зомби.")
            if sender != target:
                sender.send_message(f"§a[☣] Игрок {target.name} телепортирован на точку спавна зомби.")
        else:
            sender.send_error_message("§cТочка спавна зомби еще не установлена! Используйте /zombie setspawn")

        return True
