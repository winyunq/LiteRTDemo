# Pure-Python Werewolf reference

This directory is deliberately independent of Unreal Engine, Blueprint assets,
UMG, LiteRT-LM, and native model loading. It is the executable reference for
gameplay rules before those rules are wired to the product demo.

Implemented rules:

- Dynamic rosters from 10 players upward.
- Stable seats that are never removed or reordered.
- Scaled wolves, plus exactly one Seer, Guard, and Witch.
- Complete night → discussion → vote loop with no round cap.
- Automatic phase advancement after accepted actions.
- Wolves cannot attack wolves.
- Seer can inspect another living player and receives private memory.
- Guard cannot protect the same target on consecutive nights.
- Witch has one healing potion and one poison potion, and may take at most one
  potion action per night.
- Vote ties eliminate nobody.
- Village wins after all wolves die; wolves win at parity.

## UI input contract

Speech draft editing and speech submission are separate operations:

1. `edit_speech_draft(seat, text)` is always allowed for every known seat. The
   UI must therefore keep its text box enabled before the player's turn.
2. `submit_speech(seat)` is accepted only during that living player's speaking
   turn.
3. A rejected submission never clears or mutates the draft.
4. A successful submission consumes and clears the submitted draft.

Voting and night actions remain turn/phase gated and expose their legal targets
through `legal_targets`.

## Run tests

From this directory:

```powershell
python -m unittest -v
```

The suite includes deterministic rule tests and 60 complete seeded random
matches across 10, 11, 18, 25, and 50 player rosters.
