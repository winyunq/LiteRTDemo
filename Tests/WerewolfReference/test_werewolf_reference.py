from __future__ import annotations

import unittest

try:
    from .werewolf_reference import (
        NightStage,
        Phase,
        Role,
        Team,
        WerewolfGame,
        WitchAction,
        run_random_game,
    )
except ImportError:  # Supports running unittest directly from this directory.
    from werewolf_reference import (
        NightStage,
        Phase,
        Role,
        Team,
        WerewolfGame,
        WitchAction,
        run_random_game,
    )


FIXED_ROLES = [
    Role.WOLF,
    Role.WOLF,
    Role.WOLF,
    Role.SEER,
    Role.GUARD,
    Role.WITCH,
    Role.VILLAGER,
    Role.VILLAGER,
    Role.VILLAGER,
    Role.VILLAGER,
]


def finish_night(
    game: WerewolfGame,
    wolf_target: int = 6,
    guard_target: int = 7,
    seer_target: int = 0,
    witch_action: WitchAction = WitchAction.PASS,
    witch_target: int | None = None,
) -> None:
    while game.phase is Phase.NIGHT:
        actor = game.current_actor
        assert actor is not None
        if game.night_stage is NightStage.WOLVES:
            result = game.submit_wolf_target(actor, wolf_target)
        elif game.night_stage is NightStage.GUARD:
            target = guard_target
            if target not in game.legal_targets(actor):
                target = game.legal_targets(actor)[0]
            result = game.submit_guard_target(actor, target)
        elif game.night_stage is NightStage.SEER:
            target = seer_target
            if target not in game.legal_targets(actor):
                target = game.legal_targets(actor)[0]
            result = game.submit_seer_target(actor, target)
        elif game.night_stage is NightStage.WITCH:
            result = game.submit_witch_action(actor, witch_action, witch_target)
        else:
            raise AssertionError("unexpected night stage")
        assert result.accepted, result.reason


def finish_discussion(game: WerewolfGame) -> None:
    while game.phase is Phase.DISCUSSION:
        actor = game.current_actor
        assert actor is not None
        game.edit_speech_draft(actor, f"speech from {actor}")
        result = game.submit_speech(actor)
        assert result.accepted, result.reason


def vote_for(game: WerewolfGame, preferred_target: int) -> None:
    while game.phase is Phase.VOTE:
        actor = game.current_actor
        assert actor is not None
        target = preferred_target
        if target not in game.legal_targets(actor):
            target = game.legal_targets(actor)[0]
        result = game.submit_vote(actor, target)
        assert result.accepted, result.reason


class CompositionTests(unittest.TestCase):
    def test_dynamic_composition_for_ten_and_larger_games(self) -> None:
        for player_count in (10, 11, 18, 25, 50):
            with self.subTest(player_count=player_count):
                game = WerewolfGame.new(player_count, seed=player_count)
                roles = [player.role for player in game.players]
                self.assertEqual(len(roles), player_count)
                self.assertEqual(roles.count(Role.WOLF), max(3, player_count // 3))
                self.assertEqual(roles.count(Role.SEER), 1)
                self.assertEqual(roles.count(Role.GUARD), 1)
                self.assertEqual(roles.count(Role.WITCH), 1)
                self.assertEqual(
                    roles.count(Role.VILLAGER),
                    player_count - max(3, player_count // 3) - 3,
                )
                game.assert_invariants()

    def test_fewer_than_ten_players_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            WerewolfGame.new(9)


class NightRuleTests(unittest.TestCase):
    def test_wolves_cannot_target_wolves_or_act_out_of_turn(self) -> None:
        game = WerewolfGame.from_roles(FIXED_ROLES)
        self.assertEqual(game.night_stage, NightStage.WOLVES)
        self.assertEqual(game.current_actor, 0)
        self.assertNotIn(1, game.legal_targets(0))
        self.assertFalse(game.submit_wolf_target(1, 6).accepted)
        self.assertFalse(game.submit_wolf_target(0, 1).accepted)
        self.assertTrue(game.submit_wolf_target(0, 6).accepted)

    def test_seer_must_target_another_living_player_and_records_result(self) -> None:
        game = WerewolfGame.from_roles(FIXED_ROLES)
        while game.night_stage is NightStage.WOLVES:
            actor = game.current_actor
            self.assertTrue(game.submit_wolf_target(actor, 6).accepted)
        self.assertTrue(game.submit_guard_target(4, 7).accepted)
        self.assertEqual(game.current_actor, 3)
        self.assertNotIn(3, game.legal_targets(3))
        self.assertFalse(game.submit_seer_target(3, 3).accepted)
        self.assertTrue(game.submit_seer_target(3, 0).accepted)
        self.assertIn("seat 0 is wolf", game.players[3].private_memory[-1])

    def test_guard_cannot_protect_same_target_on_consecutive_nights(self) -> None:
        game = WerewolfGame.from_roles(FIXED_ROLES, seed=4)
        finish_night(game, wolf_target=6, guard_target=7)
        finish_discussion(game)
        vote_for(game, 8)
        self.assertEqual(game.phase, Phase.NIGHT)
        while game.night_stage is NightStage.WOLVES:
            actor = game.current_actor
            self.assertTrue(
                game.submit_wolf_target(actor, game.legal_targets(actor)[0]).accepted
            )
        self.assertEqual(game.night_stage, NightStage.GUARD)
        self.assertNotIn(7, game.legal_targets(4))
        self.assertFalse(game.submit_guard_target(4, 7).accepted)

    def test_witch_heal_and_poison_are_single_use_and_one_action_per_night(self) -> None:
        heal_game = WerewolfGame.from_roles(FIXED_ROLES)
        while heal_game.night_stage is NightStage.WOLVES:
            actor = heal_game.current_actor
            self.assertTrue(heal_game.submit_wolf_target(actor, 6).accepted)
        self.assertTrue(heal_game.submit_guard_target(4, 7).accepted)
        self.assertTrue(heal_game.submit_seer_target(3, 0).accepted)
        self.assertFalse(heal_game.submit_witch_action(5, WitchAction.HEAL, 7).accepted)
        self.assertTrue(heal_game.witch_heal_available)
        self.assertTrue(heal_game.submit_witch_action(5, WitchAction.HEAL, 6).accepted)
        self.assertFalse(heal_game.witch_heal_available)
        self.assertTrue(heal_game.players[6].alive)

        poison_game = WerewolfGame.from_roles(FIXED_ROLES)
        while poison_game.night_stage is NightStage.WOLVES:
            actor = poison_game.current_actor
            self.assertTrue(poison_game.submit_wolf_target(actor, 6).accepted)
        self.assertTrue(poison_game.submit_guard_target(4, 6).accepted)
        self.assertTrue(poison_game.submit_seer_target(3, 0).accepted)
        self.assertFalse(poison_game.submit_witch_action(5, WitchAction.POISON, 5).accepted)
        self.assertTrue(poison_game.witch_poison_available)
        self.assertTrue(poison_game.submit_witch_action(5, WitchAction.POISON, 7).accepted)
        self.assertFalse(poison_game.witch_poison_available)
        self.assertFalse(poison_game.players[7].alive)


class DraftAndDiscussionTests(unittest.TestCase):
    def test_draft_is_always_editable_and_rejected_submit_preserves_it(self) -> None:
        game = WerewolfGame.from_roles(FIXED_ROLES, seed=9)
        drafts = {
            seat: f"I prepared seat {seat}'s speech before the discussion."
            for seat in game.alive_seats
        }
        for seat, draft in drafts.items():
            self.assertTrue(game.edit_speech_draft(seat, draft).accepted)

        rejected_at_night = game.submit_speech(9)
        self.assertFalse(rejected_at_night.accepted)
        self.assertEqual(game.draft_of(9), drafts[9])

        finish_night(game, wolf_target=6, guard_target=6)
        self.assertEqual(game.phase, Phase.DISCUSSION)
        human = next(seat for seat in game.discussion_order if seat != game.current_actor)
        draft = drafts[human]
        rejected_before_turn = game.submit_speech(human)
        self.assertFalse(rejected_before_turn.accepted)
        self.assertEqual(game.draft_of(human), draft)

        while game.phase is Phase.DISCUSSION and game.current_actor != human:
            actor = game.current_actor
            game.edit_speech_draft(actor, f"speech {actor}")
            self.assertTrue(game.submit_speech(actor).accepted)
        self.assertEqual(game.current_actor, human)
        self.assertTrue(game.submit_speech(human).accepted)
        self.assertEqual(game.draft_of(human), "")
        self.assertEqual(game.speeches[-1].text, draft)

    def test_dead_player_may_keep_editing_but_cannot_submit(self) -> None:
        game = WerewolfGame.from_roles(FIXED_ROLES)
        finish_night(game, wolf_target=6, guard_target=7)
        self.assertFalse(game.players[6].alive)
        self.assertTrue(game.edit_speech_draft(6, "dead-seat draft remains editable").accepted)
        rejected = game.submit_speech(6)
        self.assertFalse(rejected.accepted)
        self.assertEqual(game.draft_of(6), "dead-seat draft remains editable")

    def test_empty_submit_is_rejected_without_mutating_draft(self) -> None:
        game = WerewolfGame.from_roles(FIXED_ROLES)
        finish_night(game, wolf_target=6, guard_target=6)
        actor = game.current_actor
        self.assertTrue(game.edit_speech_draft(actor, "   ").accepted)
        self.assertFalse(game.submit_speech(actor).accepted)
        self.assertEqual(game.draft_of(actor), "   ")


class FullMatchTests(unittest.TestCase):
    def test_actions_automatically_loop_until_village_victory(self) -> None:
        game = WerewolfGame.from_roles(FIXED_ROLES, seed=12)
        expected_night = 1
        for wolf in (0, 1, 2):
            self.assertEqual(game.night_number, expected_night)
            living_villagers = [
                seat
                for seat in game.alive_seats
                if game.role_of(seat) is Role.VILLAGER
            ]
            victim = living_villagers[-1]
            guard_target = living_villagers[0]
            if guard_target == game.previous_guard_target and len(living_villagers) > 1:
                guard_target = living_villagers[1]
            finish_night(
                game,
                wolf_target=victim,
                guard_target=guard_target,
                seer_target=wolf,
            )
            finish_discussion(game)
            vote_for(game, wolf)
            expected_night += 1
            if wolf != 2:
                self.assertEqual(game.phase, Phase.NIGHT)
        self.assertEqual(game.phase, Phase.GAME_OVER)
        self.assertEqual(game.winner, Team.VILLAGE)
        self.assertGreaterEqual(game.day_number, 3)
        self.assertTrue(game.edit_speech_draft(0, "editable after game over").accepted)
        self.assertEqual(game.draft_of(0), "editable after game over")

    def test_vote_tie_exiles_nobody_and_starts_next_night(self) -> None:
        game = WerewolfGame.from_roles(FIXED_ROLES, seed=2)
        finish_night(game, wolf_target=6, guard_target=6)
        finish_discussion(game)
        targets = [0, 1]
        index = 0
        while game.phase is Phase.VOTE:
            actor = game.current_actor
            target = targets[index % 2]
            if target == actor:
                target = targets[(index + 1) % 2]
            self.assertTrue(game.submit_vote(actor, target).accepted)
            index += 1
        self.assertIsNone(game.last_exiled)
        self.assertEqual(game.phase, Phase.NIGHT)

    def test_random_simulations_finish_for_dynamic_rosters(self) -> None:
        summaries = []
        for player_count in (10, 11, 18, 25, 50):
            for seed in range(12):
                summaries.append(run_random_game(player_count, seed))
        self.assertEqual(len(summaries), 60)
        self.assertTrue(all(summary.winner in (Team.WOLVES, Team.VILLAGE) for summary in summaries))
        self.assertTrue(all(summary.actions > 0 for summary in summaries))


if __name__ == "__main__":
    unittest.main(verbosity=2)
