"""Pure-Python reference implementation for the LiteRT-LM Werewolf demo."""

from .werewolf_reference import (
    ActionResult,
    NightStage,
    Phase,
    Role,
    SimulationSummary,
    Team,
    WerewolfGame,
    WitchAction,
    run_random_game,
)

__all__ = [
    "ActionResult",
    "NightStage",
    "Phase",
    "Role",
    "SimulationSummary",
    "Team",
    "WerewolfGame",
    "WitchAction",
    "run_random_game",
]
