# LiteRTDemo v5.0.0 — 2026-07-14

This release turns LiteRTDemo into a Blueprint-only, playable Werewolf showcase for the private `fabLiteRTLMUnreal` plugin.

## Highlights

- Complete 10+ player Werewolf loop with randomized seats, names, roles, avatars, night actions, public discussion, visible ballots, elimination, and faction victory.
- One persistent `ULiteRtLmAgent` per AI seat. Agents share one strict-GPU model and one serial queue while retaining independent incremental memory.
- Free model-authored speech and decisions. There are no canned speeches, personality templates, default votes, random AI fallbacks, or host-authored role-play answers.
- Every AI action is committed through a real native tool call: `submit_werewolf_speech` for speech and `select_werewolf_target` for voting/night decisions.
- Public speeches, host events, deaths, revealed identities, and ballots are appended to later Agents as public context; role secrets remain private.
- Human interaction is limited to preparing/sending a speech and clicking eligible player cards. The Host advances the match automatically.
- Container-based UMG layout, clickable full player cards, dead-player treatment, active-speaker emphasis, reusable avatar message rows, and mobile-friendly sizing.
- Blueprint-only project: gameplay, orchestration, validation, and UI contain no project C++ module. Native inference remains inside the plugin.

## Runtime and reliability

- Automatic first-Agent model loading with strict GPU verification and no CPU fallback.
- Gemma 4 E2B profile: 32,000-token engine context, temperature 1.0, top-p 0.95, top-k 64, and random seed.
- Bounded retry keeps the current game node authoritative; invalid/missing tools or runtime failures cannot fabricate an action or silently advance state.
- Detailed full-conversation JSONL diagnostics are off by default and can be enabled from the setup UI.
- Disabled diagnostics perform no complete-context diagnostic copy, queueing, or writer-thread work. Enabled diagnostics use a 32 MB bounded queue, batched flush, and drop-on-overflow behavior.

## Packages

- Windows 11 Shipping ZIP, including the model and required GPU/native libraries.
- ARM64 Android single APK, including four lossless model chunks and no external OBB.
- All release parts and final reconstructed artifacts are SHA-256 verified by `Assemble-LiteRTDemo-v5.0.0.ps1`.

The public repository contains the Blueprint Demo and documentation. `Plugins/LiteRT-LM-Unreal` is a gitlink to the private `fabLiteRTLMUnreal` product repository.
