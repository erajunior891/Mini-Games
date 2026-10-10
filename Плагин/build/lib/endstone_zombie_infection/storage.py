from __future__ import annotations

import json
import os
import tempfile
import time
from pathlib import Path
from typing import Dict, Optional

from endstone_zombie_infection.models import PlayerData


class StorageManager:
    def __init__(self, data_folder: Path):
        self.data_folder = data_folder
        self.data_folder.mkdir(parents=True, exist_ok=True)
        self.players_file = self.data_folder / "players.json"
        self.players: Dict[str, PlayerData] = {}
        self.load()

    def get_player(self, uuid: str, name: str = "") -> PlayerData:
        if uuid not in self.players:
            self.players[uuid] = PlayerData(uuid=uuid, name=name)
        elif name and not self.players[uuid].name:
            self.players[uuid].name = name
        return self.players[uuid]

    def set_player(self, player_data: PlayerData) -> None:
        self.players[player_data.uuid] = player_data
        self.save()

    def load(self) -> None:
        if not self.players_file.exists():
            self.players = {}
            return

        try:
            with open(self.players_file, "r", encoding="utf-8") as f:
                raw_data = json.load(f)
                self.players = {
                    uuid: PlayerData.from_dict(p_data)
                    for uuid, p_data in raw_data.items()
                }
        except Exception as e:
            # Create a backup of corrupt file
            backup_path = self.data_folder / f"players_corrupt_{int(time.time())}.json"
            try:
                if self.players_file.exists():
                    os.replace(self.players_file, backup_path)
            except Exception:
                pass
            self.players = {}

    def save(self) -> None:
        """Atomic write to prevent corruption during crash/abrupt shutdown."""
        try:
            raw_data = {
                uuid: p_data.to_dict()
                for uuid, p_data in self.players.items()
            }
            dir_name = str(self.data_folder)
            with tempfile.NamedTemporaryFile("w", dir=dir_name, delete=False, encoding="utf-8") as tf:
                json.dump(raw_data, tf, indent=2, ensure_ascii=False)
                temp_name = tf.name

            os.replace(temp_name, self.players_file)
        except Exception as e:
            pass

    def get_zombie_spawn(self) -> Optional[Dict[str, Any]]:
        spawn_file = self.data_folder / "spawn.json"
        if not spawn_file.exists():
            return None
        try:
            with open(spawn_file, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return None

    def set_zombie_spawn(self, spawn_data: Dict[str, Any]) -> None:
        spawn_file = self.data_folder / "spawn.json"
        try:
            dir_name = str(self.data_folder)
            with tempfile.NamedTemporaryFile("w", dir=dir_name, delete=False, encoding="utf-8") as tf:
                json.dump(spawn_data, tf, indent=2, ensure_ascii=False)
                temp_name = tf.name
            os.replace(temp_name, spawn_file)
        except Exception:
            pass
