# M3-1 voyage clock report

Implemented on `astra/m3-voyage-clock`, created from `main` at `8535181`. No merge or push to main.

**Behaviour changed.** Boarding a ship with a pending voyage now starts a server-owned clock using the departure spec's duration. GameState replicates `VoyageEndServerTime` and `bVoyageActive`; clients calculate remaining time using synchronized server world time. The hub starts no voyage clock or captain schedule.

The captain announces departure, a voyage-type-specific midpoint, one minute to dock, and docking. The C++ widget displays the latest line at the top centre for six seconds. At expiry, the server clears the pending voyage and uses the extracted travel method to show RETURNING TO STATION, wait the existing 0.6-second fade delay, and travel to the return door's hub level (with the specified StationHub fallback). Manual return still follows the same checks and PlayerStart rules.

`AirlockVoyageClock` calls a server RPC, clamps finite requests to the original voyage duration, rejects NaN/infinity, and reschedules announcements. A forward-only announcement cursor catches up on the next timer tick and prevents replay after a backward clock move. Equal schedule times preserve list order, including zero-length and sub-minute voyages.

**Files.**

- `Source/Airlock/Core/AirlockGameMode.h/.cpp`: entry-spec snapshot, timer, captain broadcasts, clock adjustment, shared travel and automatic return.
- `Source/Airlock/Core/AirlockGameState.h/.cpp`: replicated deadline/active flag and remaining-time accessor.
- `Source/Airlock/Core/AirlockPlayerController.h/.cpp`: captain Client RPC, local widget lifecycle, clock Exec and Server RPC.
- `Source/Airlock/Voyage/VoyageSchedule.h/.cpp`: pure schedule, cursor advancement and finite/clamping rules.
- `Source/Airlock/UI/CaptainLineWidget.h/.cpp`: code-built caption; no Blueprint or UMG assets.
- `Source/Airlock/Tests/VoyageScheduleTests.cpp`: four added automation tests.
- `Scripts/verify-voyage-clock.ps1`: repeatable, timestamp-filtered PIE proof using only the AgentConsole command file, with captures and a log export.
- This report.

**Build and tests.** `Scripts/build-editor.bat` passed using MSVC 14.50; `Scripts/gen-projectfiles.bat` passed. The editor was closed politely before both build attempts, then reopened through `Scripts/run-editor.bat`; port 8000 and the additional 30-second settling period were observed. The first build found C4458 (a local `Slot` shadowed the UMG member); it was renamed to `CanvasSlot`, and the next build passed. No error failed twice.

`Scripts/run-tests.bat Airlock` exited 0: **9 succeeded, 0 warnings, 0 failures, 0 not run**. Existing expectations were unchanged.

| Test | Change | Result |
|---|---|---|
| `Airlock.Voyage.ScheduleBoundsAndCaptain` | Added: all types; durations 0, 1, 30, 60, 120, 1200; sorted bounds, captain, minute clamp and stable ties | Pass |
| `Airlock.Voyage.ScheduleDistinctTypeLines` | Added: distinct midpoint text with the same captain | Pass |
| `Airlock.Voyage.ClockMovesFireOnceInOrder` | Added: forward jumps, rewind, repeat adjustments, deadline boundaries and equal-time catch-up | Pass |
| `Airlock.Voyage.ClockMoveClampsAndRejectsNonFinite` | Added: finite clamping, zero duration, NaN and positive/negative infinity | Pass |
| `Airlock.Core.AgentConsoleFileLivesUnderSaved` | Unchanged | Pass |
| `Airlock.Core.GameModeDefaults` | Unchanged | Pass |
| `Airlock.Station.BoardAndDoorDefaults` | Unchanged | Pass |
| `Airlock.Voyage.OffersCoverEveryType` | Unchanged | Pass |
| `Airlock.Voyage.SpecIsDeterministic` | Unchanged | Pass |

Evidence: [successful build](../../Saved/M3VoyageClock/build-editor.txt), [automation JSON](../../Saved/M3VoyageClock/automation-report.json), [automation engine log](../../Saved/M3VoyageClock/automation-engine.log).

**Manual loop verification.** The unchanged `Scripts/verify-loop.ps1` exited 0. Tail:

```text
[05:59:56]   Where: map=UEDPIE_0_Hauler_Mk1 pawn=AirlockCharacter_0 at X=-6660.000 Y=1560.000 Z=113.605 rot P=0.000000 Y=0.000000 R=0.000000 view X=-6660.000 Y=1560.000 Z=177.605 rot P=0.000000 Y=0.000000 R=0.000000
[05:59:57]   capture ship-arrival -> C:\Users\tclin\Desktop\ai_game_studio\Airlock\Saved\AgentCaptures\055957-CaptureEditorImage.png
[05:59:57] > AirlockWarp -7015 1560 116
[06:00:13] > AirlockWhere
[06:00:14]   Where: map=UEDPIE_0_StationHub pawn=AirlockCharacter_0 at X=-5640.001 Y=1559.997 Z=18.534 rot P=0.000000 Y=0.000000 R=0.000000 view X=-5640.001 Y=1559.997 Z=82.534 rot P=0.000000 Y=0.000000 R=0.000000
[06:00:14]   capture hub-return -> C:\Users\tclin\Desktop\ai_game_studio\Airlock\Saved\AgentCaptures\060014-CaptureEditorImage.png
[06:00:17] LOOP OK: hub spawn -> booked -> Dock B -> Hauler_Mk1 PlayerStart -> exit -> hub PlayerStart, voyage complete
```

Full output: [verify-loop.txt](../../Saved/M3VoyageClock/verify-loop.txt). Manual-loop captures: [hub spawn](../../Saved/AgentCaptures/055940-CaptureEditorImage.png), [ship arrival](../../Saved/AgentCaptures/055957-CaptureEditorImage.png), [hub return](../../Saved/AgentCaptures/060014-CaptureEditorImage.png).

**Timed PIE proof.** `Scripts/verify-voyage-clock.ps1` exited 0. On 2026-09-12, the clock moved to 75 seconds at 13:05:06.247 UTC; the one-minute line fired at 13:05:21.248 (**15.001 seconds later**). Docking and voyage completion fired at 13:06:21.250 (**75.003 seconds after adjustment**). The ship's return door supplied `/Game/Maps/StationHub`. Arrival used the hub PlayerStart, and `AirlockWhere` confirmed StationHub at **X=-5640.001, Y=1559.997**. No return-door warp was sent in this proof.

The screenshots were visually inspected: [one-minute caption](../../Saved/M3VoyageClock/one-minute.png), [caption hidden afterward](../../Saved/M3VoyageClock/caption-hidden.png), [RETURNING TO STATION](../../Saved/M3VoyageClock/return-card-1.png), [automatic hub arrival](../../Saved/M3VoyageClock/auto-return-hub.png). The script takes three return-card frames because a capture during world teardown can be blank; frame 1 contains the full card in the passing run.

Full output: [verify-voyage-clock.txt](../../Saved/M3VoyageClock/verify-voyage-clock.txt). The following is every `LogAirlock` line from the ship section announcement through the final hub `AirlockWhere` in the passing run (UTC):

```text
[2026.09.12-13.04.57:105][268]LogAirlock: Section Hauler_Mk1: voyage underway (type=0 ship=Hauler_Mk1 captain=Okonkwo)
[2026.09.12-13.04.57:113][268]LogAirlock: Arrival spawn for AirlockPlayerController_1: PlayerStart_0 at X=-6660.000 Y=1560.000 Z=27.958
[2026.09.12-13.04.57:115][268]LogAirlock: AirlockGameMode: listening to 10 airlock door(s)
[2026.09.12-13.04.57:115][268]LogAirlock: Voyage clock started: captain=Okonkwo duration=1200s endServerTime=1209.894
[2026.09.12-13.04.57:115][268]LogAirlock: AirlockPlayerController: default mapping context registered
[2026.09.12-13.04.57:126][268]LogAirlock: Captain Okonkwo: Captain Okonkwo here. We're underway. Welcome aboard, crew.
[2026.09.12-13.05.05:733][778]LogAirlock: AgentConsole: AirlockWhere
[2026.09.12-13.05.05:733][778]LogAirlock: Where: map=UEDPIE_0_Hauler_Mk1 pawn=AirlockCharacter_0 at X=-6660.000 Y=1560.000 Z=113.605 rot P=0.000000 Y=0.000000 R=0.000000 view X=-6660.000 Y=1560.000 Z=177.605 rot P=0.000000 Y=0.000000 R=0.000000
[2026.09.12-13.05.06:247][805]LogAirlock: AgentConsole: AirlockVoyageClock 75
[2026.09.12-13.05.06:247][805]LogAirlock: Voyage clock moved: requested=75.000s remaining=75.000s endServerTime=94.238 nextAnnouncement=1
[2026.09.12-13.05.06:260][806]LogAirlock: Captain Okonkwo: Captain Okonkwo here. We're in the ore field. Mining crew, prepare the holds.
[2026.09.12-13.05.21:248][612]LogAirlock: Captain Okonkwo: Captain Okonkwo here. One minute to dock. Secure your gear.
[2026.09.12-13.05.21:683][636]LogAirlock: AgentConsole: AirlockWhere
[2026.09.12-13.05.21:683][636]LogAirlock: Where: map=UEDPIE_0_Hauler_Mk1 pawn=AirlockCharacter_0 at X=-6660.000 Y=1560.000 Z=113.605 rot P=0.000000 Y=0.000000 R=0.000000 view X=-6660.000 Y=1560.000 Z=177.605 rot P=0.000000 Y=0.000000 R=0.000000
[2026.09.12-13.06.21:250][878]LogAirlock: Captain Okonkwo: Captain Okonkwo here. Docking now. Welcome back to the station.
[2026.09.12-13.06.21:250][878]LogAirlock: Voyage CC96A40846614763F506A99E2A08D37E complete
[2026.09.12-13.06.21:250][878]LogAirlock: Voyage clock expired: travelling to /Game/Maps/StationHub (section StationHub) in 0.6s
[2026.09.12-13.06.21:250][878]LogAirlock: Transition card: RETURNING TO STATION / Hauler_Mk1  ->  StationHub
[2026.09.12-13.06.21:250][878]LogAirlock: Transition: RETURNING TO STATION -> /Game/Maps/StationHub (section StationHub) in 0.6s
[2026.09.12-13.06.22:253][909]LogAirlock: AirlockGameMode::InitGame map=StationHub options= arrivedByAirlock=1
[2026.09.12-13.06.22:254][909]LogAirlock: Section StationHub: no voyage booked (day seed 20708)
[2026.09.12-13.06.22:255][909]LogAirlock: Arrival spawn for AirlockPlayerController_0: PlayerStart_0 at X=-5640.001 Y=1559.997 Z=20.376
[2026.09.12-13.06.22:256][909]LogAirlock: AirlockGameMode: listening to 4 airlock door(s)
[2026.09.12-13.06.22:256][909]LogAirlock: AirlockPlayerController: default mapping context registered
[2026.09.12-13.06.30:686][408]LogAirlock: AgentConsole: AirlockWhere
[2026.09.12-13.06.30:686][408]LogAirlock: Where: map=UEDPIE_0_StationHub pawn=AirlockCharacter_0 at X=-5640.001 Y=1559.997 Z=18.534 rot P=0.000000 Y=0.000000 R=0.000000 view X=-5640.001 Y=1559.997 Z=82.534 rot P=0.000000 Y=0.000000 R=0.000000
```

Raw export: [voyage-clock-log.txt](../../Saved/M3VoyageClock/voyage-clock-log.txt). Captures and raw logs remain under ignored `Saved/`; this report preserves the key text evidence in git.

**Reproduce.** From the repo root, with the editor open on clean StationHub and MCP ready, run:

```powershell
Scripts\run-tests.bat Airlock
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-loop.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-voyage-clock.ps1
```

The last script starts PIE at `(-5640, 1560, 118)`, sends the commands below one at a time through `Saved/AgentConsole/commands.txt`, waits for each file to disappear, and uses fresh log lines to synchronize the proof. It captures the captain line, its disappearance, the return card, and the final hub view, then stops PIE.

```text
AirlockWhere
AirlockWarp -4700 1560 116
AirlockBook 0
# Wait 2 seconds before crossing.
AirlockWarp -4265 1560 116
# Wait for Section Hauler_Mk1 and 8 seconds for exposure.
AirlockWhere
AirlockVoyageClock 75
# Wait for the one-minute captain line (about 15 seconds).
AirlockWhere
# Capture the line, then wait for automatic return; do not warp to the ship exit.
# After hub arrival and 8 seconds for exposure:
AirlockWhere
```

To rebuild first, use the following exact PowerShell sequence from the repo root. A modal close dialog requires the HANDOFF section 2 procedure; the sequence stops before building if the editor has not exited.

```powershell
Get-Process UnrealEditor -ErrorAction SilentlyContinue | ForEach-Object { [void]$_.CloseMainWindow() }
$closeDeadline = (Get-Date).AddSeconds(90)
while ((Get-Process UnrealEditor -ErrorAction SilentlyContinue) -and (Get-Date) -lt $closeDeadline) {
    Start-Sleep -Seconds 2
}
if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { throw 'Editor still running; resolve its close dialog before building' }
Scripts\build-editor.bat
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
Scripts\run-editor.bat
$portDeadline = (Get-Date).AddSeconds(180)
$portReady = $false
while ((Get-Date) -lt $portDeadline) {
    $probe = New-Object Net.Sockets.TcpClient
    try { $probe.Connect('127.0.0.1', 8000); $portReady = $true } catch {} finally { $probe.Dispose() }
    if ($portReady) { break }
    Start-Sleep -Seconds 3
}
if (-not $portReady) { throw 'Editor MCP port did not open' }
Start-Sleep -Seconds 30
Scripts\run-tests.bat Airlock
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-loop.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-voyage-clock.ps1
```

**Open questions and limits.** None blocking. The mandated proof used one local PIE player and the 75-second helper, not a remote multiplayer session or a full 20-minute wait. Re-booking aboard ship remains out of scope; the clock retains the spec captured at section start, and the proof did not re-book aboard. The fallback hub path was inspected in code; the actual map's return door supplied the path during PIE.

Two early proof-script attempts exposed harness issues, both corrected before the passing run: the MCP log view lagged command-file deletion, and the first hub-arrival pattern required exact Y=1560 despite the serialized PlayerStart being Y=1559.997. The final script requires command-fresh `AirlockWhere` responses and checks arrival within one centimetre. No gameplay change was required. An interim attempt to query editor asset state during PIE was rejected by MCP; final clean-map checks were performed after PIE stopped.

**Scope and final state.** No `Content/**`, pack asset, door source, existing test expectation, `VoyageSubsystem`, or `verify-loop.ps1` changes. The complete checks at the start of `HandleTransitionRequested` were compared with main and are text-identical; only the subsequent travel portion was extracted. Project-file generation temporarily rewrote `.ignore`; that generated change was restored. The added proof script and this report support the brief's verification requirements. No protected-scope change was needed.

Final MCP checks: PIE false; current level `/Game/Maps/StationHub`; StationHub dirty false; Hauler_Mk1 dirty false. The editor is left open, and the branch is committed locally without merging to main. Gameplay commits: `e358d8f` (captain schedule and pure rules), `7ff422d` (clock, captain display and automatic return).
