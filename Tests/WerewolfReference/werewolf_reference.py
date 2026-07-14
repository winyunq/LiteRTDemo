"""Deterministic, UI-independent reference state machine for Werewolf.

The module intentionally knows nothing about Unreal Engine, Blueprints, UMG, or
LiteRT-LM.  It defines the authoritative turn/target/resource rules that those
layers can drive.  Every accepted action advances the state machine as far as
possible; callers never manually advance a phase.

Speech input has two deliberately separate operations:

* ``edit_speech_draft`` is always allowed for a known player, regardless of the
  phase or whose turn it is.
* ``submit_speech`` is accepted only for the living current speaker during the
  discussion phase.  Rejection never clears or changes the draft.
"""

from __future__ import annotations

from collections import Counter
from dataclasses import dataclass, field
from enum import Enum
import random
from typing import Dict, List, Optional, Sequence, Tuple


class Role(str, Enum):
    WOLF = "wolf"
    SEER = "seer"
    GUARD = "guard"
    WITCH = "witch"
    VILLAGER = "villager"


class Team(str, Enum):
    WOLVES = "wolves"
    VILLAGE = "village"


class Phase(str, Enum):
    NIGHT = "night"
    DISCUSSION = "discussion"
    VOTE = "vote"
    GAME_OVER = "game_over"


class NightStage(str, Enum):
    WOLVES = "wolves"
    GUARD = "guard"
    SEER = "seer"
    WITCH = "witch"


class WitchAction(str, Enum):
    PASS = "pass"
    HEAL = "heal"
    POISON = "poison"


@dataclass(frozen=True)
class ActionResult:
    accepted: bool
    reason: str = ""

    def __bool__(self) -> bool:
        return self.accepted


@dataclass
class PlayerState:
    seat: int
    name: str
    role: Role
    alive: bool = True
    speech_draft: str = ""
    private_memory: List[str] = field(default_factory=list)

    @property
    def team(self) -> Team:
        return Team.WOLVES if self.role is Role.WOLF else Team.VILLAGE


@dataclass(frozen=True)
class SpeechRecord:
    day: int
    seat: int
    text: str


@dataclass(frozen=True)
class SimulationSummary:
    player_count: int
    seed: int
    winner: Team
    nights: int
    days: int
    actions: int


class WerewolfGame:
    """Complete Werewolf match state machine for ten or more stable seats."""

    MIN_PLAYERS = 10

    def __init__(self, roles: Sequence[Role], seed: int = 0) -> None:
        if len(roles) < self.MIN_PLAYERS:
            raise ValueError(f"Werewolf requires at least {self.MIN_PLAYERS} players")
        normalized_roles = [Role(role) for role in roles]
        if normalized_roles.count(Role.WOLF) < 1:
            raise ValueError("At least one wolf is required")
        for unique_role in (Role.SEER, Role.GUARD, Role.WITCH):
            if normalized_roles.count(unique_role) != 1:
                raise ValueError(f"Exactly one {unique_role.value} is required")

        self.seed = seed
        self._rng = random.Random(seed)
        self.players: List[PlayerState] = [
            PlayerState(seat=index, name=f"Player {index + 1}", role=role)
            for index, role in enumerate(normalized_roles)
        ]

        self.phase: Phase = Phase.NIGHT
        self.night_stage: Optional[NightStage] = None
        self.current_actor: Optional[int] = None
        self.winner: Optional[Team] = None
        self.night_number = 0
        self.day_number = 0

        self.witch_heal_available = True
        self.witch_poison_available = True
        self.previous_guard_target: Optional[int] = None

        self.wolf_victim: Optional[int] = None
        self.guard_target: Optional[int] = None
        self.seer_target: Optional[int] = None
        self.witch_healed_target: Optional[int] = None
        self.witch_poison_target: Optional[int] = None
        self.last_night_deaths: Tuple[int, ...] = ()
        self.last_exiled: Optional[int] = None

        self.speeches: List[SpeechRecord] = []
        self.events: List[Dict[str, object]] = []

        self._night_order: List[int] = []
        self._night_index = 0
        self._wolf_choices: Dict[int, int] = {}
        self._discussion_order: List[int] = []
        self._discussion_index = 0
        self._vote_order: List[int] = []
        self._vote_index = 0
        self._votes: Dict[int, int] = {}

        self._record("match_created", player_count=len(self.players), seed=seed)
        self._begin_night()
        self.assert_invariants()

    @classmethod
    def new(cls, player_count: int, seed: int = 0) -> "WerewolfGame":
        """Create a dynamically composed, shuffled match.

        The composition matches the demo's intended scaling: floor(N/3) wolves
        (at least three), one Seer, one Guard, one Witch, and villagers filling
        every remaining stable seat.
        """

        if player_count < cls.MIN_PLAYERS:
            raise ValueError(f"player_count must be at least {cls.MIN_PLAYERS}")
        wolf_count = min(max(3, player_count // 3), player_count - 4)
        roles: List[Role] = (
            [Role.WOLF] * wolf_count
            + [Role.SEER, Role.GUARD, Role.WITCH]
            + [Role.VILLAGER] * (player_count - wolf_count - 3)
        )
        random.Random(seed).shuffle(roles)
        return cls(roles, seed=seed ^ 0x5EED5EED)

    @classmethod
    def from_roles(cls, roles: Sequence[Role], seed: int = 0) -> "WerewolfGame":
        """Create a match with a fixed seat-to-role mapping for tests."""

        return cls(roles, seed=seed)

    @property
    def player_count(self) -> int:
        return len(self.players)

    @property
    def alive_seats(self) -> List[int]:
        return [player.seat for player in self.players if player.alive]

    @property
    def discussion_order(self) -> Tuple[int, ...]:
        return tuple(self._discussion_order)

    @property
    def vote_order(self) -> Tuple[int, ...]:
        return tuple(self._vote_order)

    @property
    def votes(self) -> Dict[int, int]:
        return dict(self._votes)

    def role_of(self, seat: int) -> Role:
        return self._player(seat).role

    def team_of(self, seat: int) -> Team:
        return self._player(seat).team

    def draft_of(self, seat: int) -> str:
        return self._player(seat).speech_draft

    def legal_targets(
        self,
        actor: int,
        witch_action: Optional[WitchAction] = None,
    ) -> List[int]:
        """Return legal targets for the actor's current action."""

        if self.winner is not None or self.current_actor != actor:
            return []
        player = self._player(actor)
        if not player.alive:
            return []

        alive = self.alive_seats
        if self.phase is Phase.NIGHT:
            if self.night_stage is NightStage.WOLVES and player.role is Role.WOLF:
                return [seat for seat in alive if self.role_of(seat) is not Role.WOLF]
            if self.night_stage is NightStage.GUARD and player.role is Role.GUARD:
                return [seat for seat in alive if seat != self.previous_guard_target]
            if self.night_stage is NightStage.SEER and player.role is Role.SEER:
                return [seat for seat in alive if seat != actor]
            if self.night_stage is NightStage.WITCH and player.role is Role.WITCH:
                action = WitchAction(witch_action) if witch_action is not None else None
                if action is WitchAction.HEAL:
                    if (
                        self.witch_heal_available
                        and self.wolf_victim is not None
                        and self._player(self.wolf_victim).alive
                    ):
                        return [self.wolf_victim]
                    return []
                if action is WitchAction.POISON and self.witch_poison_available:
                    return [seat for seat in alive if seat != actor]
                return []
        if self.phase is Phase.VOTE:
            return [seat for seat in alive if seat != actor]
        return []

    def edit_speech_draft(self, seat: int, text: str) -> ActionResult:
        """Edit a known player's draft at any time, without turn gating."""

        player = self._player_or_none(seat)
        if player is None:
            return ActionResult(False, "unknown player")
        player.speech_draft = text
        self._record("draft_edited", seat=seat, length=len(text))
        return ActionResult(True)

    def submit_speech(self, seat: int) -> ActionResult:
        """Submit the stored draft for the current living discussion speaker.

        Every rejection path returns before modifying the draft.
        """

        player = self._player_or_none(seat)
        if player is None:
            return ActionResult(False, "unknown player")
        if self.phase is not Phase.DISCUSSION:
            return ActionResult(False, "speech can only be submitted during discussion")
        if not player.alive:
            return ActionResult(False, "dead players cannot submit speech")
        if self.current_actor != seat:
            return ActionResult(False, "it is not this player's speaking turn")
        text = player.speech_draft.strip()
        if not text:
            return ActionResult(False, "speech draft is empty")

        self.speeches.append(SpeechRecord(self.day_number, seat, text))
        self._record("speech_submitted", day=self.day_number, seat=seat, text=text)
        player.speech_draft = ""
        self._discussion_index += 1
        if self._discussion_index >= len(self._discussion_order):
            self._begin_vote()
        else:
            self.current_actor = self._discussion_order[self._discussion_index]
        self.assert_invariants()
        return ActionResult(True)

    def submit_wolf_target(self, actor: int, target: int) -> ActionResult:
        error = self._validate_night_actor(actor, NightStage.WOLVES, Role.WOLF)
        if error:
            return ActionResult(False, error)
        if target not in self.legal_targets(actor):
            return ActionResult(False, "illegal wolf target")

        self._wolf_choices[actor] = target
        self._record("wolf_target", night=self.night_number, actor=actor, target=target)
        self._advance_night_actor()
        self.assert_invariants()
        return ActionResult(True)

    def submit_guard_target(self, actor: int, target: int) -> ActionResult:
        error = self._validate_night_actor(actor, NightStage.GUARD, Role.GUARD)
        if error:
            return ActionResult(False, error)
        if target not in self.legal_targets(actor):
            return ActionResult(False, "illegal guard target")

        self.guard_target = target
        self._record("guard_target", night=self.night_number, actor=actor, target=target)
        self._advance_night_actor()
        self.assert_invariants()
        return ActionResult(True)

    def submit_seer_target(self, actor: int, target: int) -> ActionResult:
        error = self._validate_night_actor(actor, NightStage.SEER, Role.SEER)
        if error:
            return ActionResult(False, error)
        if target not in self.legal_targets(actor):
            return ActionResult(False, "illegal seer target")

        self.seer_target = target
        result = "wolf" if self.role_of(target) is Role.WOLF else "good"
        self._player(actor).private_memory.append(
            f"Night {self.night_number}: seat {target} is {result}."
        )
        self._record(
            "seer_result",
            night=self.night_number,
            actor=actor,
            target=target,
            result=result,
        )
        self._advance_night_actor()
        self.assert_invariants()
        return ActionResult(True)

    def submit_witch_action(
        self,
        actor: int,
        action: WitchAction,
        target: Optional[int] = None,
    ) -> ActionResult:
        error = self._validate_night_actor(actor, NightStage.WITCH, Role.WITCH)
        if error:
            return ActionResult(False, error)
        action = WitchAction(action)

        if action is WitchAction.PASS:
            if target is not None:
                return ActionResult(False, "pass must not have a target")
        elif action is WitchAction.HEAL:
            if not self.witch_heal_available:
                return ActionResult(False, "healing potion is exhausted")
            if target not in self.legal_targets(actor, WitchAction.HEAL):
                return ActionResult(False, "witch may heal only tonight's wolf victim")
            self.witch_heal_available = False
            self.witch_healed_target = target
        elif action is WitchAction.POISON:
            if not self.witch_poison_available:
                return ActionResult(False, "poison potion is exhausted")
            if target not in self.legal_targets(actor, WitchAction.POISON):
                return ActionResult(False, "illegal poison target")
            self.witch_poison_available = False
            self.witch_poison_target = target
        else:  # pragma: no cover - Enum conversion rejects this first.
            return ActionResult(False, "unknown witch action")

        self._record(
            "witch_action",
            night=self.night_number,
            actor=actor,
            action=action.value,
            target=target,
        )
        self._advance_night_actor()
        self.assert_invariants()
        return ActionResult(True)

    def submit_vote(self, voter: int, target: int) -> ActionResult:
        player = self._player_or_none(voter)
        if player is None:
            return ActionResult(False, "unknown voter")
        if self.phase is not Phase.VOTE:
            return ActionResult(False, "voting is not active")
        if not player.alive:
            return ActionResult(False, "dead players cannot vote")
        if self.current_actor != voter:
            return ActionResult(False, "it is not this player's voting turn")
        if target not in self.legal_targets(voter):
            return ActionResult(False, "illegal vote target")

        self._votes[voter] = target
        self._record("vote", day=self.day_number, voter=voter, target=target)
        self._vote_index += 1
        if self._vote_index >= len(self._vote_order):
            self._resolve_vote()
        else:
            self.current_actor = self._vote_order[self._vote_index]
        self.assert_invariants()
        return ActionResult(True)

    def assert_invariants(self) -> None:
        seats = [player.seat for player in self.players]
        assert seats == list(range(self.player_count)), "stable seats must never be removed/reordered"
        assert len({player.seat for player in self.players}) == self.player_count
        assert isinstance(self.witch_heal_available, bool)
        assert isinstance(self.witch_poison_available, bool)
        if self.previous_guard_target is not None:
            assert 0 <= self.previous_guard_target < self.player_count

        if self.phase is Phase.GAME_OVER:
            assert self.winner is not None
            assert self.current_actor is None
        else:
            assert self.winner is None
            assert self.current_actor is not None
            assert self._player(self.current_actor).alive

        if self.phase is Phase.NIGHT:
            assert self.night_stage is not None
            actor_role = self._player(self.current_actor).role
            expected = {
                NightStage.WOLVES: Role.WOLF,
                NightStage.GUARD: Role.GUARD,
                NightStage.SEER: Role.SEER,
                NightStage.WITCH: Role.WITCH,
            }[self.night_stage]
            assert actor_role is expected
        if self.phase is Phase.DISCUSSION:
            assert self._discussion_order
            assert len(self._discussion_order) == len(set(self._discussion_order))
            assert self.current_actor == self._discussion_order[self._discussion_index]
        if self.phase is Phase.VOTE:
            assert self._vote_order
            assert len(self._vote_order) == len(set(self._vote_order))
            assert self.current_actor == self._vote_order[self._vote_index]
            assert set(self._votes).issubset(set(self._vote_order))

        if self.winner is Team.VILLAGE:
            assert not any(
                player.alive and player.role is Role.WOLF for player in self.players
            )
        if self.winner is Team.WOLVES:
            wolves = sum(
                player.alive and player.role is Role.WOLF for player in self.players
            )
            others = sum(
                player.alive and player.role is not Role.WOLF for player in self.players
            )
            assert wolves >= others

    def _player(self, seat: int) -> PlayerState:
        player = self._player_or_none(seat)
        if player is None:
            raise IndexError(f"unknown seat {seat}")
        return player

    def _player_or_none(self, seat: int) -> Optional[PlayerState]:
        if isinstance(seat, int) and 0 <= seat < self.player_count:
            return self.players[seat]
        return None

    def _record(self, kind: str, **payload: object) -> None:
        self.events.append({"kind": kind, **payload})

    def _validate_night_actor(
        self,
        actor: int,
        stage: NightStage,
        role: Role,
    ) -> str:
        player = self._player_or_none(actor)
        if player is None:
            return "unknown actor"
        if self.phase is not Phase.NIGHT or self.night_stage is not stage:
            return f"{stage.value} action is not active"
        if not player.alive:
            return "dead players cannot act"
        if player.role is not role:
            return f"actor is not the {role.value}"
        if self.current_actor != actor:
            return "it is not this player's turn"
        return ""

    def _begin_night(self) -> None:
        self.phase = Phase.NIGHT
        self.night_stage = None
        self.night_number += 1
        self.current_actor = None
        self.wolf_victim = None
        self.guard_target = None
        self.seer_target = None
        self.witch_healed_target = None
        self.witch_poison_target = None
        self.last_night_deaths = ()
        self.last_exiled = None
        self._wolf_choices.clear()
        self._record("night_started", night=self.night_number)
        self._set_night_stage(NightStage.WOLVES)

    def _set_night_stage(self, stage: NightStage) -> None:
        self.night_stage = stage
        if stage is NightStage.WOLVES:
            order = [
                player.seat
                for player in self.players
                if player.alive and player.role is Role.WOLF
            ]
        else:
            role = {
                NightStage.GUARD: Role.GUARD,
                NightStage.SEER: Role.SEER,
                NightStage.WITCH: Role.WITCH,
            }[stage]
            order = [
                player.seat
                for player in self.players
                if player.alive and player.role is role
            ]
        self._night_order = order
        self._night_index = 0
        self._record("night_stage", night=self.night_number, stage=stage.value)
        if order:
            self.current_actor = order[0]
        else:
            self._finish_night_stage()

    def _advance_night_actor(self) -> None:
        self._night_index += 1
        if self._night_index < len(self._night_order):
            self.current_actor = self._night_order[self._night_index]
        else:
            self._finish_night_stage()

    def _finish_night_stage(self) -> None:
        if self.night_stage is NightStage.WOLVES:
            if self._wolf_choices:
                counts = Counter(self._wolf_choices.values())
                max_votes = max(counts.values())
                self.wolf_victim = min(
                    target for target, count in counts.items() if count == max_votes
                )
            self._set_night_stage(NightStage.GUARD)
        elif self.night_stage is NightStage.GUARD:
            self._set_night_stage(NightStage.SEER)
        elif self.night_stage is NightStage.SEER:
            self._set_night_stage(NightStage.WITCH)
        elif self.night_stage is NightStage.WITCH:
            self._resolve_night()
        else:  # pragma: no cover - only reachable after internal corruption.
            raise AssertionError("night stage is missing")

    def _resolve_night(self) -> None:
        deaths = set()
        if self.wolf_victim is not None:
            protected = self.guard_target == self.wolf_victim
            healed = self.witch_healed_target == self.wolf_victim
            if not protected and not healed:
                deaths.add(self.wolf_victim)
        if self.witch_poison_target is not None:
            deaths.add(self.witch_poison_target)

        for seat in deaths:
            self._player(seat).alive = False
        if self.guard_target is not None:
            self.previous_guard_target = self.guard_target
        self.last_night_deaths = tuple(sorted(deaths))
        self._record(
            "night_resolved",
            night=self.night_number,
            wolf_victim=self.wolf_victim,
            guard_target=self.guard_target,
            healed=self.witch_healed_target,
            poisoned=self.witch_poison_target,
            deaths=self.last_night_deaths,
        )
        if not self._check_victory():
            self._begin_discussion()

    def _begin_discussion(self) -> None:
        self.phase = Phase.DISCUSSION
        self.night_stage = None
        self.day_number += 1
        self._discussion_order = self.alive_seats
        self._rng.shuffle(self._discussion_order)
        self._discussion_index = 0
        self.current_actor = self._discussion_order[0]
        self._record(
            "discussion_started",
            day=self.day_number,
            order=tuple(self._discussion_order),
        )

    def _begin_vote(self) -> None:
        self.phase = Phase.VOTE
        self._vote_order = self.alive_seats
        self._rng.shuffle(self._vote_order)
        self._vote_index = 0
        self._votes.clear()
        self.current_actor = self._vote_order[0]
        self._record("vote_started", day=self.day_number, order=tuple(self._vote_order))

    def _resolve_vote(self) -> None:
        counts = Counter(self._votes.values())
        exiled: Optional[int] = None
        if counts:
            max_votes = max(counts.values())
            leaders = [seat for seat, count in counts.items() if count == max_votes]
            if len(leaders) == 1:
                exiled = leaders[0]
                self._player(exiled).alive = False
        self.last_exiled = exiled
        self._record(
            "vote_resolved",
            day=self.day_number,
            votes=dict(self._votes),
            exiled=exiled,
        )
        if not self._check_victory():
            self._begin_night()

    def _check_victory(self) -> bool:
        wolves = sum(
            player.alive and player.role is Role.WOLF for player in self.players
        )
        others = sum(
            player.alive and player.role is not Role.WOLF for player in self.players
        )
        winner: Optional[Team] = None
        if wolves == 0:
            winner = Team.VILLAGE
        elif wolves >= others:
            winner = Team.WOLVES

        if winner is None:
            return False
        self.winner = winner
        self.phase = Phase.GAME_OVER
        self.night_stage = None
        self.current_actor = None
        self._record("game_over", winner=winner.value)
        return True


def run_random_game(
    player_count: int,
    seed: int,
    max_actions: int = 100_000,
) -> SimulationSummary:
    """Drive a complete match with legal random actions and invariant checks."""

    game = WerewolfGame.new(player_count, seed=seed)
    rng = random.Random(seed ^ 0xA11CE)
    actions = 0

    while game.winner is None:
        if actions >= max_actions:
            raise RuntimeError(
                f"simulation did not finish: players={player_count}, seed={seed}"
            )
        actor = game.current_actor
        if actor is None:
            raise AssertionError("non-terminal game has no current actor")

        if game.phase is Phase.NIGHT:
            if game.night_stage is NightStage.WOLVES:
                result = game.submit_wolf_target(actor, rng.choice(game.legal_targets(actor)))
            elif game.night_stage is NightStage.GUARD:
                result = game.submit_guard_target(actor, rng.choice(game.legal_targets(actor)))
            elif game.night_stage is NightStage.SEER:
                result = game.submit_seer_target(actor, rng.choice(game.legal_targets(actor)))
            elif game.night_stage is NightStage.WITCH:
                options = [WitchAction.PASS]
                if game.legal_targets(actor, WitchAction.HEAL):
                    options.append(WitchAction.HEAL)
                if game.legal_targets(actor, WitchAction.POISON):
                    options.append(WitchAction.POISON)
                action = rng.choice(options)
                targets = game.legal_targets(actor, action)
                result = game.submit_witch_action(
                    actor,
                    action,
                    rng.choice(targets) if targets else None,
                )
            else:  # pragma: no cover
                raise AssertionError("unknown night stage")
        elif game.phase is Phase.DISCUSSION:
            game.edit_speech_draft(actor, f"Day {game.day_number}, seat {actor} speaks")
            result = game.submit_speech(actor)
        elif game.phase is Phase.VOTE:
            result = game.submit_vote(actor, rng.choice(game.legal_targets(actor)))
        else:  # pragma: no cover
            raise AssertionError("unknown active phase")

        if not result.accepted:
            raise AssertionError(f"random driver generated illegal action: {result.reason}")
        actions += 1
        game.assert_invariants()

    return SimulationSummary(
        player_count=player_count,
        seed=seed,
        winner=game.winner,
        nights=game.night_number,
        days=game.day_number,
        actions=actions,
    )
