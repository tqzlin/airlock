# M3-3 report: Incident Director and task board

GPT-6 Astra, 2026-09-12. Branch `astra/m3-3-incident-director`, created from `main` at `0906d83`.
Implementation and final binary: `83eaf5f`. Work is committed locally; nothing merged to main.
Brief: [M3-3](../tasks/M3-3-incident-director.md). Operating guide: [HANDOFF](../HANDOFF.md).

## Behaviour changed

- Voyages now break ship systems automatically. A separate seeded schedule starts with a coolant leak at 150 seconds,
  spreads subsequent incidents through the voyage, and ends with two incidents 10-20 seconds apart at 78-85% of a
  normal voyage. Damage targets eligible layout modules for 30-50 points; consecutive targets never repeat.
- The server game mode creates an incident component after building the graph and starting the voyage clock. It
  samples crew once, runs one incident timer at a time, applies faults/damage through the existing graph API, and
  sends captain captions to every controller. Missing, incompatible, already-faulted, and Failed targets are skipped
  and logged. Manual damage rejects non-finite/non-positive magnitudes.
- Clock jumps consume past entries once, in order. Rewinds never replay them. Direct jumps to docking explicitly
  flush due incidents before the docking caption and return, independent of equal-time timer heap ordering.
- A passive top-left task board reads only the existing replicated game state. Failed modules come first (red),
  followed by faulted lines and Degraded modules; eight tasks are shown before a `+n more` row. Repairs remove rows.
  The panel shows the existing replicated countdown, says `All systems nominal` when empty, and hides off-voyage
  and on the hub. There is no new replication, RPC for the board, Blueprint, or widget asset.
- `AirlockIncident <CoolantLeak|PowerShort|ModuleDamage> [TargetId] [Magnitude]` injects an incident through a server
  RPC. An omitted target chooses the first available eligible layout target; omitted damage is 40. The companion
  `AirlockIncidentSchedule` logs the captured schedule and consumed-entry cursor.

## Files

- `Source/Airlock/Voyage/IncidentSchedule.h/.cpp`: pure incident types, seeded timing/targets, captain text and cursor.
- `Source/Airlock/Voyage/IncidentDirectorComponent.h/.cpp`: server timer chain, application/skip rules and schedule logging.
- `Source/Airlock/Core/AirlockGameMode.h/.cpp`: component creation, clock rescheduling and final docking catch-up.
- `Source/Airlock/UI/TaskBoardWidget.h/.cpp`: C++ HUD and task-row derivation.
- `Source/Airlock/Core/AirlockPlayerController.h/.cpp`: local HUD lifecycle and developer console RPCs.
- `Source/Airlock/Tests/IncidentScheduleTests.cpp`, `IncidentDirectorTests.cpp`, `TaskBoardTests.cpp`: ten new tests.
- `Scripts/verify-incidents.ps1`: fresh-log PIE proof, captures, clock-rewind and console-edge checks.
- This report. No other tracked files changed.

## Verification

`Scripts\build-editor.bat`: **Succeeded**, UE 5.8.2 / MSVC 14.50. Final build log:
[build-editor.txt](../../Saved/AgentCaptures/M3-3/build-editor.txt).

In-editor `DiscoverTests` -> `RunTestsByFilter Airlock` -> `GetTestStatus`: **34 complete, 34 passed, 0 failed**.
The six pure schedule tests were also built, run and committed before the director was implemented.
Final [per-test results](../../Saved/AgentCaptures/M3-3/all-tests.json) contain no test errors or warnings.
The equivalent headless reviewer command is `Scripts\run-tests.bat Airlock`; this session used the in-editor
automation runner allowed by HANDOFF section 2.

| Test | Result |
|---|---|
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

The schedule timing/target tests sweep 100 seeds; normal timing is checked at 900, 1200 and 1800 seconds and
crew counts 1, 4 and 9. Short/boundary durations include 0, 1, 30, 120, 239, 240, 300 and 600 seconds, plus invalid
durations. Director tests use transient worlds with an actual booked voyage and the game-mode-created component.

All three PIE scripts were rerun after the final C++ change, **each exit 0**:

```text
[13:11:29] Where: map=UEDPIE_0_Hauler_Mk1 pawn=AirlockCharacter_0 at X=-6660.000 Y=1560.000
[13:11:47] Where: map=UEDPIE_0_StationHub pawn=AirlockCharacter_0 at X=-5640.001 Y=1559.997
[13:11:50] LOOP OK: hub spawn -> booked -> Dock B -> Hauler_Mk1 PlayerStart -> exit -> hub PlayerStart, voyage complete
GRAPH OK: seven nominal modules -> coolant leak -> degraded coolant drifts core -> hold repairs line -> coolant restored -> hub PlayerStart, empty graph
INCIDENTS OK: solo schedule -> teaching leak and captain -> hold repair clears row -> core task -> climax -> no replay -> hub
```

Complete script outputs: [loop](../../Saved/AgentCaptures/M3-3/verify-loop-output.txt),
[graph](../../Saved/AgentCaptures/M3-3/verify-ship-graph-output.txt),
[incidents](../../Saved/AgentCaptures/M3-3/verify-incidents-output.txt).
The graph regression again showed PowerCore at 93.75 after coolant degradation and a successful three-second repair.

## Incident PIE evidence

Solo mining voyage, seed `20708`, Captain Okonkwo. The four scheduled entries were:

```text
Incident schedule entry=0 at 150.000s CoolantLeak Coolant_1 (0.00)
Incident schedule entry=1 at 358.513s CoolantLeak Coolant_2 (0.00)
Incident schedule entry=2 at 995.480s CoolantLeak Coolant_1 (0.00)
Incident schedule entry=3 at 1014.964s PowerShort Power_1 (0.00)
```

Selected fresh `LogAirlock` lines (UTC; full [incident log](../../Saved/AgentCaptures/M3-3/incidents-log.txt)):

```text
20:13:33.614 Line Coolant_1 fault=Leak
20:13:33.615 Incident CoolantLeak Coolant_1 (0.00) at 150.000s elapsed=160.017s
20:13:33.615 Captain Okonkwo: Captain Okonkwo here. Pressure drop on the coolant loop - find the leak at Coolant_1.
20:13:37.613 Hold Coolant_1 complete by AirlockCharacter_0
20:13:37.613 Line Coolant_1 fault=None
20:13:39.708 Ship PowerCore Degraded cond=55.00
20:13:41.746 Incident CoolantLeak Coolant_2 (0.00) at 358.513s elapsed=1130.017s
20:13:41.748 Incident CoolantLeak Coolant_1 (0.00) at 995.480s elapsed=1130.017s
20:13:41.748 Incident PowerShort Power_1 (0.00) at 1014.964s elapsed=1130.017s
20:13:45.261 Incident schedule: 4 entries next=4 duration=1200s
20:13:46.773 Incident PowerShort Power_1 (0.00) at 1133.028s elapsed=1133.028s skipped: already faulted
20:13:47.279 Incident ModuleDamage Missing (45.00) at 1133.534s elapsed=1133.534s skipped: missing or incompatible target
20:13:48.292 Incident ModuleDamage PowerCore (40.00) at 1134.546s elapsed=1134.546s
20:13:58.335 Incident schedule empty: no active incident director
```

The climax is scheduled **19.484 seconds apart**, at 82.957% and 84.580% of the voyage. Both applied in the same
server frame when the clock jumped to 70 seconds remaining. The script also rewound to 1200 then repeated 70 and
asserted no repeated incidents. Omitted target/magnitude, an already-faulted target and a missing target were exercised.

Captures were inspected visually: the teaching caption and leak row appear together; completing the hold replaces
the row with the nominal state; the core row reads `PowerCore - Degraded 55%`; climax faults precede that row; the
hub has no task board. `AirlockWhere` verified the pawn before the captures; no lighting/map adjustments were needed.

![Teaching leak and captain caption](../../Saved/AgentCaptures/M3-3/teaching-leak-and-captain.png)

![The repaired leak leaves the nominal task board](../../Saved/AgentCaptures/M3-3/teaching-leak-repaired.png)

Additional captures: [nominal](../../Saved/AgentCaptures/M3-3/tasks-nominal.png),
[core damage](../../Saved/AgentCaptures/M3-3/core-damaged-task.png),
[climax](../../Saved/AgentCaptures/M3-3/climax-tasks.png),
[hub return](../../Saved/AgentCaptures/M3-3/returned-to-hub.png).
Evidence stays under ignored `Saved/`; rerunning the scripts recreates it on another checkout.

## Exact reproduction commands

Run in PowerShell from `C:\Users\tclin\Desktop\ai_game_studio\Airlock`. Close the editor first as required by
HANDOFF section 2; wait for exit and use that section's 90-second recovery rule if necessary. Then:

```powershell
git switch astra/m3-3-incident-director
Scripts\build-editor.bat
Scripts\run-editor.bat
```

Wait for port 8000 to listen, then another 30 seconds. To reproduce the same in-editor automation run:

```powershell
. Scripts\lib\mcp.ps1
Mcp-Init
Mcp-Text $TS_Tests 'DiscoverTests' @{}
Mcp-Text $TS_Tests 'RunTestsByFilter' @{ filterExpression = 'Airlock' }
Mcp-Text $TS_Tests 'GetTestStatus' @{}
Mcp-Text $TS_Tests 'GetTestResults' @{}
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-loop.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-ship-graph.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-incidents.ps1
```

Each proof starts and stops its own PIE session and uses `Saved\AgentConsole\commands.txt`. The incident proof
discovers the teaching target from the schedule, warps to it, sends `AirlockVoyageClock 1040`, holds to repair,
injects `AirlockIncident ModuleDamage PowerCore 45`, sends `AirlockVoyageClock 70`, checks replay/console edges,
and returns through the exit. Inspect its six captures as part of review.

## Interpretations, limitations and recovery notes

- **Conflicting crew requirements:** the brief says to clamp crew to four (which implies at most seven incidents),
  but explicitly requires `9 -> 8` in the tests. I asked during the session; no answer arrived. This implementation
  follows the explicit test examples: `1 -> 4`, `4 -> 7`, `9 -> 8`, saturating at eight. The reviewer should retain
  or revise that interpretation deliberately; both statements cannot be satisfied simultaneously.
- **Constrained durations:** below 240 seconds the curve maps into the middle 50%, with proportionally scaled gaps.
  Some durations above that threshold also cannot fit the specified count, 90-second gaps, and climax placement.
  Those preserve count/order and calm/docking bounds, compress ordinary gaps only when necessary, and clamp the
  climax into the available window. Normal 15-, 20-, and 30-minute voyages satisfy the unscaled bounds. Invalid
  durations or layouts incapable of a teaching leak/non-repeating targets yield an empty schedule.
- PIE proof is solo/listen-server. Separate network-client testing remains M4 work. These developer console RPCs
  have the same unchecked-cheat caveat as the existing helpers and need gating before M4.
- Slate accessibility snapshots omit the PIE HUD even when it is rendered. The first incident proof stopped on
  that unsupported assertion; the final script checks graph logs, requires captures, and leaves their visual
  inspection to the reviewer. The final captures were inspected in this session.
- One build failed on C4458 (`Slot` shadowed `UWidget::Slot`); renaming the local variable fixed it on the next
  build. No build error failed twice. Two editor closes remained stuck beyond 90 seconds; the processes were
  terminated and relaunched using the documented recovery. No pack assets or maps were saved by this work.

## Scope and final state

Nothing outside the brief was touched. Git comparison confirms no changes to `Content/**`, `Ship/AirlockDoor.*`,
`Ship/ShipGraph.*`, `Ship/ShipLayouts.*`, or `Voyage/VoyageSchedule.*`. Direct function comparison confirms
`HandleTransitionRequested`, `TravelToSection`, and `ReturnToHub` are unchanged. Existing test expectations are unchanged.

Final MCP checks: `IsPIERunning = false`, current level `/Game/Maps/StationHub`, and `is_dirty = false` for both
StationHub and Hauler_Mk1. The editor remains open. `main` remains at `0906d83`; this branch is committed locally.
