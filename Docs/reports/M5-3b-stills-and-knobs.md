# M5-3b: Stills and knobs

GPT-6 Astra, 2026-09-13. Branch `astra/m5-3b-stills-and-knobs`, created from `main` at `f92eb5c`.
Checkout `Airlock`, editor MCP port 8000. No work in the Claude worktree or its editor.

## What the crew can do by hand

Each still now has a carried-cask hopper, six heat/feed/vent handles, reset/purge/scrape/repair levers, and a
calibration control. A handle tap turns one 5% step; holding repeats every 0.25 seconds, pauses outside reach,
and stops on release. The endpoints remain silent. The hopper loads through the character's server RPC and the
existing refinery API, hides the cask, and frees the carrier in place. Pressing another control while carrying
puts the cask down. Empty-handed hopper presses do nothing.

Levers reject the wrong status before starting a hold. Reset takes 4 seconds, purge 5, scrape 8, repair 12,
calibrate 3. The three gauge lines read the replicated refinery mirror, including batch progress from charge,
red pressure and amber calibration. The original fixture body, base label and six-fixture count remain.
Okafor warns when pressure enters its warning band and calibration drifts; each cue shares a 20-second cooldown
across all stills. The console's `AirlockPress` exercises the same verbs, behind the cheat gate.

## Files and decisions

- New `Ship/Still.*`, `Ship/StillControl.*`, `Tests/StillTests.cpp`. The fixture header only exposes the specified
  virtual refresh, protected state/readiness, and const spec accessor. Its `.cpp` is unchanged.
- `Ship/RefineryLayouts.*` holds the 12-control table, enum and replicated spec; `Core/AirlockGameMode.cpp` selects
  the still subclass, spawns its controls and speaks the two warning transitions.
- `Core/Interactable.h` has default-false repetition and default-empty refusal events. The character handles
  their server execution and hopper loading. The controller supplies `AirlockPress` / `ServerPress`;
  `DevCheatsTests.cpp` adds only that RPC and the count from 31 to 32.
- Every mutation uses `RefineryLoadHopper`, `RefineryStepKnob`, or `RefineryLever`. Actors read only the game-state
  mirror. The model, cargo model, floor fixture implementation, task board, voyage files, maps, incident/fuel
  systems, and existing refinery tests were not edited.
- Control transforms rotate unscaled offsets by the still rotation, then use their own cosmetic scale. Trace
  boxes and label dimensions compensate for that scale: handles/levers have world half-extents (20,20,45),
  centred 60 cm up; the hopper marker has half-extents (32,32,12), centred 18 cm up. They block Visibility only.
- Labels use 8 cm text on the tightly spaced handles/levers and 12 cm on the hopper. The calibration panel turns
  90 degrees to face outward from the east flank. The prescribed gauge offsets and 18 cm text are unchanged.
- Scrape requires an idle, empty current cask and empty hopper before its hold starts. A queued hopper starts
  running on the next model tick. Other lever predicates match the model's status rules.
- The interface declarations landed with the controls so each commit built; their character execution and the
  hopper's press implementation followed in the next commit.
- The headless test pawn has zero gravity because its transient world contains no deck; otherwise its live
  movement tick falls out of hold range. World ticks advance `GFrameCounter`, as required by the existing helper.
- `verify-floor.ps1` runs the C2 light batch through controls alongside C1's console batch. The tank wait accepts
  the 300-999 L threshold log, since two simultaneous stills can cross 300 at a value above exactly 300.
  The result assertions require both batches, `2/2` refined. Capture output moves to `Saved/AgentCaptures/M5-3b`.

## Automation and builds

All eight new tests passed on their first run; **146/146** total. The 138 existing tests passed after each of the
first four implementation steps; the full 146 passed again after the proof-script step.

| New test (`Airlock.Floor.` prefix) | Result |
| --- | --- |
| `StillSpawnsControlsAndUnscaledOffsets` | PASS |
| `StillHopperLoadsOnlyCarriedCasks` | PASS |
| `StillHandleTapRepeatPauseReleaseAndSilentLimits` | PASS |
| `StillLeverRefusalsAndDurations` | PASS |
| `StillPressureWarnsOnceBeforePopAndResetHold` | PASS; pop at 65 quarter-second ticks after 240 preheat ticks; warning precedes pop; one cue for two stills |
| `StillDriftCueSharedAndCalibrationHold` | PASS; both traps drift after 220 ticks; one cue; 3-second calibration |
| `StillGaugesReadMirrorAndWarningColours` | PASS |
| `StillPressRoutesTapLoadPutDownAndRejectsDistance` | PASS |

The existing `Airlock.Cheats.*` tests pass with all 32 console RPCs gated. Existing refinery-floor, refinery-model,
ship hold, carry, fuel and exposure assertions were unchanged.

Build lines: `Result: Succeeded` at each step (17.26 s, 4.83 s, 13.33 s, 12.21 s, 11.74 s), then 9.47 s for the placement pass and 3.59 s for the ramp adjustment. Both placement builds also passed 146/146 tests.
The controls' first compile failed once on my use of a nonexistent `GetCarriedCrate` accessor (C2039, with a
follow-on format-string diagnostic); using the existing `CarriedCrate` member fixed it. Its retry succeeded.
No error failed twice. Logs: `Saved/m5-3b-step2-build.txt`, `Saved/m5-3b-step2-build-retry.txt`, and
`Saved/m5-3b-step3-build.txt` through `step5-build.txt`; test runs `Saved/m5-3b-step1-tests.txt` through
`step6-tests.txt`. The exported suite report is `Saved/Automation/Report/index.json`.

## Position probes, captures and PIE proofs

The initial probe found Still 3 VentMinus / VentPlus on props (feet +84.823 / +84.226 cm), Repair at +98.067 cm, and Calibrate at +22.400 cm. Its working controls moved from their original rows to Y=1600; its hopper stayed at its clear spot. The remaining rise is a ramp: probes at Y=1500 and 1450 reproduced the same feet heights at X=-4880 / -4850. VentPlus was raised 50 cm (actor Z=-15); Calibrate was raised 80 cm (actor Z=125). The first two stills retain the original XY table, with their four front levers lowered from offset Z=110 to 85 to separate their labels from the fixed batch gauge. The initial CSV and face captures remain under `Saved/AgentCaptures/M5-3b/initial-positions/`. Final measurements and proof tails follow.

| Control | World X, Y, Z (cm) | Probed feet Z (cm) |
| --- | --- | --- |
| Still_1_Hopper | -6000, 1700, -65 | -64.846 |
| Still_1_HeatMinus | -5920, 1700, -65 | -64.846 |
| Still_1_HeatPlus | -5880, 1700, -65 | -64.846 |
| Still_1_FeedMinus | -5820, 1700, -65 | -64.846 |
| Still_1_FeedPlus | -5780, 1700, -65 | -64.846 |
| Still_1_VentMinus | -5720, 1700, -65 | -64.846 |
| Still_1_VentPlus | -5680, 1700, -65 | -64.846 |
| Still_1_Reset | -5875, 1770, 20 | -64.846 |
| Still_1_Purge | -5825, 1770, 20 | -64.846 |
| Still_1_Scrape | -5775, 1770, 20 | -64.846 |
| Still_1_Repair | -5725, 1770, 20 | -64.846 |
| Still_1_Calibrate | -5650, 1840, 45 | -64.846 |
| Still_2_Hopper | -5600, 1700, -65 | -64.846 |
| Still_2_HeatMinus | -5520, 1700, -65 | -64.846 |
| Still_2_HeatPlus | -5480, 1700, -65 | -64.846 |
| Still_2_FeedMinus | -5420, 1700, -65 | -64.846 |
| Still_2_FeedPlus | -5380, 1700, -65 | -64.846 |
| Still_2_VentMinus | -5320, 1700, -65 | -64.846 |
| Still_2_VentPlus | -5280, 1700, -65 | -64.846 |
| Still_2_Reset | -5475, 1770, 20 | -64.846 |
| Still_2_Purge | -5425, 1770, 20 | -64.846 |
| Still_2_Scrape | -5375, 1770, 20 | -64.846 |
| Still_2_Repair | -5325, 1770, 20 | -64.846 |
| Still_2_Calibrate | -5250, 1840, 45 | -64.846 |
| Still_3_Hopper | -5200, 1700, -65 | -64.846 |
| Still_3_HeatMinus | -5120, 1600, -65 | -64.846 |
| Still_3_HeatPlus | -5080, 1600, -65 | -64.846 |
| Still_3_FeedMinus | -5020, 1600, -65 | -64.846 |
| Still_3_FeedPlus | -4980, 1600, -65 | -65.509 |
| Still_3_VentMinus | -4920, 1600, -65 | -60.85 |
| Still_3_VentPlus | -4880, 1600, -15 | -16.046 |
| Still_3_Reset | -5075, 1600, 45 | -64.846 |
| Still_3_Purge | -5025, 1600, 45 | -64.846 |
| Still_3_Scrape | -4975, 1600, 45 | -66.303 |
| Still_3_Repair | -4925, 1600, 45 | -64.468 |
| Still_3_Calibrate | -4850, 1600, 125 | 14.887 |

All 36 controls retained their intended X/Y (`Moved=False`). Feet heights include the east walkway's ramp;
actor Z is set separately for the panels and the raised vent handle. The accepted probe reported 55 total spots
(fixtures, cask staging and controls) and exited 0: `FLOOR POSITIONS REPORTED: 55 spots`.
CSV: `Saved/AgentCaptures/M5-3b/floor-position-probe.csv`; full command tail: `Saved/m5-3b-probe-accepted.txt`.

Mesh measurements through MCP `StaticMeshTools.get_bounds` (read only): the hopper pallet bounds are
(-96.162,-96.274,-2.902) to (96.084,96.024,18.647) cm, or about 115.35 x 115.38 x 12.93 cm at 0.6.
The tank's measured bounds are (-148.413,-148.206,-32.016) to (144.481,156.944,212.724) cm.
A literal geometry detail in the brief: with the inherited 0.8 actor scale, the three prescribed relative gauge
heights become 172,156,140 cm above the actor origin; only the top row is above the tank's 170.18 cm upper bound.
The lower rows read against its front face in the captures. I retained the specified offsets (pinned by the gauge
test), rather than silently changing those numbers. No model constants changed.

### Floor proof on the final binary

`verify-floor.ps1` exited **0**, `FLOOR OK`, with one-player PIE (`Incident director started: crew=1`).
C1 produced a logged 753 L, C2 741 L; their precise fractional yields accumulate to the tank's floored 1495 L
(the event lines floor each batch separately). No spills, pops or criticals. Floor pay 708 cr; combined pay
1244 cr; the ship refuel was taken before 13 t went home. The proof reset the save afterward.

```text
Refinery Still_2 loaded C2 (hopper 1/3)
Cargo C2 Loaded temp=0
Load C2 into Still_2 by AirlockCharacter_0
Refinery Still_2 heat 6/20
Held heat handle: 7 steps
Refinery Still_2 feed 10/20
Still Still_2 Running cask=C2
Hold Still_2_Calibrate complete by AirlockCharacter_0
Refinery Still_2 calibrated
Hold Still_2_Reset rejected: Running
Refinery BatchDone Still_1 C1 diesel=753 gas=50 tar=196
Refinery BatchDone Still_2 C2 diesel=741 gas=50 tar=208
Floor tally now: floor: 2/2 casks refined, 1495 L diesel, 404 L tar, 0 spills, 0 pops, 0 criticals, pay 708, fuel +13 t
Floor result: floor: 2/2 casks refined, 1495 L diesel, 404 L tar, 0 spills, 0 pops, 0 criticals, pay 708, fuel +13 t
Voyage result: 2/4 crates intact, 0 vented, 2 loose, hull 92.80%, pay 1244, air 0 ppm, 0 blackouts, 0 burned | floor: 2/2 casks refined, 1495 L diesel, pay 708, fuel +13 t
Save reset: base fuel 200 t, runs 0
FLOOR OK: Milk run -> C1 secured, C2 racked -> floor: two casks, six fixtures, 36 controls -> Still 1 console batch and Still 2 by-hand light batch (load, taps, repeat, calibrate, reset refusal), both done, 1495 L diesel -> horn -> combined result (2/2 refined, +13 t), undocking card -> hub board, save reset
```

Full proof output: `Saved/m5-3b-floor-proof.txt`; fresh LogAirlock excerpt:
`Saved/AgentCaptures/M5-3b/floor-log.txt`. All captures were preceded by `AirlockWhere` and an explicit look direction.

### Mandatory regression proofs on the same binary

All exited **0** after `FLOOR OK`, without changing the binary. Single-player settings were forced and read back
before the batch; only the two-client proof selected listen-server mode, then restored standalone / one player.

| Proof | Result | Elapsed |
| --- | --- | --- |
| `verify-ship-graph.ps1` | GRAPH OK | 72 s |
| `verify-fuel.ps1` | FUEL OK | 148 s |
| `verify-cargo.ps1` | CARGO OK | 89 s |
| `verify-voyage-clock.ps1` | CLOCK OK | 116 s |
| `verify-two-clients.ps1` | TWO CLIENTS OK | 74 s |

```text
GRAPH OK: seven nominal modules -> coolant leak -> degraded coolant drifts core -> hold repairs line -> coolant restored -> hub PlayerStart, empty graph
FUEL OK: feed leak -> shut -> bypass -> burn C3/C1 -> clear injectors -> clean air, pay 270 -> hub last run
CARGO OK: secure two -> shift and cool C2 -> vent C4 -> 2/4 intact, pay 400 -> hub last run
CLOCK OK: 75 seconds remaining -> one-minute captain line -> automatic return card -> StationHub PlayerStart, voyage complete
TWO CLIENTS OK: server + client 1 on the hub -> the client crosses Dock B, both aboard -> client carries and secures C1 -> client at the seat -> client scrubs -> cheat refused at DevCheats 0 -> dock, both home, the second arrival steps aside
```

Full outputs: `Saved/m5-3b-proof-ship-graph.txt`, `Saved/m5-3b-proof-fuel.txt`,
`Saved/m5-3b-proof-cargo.txt`, `Saved/m5-3b-proof-voyage-clock.txt`, `Saved/m5-3b-proof-two-clients.txt`.
Regression captures are in `Saved/AgentCaptures/M3-2`, `M3-6`, `M3-4`, `Saved/M3VoyageClock`, and
`Saved/AgentCaptures/M4-1`. The reviewer still runs the full twelve-proof batch before merging.

### Captures inspected

- [Still 1 face](../../Saved/AgentCaptures/M5-3b/floor-still-1-face.png),
  [Still 2 face](../../Saved/AgentCaptures/M5-3b/floor-still-2-face.png),
  [Still 3 face](../../Saved/AgentCaptures/M5-3b/floor-still-3-face.png): the fixed gauge stack and separated levers;
  the third still's controls sit forward of the east-end props. All taken at Y=1450 facing north.
- [Running gauges](../../Saved/AgentCaptures/M5-3b/floor-still-gauges.png): Still 2, C2 at 12%, heat 30%, feed 50%,
  vent 55%, calibration 97%, pressure 1.15 bar; floor board says all systems nominal, 08:50 to horn.
- [Handles and hoppers](../../Saved/AgentCaptures/M5-3b/floor-still-handles.png): both handles per knob show the
  values, the empty hopper queues, and the gauge progressing at 19%. View moved back to Y=1450 and pitched down
  20 degrees to include the full row.
- [Hub result](../../Saved/AgentCaptures/M5-3b/hub-board-floor.png): the combined run and the banked surplus.
  The inherited arrival/pallet/panel/east/running/tank-labelled captures are in the same directory. The
  `floor-tank` capture retains the old proof's Still 1 viewpoint; the tank quantity is asserted from the log.

### Open questions for M5-4 and M5-5

- M5-4 still owns puddles and squeegeeing, pop/flash FX and sound, critical exposure and the bunk, and the decision
  on a floor cold rack. This task adds no hazard model changes or visual effects.
- M5-5 should playtest two crew operating the floor controls together. The mandatory two-client regression
  covers existing client interactions and the floor hop; it does not operate these new controls on the floor.
- The floor's existing lighting is dark; labels are readable but contrast and the side-facing trap panels need
  the planned dressing/legibility pass. Still 3's layout differs to accommodate the props and ramp. Gauge rows
  retain the exact requested relative offsets, with the tank-height detail recorded above.
- Press prompts remain API text until the future crosshair prompt widget; the hold widget displays active hold
  prompts. A batch still running at the horn is still lost under the existing model rule.

### Scope and reproduction

No source, map, existing test assertion or tracked file outside the allowed list was changed. No Computer Use or
GUI automation was used. The editor was driven by its port-8000 MCP and the file console. Each implementation,
proof-script and placement step was committed separately; no merge to main.

From this checkout:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\close-editor.ps1
Scripts\build-editor.bat
Scripts\run-tests.bat Airlock
Scripts\run-editor.bat
# Wait for port 8000, then another 30 seconds; ensure no PIE is running.
. Scripts\lib\mcp.ps1
Mcp-Init
Mcp-Call $TS_Object 'list_properties' @{instance=@{refPath='/Script/UnrealEd.Default__LevelEditorPlaySettings'}}
Mcp-Call $TS_Object 'set_properties' @{instance=@{refPath='/Script/UnrealEd.Default__LevelEditorPlaySettings'}; values='{"PlayNetMode":"PIE_Standalone","PlayNumberOfClients":1,"RunUnderOneProcess":true,"bLaunchSeparateServer":false}'}
Mcp-Call $TS_Object 'get_properties' @{instance=@{refPath='/Script/UnrealEd.Default__LevelEditorPlaySettings'}; properties=@('PlayNetMode','PlayNumberOfClients')}
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-floor.ps1 -ProbePositions
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-floor.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-ship-graph.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-fuel.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-cargo.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-voyage-clock.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-two-clients.ps1
```

Final editor check (port 8000): `IsPIERunning=false`, current level `/Game/Maps/StationHub`; StationHub,
Hauler_Mk1 and RefineryFloor all report `is_dirty=false`; `PlayNetMode=PIE_Standalone`, `PlayNumberOfClients=1`.
The editor remains open. Evidence: `Saved/m5-3b-final-editor.txt`. Branch is committed locally, not merged.

## Reviewer verification (Claude, 2026-09-13 early afternoon)

Reviewed from Claude Code after Astra's process had exited (55 min, nine commits, no GUI tool, the editor on port
8000 left on StationHub with no PIE and clean maps).

- **Diff read** against the brief: every changed file is on the allowed list, `FloorFixture.h` took exactly the
  three-line edit (a virtual `Refresh`, protected `FloorState` / `bVisualsReady`, `GetSpec()`), the two new
  interface events are default-false / default-empty, the repeating hold starts server-side in `ServerBeginHold`
  and resets without clearing in the tick, the lever refusal is checked before the hold starts, the hopper goes
  through `ServerLoadStill` with the feed verb's checks, every mutation through the `Refinery*` API. Astra
  recorded the brief's own mistake instead of fixing it silently: the gauge offsets I specified are relative to a
  root scaled 0.8, so two of the three lines sit against the tank's face (the report's geometry note; the test
  pins the offsets). That is the behaviour we want from a brief-follower; the tuning is M5-5's.
- **Captures read**: `floor-still-gauges.png` (Still 2 running C2 at 12 %, the three lines legible, red / amber
  states not exercised in that frame), `floor-still-handles.png` (`HEAT - 30%` ... `VENT + 55%` in a row, both
  hoppers `0 queued`, the gauge at 19 %). Crowded but readable; the base label's second line (M5-3a's
  `C2 12% 151K 1.1bar`) now repeats the first gauge line and should go on `AStill` - M5-5.
- **Merge**: `--no-ff` into `main` at ba9a0b9, followed by `claude/m5-2b-floor-lighting` (3a25420; the proof
  script's tank wait kept Astra's wider `[3-9][0-9][0-9]` pattern over the lighting branch's `3[0-9][0-9]`) and
  `claude/m5-4a-floor-hazards-model` (d3d2dd2). Merged tree: clean build, **150 / 150**, twelve proofs:

  | Proof | Result |
  | --- | --- |
  | `verify-loop.ps1` | LOOP OK (53 s) |
  | `verify-ship-graph.ps1` | GRAPH OK (71 s) |
  | `verify-incidents.ps1` | INCIDENTS OK (57 s) |
  | `verify-voyage-clock.ps1` | CLOCK OK (117 s, with the floor hop) |
  | `verify-level-pass.ps1` | LEVEL OK (151 s) |
  | `verify-cargo.ps1` | CARGO OK (87 s) |
  | `verify-atmosphere.ps1` | ATMOSPHERE OK (123 s) |
  | `verify-fuel.ps1` | FUEL OK (147 s) |
  | `verify-refinery.ps1` | REFINERY OK (64 s) |
  | `verify-legibility.ps1` | LEGIBILITY OK (67 s) |
  | `verify-two-clients.ps1` | TWO CLIENTS OK (74 s, with the floor hop) |
  | `verify-floor.ps1` | FLOOR OK (212 s: both batches, 2/2 refined, on the lit map) |

  Twelve for twelve; the three merges stand. M5-4b (the hazards' bodies) launches on this tree.
