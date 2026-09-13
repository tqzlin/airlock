# M3-5: Hazard atmosphere, scrubbers and crew exposure

GPT-6 Astra, 2026-09-12. Branch `astra/m3-5-hazard-atmosphere`, created from `main` at
`6576899`. Implementation is committed locally; this branch has not been merged into `main`.

## Behaviour changed

- Cargo sections now have one server-owned atmosphere. Warm volatile casks puff at 2 ppm/s for ten seconds
  out of every twenty, including when clamped; racking or cooling below 40 C stops the cycle. A newly vented
  cask emits 5 ppm/s for exactly thirty seconds, once. The model integrates partial cycles, retains finished
  rupture clocks, removes vanished ids, and publishes floored ppm plus the ids driving the steam effects.
- Normal scrubbers remove 0.5 ppm/s; hazard mode removes 3 ppm/s. A non-Nominal core or Degraded life support
  halves removal, and Failed life support disables it. The rack cools at half rate with non-Nominal coolant,
  with a further half-rate factor when hazard scrubbing shares a non-Nominal core. The air and cargo run on
  the existing 4 Hz active-voyage timer, with cargo first so this tick's heat and venting feed this tick's air.
- The life-support module is a press interaction: Normal -> Hazard -> Normal; Off -> Normal. The server checks
  panel reach and blackout state; the console skips reach. Its label displays the selected mode. Captain lines
  announce warning, danger, clearing and a crew blackout, each with its own twenty-second cooldown; a jump
  across both upward thresholds speaks only the danger line.
- Crew exposure rises at 1%/s at 20 ppm and 2.5%/s at 50 ppm, and recovers at 0.5%/s below 20 ppm. Exposure at
  60% multiplies cached walk speed by 0.7, also combining with the existing carry factor of 0.6. Repeated
  RepNotifies assign from the baseline. At 100%, blackout drops carried cargo at the existing put-down spot,
  vacates the seat without its exit teleport, cancels holds, and stops movement for thirty seconds. Recovery
  sets exposure to 50% and restores walking. Pickup, placement, holds, seating and scrub commands are refused
  during blackout; warp remains available. The local C++ overlay shows the exposure tint and blackout countdown.
- The task board puts unsafe hold air above cargo and system tasks, red at 50 ppm, with `scrub`, `scrubbing`,
  or `repair life support` wording. The control board adds the air/scrubber header. Warm puffing casks reuse
  their steam and alternate their assigned heat-light intensity every 0.4 seconds; vented visuals retain
  their previous behaviour.
- Docking under 10 ppm adds 150 credits; each blackout deducts 50, clamped at zero pay. The four specified
  examples pay 900, 750, 500 and 400. Tally and result logs append air and blackout fields only with atmosphere
  data. The hub board appends `air clean` or a whole-ppm reading; results without air data retain their old text.
- Developer helpers are `AirlockAir`, `AirlockScrub <Off|Normal|Hazard>`, `AirlockAirSet <ppm>` and
  `AirlockExpose <0..100>`. The hub has no atmosphere and air mutators report that. Exposure injection there
  sets the value but cannot trigger blackout because the exposure tick is gated by section and active voyage.

## Files and commits

| Commit | Gameplay effect / files under Source/Airlock unless stated |
|---|---|
| `ae4d057` | Cask fumes, scrub sinks and pure exposure: `Ship/ShipAtmosphere.*`, `Tests/ShipAtmosphereTests.cpp` |
| `024f435` | Server ownership/publication and rack dependencies: `Core/AirlockGameMode.*`, `AirlockGameState.*`, `Ship/CargoModel.*`, model tests |
| `72b7891` | Life-support panel and captain warnings: game mode/character, `Ship/ShipModule.*` |
| `bf9e461` | Crew slowdown, blackout and recovery: character/game mode, `UI/ExposureWidget.*`, `Tests/CrewExposureTests.cpp` |
| `889cb82` | Air task/control-board lines and visible puffs: `UI/TaskBoardWidget.*`, `ControlBoardWidget.*`, `Ship/CargoCrate.*`, task-board tests |
| `a35a287` | Clean-air pay, blackout deductions and hub text: cargo model, game mode, `UI/VoyageBoardWidget.cpp`, tests |
| `eb8af31` | Atmosphere console helpers: `Core/AirlockPlayerController.*` |
| `3d55af6` | New PIE proof: `Scripts/verify-atmosphere.ps1` |

This report is the final documentation commit after acceptance verification.

## Build and automation

- Pure model: **6/6 passed** before the model commit. The first build rejected an implicit integer-to-bool
  conversion in the determinism test; explicit comparison fixed it on the next build.
- Integration: the first run was **67 passed / 2 failed**. Both failures were new transient-world tests that
  advanced several timer steps within one engine frame. They now use latent steps, following the existing
  cargo timer harness. While making that change, one build rejected an unqualified `EnqueueLatentCommand`;
  qualifying it through `FAutomationTestFramework::Get()` fixed the next build. No build failed twice on the
  same error. The next run was **70/70 passed**.
- Final implementation build: **Succeeded**, UE 5.8.2, MSVC 14.50.35738. Final automation: **72/72 passed**,
  comprising the original 56 untouched tests and sixteen additions below.

Evidence: [model build](../../Saved/AgentCaptures/M3-5/build-model-retry.txt),
[model tests](../../Saved/AgentCaptures/M3-5/model-tests.json),
[final build](../../Saved/AgentCaptures/M3-5/build-final.txt),
[all test results](../../Saved/AgentCaptures/M3-5/all-tests.json).

| Added test | Result |
|---|---|
| `Airlock.Atmosphere.PuffTimingAndEligibility` | Success |
| `Airlock.Atmosphere.RupturesOnceAndReset` | Success |
| `Airlock.Atmosphere.ScrubModesBrownoutAndDisabled` | Success |
| `Airlock.Atmosphere.SettersValidationAndOwnTickChanges` | Success |
| `Airlock.Atmosphere.Determinism2000Ticks` | Success |
| `Airlock.Atmosphere.ExposureRatesRecoveryAndClamp` | Success |
| `Airlock.Atmosphere.PublicationAndActiveVoyageGating` | Success |
| `Airlock.Atmosphere.CrewExposureCarrySpeedAndRecovery` | Success |
| `Airlock.Atmosphere.BlackoutDropsCargoRefusesActionsAndRecovers` | Success |
| `Airlock.Atmosphere.BlackoutSeatHoldAndMovementPredicate` | Success |
| `Airlock.Atmosphere.LifeSupportPanelRangeModesAndFailure` | Success |
| `Airlock.Atmosphere.HubHasNoAirOrExposureTick` | Success |
| `Airlock.Atmosphere.TaskBoardAirPriorityWordsAndCap` | Success |
| `Airlock.Atmosphere.HubBoardAirResultText` | Success |
| `Airlock.Cargo.RackCoolFactorClampsAndResets` | Success |
| `Airlock.Cargo.CleanAirBlackoutPayExamplesAndSummary` | Success |

## PIE acceptance

All seven proofs ran sequentially on the final binary, on this checkout's port-8000 editor. All exited **0**;
the six existing scripts were run without edits. The new proof passed on its first PIE run.

| Proof | Result | Time | Full output |
|---|---|---:|---|
| `verify-loop.ps1` | `LOOP OK` | about 53 s | [tail/output](../../Saved/AgentCaptures/M3-5/verify-loop-output.txt) |
| `verify-ship-graph.ps1` | `GRAPH OK` | 71.4 s | [tail/output](../../Saved/AgentCaptures/M3-5/verify-ship-graph-output.txt) |
| `verify-incidents.ps1` | `INCIDENTS OK` | 59.1 s | [tail/output](../../Saved/AgentCaptures/M3-5/verify-incidents-output.txt) |
| `verify-level-pass.ps1` | `LEVEL OK` | 154.1 s | [tail/output](../../Saved/AgentCaptures/M3-5/verify-level-pass-output.txt) |
| `verify-cargo.ps1` | `CARGO OK` | 88.4 s | [tail/output](../../Saved/AgentCaptures/M3-5/verify-cargo-output.txt) |
| `verify-voyage-clock.ps1` | `CLOCK OK` | 105.8 s | [tail/output](../../Saved/AgentCaptures/M3-5/verify-voyage-clock-output.txt) |
| `verify-atmosphere.ps1` | `ATMOSPHERE OK` | 124.2 s | [tail/output](../../Saved/AgentCaptures/M3-5/verify-atmosphere-output.txt) |

Regression proof tails:

```text
LOOP OK: hub spawn -> booked -> Dock B -> Hauler_Mk1 PlayerStart -> exit -> hub PlayerStart, voyage complete
GRAPH OK: seven nominal modules -> coolant leak -> degraded coolant drifts core -> hold repairs line -> coolant restored -> hub PlayerStart, empty graph
INCIDENTS OK: solo schedule -> teaching leak and captain -> hold repair clears row -> core task -> climax -> no replay -> hub
LEVEL OK: hold -> engineering (2 lines repaired) -> corridor (2 lines repaired) -> galley -> bridge seat, board, stand -> hub PlayerStart
CARGO OK: secure two -> shift and cool C2 -> vent C4 -> 2/4 intact, pay 400 -> hub last run
CLOCK OK: 75 seconds remaining -> one-minute captain line -> automatic return card -> StationHub PlayerStart, voyage complete
```

The cargo regression recorded `Tally now: 2/4 crates intact, 1 vented, 1 loose, hull 100.00%, pay 400,
air 52 ppm, 0 blackouts` and docked at 59 ppm with the same 400 pay. No clean-air bonus or blackout deduction
changed its original expected result.

Atmosphere proof extracts (timestamps UTC on 2026-09-13, local evening 2026-09-12):

```text
02:33:52.546 LogAirlock: Air 0 ppm scrub=Normal brownout=0 disabled=0 sources=0
02:33:55.062 LogAirlock: Cargo C2 Loose temp=45
02:34:21.761 LogAirlock: Air 20 ppm scrub=Normal
02:34:21.761 LogAirlock: Captain Okonkwo: Captain Okonkwo here. Nitriles in the hold air. Scrubbers to hazard mode - the life-support panel.
02:34:24.387 LogAirlock: Air 80 ppm scrub=Normal
02:34:24.387 LogAirlock: Captain Okonkwo: Captain Okonkwo here. Air's bad. Get those scrubbers up and keep moving.
02:34:28.406 LogAirlock: Scrub Hazard by AirlockCharacter_0
02:34:28.406 LogAirlock: Air 79 ppm scrub=Hazard
02:34:33.904 LogAirlock: Where: map=UEDPIE_0_Hauler_Mk1 pawn=AirlockCharacter_0 at X=-1300.000 Y=760.000 Z=118.000
02:34:38.923 LogAirlock: Cargo C2 Racked temp=45
02:34:50.767 LogAirlock: Air 19 ppm scrub=Hazard
02:34:50.767 LogAirlock: Captain Okonkwo: Captain Okonkwo here. Air's clearing. Good work.
02:34:51.628 LogAirlock: Air 120 ppm scrub=Hazard
02:34:53.871 LogAirlock: Exposure AirlockCharacter_0 100%
02:34:53.871 LogAirlock: Blackout AirlockCharacter_0
02:34:53.871 LogAirlock: Captain Okonkwo: Captain Okonkwo here. AirlockCharacter_0 is down - med-bot's on it.
02:35:23.873 LogAirlock: Recovered AirlockCharacter_0
02:35:29.193 LogAirlock: Air 7 ppm scrub=Hazard brownout=0 disabled=0 sources=0
02:35:29.703 LogAirlock: Scrub Hazard by AirlockCharacter_0
02:35:30.211 LogAirlock: Tally now: 1/4 crates intact, 0 vented, 3 loose, hull 100.00%, pay 400, air 4 ppm, 1 blackouts
02:35:31.720 LogAirlock: Voyage result: 1/4 crates intact, 0 vented, 3 loose, hull 100.00%, pay 400, air 0 ppm, 1 blackouts
02:35:33.757 LogAirlock: No atmosphere on this section
02:35:33.757 LogAirlock: Crew AirlockCharacter_0 exposure=0% blackedOut=0
02:35:34.770 LogAirlock: Voyage board opened (day seed 20709)
ATMOSPHERE OK: warm cask puffs -> hazard scrub -> rack -> blackout and recovery -> clean air, pay 400 -> hub last run
```

The first warning occurred 26.699 seconds after heating. Hazard scrubbing crossed below warning 22.361 seconds
after the mode command. Recovery occurred 30.002 seconds after blackout. Seven modules remained at 100%, no
scheduled incident fired, the second same-mode Hazard command was accepted, and the hub spawned a fresh pawn
at zero exposure. The full [atmosphere log](../../Saved/AgentCaptures/M3-5/atmosphere-log.txt) also records coughs,
threshold crossings and both clock rewinds.

## Captures reviewed

All are local evidence under `Saved/AgentCaptures/M3-5` (git-ignored, retained for the reviewer):

| Capture | Observation |
|---|---|
| [first-puff](../../Saved/AgentCaptures/M3-5/first-puff.png) | C2 is Loose at 45 C under its red heat light; the captain's nitrile warning is readable and the board says `Hold air 22 ppm - scrub`. Steam remains faint against the case and the red lighting, as with the existing vent effect. |
| [scrubbing](../../Saved/AgentCaptures/M3-5/scrubbing.png) | Captured within eight seconds of selecting Hazard. The red first row says `Hold air 62 ppm - scrubbing`; exposure reads 38%. The existing engineering status lights dominate the panel view. |
| [blackout](../../Saved/AgentCaptures/M3-5/blackout.png) | Full viewport exposure tint, `BLACKED OUT - 29 s` bottom-right, and the captain's med-bot caption. The racked cask and task board remain visible through the tint. |
| [hub-last-run-air](../../Saved/AgentCaptures/M3-5/hub-last-run-air.png) | `Last run: 1/4 crates intact - hull 100% - pay 400 cr - air clean` is readable beneath DEPARTURES. |
| [bridge-control-board](../../Saved/AgentCaptures/M3-5/bridge-control-board.png) | Copied from the successful level-pass proof. `AIR 0 ppm SCRUBBERS Normal` appears beneath the clock; all module and line rows fit, with the stand-up footer visible. |

Final [editor-state check](../../Saved/AgentCaptures/M3-5/final-editor-state.txt): `IsPIERunning=false`, current
level `/Game/Maps/StationHub`, `is_dirty=false` for both StationHub and Hauler_Mk1. The editor is left open.

## Reproduce the proof

From `C:\Users\tclin\Desktop\ai_game_studio\Airlock`:

```powershell
git switch astra/m3-5-hazard-atmosphere
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\close-editor.ps1
Scripts\build-editor.bat
$env:AIRLOCK_MCP_PORT = '8000'
Scripts\run-editor.bat
```

Wait for port 8000 and thirty more seconds, then:

```powershell
Scripts\run-tests.bat Airlock
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-loop.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-ship-graph.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-incidents.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-level-pass.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-cargo.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-voyage-clock.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-atmosphere.ps1
```

The new proof warms C2 during the calm, raises air to warning, switches hazard scrubbing from the probed
engineering walkway, racks C2, injects exposure to prove blackout/recovery, and docks with one intact cask,
clean air, one blackout and pay 400. It rewinds the clock twice and never advances it. Log waits and counts
are anchored on the category boundary; the HUD is checked through captures. Each proof starts/stops its own
PIE session. If the editor is minimized, use `Scripts\restore-editor-window.ps1` before captures.

## Scope and open questions

- [Scope audit](../../Saved/AgentCaptures/M3-5/scope-audit.txt): no protected file or proof script changed;
  no `Content/**`, maps, pack assets, engine files, Blueprint or UMG assets touched. The four protected travel
  functions are byte-for-byte equivalent after newline normalization. Original test bodies remain intact.
- Nothing was touched outside the brief. Claude advanced `main` with M3-6/design documentation while this
  branch ran; compare this task with `git diff main...astra/m3-5-hazard-atmosphere` to exclude those independent
  documentation changes. This branch remains based on `6576899`.
- All editor work used the checkout's scripts, file console and MCP port 8000. No Computer Use or GUI automation
  tool was used; no port-8001 editor was touched. Editor closes completed normally; no Restore Packages recovery
  was required.
- Separate network-client verification remains outside the solo PIE acceptance. The new console helpers share
  the existing unchecked development RPC convention and still need the project-wide dev-cheats gate before M4.
- No unresolved gameplay or acceptance failure. Visual follow-up for the existing legibility milestone: the
  inherited steam is faint beside the red heat light, and engineering's existing module labels/lights crowd
  the close panel view. No pack, map or unrelated visual changes were made to address those pre-existing issues.
