from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Optional, List, Dict, Any


class InfectionStage(str, Enum):
    HUMAN = "human"
    INCUBATING = "incubating"
    ZOMBIE = "zombie"


@dataclass
class ZombieClassInfo:
    name: str
    display_name: str
    hp: float
    speed: float  # multiplier relative to base 0.1 (e.g. 1.0 = 0.1, 0.8 = 0.08)
    damage: float  # multiplier for attack damage
    weight: int = 10
    bonus_infect_chance: float = 0.0


@dataclass
class PlayerData:
    uuid: str
    name: str
    stage: InfectionStage = InfectionStage.HUMAN
    zombie_class: Optional[str] = None
    infected_at: float = 0.0
    infected_by: Optional[str] = None
    infect_count: int = 0
    incubation_remaining: float = 0.0
    respawn_remaining: float = 0.0
    in_respawn_cooldown: bool = False

    def to_dict(self) -> Dict[str, Any]:
        return {
            "uuid": self.uuid,
            "name": self.name,
            "stage": self.stage.value,
            "zombie_class": self.zombie_class,
            "infected_at": self.infected_at,
            "infected_by": self.infected_by,
            "infect_count": self.infect_count,
            "incubation_remaining": self.incubation_remaining,
            "respawn_remaining": self.respawn_remaining,
            "in_respawn_cooldown": self.in_respawn_cooldown,
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> PlayerData:
        stage_str = data.get("stage", InfectionStage.HUMAN.value)
        try:
            stage = InfectionStage(stage_str)
        except ValueError:
            stage = InfectionStage.HUMAN

        return cls(
            uuid=data.get("uuid", ""),
            name=data.get("name", ""),
            stage=stage,
            zombie_class=data.get("zombie_class"),
            infected_at=float(data.get("infected_at", 0.0)),
            infected_by=data.get("infected_by"),
            infect_count=int(data.get("infect_count", 0)),
            incubation_remaining=float(data.get("incubation_remaining", 0.0)),
            respawn_remaining=float(data.get("respawn_remaining", 0.0)),
            in_respawn_cooldown=bool(data.get("in_respawn_cooldown", False)),
        )


@dataclass
class SafeZone:
    name: str
    shape: str  # "sphere" or "box"
    dimension: str
    center: List[float] = field(default_factory=lambda: [0.0, 64.0, 0.0])
    radius: float = 20.0
    min_point: List[float] = field(default_factory=lambda: [0.0, 0.0, 0.0])
    max_point: List[float] = field(default_factory=lambda: [0.0, 0.0, 0.0])

    def contains(self, x: float, y: float, z: float, dimension_name: str) -> bool:
        if self.dimension.lower() not in dimension_name.lower():
            return False

        if self.shape == "box":
            min_x, max_x = min(self.min_point[0], self.max_point[0]), max(self.min_point[0], self.max_point[0])
            min_y, max_y = min(self.min_point[1], self.max_point[1]), max(self.min_point[1], self.max_point[1])
            min_z, max_z = min(self.min_point[2], self.max_point[2]), max(self.min_point[2], self.max_point[2])
            return min_x <= x <= max_x and min_y <= y <= max_y and min_z <= z <= max_z
        else:
            # Sphere default
            cx, cy, cz = self.center[0], self.center[1], self.center[2]
            dist_sq = (x - cx) ** 2 + (y - cy) ** 2 + (z - cz) ** 2
            return dist_sq <= (self.radius ** 2)
