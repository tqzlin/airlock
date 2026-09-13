# Task M4-1: Dev-cheats gate, per-instance file console, the first two-client proof

Owner: Claude. Branch: `claude/m4-1-dev-cheats-and-two-clients` (from `main` after M3-7, in parallel with Astra's
M3-8). Design source: `Docs/STATUS.md` "M4 groundwork" and the standing note in `AGENTS.md` that every console
helper is an unchecked Server RPC. This brief is the contract. No map, no pack asset, no content.

**Parallel work.** Astra's M3-8 touches actors, widgets, layouts and `Ship/ShipVisuals.*`; this task must not edit any
of `Ship/**` (except nothing), `UI/**`, `Station/**`, `Tests/LegibilityTests.cpp` or any existing test file. It edits
`Core/AirlockPlayerController.*`, `Core/AirlockGameMode.*`, `Core/AgentConsoleSubsystem.*`, adds
`Tests/DevCheatsTests.cpp`, `Tests/ArrivalSpawnTests.cpp`, `Scripts/verify-two-clients.ps1`, this brief, the report
and the docs. The editor is the
worktree's, on port 8001.

## Why this task

M4 is multiplayer. Before any client other than the listen server's own player exists, two things must be true:
nothing a client can send to the server changes state without a check, and the agents can drive and observe more
than one player. Today every `Airlock*` console helper is a Server RPC that any client could call to warp, refuel,
heat casks or reset the save, and the file console is a single file polled by every game instance in the process,
so a second PIE client is undriveable. Both are groundwork, not features; the proof that comes with them is the
first time the game has been seen from a client that is not the server.

## Definition of done

1. **The gate.** A console variable `airlock.DevCheats` (`TAutoConsoleVariable<int32>` in `AirlockGameMode.cpp`,
   default `1` in every build except Shipping, where it is `0`, `ECVF_Default` so a server operator can turn it on)
   and `AAirlockGameMode::AllowCheats(APlayerController*)` overridden to return `airlock.DevCheats != 0` (this is
   also what the engine asks before creating its own `UCheatManager`, which the project does not use).
   `AAirlockPlayerController` gains `bool CheatAllowed(const TCHAR* Helper)` (server side; true when the game mode
   allows cheats for this controller; otherwise logs `Cheat <Helper> refused for <PlayerState name or controller
   name>: airlock.DevCheats is 0` and returns false). It is the first statement of every `Server*_Implementation`
   that backs an `Airlock*` Exec helper, with the helper's name: `ServerSetVoyageClock` (AirlockVoyageClock),
   `ServerWarp`, `ServerShipFault`, `ServerAdjustShipModule` (AirlockDamage / AirlockRepairModule),
   `ServerShipStatus`, `ServerHoldByLabel`, `ServerReleaseHold`, `ServerIncident`, `ServerIncidentSchedule`,
   `ServerCargoStatus`, `ServerCarryById`, `ServerDropCargo`, `ServerPlaceById`, `ServerCrateHeat`,
   `ServerCargoTally`, `ServerFuelStatus`, `ServerFuelSet`, `ServerFuelLeak`, `ServerBypassFromConsole`,
   `ServerFeedById`, `ServerAirStatus`, `ServerScrubFromConsole`, `ServerAirSet`, `ServerExpose`, `ServerBaseFuel`,
   `ServerSitByStationId`, `ServerStandUpFromConsole` - 27 RPCs. `ServerChooseVoyage` is the board's own path and
   stays open (its input is an enum the server already validates). Refused calls do nothing else; nobody is kicked
   (a `_Validate` that fails disconnects the client, which is wrong for a dev switch). The local Exec helpers
   (`AirlockBoard`, `AirlockWhere`, `AirlockLook`, `AirlockCapture` below) run on the caller and are not gated.
2. **The file console per instance.** `UAgentConsoleSubsystem` decides its command file from its PIE instance id
   (`GetGameInstance()->GetWorldContext()->PIEInstance`, known at `Initialize`; the listen server or the only player
   is instance 0, clients are 1..N; a non-PIE game has none): instance 0 or none keeps
   `Saved/AgentConsole/commands.txt`; client N polls `Saved/AgentConsole/commands.client<N>.txt`. The pure part is
   `CommandFilePathFor(PIEInstance)` / `EchoPrefixFor(PIEInstance)`. The subsystem logs `<prefix>: watching <path>`
   at `Initialize` and `<prefix>: ready (map=<world>)` every time it finds a player controller in a world it has not
   announced yet (so a script can wait for a client after each travel), and echoes `AgentConsole: <line>` as today
   for the primary file and `AgentConsole client <N>: <line>` for a client file. Single-player PIE is unchanged
   apart from the `ready` line (every existing proof anchors on `: <text>` after the echo; none pins the watching
   line). Net mode is deliberately not used: a client's world reads as standalone until it connects.
3. **A capture that belongs to one instance.** `AirlockCapture <Label>` (Exec, local, non-Shipping): takes a
   screenshot of the Slate window that owns this instance's game viewport (`FSlateApplication::TakeScreenshot` of
   `GameViewport->GetWindow()`, alpha forced opaque) and writes `Saved/AgentCaptures/<Label>.png` through
   `FImageUtils::SaveImageByExtension`, logging `Capture <Label> saved <w>x<h>: <path>` or `Capture <Label> failed:
   <why>`. UMG widgets are in the window, so the task board, captions and the exposure widget are in the file; the
   editor chrome is not when the viewport is in its own window. Existing proofs keep `CaptureEditorImage`.
4. **Tests** `Tests/DevCheatsTests.cpp` (a spawned test world with the game mode, a controller and a possessed crew
   pawn, modelled on `FFuelTestWorld`; the console variable is restored by a scope guard whatever the outcome):
   - `Airlock.Cheats.GateFollowsConsoleVariable`: with the variable at 1 `ServerWarp` moves the pawn; at 0 the pawn
     stays and exactly one `Cheat AirlockWarp refused` line is logged; back at 1 it moves again.
   - `Airlock.Cheats.EveryConsoleRpcIsGated`: with the variable at 0, calling all 27 RPCs once logs exactly 27
     `Cheat ` lines, the pawn, the fuel, the air, the graph, the cargo and the clock are unchanged, and
     `ServerChooseVoyage` still books.
   - `Airlock.Cheats.ConsoleFileFollowsPIEInstance`: `CommandFilePathFor` gives `commands.txt` for no instance and
     instance 0 and `commands.client<N>.txt` for 1 and 2; `EchoPrefixFor` matches.
   - `Airlock.Multiplayer.ArrivalsStepAsideAtOneStart` (item 6): five controllers restarted at one PlayerStart get
     pawns at 0, +120, -120, +240, -240 cm along its right axis; a sixth is forced in (expected warning) and still
     has a pawn.
6. **Arrivals step aside** (found by the first run of the proof, item 5): the second player through an airlock
   found the only PlayerStart occupied by the first and came home as a bodiless spectator (`SpawnActor failed
   because of collision`, then `Where: ... pawn=none`). `AAirlockGameMode::SpawnDefaultPawnAtTransform_Implementation`
   skips a candidate with a pawn within 90 cm and tries `ArrivalSpacing` (120 cm) steps along the start's right axis,
   alternating sides up to two steps (`Arrival spawn for <PC> stepped +120 cm aside: the start was occupied`); with
   nothing clear it spawns anyway with `AdjustIfPossibleButAlwaysSpawn` and a warning. Single-player arrivals are
   unchanged (offset 0), so the existing arrival assertions hold.
5. **PIE proof** `Scripts/verify-two-clients.ps1` (conventions of `verify-refinery.ps1`; captures under
   `Saved/AgentCaptures/M4-1`; `TWO CLIENTS OK`). It reads the editor's play settings
   (`/Script/UnrealEd.Default__LevelEditorPlaySettings`: `PlayNetMode`, `PlayNumberOfClients`, `RunUnderOneProcess`,
   `NewWindowWidth`, `NewWindowHeight`), sets listen server / 2 clients / one process / 960 x 540, restores every
   value in `finally`, and starts PIE in the viewport from the hub PlayerStart. Steps, each log-anchored:
   - wait for `AgentConsole: client 1 watching`; `AirlockWhere` on both files -> `map=UEDPIE_0_StationHub` (server)
     and `map=UEDPIE_1_StationHub` (client);
   - server `AirlockBook 2`; client `AirlockWarp -4700 1560 116` then `-4265 1560 116` (the client crosses Dock B;
     the server travels everyone) -> `Section Hauler_Mk1`, `client 1: ready (map=UEDPIE_1_Hauler_Mk1)`, two
     `Arrival spawn for` lines and one `stepped +120 cm aside`, `Where` on both -> `UEDPIE_0_Hauler_Mk1` /
     `UEDPIE_1_Hauler_Mk1`;
   - client `AirlockWarp -6330 1700 116`, `AirlockCarry C1` -> `: Carry C1 by`; server `AirlockCargoStatus` ->
     `Cargo C1 Carried`; client `AirlockWarp -6430 1400 116`, `AirlockPlace Clamp_1` -> `Cargo C1 Secured`; client
     `AirlockCapture m4-1-client-hold` and server `AirlockCapture m4-1-server-hold` (the same crate secured on both
     boards);
   - client `AirlockWarp 1760 240 130`, `AirlockSit Bridge` -> `Seat Bridge taken by`; client `AirlockCapture
     m4-1-client-seat` (the control board on the client); `AirlockStand` -> `released by`;
   - client `AirlockScrub Hazard` -> `Air ` line with `scrub=Hazard`;
   - server `airlock.DevCheats 0`; client `AirlockWarp 0 0 0` -> `Cheat AirlockWarp refused`; client `AirlockWhere`
     still on the ship at the seat spot; server `airlock.DevCheats 1`;
   - server `AirlockVoyageClock 5` -> `Voyage result:`, `client 1: ready (map=UEDPIE_1_StationHub)`, the hub's
     `stepped +120 cm aside`; `Where` on both -> `UEDPIE_0_StationHub` / `UEDPIE_1_StationHub` with a pawn each;
     `CaptureEditorImage` -> `m4-1-editor-two-windows` (whatever the desktop shows, best effort);
   - StopPIE, settings restored in `finally`.

## Do not touch
- `Ship/**`, `UI/**`, `Station/**`, `Voyage/**` (the subsystem's save stays as it is), every existing test file, every
  existing proof script (all nine must exit 0 unedited on this branch), `Content/**`, every map, the pack folders.
- No `_Validate` on any RPC in this task; no CheatManager subclass.

## Edge cases (decided)
- A refused cheat logs every time (cheap, and the proof needs the line); nothing is rate-limited.
- The variable is read on the game thread (`GetValueOnGameThread`) inside `AllowCheats`; tests flip it through
  `IConsoleManager::Get().FindConsoleVariable("airlock.DevCheats")->Set(...)` and restore it.
- `AirlockCapture` on an instance whose viewport has no window (headless) logs the failure and returns.
- If PIE seamless travel with a second client does not carry the client to the ship (`net.AllowPIESeamlessTravel` is
  on), the proof stops there and the report says so: that is a finding for M4, not something to paper over.

## Acceptance
`Scripts\build-editor.bat` clean; `Scripts\run-tests.bat Airlock` all green (90 + 4); all nine existing proofs and
`verify-two-clients.ps1` exit 0 on port 8001; the three `AirlockCapture` files show the client's own HUD.
