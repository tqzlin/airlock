# Task M4-1: Dev-cheats gate, per-instance file console, the first two-client proof

Owner: Claude. Branch: `claude/m4-1-dev-cheats-and-two-clients` (from `main` after M3-7, in parallel with Astra's
M3-8). Design source: `Docs/STATUS.md` "M4 groundwork" and the standing note in `AGENTS.md` that every console
helper is an unchecked Server RPC. This brief is the contract; it was red-teamed by a second reader on 2026-09-12
(24 findings, folded below) after the first implementation, and items 6 and 7 come from what the proof found. No map,
no pack asset, no content.

**Parallel work.** Astra's M3-8 touches actors, widgets, layouts and `Ship/ShipVisuals.*`; this task must not edit any
of `Ship/**`, `UI/**`, `Station/**`, `Tests/LegibilityTests.cpp` or any existing test file. It edits
`Core/AirlockPlayerController.*`, `Core/AirlockGameMode.*`, `Core/AgentConsoleSubsystem.*`, adds
`Tests/DevCheatsTests.cpp`, `Tests/ArrivalSpawnTests.cpp`, `Scripts/verify-two-clients.ps1`, this brief, the report
and the docs. The editor is the worktree's, on port 8001.

## Why this task

M4 is multiplayer. Before any client other than the listen server's own player exists, two things must be true:
nothing a client can send to the server changes state without a check, and the agents can drive and observe more
than one player. Today every `Airlock*` console helper is a Server RPC that any client could call to warp, refuel,
heat casks or reset the save, and the file console is a single file polled by every game instance in the process,
so a second PIE client is undriveable. Both are groundwork, not features; the proof that comes with them is the
first time the game has been seen from a client that is not the server.

## Definition of done

1. **The gate.** A console variable `airlock.DevCheats` (`TAutoConsoleVariable<int32>` in `AirlockGameMode.cpp`,
   `ECVF_Default` so a server operator can set it; default `1` wherever an editor is compiled in (`WITH_EDITOR`: PIE
   and `-game` from the editor binary) and `0` in every packaged build, Development and Test included - a packaged
   listen server for the two-person playtest must not accept `AirlockSaveReset` from a guest; opt in with
   `-ExecCmds="airlock.DevCheats 1"` or `[ConsoleVariables]`) and `AAirlockGameMode::AllowCheats(APlayerController*)`
   (non-const, public in the engine) overridden to return `airlock.DevCheats != 0`. The engine consults
   `AllowCheats` only when a controller is created (`AddCheats` from `PostInitializeComponents`) to decide whether
   it gets a `UCheatManager`, so a controller spawned at 0 has none (no `ToggleDebugCamera` / `Slomo`; no proof uses
   them) and flipping the variable mid-session changes nothing but the gate. The gate must never test
   `CheatManager != nullptr`. `AAirlockPlayerController` gains `bool CheatAllowed(const TCHAR* Helper)` (server
   side; true when the game mode allows cheats for this controller; otherwise logs `Cheat <Helper> refused for
   <PlayerState name or controller name>: airlock.DevCheats is 0` and returns false). It is the first statement of
   every `Server*_Implementation` that backs an `Airlock*` Exec helper, with the helper's name: `ServerSetVoyageClock`
   (AirlockVoyageClock), `ServerWarp`, `ServerShipFault`, `ServerAdjustShipModule` (AirlockDamage /
   AirlockRepairModule by flag), `ServerShipStatus`, `ServerHoldByLabel`, `ServerReleaseHold`, `ServerIncident`,
   `ServerIncidentSchedule`, `ServerCargoStatus`, `ServerCarryById`, `ServerDropCargo`, `ServerPlaceById`,
   `ServerCrateHeat`, `ServerCargoTally`, `ServerFuelStatus`, `ServerFuelSet`, `ServerFuelLeak`,
   `ServerBypassFromConsole`, `ServerFeedById`, `ServerAirStatus`, `ServerScrubFromConsole`, `ServerAirSet`,
   `ServerExpose`, `ServerBaseFuel` (AirlockBaseFuel / BaseFuelSet / SaveReset by mode), `ServerSitByStationId`,
   `ServerStandUpFromConsole` - 27 RPCs, the whole list in the header. `ServerChooseVoyage` is the board's own path
   and stays open (its input is an enum the server already validates). Refused calls do nothing else; nobody is
   kicked (a `_Validate` that fails disconnects the client, which is wrong for a dev switch). The local Exec helpers
   (`AirlockBoard`, `AirlockWhere`, `AirlockLook`, `AirlockCapture` below) run on the caller and are not gated. The
   variable is process-wide: in a one-process PIE session `airlock.DevCheats 0` from any instance switches every
   instance, and it survives StopPIE.
2. **The file console per instance.** `UAgentConsoleSubsystem` decides its command file from its PIE instance id
   (`GetGameInstance()->GetWorldContext()->PIEInstance`, known at `Initialize`; the listen server or the only player
   is instance 0, clients are 1..N; a non-PIE game has none; in Play-As-Client with an in-process dedicated server,
   instance 0 is that server and the first window is `client1`): instance 0 or none keeps
   `Saved/AgentConsole/commands.txt`; client N polls `Saved/AgentConsole/commands.client<N>.txt`. The pure part is
   `CommandFilePathFor(PIEInstance)` / `EchoPrefixFor(PIEInstance)`. Net mode is deliberately not used: a client's
   first world is an Entry world with no net driver, so it reads as standalone until it connects. The subsystem is
   not created for a game instance without a world context (the automation harnesses), so a test run never eats a
   proof's command. It logs `<prefix>: watching <path>` at `Initialize` (which runs before a client has even browsed
   to the server - not a readiness signal) and `<prefix>: ready (map=<world>, pc=<controller>)` every time it finds
   the *local* player controller (`GetFirstLocalPlayerController`, never "the first controller", which on a listen
   server may be a remote client's) in a world it has not announced yet. While the instance's world context is in a
   seamless travel (`SeamlessTravelHandler.IsInTransition()`) it neither polls nor announces, so a command file is
   only ever consumed in the destination world and "consumed" means "executed there". It echoes
   `AgentConsole: <line>` as today for the primary file and `AgentConsole client <N>: <line>` for a client file.
   Existing proofs are unaffected (they anchor on `: <text>` after the echo; none pins the watching line).
3. **A capture that belongs to one instance.** `AirlockCapture <Label>` (Exec, local; the Shipping guard is inside the
   function body, not around the `UFUNCTION`): `FSlateApplication::TakeScreenshot` of this instance's
   `GetGameViewportWidget()` - the widget, never `GetWindow()` (for in-viewport PIE that is the editor's main
   window, chrome included) and never `GEngine->GameViewport` (reassigned per instance, ends up on the last client).
   The call redraws the owning window and reads back only the widget's rect, so UMG overlays are in the pixels and
   the chrome is not, for both instances. Alpha is forced opaque; the file is
   `Saved/AgentCaptures/<Label>.png` through `FImageUtils::SaveImageByExtension` (ENGINE_API; `FImageView` is in
   ImageCore, a public dependency of Engine, so `Airlock.Build.cs` is unchanged); log `Capture <Label> saved
   <w>x<h>: <path>` or `Capture <Label> failed: <why>`. Existing proofs keep `CaptureEditorImage`.
4. **Tests** `Tests/DevCheatsTests.cpp` (a spawned test world with the game mode, a controller and a possessed crew
   pawn, modelled on `FFuelTestWorld`; the console variable is set and restored with `ECVF_SetByConsole` by a scope
   guard whatever the outcome - a `SetByCode` set is silently ignored once a proof has touched the variable from the
   console in the same editor):
   - `Airlock.Cheats.GateFollowsConsoleVariable`: with the variable at 1 `ServerWarp` moves the pawn and a spawned
     controller has a cheat manager; at 0 the pawn stays, exactly one `Cheat AirlockWarp refused` line is logged and
     a controller spawned now has none; back at 1 it moves again.
   - `Airlock.Cheats.EveryConsoleRpcIsGated`: with the variable at 0, calling all 27 RPCs once (`ServerBaseFuel` in
     its read-only mode, so a regression can never touch the real save slot) logs exactly 27 `Cheat ` lines, the
     pawn, the fuel, the air, the graph, the cargo and the clock are unchanged, and `ServerChooseVoyage` still books.
   - `Airlock.Cheats.ConsoleFileFollowsPIEInstance`: `CommandFilePathFor` gives `commands.txt` for no instance and
     instance 0 and `commands.client<N>.txt` for 1 and 2; `EchoPrefixFor` matches.
   - `Tests/ArrivalSpawnTests.cpp`: `Airlock.Multiplayer.ArrivalsStepAsideAtOneStart` (five controllers restarted at
     one PlayerStart get pawns at 0, +120, -120, +240, -240 cm along its right axis; a sixth is forced in (expected
     warning) and still has a pawn) and `Airlock.Multiplayer.ArrivalSkipsBlockedSideStep` (a blocking box on the
     +120 spot sends the second arrival to -120).
   Facts these tests rely on, verified so nobody re-derives them: a Server RPC called on a controller in a spawned
   standalone world runs its `_Implementation` synchronously (`GetFunctionCallspace` is Local for
   `NM_Standalone` with authority); `RequestVoyage` refuses only `NM_Client`, and the harness game instance has no
   world context; a controller possesses a pawn without a `LocalPlayer` (`IsLocalController()` is true in
   standalone, the pawn's input setup needs only the controller's actions); `AddExpectedError` also matches
   warnings.
5. **PIE proof** `Scripts/verify-two-clients.ps1` (conventions of `verify-refinery.ps1`; captures under
   `Saved/AgentCaptures/M4-1`; `TWO CLIENTS OK`). It reads the editor's play settings
   (`/Script/UnrealEd.Default__LevelEditorPlaySettings`: `PlayNetMode`, `PlayNumberOfClients`, `RunUnderOneProcess`,
   `bLaunchSeparateServer`, `NewWindowWidth`, `NewWindowHeight`; `get_properties` returns `returnValue` as a JSON
   *string*), snapshots the `[/Script/UnrealEd.LevelEditorPlaySettings]` section of
   `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`, sets listen server / `PlayNumberOfClients` 2 (the
   listen server counts: exactly one client window) / one process / no separate server / 960 x 540, and starts PIE
   in the viewport from the hub PlayerStart (`RequestPlaySession` copies the settings at request time). In
   `finally`, while PIE still runs, it sends `airlock.DevCheats 1` best-effort (the variable is process-wide and
   outlives PIE), then StopPIE, then restores every value in memory, re-reads and compares (on a mismatch it writes
   the single-player defaults), and writes the ini section back - PIE start persists the settings to that ini and an
   in-memory restore alone leaves a relaunched editor in two-client mode. Steps, each log-anchored and every
   anchor carrying `map=UEDPIE_<n>_` or `AgentConsole client 1:` because the shared log has no instance prefix:
   - wait for `client 1: ready` with `UEDPIE_1_StationHub`; `AirlockWhere` on both files -> `map=UEDPIE_0_StationHub`
     (server) and `map=UEDPIE_1_StationHub` (client);
   - server `AirlockBook 2`; client `AirlockWarp -4700 1560 116` then `-4265 1560 116` (the client crosses Dock B;
     the server travels everyone) -> `Section Hauler_Mk1`, `client 1: ready` with `UEDPIE_1_Hauler_Mk1`, two
     `Arrival spawn for` lines and one `stepped +120 cm aside`, `Where` on both -> `UEDPIE_0_Hauler_Mk1` /
     `UEDPIE_1_Hauler_Mk1`;
   - client `AirlockWarp -6330 1700 116`, `AirlockCarry C1` -> `: Carry C1 by`; the server's `Where` still at its
     arrival X (the log cannot say which player acted; the server's pawn not moving does); server
     `AirlockCargoStatus` -> `Cargo C1 Carried`; client `AirlockWarp -6430 1400 116`, `AirlockPlace Clamp_1` ->
     `Cargo C1 Secured`; each `AirlockCapture` is preceded by that instance's `AirlockWhere` (map asserted): client
     `m4-1-client-hold`, server `m4-1-server-hold` (the same crate secured on both boards);
   - client `AirlockWarp 1760 240 130`, `AirlockSit Bridge` -> `Seat Bridge taken by`; client `AirlockCapture
     m4-1-client-seat` (the control board on the client); `AirlockStand` -> `released by`;
   - client `AirlockScrub Hazard` -> `Air ` line with `scrub=Hazard`;
   - server `airlock.DevCheats 0` (editor-wide); client `AirlockWarp 0 0 0` -> `Cheat AirlockWarp refused`; client
     `AirlockWhere` still at the seat's exit point (`X=1790 Y=240`, `ExitDistance` behind the seat); server
     `airlock.DevCheats 1`;
   - server `AirlockVoyageClock 5` -> `Voyage result:`, `client 1: ready` with `UEDPIE_1_StationHub`, the hub's
     `stepped +120 cm aside`; `Where` on both -> `UEDPIE_0_StationHub` / `UEDPIE_1_StationHub` with a pawn each;
     `CaptureEditorImage` -> `m4-1-editor-two-windows` (whatever the desktop shows, best effort);
   - StopPIE, settings restored as above.
6. **Arrivals step aside** (found by the first run of the proof): the second player through an airlock found the
   only PlayerStart occupied by the first and came home as a bodiless spectator (`SpawnActor failed because of
   collision`, then `Where: ... pawn=none`: the PIE start exists only in the server world, the engine hands the one
   start to every controller, `APawn` spawns with `AdjustIfPossibleButDontSpawnIfColliding` and the base
   `SpawnDefaultPawnAtTransform` adds no override). `AAirlockGameMode::SpawnDefaultPawnAtTransform_Implementation`
   tries the start and then `ArrivalSpacing` (120 cm) steps along its right axis, alternating sides up to two steps:
   a candidate is *occupied* when a pawn possessed by another controller stands within 90 cm and 200 cm of height
   (an unpossessed or dying pawn - the arriving player's own old body, a spectator, a leftover - does not count; the
   first version counted every pawn and pushed a lone player aside at every PIE start); a side step that
   `EncroachingBlockingGeometry` says is inside a wall is skipped quietly (the start itself keeps the engine's
   adjust-or-refuse behaviour); the chosen step logs `Arrival spawn for <PC> stepped +120 cm aside: the start was
   occupied`; with nothing clear it spawns anyway with `AdjustIfPossibleButAlwaysSpawn` and a warning. Single-player
   arrivals are unchanged (offset 0), so the existing arrival assertions hold.
7. **Two-player facts the proof established** (so the report does not re-litigate them): with
   `net.AllowPIESeamlessTravel=1` the in-process client follows every `ServerTravel` (Dock B and the docking
   return) and its worlds are `UEDPIE_1_<Map>`; `ChoosePlayerStart` runs on the server for both controllers;
   `bArrivingByAirlock` lives on the server's game instance only and the client never needs it; the client's
   transition card and captions show on its own viewport; the replicated game state carries everything the
   client's task board and control board need; the incident director samples `crew=2`.

## Do not touch
- `Ship/**`, `UI/**`, `Station/**`, `Voyage/**` (the subsystem's save stays as it is), every existing test file, every
  existing proof script (all nine must exit 0 unedited on this branch), `Content/**`, every map, the pack folders.
- No `_Validate` on any RPC in this task; no CheatManager subclass.

## Edge cases (decided)
- A refused cheat logs every time (cheap, and the proof needs the line); nothing is rate-limited.
- The variable is read on the game thread (`GetValueOnGameThread`) inside `AllowCheats`; RPC and Exec bodies run
  there.
- `AirlockCapture` on an instance whose viewport has no window (headless) logs the failure and returns; a minimized
  window renders nothing (`the viewport did not render`), so both PIE windows must be restored for the capture
  steps - overlap is fine, each window has its own backbuffer. The server's capture is the level-viewport widget
  rect, so its size depends on the editor layout (the log line records `saved WxH`).
- If PIE seamless travel with a second client had not carried the client to the ship, the proof would have stopped
  there and the report would say so; it did carry it (item 7).
- Not covered here (follow-ups for M4): server-side gameplay log lines (`Carry C1 by AirlockCharacter_1`) do not
  name the player - add the PlayerState name; a client joining mid-voyage or leaving during a travel; more than
  two players in PIE; a `-ProbePositions` mode for the +-120 spots at both starts.

## Acceptance
`Scripts\build-editor.bat` clean; `Scripts\run-tests.bat Airlock` all green (90 + 5); all nine existing proofs and
`verify-two-clients.ps1` exit 0 on port 8001; the three `AirlockCapture` files show the client's own HUD.
