# M3-8: Labels, cargo summaries, field readouts and hazard visuals

GPT-6 Astra, 2026-09-12. Branch `astra/m3-8-legibility`, created from `main` at `6e44c48`.
Brief: `Docs/tasks/M3-8-legibility.md`. Implementation and proof committed through `0568285`; no merge to `main`.

**Stopped at visual acceptance under the requested scope rule.** The specified C++ pass is implemented,
**99/99 tests pass**, and **all nine existing proofs plus the new log-driven proof exit 0**. However, the required
north-facing capture is filled by existing map barrel props, hiding the crate labels. Making that exact view
show the labels requires a map change or a revised proof viewpoint; maps and existing expectations are protected.
No such change was made. The engineering close-up also remains green-flooded with LifeSupport text outside the
view, and steam growth is not visually established. The visual definition of done is **not met**; the log-only
`LEGIBILITY OK` is not visual approval. Details and reproducible captures follow.

## Behaviour changed

The starting verified run booked a 4/8/12-cask refining contract at the Vell Extractor, travelled through Dock B
into the hauler, ran cargo handling, ship faults, air/exposure and the fuel/bypass reversal, then docked at Halder
Solar Refinery and returned to the hub with pay and saved base fuel. That merged tree had 90 passing tests and
nine passing PIE proofs. This pass changes its presentation.

- Every labelled actor uses `ShipVisuals::ConfigureLabel`: horizontal and vertical text centring, no collision,
  sRGB colour conversion, the existing relative offset and a relative yaw calculated from the current owner.
  IDs and states occupy separate lines. LifeSupport retains `Scrubbers <Mode>` as its third line, and the control
  station retains its existing two lines. Signs retain their size, colour and actor-facing direction.
- Module, line, fixture and cargo text uses the brief's 22 cm size. The old code actually had module/line sizes
  of 30, control-station size 24, and fuel fixtures of 12/16/16; the brief's explicit values win. Heights and
  fixture offsets stay as authored, including the valves' existing inverse-transform conversion.
- Cargo layout specs carry a replicated initial `LabelYaw`, default 180 for hand-built specs. All twelve north
  crate spots face -90; the nine south clamp spots and cold rack face 90. No position or mesh transform changed.
- A crate's white ID stays at its 95 cm label offset, with a separate centred `StateLabel` one 22 cm line below it.
  This gives the state line its own white/amber/red colour (warning at 40, red at 80; Vented stays red).
  Both lines disappear while carried or Burned and return after putting down or placing. Each refresh recomputes
  relative yaw; a root-transform notification also refreshes the labels because the existing server placement
  path publishes cargo before applying the slot transform. No character or replication-state code changed.
- The live task board opts into summaries: more than four eligible loose casks become `Secure N loose casks`;
  more than two heating casks become `Cool N casks - hottest C4 85C`, red from 80. Heating, loose and excluded
  states use the existing eligibility rules. Fuel/air/injector/module/line priority and the eight-row cap remain.
  `BuildRows` defaults to `bSummaries = false`, preserving every older caller's output.
- The board has a `SHIP TASKS` title and a second text block for `AIR 22 ppm  |  FUEL 80%  |  19:29 to dock`.
  Air and fuel are floored; absent systems and a negative clock are omitted. Replicated state publications and
  the controller's existing clock updates both refresh it. The panel is 400 wide with a 12-point readout to
  keep the three values on one line; task rows retain their 16-point font.
- Module lights use 180 cm radius / 150 cd; crate heat lights use 120 cm radius. Valve lights are unchanged.
  Case-material parameters are probed once per process. When available, `Emissive Boost` and `Emissive Light`
  are driven on a runtime dynamic material by the clamped heat fraction, never on the pack asset. A missing
  parameter or cube fallback logs one skip per process. State-line colour supplies the unconditional cue.
- All three `PS_SteamDoor` components use relative scale 2.5. Origins are 80 cm for crate steam, 150 for line
  leaks and 130 for the feed leak. Their activation, restart and puff logic is unchanged.

## Files

All source paths below are under `Source/Airlock`:

- `Ship/ShipVisuals.h/.cpp`: shared label policy and optional case emissive.
- `Ship/CargoLayouts.h/.cpp`, `CargoCrate.h/.cpp`, `CargoSlot.cpp`: facing specs, crate state line and visibility.
- `Ship/ShipModule.h/.cpp`, `ShipLine.h/.cpp`, `ControlStation.h/.cpp`, `FuelValve.cpp`, `BypassValve.cpp`,
  `FuelIntake.cpp`, `Station/StationSign.cpp`: common labels, stacked text and prescribed hazard visuals.
- `UI/TaskBoardWidget.h/.cpp`: optional summaries, pure readout and live header refresh.
- `Tests/LegibilityTests.cpp`: nine new tests; `FLegibilityTestWorld` copies the existing cargo-world harness.
- `Scripts/verify-legibility.ps1`: new file-console/MCP proof; this report.

## Build and automation

Both builds succeeded on the first attempt with UE 5.8.2 / MSVC 14.50.35738. The label-policy checkpoint passed
**92/92** tests before its commit. The complete pass passed **99/99**, with zero failures or unrun tests.
The final unity source includes both new C++ files; no project-file regeneration was needed.

Evidence: [policy build](../../Saved/AgentCaptures/M3-8/build-label-policy.txt),
[policy results](../../Saved/AgentCaptures/M3-8/tests-label-policy.json),
[complete build](../../Saved/AgentCaptures/M3-8/build-visuals.txt),
[complete results](../../Saved/AgentCaptures/M3-8/tests-visuals.json).

All added tests passed (prefix `Airlock.Legibility.`):

| Test | Coverage | Result |
|---|---|---|
| `ConfigureLabel` | Size, centres, relative/current-owner yaw, ownerless yaw, colour, offset, collision | Success |
| `LifeSupportKeepsScrubberLine` | Third line in Normal and Hazard | Success |
| `CargoLabelsFaceWalkway` | Boundary rule, hand-built defaults, all 12 crate / 10 slot spots | Success |
| `CarriedDroppedRackedAndTerminalLabels` | Colour thresholds, server carry/drop/place, yaw after movement, Vented/Burned | Success |
| `LooseSummaryThresholdAndLegacyRows` | 1-4 unchanged, 5 summarized, legacy default | Success |
| `HeatingSummaryThresholdOrderAndTerminalExclusions` | 2/3 threshold, stable hottest ties, red at 80, state exclusions, legacy overflow | Success |
| `SummariesKeepHazardPriorityAndOverflow` | Fuel/air/injector/heat/module/line/loose ordering and cap | Success |
| `ReadoutPartsFlooringAndClock` | All eight system/clock combinations, fractions, zero/negative clock | Success |
| `FixtureLabelsAndHazardVisuals` | Fixture text states, label size/centre, unchanged valves, light and steam defaults | Success |

## PIE verification

All proofs used port 8000 and the committed source. The nine existing scripts and all existing test files are
unchanged. Each completed proof stopped PIE and left both maps clean.

| Proof | Exit | Seconds | Output |
|---|---:|---:|---|
| `verify-loop.ps1` | 0 | 54.13 | [log](../../Saved/AgentCaptures/M3-8/verify-loop-output.txt) |
| `verify-ship-graph.ps1` | 0 | 72.96 | [log](../../Saved/AgentCaptures/M3-8/verify-ship-graph-output.txt) |
| `verify-incidents.ps1` | 0 | 61.61 | [log](../../Saved/AgentCaptures/M3-8/verify-incidents-output.txt) |
| `verify-voyage-clock.ps1` | 0 | 106.40 | [log](../../Saved/AgentCaptures/M3-8/verify-voyage-clock-output.txt) |
| `verify-level-pass.ps1` | 0 | 155.52 | [log](../../Saved/AgentCaptures/M3-8/verify-level-pass-output.txt) |
| `verify-cargo.ps1` | 0 | 89.85 | [log](../../Saved/AgentCaptures/M3-8/verify-cargo-output.txt) |
| `verify-atmosphere.ps1` | 0 | 128.28 | [log](../../Saved/AgentCaptures/M3-8/verify-atmosphere-output.txt) |
| `verify-fuel.ps1` | 0 | 152.79 | [log](../../Saved/AgentCaptures/M3-8/verify-fuel-output.txt) |
| `verify-refinery.ps1` | 0 | 68.36 | [log](../../Saved/AgentCaptures/M3-8/verify-refinery-output.txt) |
| `verify-legibility.ps1` | 0 | 70.67 | [log](../../Saved/AgentCaptures/M3-8/verify-legibility-output.txt) |

The first fuel attempt's PowerShell process exited `-1` at 129.18 s while polling for the injector hold. It printed
no assertion failure or PowerShell exception. The editor remained alive and logged `Injectors cleared` and
`Hold Engines complete` four seconds after the hold began. Its HTTP log recorded a client socket-send failure.
The cause of the client exit is unknown. I preserved the attempt, stopped its abandoned PIE through MCP,
verified both maps clean, and reran the **unchanged** fuel proof successfully before the remaining two proofs.
No build or gameplay code changed between the attempts.

Evidence: [first attempt](../../Saved/AgentCaptures/M3-8/verify-fuel-attempt1-output.txt),
[editor tail](../../Saved/AgentCaptures/M3-8/fuel-runner-exit-log.txt),
[proof results](../../Saved/AgentCaptures/M3-8/proof-results.json).

Proof tails:

```text
[21:56:21] LOOP OK: hub spawn -> booked -> Dock B -> Hauler_Mk1 PlayerStart -> exit -> hub PlayerStart, voyage complete
GRAPH OK: seven nominal modules -> coolant leak -> degraded coolant drifts core -> hold repairs line -> coolant restored -> hub PlayerStart, empty graph
INCIDENTS OK: solo schedule -> teaching leak and captain -> hold repair clears row -> core task -> climax -> no replay -> hub
CLOCK OK: 75 seconds remaining -> one-minute captain line -> automatic return card -> StationHub PlayerStart, voyage complete
LEVEL OK: hold -> engineering (2 lines repaired) -> corridor (2 lines repaired) -> galley -> bridge seat, board, stand -> hub PlayerStart
CARGO OK: secure two -> shift and cool C2 -> vent C4 -> 2/4 intact, pay 400 -> hub last run
ATMOSPHERE OK: warm cask puffs -> hazard scrub -> rack -> blackout and recovery -> clean air, pay 400 -> hub last run
FUEL OK: feed leak -> shut -> bypass -> burn C3/C1 -> clear injectors -> clean air, pay 270 -> hub last run
REFINERY OK: reset -> priority contract, 12 casks -> 2 intact -> refinery dock, +20 t -> hub board -> empty run adds 0 -> reset
LEGIBILITY OK: Priority hold -> north/south labels -> carried label hidden -> drop -> cargo summaries -> engineering panel -> feed steam -> refinery result -> hub, save reset
```

The new proof captured the summary **3.01 s** after C4's heat command, inside the required five seconds. Selected
category-anchored action logs (the full sequence is in [legibility-log.txt](../../Saved/AgentCaptures/M3-8/legibility-log.txt)):

```text
[2026.09.13-05.13.38:165][529]LogAirlock: Cargo manifest: 12 casks, 10 slots
[2026.09.13-05.13.59:848][799]LogAirlock: Carry C1 by AirlockCharacter_0
[2026.09.13-05.14.09:868][385]LogAirlock: PutDown C1 by AirlockCharacter_0
[2026.09.13-05.14.11:374][469]LogAirlock: Cargo C4 Loose temp=85
[2026.09.13-05.14.25:550][287]LogAirlock: SetPiece FuelLeak at 633.98s elapsed=0.50s
[2026.09.13-05.14.36:570][935]LogAirlock: Voyage result: 0/12 crates intact, 0 vented, 12 loose, hull 100.00%, pay 200, air 78 ppm, 0 blackouts, 0 burned, fuel +0 t
[2026.09.13-05.14.38:100][997]LogAirlock: Save reset: base fuel 200 t, runs 0
```

The material probe ran once across the editor process. `MI_Case_A` exposes scalar `Emissive Boost` and vector
`Emissive Light`, so the dynamic emissive path was available; no fallback skip occurred in these PIE runs.
[Full parameter list](../../Saved/AgentCaptures/M3-8/cargo-material-probe.txt).


## Captures and visual assessment

I inspected every requested capture. These are the actual results, including the failed visual checks:

| Capture | What the reviewer should inspect |
|---|---|
| [hold-north](../../Saved/AgentCaptures/M3-8/hold-north.png) | **Blocked:** the required north-facing view is filled by existing barrel props; crate labels cannot be judged. The board correctly says `Secure 12 loose casks`. |
| [hold-south](../../Saved/AgentCaptures/M3-8/hold-south.png) | Clamp_7 and Clamp_8 face the camera with ID/state on two lines; the cold rack is outside this prescribed frame. |
| [carrying-no-label](../../Saved/AgentCaptures/M3-8/carrying-no-label.png) | C1's carried case has no text; the bottom prompt names it and the board counts 11 loose casks. |
| [board-summaries](../../Saved/AgentCaptures/M3-8/board-summaries.png) | Red `Cool 3 casks - hottest C4 85C`, then `Secure 9 loose casks`; readout `AIR 8 ppm / FUEL 87% / 19:58`. |
| [engineering-panel](../../Saved/AgentCaptures/M3-8/engineering-panel.png) | **Not accepted:** strong green wash remains, LifeSupport's label is outside the level view, adjacent Engines text is cropped. The air warning and summary rows remain readable. |
| [feed-leak-steam](../../Saved/AgentCaptures/M3-8/feed-leak-steam.png) | Feed text is stacked but very large at the required 22 cm size; a haze is visible, but I cannot establish a larger puff distinct from the coloured lighting. |

Retained pre-change comparisons: [feed leak](../../Saved/AgentCaptures/M3-8/baseline-feed-leak.png),
[engineering](../../Saved/AgentCaptures/M3-8/baseline-engineering.png),
[Priority hold](../../Saved/AgentCaptures/M3-8/baseline-priority-hold.png). The old engineering overview uses a
wider viewpoint, so it is context rather than a controlled brightness comparison.

`AirlockWhere` for the obstructed north view logged pawn `X=-5450.000 Y=1547.888 Z=30.359`, view
`X=-5450.000 Y=1547.888 Z=94.359`, yaw 90, pitch 0: the requested warp also depenetrated slightly south.
Engineering logged the requested pawn `(-1300, 760, 118)`, view Z 182, yaw/pitch 0. These are actual PIE views,
not misplaced editor-camera captures. No alternate camera was substituted to conceal the failed checks.


## Scope and open questions

- **Stop-rule blocker:** the fixed `hold-north` viewpoint cannot show the casks through existing map props.
  The map owner must resolve the obstruction, or the reviewer must revise the brief's camera position.
  I stopped implementation when this became visible; the already-running proof had completed its logs and all
  six captures before inspection finished. Nothing was moved or hidden to make a misleading passing capture.
- **Engineering needs another visual decision.** The requested 180 cm / 150 cd module lights still leave the
  close wall strongly green. The fixed LifeSupport actor position plus its preserved 210 cm label offset put
  the label above the prescribed level view. The reviewer needs to reconcile the view, label offset and lighting
  targets; I did not change protected layout positions, maps, exposure settings or required label heights.
- **Steam growth remains unproven.** Scale 2.5 and the specified higher origins are applied and tested. The
  capture does not establish that the Cascade puff grew; per item 6 I report that result without further tuning.
  The 22 cm feed label is also oversized at this close camera compared with the former 12 cm value.
- The north-row overlap and the rack's facing still need a visible PIE frame to judge; pure tests prove every
  layout yaw assignment, but they do not substitute for that missing visual evidence.
- **No out-of-brief files changed.** Protected Core/Voyage/model/layout-position files, Content, maps, packs,
  every existing test and all nine old proof scripts are unchanged. The scope audit compares against `6e44c48`;
  [audit](../../Saved/AgentCaptures/M3-8/scope-audit.json). No Computer Use or GUI-automation tools were used.
  The port-8001 editor was neither driven nor closed.
- Final state: port 8000 open on `/Game/Maps/StationHub`, PIE stopped, StationHub and Hauler_Mk1 both clean,
  save reset to base fuel 200 t / zero runs. [MCP verification](../../Saved/AgentCaptures/M3-8/final-editor-state.txt).
- The M3-6 ten-second reserve question was already resolved on main by the condition-driven captain call;
  no fuel rules were reopened. Solo balance, M4 authority/cheat gating, sounds, incident variety and map dressing
  remain outside this brief.


## Reproduce

From `C:\Users\tclin\Desktop\ai_game_studio\Airlock`:

```powershell
git switch astra/m3-8-legibility
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\close-editor.ps1
Scripts\build-editor.bat
$env:AIRLOCK_MCP_PORT = '8000'
Scripts\run-editor.bat
while (-not (Test-NetConnection 127.0.0.1 -Port 8000 -InformationLevel Quiet -WarningAction SilentlyContinue)) {
    Start-Sleep -Seconds 1
}
Start-Sleep -Seconds 30
Scripts\run-tests.bat Airlock
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-loop.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-ship-graph.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-incidents.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-voyage-clock.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-level-pass.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-cargo.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-atmosphere.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-fuel.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-refinery.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-legibility.ps1
```

The final command reproduces all six requested M3-8 captures and the log-anchored Priority/carry/drop/heat/leak/
docking sequence. HUD text is judged from captures only. It captures the summary within five seconds of C4's
40-degree crossing, resets the save after docking, stops PIE and verifies StationHub and Hauler_Mk1 are clean.
All captures and raw logs are retained under git-ignored `Saved/AgentCaptures/M3-8`; the committed script
regenerates them. No image binaries or pack assets are committed.
