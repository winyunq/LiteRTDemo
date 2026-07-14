# LiteRTDemo

LiteRTDemo is an Unreal Engine 5.8 teaching project for the `fabLiteRTLMUnreal` plugin. Its main showcase is a complete, Blueprint-authored Werewolf game driven by local Gemma inference.

The demo is deliberately more than a chat box: it demonstrates automatic model loading, persistent Agent memory, multi-agent request orchestration, native tool calls, streamed callbacks, failure isolation, and a mobile-friendly UMG workflow in one playable example.

“Blueprint-authored” refers to the gameplay and orchestration layer. Gemma inference still runs through the LiteRT-LM native library and the Unreal plugin wrapper.

## What the demo teaches

### Strict GPU configuration and automatic loading

The shared model is configured in `Project Settings > Plugins > LiteRT-LM`. The first `New LiteRT-LM Agent` automatically starts loading `Content/Models/gemma-4-E2B-it.litertlm`; `Ask` also triggers loading if necessary. Gameplay Blueprints do not need an explicit initialization node.

There is no CPU mode, retry, or fallback branch. This Gemma 4 E2B artifact is configured for a 32,000-token context with the official `Temperature=1.0`, `TopP=0.95`, `TopK=64`, and random seed settings. Strict loading and every accepted request must prove the configured and resolved backend is GPU.

### One persistent Agent per AI player

At match setup, Blueprint creates one `ULiteRtLmAgent` for each AI seat. All Agents share one model and one physical GPU queue, but each keeps independent canonical memory. Switching Agents rebuilds the current native Conversation from that Agent's memory; no model is copied and inference never runs concurrently.

Public speeches, host announcements, deaths, and individual ballots are appended once to every Agent allowed to see them. Role identity, wolf teammates, seer results, witch resources, and unresolved night actions remain private. The per-turn `Ask` contains only the current natural-language task and legal options instead of a personality script.

The showcase declares exactly two native tools for every Agent:

- `submit_werewolf_speech(speech)` commits the model's final public speech;
- `select_werewolf_target(seat)` commits exile votes and night decisions. Witch may use `seat=0` to decline both potions.

Blueprint accepts only real entries in `Result.ToolCalls`, parses typed arguments, and validates phase, actor, alive state, frozen candidates, role permissions, and consumable resources before changing game state. Plain text that resembles JSON cannot impersonate a tool call.

There are no canned lines, personality/strategy templates, example speeches, mandatory talking points, default ballots, or host-authored role-play answers. If the expected tool is missing or invalid, the same game node remains active and the Agent receives another direct reminder; exhausting the infrastructure retry budget pauses with a visible reason instead of inventing an action.

The runtime is single-flight, so speeches and decisions use a fixed FIFO seat order. Every accepted ballot is immediately shown as a public “voter → target” message and broadcast to later Agents; the frozen candidate set still provides the authoritative rule boundary.

### Reliability instead of scripted impersonation

AI output is only a proposal. Blueprint parses it, validates the phase, actor, epoch, target, alive state, role permissions, and frozen snapshot, and only then mutates authoritative game state.

If loading, inference, callbacks, JSON parsing, or rule validation fails:

- the current actor remains at the head of the queue;
- deaths, votes, role resources, and phase state remain unchanged;
- retry is bounded and remains GPU-configured;
- structural/runtime failure pauses the phase after the bounded retry and is recorded in the Unreal log plus optional detailed diagnostics.

There are no canned AI speeches, random fallback votes, random kills, or fast phase advancement that could disguise a failed model request as live inference.

An exact normalized duplicate is treated as a model-quality signal rather than a native crash. The Core spends one quality retry; if the model still repeats, it displays that real model output and continues the match with reason `speech_accepted_duplicate_after_retry`. The diversity acceptance test still fails such a run, so continuation never becomes evidence that output quality passed.

### Bounded performance diagnostics

Detailed JSONL tracing is **off by default**. The setup screen exposes `记录详细对话（调试）`; enabling it calls the plugin's runtime `Set Detailed Diagnostics Enabled` node. When disabled, the runtime does not copy complete Agent context into diagnostic records, does not keep a diagnostic queue, and does not run a writer thread.

When explicitly enabled, the private trace records the Agent's complete visible memory, current request, model response, token/timing/backend evidence, structured errors, and current/peak process memory. The writer is isolated from the serial inference worker, flushes once per drained batch, caps queued records at 32 MB, and drops excess diagnostic records instead of allowing logging to consume unbounded memory. A fixed 32-request trend window reports prompt/output/elapsed and peak-memory growth.

KV-cache size telemetry is deliberately unavailable in the request path: `kv_cache_size_telemetry_available=false` with reason `native_size_query_materializes_full_cache_disabled_in_request_path`. The current native `GetKVCache(nullptr, &size)` materializes/copies the full cache, so using it as a harmless size probe can itself create the memory spike being investigated. The plugin does not call it on reset, submit, or completion hot paths. A future wrapper should expose a separate O(1), non-materializing cache-size query.

On Windows, DXGI can report this process's local-segment usage and budget. That is process resource telemetry, not proof that a particular LiteRT operator executed on the GPU. Android currently reports GPU-VRAM telemetry unavailable instead of inventing a value. Configured/resolved backend evidence and hardware resource/operator evidence remain separate.

On Windows, diagnostics are written below the project's `Saved/LiteRTLM` directory. Android uses the app's persistent download/external-files directory. The current path is published only while detailed diagnostics are enabled and should be retrieved with `adb pull` for private debugging; it is never part of the public game feed.

## The Werewolf showcase

- Player count starts at 10 and is stored automatically. The UI imposes no fixed product maximum; roster and state are generated from runtime arrays.
- Seats, the human seat, roles, and avatars are randomized. Role preference defaults to Random and is remembered.
- Villager, Werewolf, Seer, Guard, and Witch behavior is supported.
- Night, discussion, vote, elimination, and victory evaluation repeat until a faction actually wins; there is no artificial day/night limit.
- Every AI acts through an individual FIFO request rather than one aggregate response for the whole table.
- Every accepted ballot publicly shows who voted for whom and becomes visible context for later Agents.
- Setup has independent, default-off thinking switches for public speech and daytime exile voting; night skills remain non-thinking.
- During a match, the human only submits their own speech or taps an eligible player card for voting and role-specific night actions. The host advances the game automatically.
- Player cards and message rows are reusable UMG components with avatars and container-based Auto/Fill sizing. The showcase UI does not depend on a Canvas Panel.
- The player-facing interface keeps Engine/backend/tools evidence but never shows an active request id, AI seat, hidden target, thought text, or raw error. Complete traces exist only when the user explicitly enables detailed diagnostics.

## Run the project

1. Clone the public Demo repository:

   ```bash
   git clone https://github.com/winyunq/LiteRTDemo.git
   ```

2. The gameplay project is open source, while `Plugins/LiteRT-LM-Unreal` points to the private `fabLiteRTLMUnreal` submodule. Authorized plugin developers can run `git submodule update --init`; everyone else can use the packaged Windows/Android release without plugin source access.
3. Place the Gemma artifact at `Content/Models/gemma-4-E2B-it.litertlm`. The multi-gigabyte model is intentionally excluded from Git source and is bundled in the packaged releases.
4. Open `LiteRTDemo.uproject` with Unreal Engine 5.8.
5. Play `/Game/WerewolfShowcase/Maps/L_WerewolfShowcase`.
6. On the setup page, choose the player count, optional role preference, and whether speech/voting should use thinking. Leave detailed diagnostics off for normal play; enable it only when a complete conversation trace is needed. Wait for automatic strict-GPU model preparation to finish.
7. Start the match. Speak when prompted and tap a player card when the current human action requires a target.

The player never needs an Advance, Skip AI, Next Phase, or inference-configuration control.

## Learn the plugin API

The [Blueprint integration tutorial](DemoDocs/WerewolfShowcase/LITERTLM_BLUEPRINT_TUTORIAL_zh.md) explains:

- automatic Gemma project-model loading;
- strict GPU configuration;
- one persistent `ULiteRtLmAgent` per AI player;
- public/private memory broadcasting;
- `submit_werewolf_speech` and `select_werewolf_target` tool handling;
- Blueprint FIFO orchestration;
- Blueprint rule validation and safe state mutation;
- per-request thinking and diagnostic trace format.

The [ABI v2 runtime tutorial](DemoDocs/WerewolfShowcase/LITERTLM_ABI_V2_RUNTIME_BLUEPRINT_TUTORIAL_zh.md) documents strict GPU evidence, initial ToolsJson loading, Engine/Conversation generations, real native reset semantics, RuntimeStatus evidence levels, and Android JSONL diagnostics.

The [plugin reliability issue log](DemoDocs/WerewolfShowcase/LITERTLM_PLUGIN_ISSUES_zh.md) records the native-context bug found through this demo, the implemented ABI v2 reset/loader/status/logging fixes, and open work such as plugin-level FIFO, structured errors, Android lifecycle recovery, token telemetry, and hardware execution telemetry.

For the game-specific walkthrough and acceptance checklist, see the [Chinese Werewolf showcase guide](DemoDocs/WerewolfShowcase/README_zh.md).

## Android packaging

The repository includes a reproducible single-APK packaging entry point:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\Scripts\Package-AndroidSingleApk.ps1 `
  -Configuration Shipping
```

The model is losslessly split for Android packaging and reassembled in app-persistent storage before loading. The packaging script also extracts the wrapper and core LiteRT library from the final signed APK and verifies the stable `LiteRtLm_GetApi` / `LiteRtCreateModelFromFd` symbol contract. Packaging prerequisites, ordering, validation, installation, JSONL retrieval, and optional Logcat checks are documented in [ANDROID_SINGLE_APK_zh.md](DemoDocs/WerewolfShowcase/ANDROID_SINGLE_APK_zh.md). Treat a newly produced APK as a build artifact that must pass those checks and real-device strict-backend testing; this README does not identify an in-progress package as a release.

## v5.0.0 packaged downloads

The `v5.0.0` GitHub Release contains the complete Windows 11 package and single-file ARM64 Android APK. Because each complete artifact exceeds GitHub's per-asset size limit, the release stores verified parts plus a small assembly script. Download that script and run:

```powershell
powershell -ExecutionPolicy Bypass -File .\Assemble-LiteRTDemo-v5.0.0.ps1 -Target Android -Download
powershell -ExecutionPolicy Bypass -File .\Assemble-LiteRTDemo-v5.0.0.ps1 -Target Windows -Download
```

The script downloads every required part, verifies each SHA-256, reconstructs the APK or ZIP, and verifies the final SHA-256 before reporting success. `SHA256SUMS.txt` is also included for independent verification.

The dated [2026-07-11 identity/diversity/privacy build record](DemoDocs/WerewolfShowcase/ANDROID_DIVERSITY_FIX_BUILD_RECORD_2026-07-11.md), [2026-07-11 initial ABI v2/v5 record](DemoDocs/WerewolfShowcase/ANDROID_ABI_V2_V5_BUILD_RECORD_2026-07-11.md), and [2026-07-10 ABI v1 record](DemoDocs/WerewolfShowcase/ANDROID_V6_BUILD_RECORD_2026-07-10.md) describe historical artifacts only. They do not contain or validate every current free-speech, one-tool, setup-pause, and growth-diagnostics change. This repository does not claim that the current development snapshot has passed Android device verification or the gameplay GPU diversity soak.

## Project structure

- `Content/WerewolfShowcase`: maps, Blueprint gameplay/core assets, UMG widgets, avatars, and tests.
- `Content/Models`: model checksum metadata; the multi-gigabyte model itself is release/build input and is ignored by Git.
- `Plugins/LiteRT-LM-Unreal`: private `fabLiteRTLMUnreal` submodule containing the Unreal wrapper and native integration.
- `DemoDocs/WerewolfShowcase`: tutorial, issue log, game guide, and Android packaging documentation.
- `Scripts/Package-AndroidSingleApk.ps1`: reproducible Android packaging entry point.
- `Scripts/Assemble-LiteRTDemo-v5.0.0.ps1`: verified release downloader/assembler.

The project has no gameplay `Source` module. Rules, role state, serialized AI scheduling, native tool validation, and UMG flow are authored in Blueprint; the C++ implementation lives only in the plugin. Editor MCP authoring helpers are not runtime dependencies and are excluded from source and packaged builds.

## License

MIT License. See [LICENSE](LICENSE).

---

Created by **Winyunq**.
