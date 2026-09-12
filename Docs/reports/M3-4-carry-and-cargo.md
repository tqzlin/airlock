# M3-4 checkpoint report: Carry, cargo and delivery tally

GPT-6 Astra, 2026-09-12. Branch `astra/m3-4-carry-and-cargo`, created from `main` at `9a01bf4`.
Implementation and current binary: `de9aaae`. All implementation steps are committed locally; nothing merged to main.

**Incomplete verification / interrupted session.** The Computer Use tool reported that the user pressed the physical
Escape key and stopped Computer Use. App input stopped at that point. This is a checkpoint, not a completed acceptance
report. The final build succeeded; the final automation run completed **51 passed / 1 failed / 52 total**. The four final
PIE regression/proof scripts and visual capture inspection remain outstanding. The editor was relaunched on port 8000
and left open; no further PIE was started after the interruption. A final MCP clean-map/state check remains outstanding.

## Behaviour implemented

- Four loose cargo crates, including two volatile crates, and three clamps plus one cold rack spawn from the new
  `CargoLayouts` table. Their replicated labels, heat lights, steam and slot occupancy read `GameState.Cargo` and share
  `OnShipGraphChanged`. Crates and slots block Visibility traces and ignore pawns. No physics or content assets were added.
- The pure cargo model heats loose/carried volatile crates only after the 150-second calm period, cools racked crates
  at any time, preserves occupied slots when a crate vents, and computes intact/vented/loose counts and hull-based pay.
  The game mode ticks it on the existing active-voyage systems timer and publishes whole-degree temperatures on changes.
- Server RPCs validate pickup range, existing carriers, venting, full hands and slot occupancy. Carrying attaches to the
  character's default-subobject carry socket and assigns 60% of cached walk speed. Drop/place/vent restore the base speed.
  Destroyed pawns drop cargo; level teardown does not intentionally drop it. Both interaction/drop traces ignore the
  carried crate. The local C++ carry prompt explains E; hold-E refuses while carrying.
- Task rows put heating cargo first, hottest first, red at 80C; ship-system priorities are preserved; cool loose crates
  appear last. Each crate contributes at most one row. Racked/vented cargo and cool carried cargo have no row.
- Cargo-enabled schedules replace exactly one climax entry with `CargoShift`; eligible middle shifts require at least
  two clamps and 40% elapsed voyage time. No-cargo schedules consume the original random stream unchanged. The director
  releases a secured crate in the chosen clamp, or logs `skipped: clamp empty`. Omitted targets select a filled clamp.
- `RecordVoyageResult()` guards unbooked sections and repeated recording. The only changes in the two return functions
  are calls immediately before their existing booking clears. The subsystem retains the result across travel, the hub
  game state mirrors it, and the departure board displays the last run's intact count, rounded hull and pay.
- Console helpers: `AirlockCargoStatus`, `AirlockCarry`, `AirlockDrop`, `AirlockPlace`, `AirlockCrateHeat`, `AirlockTally`.
  `AirlockRelease` still ends a hold. `AirlockIncident CargoShift [ClampId]` uses the director.

## Files and commits

| Commit | Gameplay effect / files |
|---|---|
| `d4999b7` | Pure cargo model, separate layout and six model tests (`Ship/CargoModel.*`, `CargoLayouts.*`, `Tests/CargoModelTests.cpp`) |
| `7066499` | Replicated crate/slot scenery and game-mode/game-state publication (`Ship/CargoCrate.*`, `CargoSlot.*`, `Core/AirlockGameMode.*`, `AirlockGameState.*`) |
| `7c3fe35` | Carry verb, press interaction wiring, prompt and six spawned-world tests (`Core/AirlockCharacter.*`, actors, `UI/CarryPromptWidget.*`, `Tests/CargoInteractionTests.cpp`) |
| `dbf2d36` | Cargo console helpers (`Core/AirlockPlayerController.*`) |
| `0e89171` | Cargo task priorities and test (`UI/TaskBoardWidget.*`, `Tests/TaskBoardTests.cpp`) |
| `96c9fa4` | Cargo-shift schedule/director/parser, three tests, and only the two allowed regex changes in `verify-incidents.ps1` |
| `28d6283` | Recording, persisted result, hub board and result tests (`Voyage/VoyageSubsystem.*`, game mode/controller, `UI/VoyageBoardWidget.*`, tests) |
| `0fced5b` | Probed cargo positions, including rack clearance from the walkway |
| `de9aaae` | `Scripts/verify-cargo.ps1`, including optional `-ProbePositions` mode |

Paths above are under `Source/Airlock` unless otherwise noted. This report is the final checkpoint commit.

## Build and automation evidence

- Model build: succeeded after one failed compile of a test initializer mixing `NAME_None` (`EName`) and `FName`.
  Explicitly constructing `FName(NAME_None)` fixed it on the next build. No build failed twice on the same error.
- Six pure model tests: **6/6 passed**, before actor implementation and before the model commit.
- Carry build: succeeded; **46/46 tests passed**, including the original 34 and twelve cargo tests.
- Final unity build: **Succeeded**, UE 5.8.2, MSVC 14.50. All implementation compiled together.
- Final automation: **51/52 passed**. The original 34 expectations were not rewritten and all passed.

Evidence: [model build](../../Saved/AgentCaptures/M3-4/build-model.txt),
[model tests](../../Saved/AgentCaptures/M3-4/model-tests.json),
[carry build](../../Saved/AgentCaptures/M3-4/build-carry.txt),
[carry tests](../../Saved/AgentCaptures/M3-4/carry-tests.json),
[final build](../../Saved/AgentCaptures/M3-4/build-final.txt),
[final test results](../../Saved/AgentCaptures/M3-4/all-tests.json).

The failing test is `Airlock.Cargo.DirectorReleasesFilledClampAndSkipsEmpty`, with one error:

```text
Expected 'Empty clamp reason logged' to be true.
```

Its gameplay state assertions passed. The editor log actually contains the expected message:

```text
[2026.09.12-22.09.56:638][613]LogAirlock: Incident CargoShift Clamp_2 (0.00) at 0.000s elapsed=0.000s
[2026.09.12-22.09.56:638][613]LogAirlock: Incident CargoShift Clamp_2 (0.00) at 0.000s elapsed=0.000s skipped: clamp empty
```

Investigate the test's `FCargoIncidentLogCapture` / log-delivery timing before re-running; this was not fixed or
retested after the interruption. Do not infer that the failed assertion can simply be discarded.

| Test | Final result |
|---|---|
| `Airlock.Cargo.BuildManifestAndSlotOperations` | Success |
| `Airlock.Cargo.CalmHeatingCoolingAndWholeDegrees` | Success |
| `Airlock.Cargo.DestroyedPawnDropsCargo` | Success |
| `Airlock.Cargo.Determinism` | Success |
| `Airlock.Cargo.DirectorReleasesFilledClampAndSkipsEmpty` | Fail |
| `Airlock.Cargo.InteractTraceIgnoresCarriedCrate` | Success |
| `Airlock.Cargo.InvalidInputsAreNoOps` | Success |
| `Airlock.Cargo.PickupRangeHandsSpeedAndHold` | Success |
| `Airlock.Cargo.PublicationAndSectionSpawning` | Success |
| `Airlock.Cargo.PutDownFloorAndSlotPlacement` | Success |
| `Airlock.Cargo.ResultRecordsOnceAndNextVoyageOverwrites` | Success |
| `Airlock.Cargo.ShiftScheduleDeterminismAndClimax` | Success |
| `Airlock.Cargo.SingleClampAndAbsentCargoSchedules` | Success |
| `Airlock.Cargo.TallyPayExamplesAndCarriedLoss` | Success |
| `Airlock.Cargo.TaskBoardHeatingPriorityAndLooseRows` | Success |
| `Airlock.Cargo.UnbookedSectionRecordsNothingAndResultText` | Success |
| `Airlock.Cargo.VentingIsInertAndRetainsSlot` | Success |
| `Airlock.Cargo.VoyageTimerVentsAndFreesCarrier` | Success |
| `Airlock.Core.AgentConsoleFileLivesUnderSaved` | Success |
| `Airlock.Core.GameModeDefaults` | Success |
| `Airlock.Incidents.ClockMovesFireOnceInOrder` | Success |
| `Airlock.Incidents.CountByCrew` | Success |
| `Airlock.Incidents.DirectorAppliesAndSkipsInvalidTargets` | Success |
| `Airlock.Incidents.DirectorTimerClockMovesAndCrewSnapshot` | Success |
| `Airlock.Incidents.DockingJumpConsumesBeforeReturn` | Success |
| `Airlock.Incidents.OrderWindowAndClimax` | Success |
| `Airlock.Incidents.ScheduleDeterminism` | Success |
| `Airlock.Incidents.ShortAndInvalidWindows` | Success |
| `Airlock.Incidents.TaskBoardPriorityRepairAndOverflow` | Success |
| `Airlock.Incidents.TeachingFaultTargetsAndCaptain` | Success |
| `Airlock.ShipGraph.AuthorityPublicationAndSectionSpawning` | Success |
| `Airlock.ShipGraph.Brownout` | Success |
| `Airlock.ShipGraph.BuildHauler` | Success |
| `Airlock.ShipGraph.ClampAndStatusThresholds` | Success |
| `Airlock.ShipGraph.ClearFaultStopsDrain` | Success |
| `Airlock.ShipGraph.CompetingHoldsAndDestroyedTarget` | Success |
| `Airlock.ShipGraph.CoolantPropagation` | Success |
| `Airlock.ShipGraph.Determinism` | Success |
| `Airlock.ShipGraph.HoldPausesReleasesAndRepairs` | Success |
| `Airlock.ShipGraph.InvalidDeltaNoOp` | Success |
| `Airlock.ShipGraph.LeakDrain` | Success |
| `Airlock.ShipGraph.ShortDrain` | Success |
| `Airlock.ShipGraph.StatusChangesInLayoutOrder` | Success |
| `Airlock.ShipGraph.TimerRunsOnlyDuringVoyage` | Success |
| `Airlock.ShipGraph.UnknownIdsAndInvalidInputs` | Success |
| `Airlock.Station.BoardAndDoorDefaults` | Success |
| `Airlock.Voyage.ClockMoveClampsAndRejectsNonFinite` | Success |
| `Airlock.Voyage.ClockMovesFireOnceInOrder` | Success |
| `Airlock.Voyage.OffersCoverEveryType` | Success |
| `Airlock.Voyage.ScheduleBoundsAndCaptain` | Success |
| `Airlock.Voyage.ScheduleDistinctTypeLines` | Success |
| `Airlock.Voyage.SpecIsDeterministic` | Success |

The new cargo schedule tests sweep 100 seeds, multiple crew counts, one-clamp layouts and absent-clamp layouts;
the one-clamp path also checks short and zero durations. Interaction tests exercise the real actors/game mode in
transient worlds, without loading or saving maps.

## Final cargo position table

`AssetTools.exists` returned true for all three requested meshes. `StaticMeshTools.get_bounds` measured:

| Mesh | Local bounds / dimensions (cm) |
|---|---|
| `SM_Case_A` | min (-61.517, -34.180, -0.693), max (58.972, 33.371, 50.308); **120.490 x 67.551 x 51.000** |
| `SM_PlasticPallet` | min (-96.162, -96.274, -2.902), max (96.084, 96.024, 18.647) |
| `SM_Shelf_A_v1` | min (-1.491, -1.918, -0.236), max (181.639, 61.919, 141.442) |

The case fits the specified 50..130 cm dimensions, so it uses the requested mesh; no fallback was needed on this
machine. The rack's corner pivot and yaw 90 would put its far edge at Y=1581.639 from the proposed Y=1400, inside
the walkway. Moving its root to Y=1300 puts the far edge at Y=1481.639, leaving approximately 78 cm to the walkway
centre. Pallets end around Y=1496 and crates begin around Y=1666. Actors also ignore pawn collision.

Every final centre below was tested with `AirlockWarp x y 300`, at least four seconds to fall, then `AirlockWhere`.
All final centres returned pawn Z=31.154 (96 cm capsule half-height; feet approximately -64.846, consistent with
walking floor -65). The final table is encoded in `CargoLayouts.cpp` and `verify-cargo.ps1`.

| Id | Kind | Final X | Final Y | Final Z | Yaw | Probed pawn Z | Probe source |
|---|---|---:|---:|---:|---:|---:|---|
| Clamp_1 | clamp | -6200 | 1400 | -65 | 0 | 31.154 | alternatives, clamp-alt1 |
| Clamp_2 | clamp | -5700 | 1400 | -65 | 0 | 31.154 | initial probe |
| Clamp_3 | clamp | -5200 | 1400 | -65 | 0 | 31.154 | final candidates, Clamp_3b |
| ColdRack | rack | -5000 | 1300 | -65 | 90 | 31.154 | corrected rack probe, ColdRack-south |
| C1 | crate | -6100 | 1700 | -65 | 0 | 31.154 | final candidates, C1b |
| C2 | volatile crate | -5600 | 1700 | -65 | 0 | 31.154 | initial probe |
| C3 | crate | -5300 | 1700 | -65 | 0 | 31.154 | alternatives, crate-alt2 |
| C4 | volatile crate | -5000 | 1700 | -65 | 0 | 31.154 | final candidates, C4a |

The proposed centres sometimes landed on raised geometry: Clamp_1 pawn Z=63.922; ColdRack Z=196.238; C4 Z=120.213.
Several others differed from the glass floor by a few centimetres and were also replaced with exact floor probes.
No map objects were moved to make space.

Raw position evidence: [initial](../../Saved/AgentCaptures/M3-4/probe-cargo-output.txt),
[alternatives](../../Saved/AgentCaptures/M3-4/probe-alternatives-output.txt),
[final candidates](../../Saved/AgentCaptures/M3-4/probe-final-candidates-output.txt),
[rack](../../Saved/AgentCaptures/M3-4/probe-rack-output.txt).
The first temporary single-point rack script flattened its PowerShell tuple and produced invalid coordinates;
that run was discarded and the corrected two-point rack probe above produced the valid result. Initial MCP log
reads could lag one query; the underlying `Saved/Logs/Airlock.log` confirmed each of the eight original positions.
Subsequent probes allowed an extra second for log delivery.

## PIE acceptance still outstanding

`verify-loop.ps1`, `verify-ship-graph.ps1`, `verify-incidents.ps1`, and the newly written `verify-cargo.ps1`
were **not run on the final binary in this session**. There is no final `LOOP OK` tail and no `CARGO OK` claim.
No cargo HUD/carry/steam/hub-board captures have been inspected. The new proof script passes PowerShell syntax
parsing, but its runtime steps and capture framing remain unverified. `-ProbePositions` is also not yet run as a
single final-table acceptance check; the individual position probes above were run separately.

## Exact reproduction / continuation commands

From `C:\Users\tclin\Desktop\ai_game_studio\Airlock`, after resolving the failed cargo log-capture test:

```powershell
git switch astra/m3-4-carry-and-cargo
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\close-editor.ps1
Scripts\build-editor.bat
$env:AIRLOCK_MCP_PORT = '8000'
Scripts\run-editor.bat
```

Wait for port 8000 plus approximately 30 seconds. If an unclean-shutdown restore dialog appears, skip restoring
pack autosaves; none of this task's implementation requires changing pack assets. The already successful final
build is `Saved/AgentCaptures/M3-4/build-final.txt`.

For the same in-editor automation route used here (the alternative is `Scripts\run-tests.bat Airlock`):

```powershell
. Scripts\lib\mcp.ps1
Mcp-Init
Mcp-Text $TS_Tests 'DiscoverTests' @{}
Mcp-Text $TS_Tests 'RunTestsByFilter' @{ filterExpression = 'Airlock' }
Mcp-Text $TS_Tests 'GetTestStatus' @{}
Mcp-Text $TS_Tests 'GetTestResults' @{}
```

Then, sequentially on this checkout's port 8000 editor, open on clean StationHub:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-cargo.ps1 -ProbePositions
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-loop.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-ship-graph.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-incidents.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-cargo.ps1
```

The cargo script uses `Saved/AgentConsole/commands.txt`, fresh log timestamps and required captures under
`Saved/AgentCaptures/M3-4`: `loose-crate-tasks.png`, `carrying-C1.png`, `hot-crate-task.png`, `vented-C4.png`,
`hub-last-run.png`. It secures C1/C2, manually shifts Clamp_2, injects 60C into C2, racks it and checks cooling,
vents C4, checks tally pay 400, rewinds the clock to 1200, verifies nominal hull, exits and opens the hub board.
It never jumps the voyage clock forward. Visual row/text/steam inspection is required; Slate snapshots omit the
PIE HUD. The probe option exports `cargo-position-probes.csv` and checks all eight centres against the -65 floor.

After proofs, inspect `IsPIERunning`, `get_current_level`, and both maps' `is_dirty`, leave StationHub open,
update this report to actual final proof results/captures, and commit. Do not merge to main.

## Scope, recovery and uncertainties

- A direct diff against `9a01bf4` showed **no changes** to `Content/**`, `Ship/AirlockDoor.*`, `Ship/ShipGraph.*`,
  `Ship/ShipLayouts.*`, `Voyage/VoyageSchedule.*`, `Voyage/VoyageTypes.h`, `Core/Interactable.h`, or the three
  protected proof scripts. `verify-incidents.ps1` changes only the two permitted cargo-shift regex alternatives.
- Function comparison passed: `HandleTransitionRequested` and `ReturnToHub` differ only by one
  `RecordVoyageResult()` line each; `TravelToSection` and `ChoosePlayerStart_Implementation` are identical.
- No existing automation expectations were replaced. The docking test gained the requested before/after result
  checks; new tests were appended under unique helper names.
- Only `Scripts/close-editor.ps1` closed this checkout's editor. It needed its 90-second forced recovery twice;
  one intermediate close succeeded normally. After the first forced close, the Restore Packages modal blocked
  MCP startup until Skip Restore was selected. No pack restoration/save was performed. Another agent's editor
  was not closed or driven. The final app-input interruption was the Computer Use physical Escape stop.
- The failed log-capture assertion, unrun full PIE proofs, UI framing/steam visibility and final clean-map check
  are open work. Separate network-client verification remains outside this solo MVP proof.
- `.codex/` was already untracked and has been left alone. The branch's implementation is committed locally.

## Reviewer verification (Claude, 2026-09-12, after the checkpoint)

Astra's run ended at the checkpoint above (its Computer Use tool reported a physical Escape and stopped). The reviewer
finished the acceptance from Claude Code in the same checkout (`Airlock\`, editor on port 8000) once the Codex process
had exited. Everything below ran on the branch after `main` (the level pass) was merged in.

- **Merge with the level pass** (`05d32a6`): conflicts in `AirlockCharacter.*` and `AirlockPlayerController.h` only.
  One character keeps both verbs: `Interact` stands up first when seated, then traces through `TraceInteractTarget`
  (ignoring the carried crate), then places / puts down / interacts. Two guards were added at the join, outside the
  brief: sitting is refused with a crate in hand (`Seat <Id> rejected: hands full`) and picking up is refused while
  seated (`PickUp rejected: seated`).
- **The failing test** `Airlock.Cargo.DirectorReleasesFilledClampAndSkipsEmpty` (`167cc6f`): not a director bug. The
  capture device was a buffered `FOutputDevice`, so with the engine's dedicated log thread the second line was still
  queued when the test read it. The device now returns `true` from `CanBeUsedOnAnyThread()` (mutex-guarded) and the
  test calls `GLog->Flush()` before asserting.
- **The proof script** (`9b3f190`): its calm-voyage check counted every fresh line containing `Incident <Type> `, which
  also matched the two console echoes of the manual `AirlockIncident CargoShift Clamp_2` (`AgentConsole: ...` and
  `Console: ... accepted=1`), so the first run threw "A scheduled incident fired" after every gameplay step had
  passed. It now anchors on the director's own `: Incident <Type> ` line.
- **Build**: `Scripts\build-editor.bat` on the merged tree: Succeeded (UBA, 14 s incremental).
- **Tests**: `Scripts\run-tests.bat Airlock` headless: **56 / 56 passed** (34 + 4 level pass + 18 cargo; report
  `Saved\Automation\Report\index.json`, 1.3 s of test time).
- **Proofs** on the merged binary, editor on StationHub, both maps clean, one at a time:

| Proof | Result | Time |
|---|---|---|
| `verify-loop.ps1` | `LOOP OK` | 54 s |
| `verify-ship-graph.ps1` | `GRAPH OK` | 71 s |
| `verify-incidents.ps1` | `INCIDENTS OK` | 60 s |
| `verify-level-pass.ps1` | `LEVEL OK` | 151 s |
| `verify-cargo.ps1` | `CARGO OK` | 89 s |
| `verify-cargo.ps1 -ProbePositions` | `CARGO POSITIONS OK` (all eight at feet Z -64.85) | 45 s |

  Tail of the cargo proof:

```text
> AirlockTally            -> Tally now: 2/4 crates intact, 1 vented, 1 loose, hull 100.00%, pay 400
> AirlockVoyageClock 1200 ; AirlockShipStatus -> Ship Galley Nominal cond=100.00 (7 modules at 100)
> AirlockWarp -7015 1560 116 -> Voyage result: 2/4 crates intact, 1 vented, 1 loose, hull 100.00%, pay 400
                              Arrival spawn for AirlockPlayerController_0: PlayerStart_0 at X=-5640.001
> AirlockCargoStatus      -> Cargo empty
> AirlockBoard            -> Voyage board opened (day seed 20708)
CARGO OK: secure two -> shift and cool C2 -> vent C4 -> 2/4 intact, pay 400 -> hub last run
```

- **Captures reviewed** (`Saved\AgentCaptures\M3-4\`, copies in `agent\results\images\astra-m3-4\`):
  `loose-crate-tasks` (board: four `Secure crate` rows; `C1  Loose`, `C2  Loose  0C`, empty pads and the rack labelled),
  `carrying-C1` (the case hangs at the carry socket, the prompt `Carrying C1 - E: put down, or place in a clamp / the
  cold rack` bottom-centre, C1 gone from the board), `hot-crate-task` (`Cool crate C2 - 60C` on top, `C2  Loose  60C`
  under a red heat light), `vented-C4` (`C4  Vented  100C`, full red light, only `Secure crate C3` left on the board),
  `hub-last-run` (`Last run: 2/4 crates intact - hull 100% - pay 400 cr` under DEPARTURES).
- **Legibility notes for M3-6**: the carried crate's label sits 80 cm from the camera and fills the view; the vent
  steam is faint next to the red light (the light carries it); crate and pad labels face -X only.
- Editor left on StationHub, no PIE, both maps clean. Merged into `main` with `--no-ff` after this section was added.
