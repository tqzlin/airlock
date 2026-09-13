# M4-1 report: dev-cheats gate, the file console per PIE instance, the first two-client run

Claude, 2026-09-12 (night), branch `claude/m4-1-dev-cheats-and-two-clients` from `main` at 6e44c48 (after M3-7), in
the worktree `..\Airlock-claude` with its own editor on port 8001 while Astra ran M3-8 on port 8000. Brief:
`Docs/tasks/M4-1-dev-cheats-and-two-clients.md`.

## Behaviour changed

- **Every console helper is gated on the server.** `airlock.DevCheats` (console variable, 1 wherever an editor is
  compiled in, 0 in every packaged build until an operator sets it) is read by `AAirlockGameMode::AllowCheats`; `AAirlockPlayerController::CheatAllowed(Helper)`
  is the first statement of all 27 `Server*` RPCs that back the `Airlock*` Exec helpers. With the variable at 0 a
  call does nothing and logs `Cheat AirlockWarp refused for <player>: airlock.DevCheats is 0`; nobody is kicked (no
  `_Validate`, on purpose - a dev switch must not disconnect a tester). `ServerChooseVoyage` stays open: it is the
  departure board's own path and its input is an enum the server already validates. Local helpers (`AirlockBoard`,
  `AirlockWhere`, `AirlockLook`, the new `AirlockCapture`) are not RPCs and are not gated. In the editor nothing
  changes: the variable defaults to 1, so every existing proof runs as before.
- **The file console has one file per game instance.** PIE instance 0 (the listen server, or the only player) and a
  non-PIE game poll `Saved/AgentConsole/commands.txt` as before; PIE client N polls `commands.client<N>.txt` and
  echoes `AgentConsole client N: <line>`. Every instance logs `<prefix>: watching <path>` when it starts and
  `<prefix>: ready (map=<world>, pc=<controller>)` each time it finds the local player controller in a world it has
  not announced - a script waits for `client 1: ready` with `UEDPIE_1_Hauler_Mk1` before driving the client after a
  travel. During a seamless travel the subsystem neither polls nor announces, so a command is only ever executed in
  the destination world. The decision uses the PIE instance id, not the net mode: a client's world reads as
  standalone until it has connected. The subsystem is no longer created for a game instance without a world context
  (the automation harnesses), so a test run cannot eat a proof's command.
- **`AirlockCapture <Label>`** (local Exec, not in Shipping): a screenshot of this instance's own game viewport
  through `FSlateApplication::TakeScreenshot` on the viewport widget, UMG included, saved to
  `Saved/AgentCaptures/<Label>.png` (`Capture <Label> saved 640x483: <path>`). The in-viewport server instance gives
  the game area without the editor chrome (2032 x 1273 here); the client window gives its 640 x 483 view.
- **Crew arriving at one PlayerStart step aside** (found by the proof's first run, see below).
  `AAirlockGameMode::SpawnDefaultPawnAtTransform` skips a candidate spot where a pawn possessed by another
  controller stands within 90 cm (and 200 cm of height) and tries `ArrivalSpacing` (120 cm) steps along the start's
  right axis, alternating sides up to two steps, skipping a side step that is inside a wall
  (`Arrival spawn for AirlockPlayerController_1 stepped +120 cm aside: the start was occupied`); with nothing clear
  it still spawns (`AdjustIfPossibleButAlwaysSpawn`) with a warning. A single player spawns exactly where it did.
  The first version counted every pawn and stepped a lone player aside at every PIE start; the nine regression
  proofs caught it at once (`Arrival missed PlayerStart`).

## Files

- `Source/Airlock/Core/AirlockGameMode.h/.cpp` - the console variable, `AllowCheats`, `SpawnDefaultPawnAtTransform`,
  `ArrivalSpacing`.
- `Source/Airlock/Core/AirlockPlayerController.h/.cpp` - `CheatAllowed`, the 27 gate lines, `AirlockCapture`.
- `Source/Airlock/Core/AgentConsoleSubsystem.h/.cpp` - `CommandFilePathFor`, `EchoPrefixFor`, the instance-aware
  poll, the `watching` / `ready` lines.
- `Source/Airlock/Tests/DevCheatsTests.cpp` (3 tests), `Source/Airlock/Tests/ArrivalSpawnTests.cpp` (2 tests).
- `Scripts/verify-two-clients.ps1`, `Docs/tasks/M4-1-dev-cheats-and-two-clients.md`, this report.
- Nothing under `Ship/`, `UI/`, `Station/`, `Voyage/`, `Content/`, no map, no existing test or proof script.

## Verification

- `Scripts\build-editor.bat` clean. `Scripts\run-tests.bat Airlock`: **95 / 95** (90 + `Airlock.Cheats.GateFollowsConsoleVariable`,
  `Airlock.Cheats.EveryConsoleRpcIsGated`, `Airlock.Cheats.ConsoleFileFollowsPIEInstance`,
  `Airlock.Multiplayer.ArrivalsStepAsideAtOneStart`, `Airlock.Multiplayer.ArrivalSkipsBlockedSideStep`).
- `Scripts\verify-two-clients.ps1` on port 8001: see "The two-client run" below.
- The nine existing proofs on this branch's binary: see the table below.

## The two-client run

The proof switches the editor's play settings to listen server / 2 clients / one process / 960 x 540 client window
(restored in `finally`), starts PIE in the viewport from the hub PlayerStart, and drives the server through
`commands.txt` and client 1 through `commands.client1.txt`.

Tail of the passing run (the console echoes and the `LogAirlock` lines the script waited for; `server>` / `client>` show which file the command went through):

```text
AgentConsole: ready (map=UEDPIE_0_StationHub)
AgentConsole client 1: ready (map=UEDPIE_1_StationHub)
server> AirlockWhere
Where: map=UEDPIE_0_StationHub pawn=AirlockCharacter_0 at X=-5640.000 Y=1680.000 Z=30.997 rot P=0.000000 Y=0.000000 R=0.000000 view X=-5640.
client> AirlockWhere
Where: map=UEDPIE_1_StationHub pawn=AirlockCharacter_0 at X=-5640.000 Y=1560.000 Z=18.534 rot P=0.000000 Y=0.000000 R=0.000000 view X=-5640.
server> AirlockBook 2
client> AirlockWarp -4700 1560 116
client> AirlockWarp -4265 1560 116
Section Hauler_Mk1: voyage underway (type=2 ship=Hauler_Mk1 captain=Tanaka)
AgentConsole client 1: ready (map=UEDPIE_1_Hauler_Mk1)
Arrival spawn for AirlockPlayerController_2: PlayerStart_0 at X=-6660.000 Y=1560.000 Z=27.958
Arrival spawn for AirlockPlayerController_2 stepped +120 cm aside: the start was occupied
Arrival spawn for AirlockPlayerController_3: PlayerStart_0 at X=-6660.000 Y=1560.000 Z=27.958
Where: map=UEDPIE_0_Hauler_Mk1 pawn=AirlockCharacter_0 at X=-6660.000 Y=1680.000 Z=113.730 rot P=0.000000 Y=0.000000 R=0.000000 view X=-6660
Where: map=UEDPIE_1_Hauler_Mk1 pawn=AirlockCharacter_0 at X=-6660.000 Y=1560.000 Z=113.605 rot P=0.000000 Y=0.000000 R=0.000000 view X=-6660
client> AirlockWarp -6330 1700 116
client> AirlockCarry C1
Carry C1 by AirlockCharacter_1
server> AirlockCargoStatus
Cargo C1 Carried temp=0 volatile=0 slot=None
client> AirlockWarp -6430 1400 116
client> AirlockPlace Clamp_1
Cargo C1 Secured temp=0
client> AirlockLook 90
client> AirlockCapture m4-1-client-hold
Capture m4-1-client-hold saved 640x483: C:/Users/tclin/Desktop/ai_game_studio/Airlock-claude/Saved/AgentCaptures/m4-1-client-hold.png
server> AirlockWarp -6430 1700 116
server> AirlockLook -90
server> AirlockCapture m4-1-server-hold
Capture m4-1-server-hold saved 2032x1273: C:/Users/tclin/Desktop/ai_game_studio/Airlock-claude/Saved/AgentCaptures/m4-1-server-hold.png
client> AirlockWarp 1760 240 130
client> AirlockSit Bridge
Seat Bridge taken by AirlockCharacter_1
client> AirlockCapture m4-1-client-seat
Capture m4-1-client-seat saved 640x483: C:/Users/tclin/Desktop/ai_game_studio/Airlock-claude/Saved/AgentCaptures/m4-1-client-seat.png
client> AirlockStand
Seat Bridge released by AirlockCharacter_1
client> AirlockScrub Hazard
Air 0 ppm scrub=Hazard
server> airlock.DevCheats 0
client> AirlockWarp 0 0 0
Cheat AirlockWarp refused for stack-1E80C89C40BD49: airlock.DevCheats is 0
Where: map=UEDPIE_1_Hauler_Mk1 pawn=AirlockCharacter_0 at X=1790.000 Y=240.000 Z=118.313 rot P=0.000000 Y=90.000000 R=-0.000000 view X=1790.
server> airlock.DevCheats 1
server> AirlockVoyageClock 5
Voyage result: 0/12 crates intact, 0 vented, 12 loose, hull 85.59%, pay 321, air 0 ppm, 0 blackouts, 0 burned, fuel +0 t
Arrival spawn for AirlockPlayerController_0 stepped +120 cm aside: the start was occupied
Where: map=UEDPIE_0_StationHub pawn=AirlockCharacter_0 at X=-5640.001 Y=1679.997 Z=30.997 rot P=0.000000 Y=0.000000 R=0.000000 view X=-5640.
Where: map=UEDPIE_1_StationHub pawn=AirlockCharacter_0 at X=-5640.000 Y=1560.000 Z=18.500 rot P=0.000000 Y=0.000000 R=0.000000 view X=-5640.
TWO CLIENTS OK: server + client 1 on the hub -> the client crosses Dock B, both aboard -> client carries and secures C1 -> client at the sea
```

**TWO CLIENTS OK** in about 2 min 30 s.

**What the first run found.** Everything up to the docking return passed on the first try (per-instance console,
the client crossing Dock B travelling everyone, the client's carry / place / seat / scrub applied on the server and
mirrored on both boards, the refused warp). After the automatic return the client's `Where` said `pawn=none`: the
engine log showed `SpawnActor failed because of collision at the spawn location ... for [AirlockCharacter]` and
`SpawnDefaultPawnAtTransform: Couldn't spawn Pawn`, because the server's pawn already stood on the hub's only
PlayerStart and `APawn`'s default collision handling gives up rather than overlap. On the ship the same collision
happened to resolve with an 84 cm nudge (client at Y 1476 instead of 1560). The second player home from a run was
a bodiless spectator - a real M4 bug no single-player proof could see. The arrival step-aside above fixes it and the
proof now asserts the `stepped +120 cm aside` line on both the ship and the hub.

## Captures (`Saved\AgentCaptures\M4-1\`)

- `m4-1-client-hold.png` - the client's own window: `SHIP TASKS | 19:44 to dock` with `Secure crate C2..C9` and
  `+3 more` (C1 is the one it secured), the hold from its side of the walkway.
- `m4-1-server-hold.png` - the server's viewport, same board state (`C1 Secured` label at the right edge), no editor
  chrome.
- `m4-1-client-seat.png` - the client seated at the Bridge: the control board with `Hauler_Mk1 | 19:30 to dock`,
  `AIR 0 ppm SCRUBBERS Normal`, `FUEL 88% BYPASS closed`, seven modules and four lines, all replicated to a client
  that is not the server.
- `m4-1-editor-two-windows.png` - `CaptureEditorImage` at the end (best effort; the client window is a separate
  top-level window and may not be in it).
- `two-clients-log.txt` - every `LogAirlock` line of the run.

## Red team (second reader, 24 findings, folded before the final proofs)

Folded: the test's variable scope sets with `ECVF_SetByConsole` (a `SetByCode` set is ignored once a proof has
touched the variable from the console); the proof restores `airlock.DevCheats 1` in `finally` while PIE still runs;
the default is 1 only where an editor is compiled in (a packaged Development listen server no longer accepts
`AirlockSaveReset` from a guest); the file console skips polling and the `ready` line during a seamless travel and
drives the *local* player controller, naming it in the `ready` line; no subsystem for a game instance without a
world context; the arrival check has a Z tolerance and an `EncroachingBlockingGeometry` pre-check on the side
steps, with a test for a blocked side; the RPC test calls `ServerBaseFuel` in its read-only mode; a controller
spawned at 0 is asserted to have no cheat manager; the proof restores `bLaunchSeparateServer` too, re-reads the
settings after the restore and falls back to the single-player defaults, does an `AirlockWhere` before every
capture, checks the server's pawn did not move during the client's carry, and pins the seat's exit point; the brief
records the engine facts the reader verified (RPC callspace in a standalone test world, `AllowCheats` only consulted
at controller creation, `TakeScreenshot` on the widget not the window, ImageCore public through Engine,
`PlayNumberOfClients` counting the listen server, PIE client world names). Not folded: a `-ProbePositions` mode for
the +-120 spots (both maps' arrival spots were seen in the proof), PlayerState names in the server's gameplay log
lines (an M4 follow-up), joining mid-voyage.

## Decisions and open questions

- The gate is a console variable rather than a CheatManager subclass or `_Validate`: it is one switch a server
  operator can flip, it defaults the right way per build, and a refused call is logged rather than punished.
  `AllowCheats` is also what the engine asks before creating its own `UCheatManager`, which the project does not use.
- **The editor persists the play settings at PIE start, not at restore.** After the first two runs the editor was
  rebuilt and relaunched and came back in listen-server / 2-client mode: `StartPIE` had written the settings the
  proof set to `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`, and the restore through
  `set_properties` only changed memory. The nine regression proofs then failed on arrival positions until the ini
  was fixed by hand. The proof now snapshots the `[/Script/UnrealEd.LevelEditorPlaySettings]` section before it
  starts and writes it back in `finally`, after the in-memory restore.
- **Interference with Astra's M3-8 run (my fault, 22:08).** While clearing my own stuck proof processes after the
  two-client-mode mishap I stopped every `powershell.exe` whose command line contained `verify-`; one of them was the
  Codex shell wrapper running Astra's `verify-fuel.ps1` on port 8000, which died with exit -1 two minutes in ("while
  waiting for the injector hold" in Astra's transcript). Astra's editor was untouched and it re-ran. If the M3-8
  report mentions a fuel proof that exited -1 unexpectedly, that was this, not the code. Rule from now on: filter
  by checkout path (`Airlock-claude`) before stopping anything, never by script name.
- `airlock.DevCheats` is process-wide: in a one-process PIE session the server's `airlock.DevCheats 0` also applies
  to the client's local helpers, which is fine because the gate is checked on the server anyway.
- The client's hold capture faces +Y from the walkway (`AirlockLook 90`), which is the wrong way for the crate rows
  north of it; the point of the capture is the board, and M3-8 is changing what the labels look like.
- Exposure on the PlayerState and a server-side base fuel stock (the other two M4 groundwork items in STATUS) are
  not in this task: the two-client run showed the replicated game state already carries what the client needs, and
  exposure resets on the hub anyway. They stay on the list.
- Not proven: a client joining mid-voyage, a client leaving during travel, more than two players (the arrival row
  is tested to five in a spawned world only).

## Reproduce

From `C:\Users\tclin\Desktop\ai_game_studio\Airlock-claude` (or any checkout of the branch), with the editor open on a
clean StationHub on the port in `AIRLOCK_MCP_PORT`:

```powershell
Scripts\run-tests.bat Airlock
$env:AIRLOCK_MCP_PORT = '8001'
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-two-clients.ps1
```
