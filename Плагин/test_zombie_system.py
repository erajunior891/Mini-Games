from __future__ import annotations

import tempfile
import time
from pathlib import Path
from unittest.mock import MagicMock

from endstone_zombie_infection.models import InfectionStage, PlayerData, SafeZone, ZombieClassInfo
from endstone_zombie_infection.storage import StorageManager
from endstone_zombie_infection.manager import InfectionManager


def test_player_data_serialization():
    data = PlayerData(
        uuid="test-uuid-1234",
        name="Player1",
        stage=InfectionStage.INCUBATING,
        zombie_class="tank",
        infected_at=1000.0,
        infected_by="ZombieBoss",
        infect_count=3,
        incubation_remaining=30.0,
        respawn_remaining=25.0,
        in_respawn_cooldown=True,
    )
    raw = data.to_dict()
    restored = PlayerData.from_dict(raw)

    assert restored.uuid == "test-uuid-1234"
    assert restored.name == "Player1"
    assert restored.stage == InfectionStage.INCUBATING
    assert restored.zombie_class == "tank"
    assert restored.infected_at == 1000.0
    assert restored.infected_by == "ZombieBoss"
    assert restored.infect_count == 3
    assert restored.incubation_remaining == 30.0
    assert restored.respawn_remaining == 25.0
    assert restored.in_respawn_cooldown is True
    print("test_player_data_serialization PASSED")


def test_safe_zone():
    sphere_zone = SafeZone(
        name="spawn",
        shape="sphere",
        dimension="Overworld",
        center=[0.0, 64.0, 0.0],
        radius=20.0,
    )
    assert sphere_zone.contains(0.0, 64.0, 0.0, "Overworld") is True
    assert sphere_zone.contains(10.0, 64.0, 10.0, "Overworld") is True
    assert sphere_zone.contains(50.0, 64.0, 50.0, "Overworld") is False
    assert sphere_zone.contains(0.0, 64.0, 0.0, "Nether") is False

    box_zone = SafeZone(
        name="bunker",
        shape="box",
        dimension="Overworld",
        min_point=[-10.0, 0.0, -10.0],
        max_point=[10.0, 100.0, 10.0],
    )
    assert box_zone.contains(5.0, 50.0, 5.0, "Overworld") is True
    assert box_zone.contains(15.0, 50.0, 5.0, "Overworld") is False
    print("test_safe_zone PASSED")


def test_storage_atomic():
    with tempfile.TemporaryDirectory() as tmp_dir:
        storage = StorageManager(Path(tmp_dir))
        p = storage.get_player("p1", "Steve")
        p.stage = InfectionStage.ZOMBIE
        p.zombie_class = "runner"
        storage.set_player(p)

        # Re-load in a new instance
        storage2 = StorageManager(Path(tmp_dir))
        p_loaded = storage2.get_player("p1")
        assert p_loaded.stage == InfectionStage.ZOMBIE
        assert p_loaded.zombie_class == "runner"
        assert p_loaded.name == "Steve"
    print("test_storage_atomic PASSED")


def test_infection_chance_and_classes():
    config = {
        "infection": {
            "infect-chance": 0.75,
            "armor-reduction": {
                "leather": 0.03,
                "iron": 0.07,
                "diamond": 0.10,
                "netherite": 0.15,
            },
        },
        "classes": {
            "normal": {"display-name": "Обычный", "hp": 20, "speed": 1.0, "damage": 1.0, "weight": 40},
            "tank": {"display-name": "Танк", "hp": 40, "speed": 0.8, "damage": 1.1, "weight": 15},
            "infector": {"display-name": "Инфектор", "hp": 18, "speed": 1.0, "damage": 0.9, "weight": 10, "bonus-infect-chance": 0.15},
        },
        "safe-zones": {
            "enabled": True,
            "zones": [
                {"name": "spawn", "shape": "sphere", "dimension": "Overworld", "center": [0, 64, 0], "radius": 20}
            ]
        }
    }

    mock_server = MagicMock()
    with tempfile.TemporaryDirectory() as tmp_dir:
        storage = StorageManager(Path(tmp_dir))
        manager = InfectionManager(mock_server, storage, config)

        # Mock victim with netherite helmet + diamond chestplate
        mock_victim = MagicMock()
        mock_helmet = MagicMock()
        mock_helmet.type.id = "minecraft:netherite_helmet"
        mock_chest = MagicMock()
        mock_chest.type.id = "minecraft:diamond_chestplate"
        mock_victim.inventory.helmet = mock_helmet
        mock_victim.inventory.chestplate = mock_chest
        mock_victim.inventory.leggings = None
        mock_victim.inventory.boots = None

        # Chance without bonus: 0.75 - 0.15 (netherite) - 0.10 (diamond) = 0.50
        chance = manager.calculate_infection_chance(mock_victim)
        assert abs(chance - 0.50) < 1e-4

        # Chance with Infector class: 0.50 + 0.15 = 0.65
        chance_infector = manager.calculate_infection_chance(mock_victim, attacker_class="infector")
        assert abs(chance_infector - 0.65) < 1e-4

    print("test_infection_chance_and_classes PASSED")


def test_zombie_spawn():
    with tempfile.TemporaryDirectory() as tmp_dir:
        storage = StorageManager(Path(tmp_dir))
        assert storage.get_zombie_spawn() is None

        spawn_data = {
            "enabled": True,
            "x": 100.5,
            "y": 70.0,
            "z": -200.5,
            "dimension": "Overworld",
            "pitch": 10.0,
            "yaw": 180.0,
        }
        storage.set_zombie_spawn(spawn_data)

        # Load back
        storage2 = StorageManager(Path(tmp_dir))
        loaded = storage2.get_zombie_spawn()
        assert loaded is not None
        assert loaded["x"] == 100.5
        assert loaded["y"] == 70.0
        assert loaded["z"] == -200.5
        assert loaded["dimension"] == "Overworld"
    print("test_zombie_spawn PASSED")


if __name__ == "__main__":
    test_player_data_serialization()
    test_safe_zone()
    test_storage_atomic()
    test_infection_chance_and_classes()
    test_zombie_spawn()
    print("\nALL TESTS PASSED SUCCESSFULLY!")
