# Level pass report: stations on the ship, the control seat, two-editor tooling

Claude (Fable, Claude Code on stack), 2026-09-12. Branch `claude/level-pass` in the worktree `..\Airlock-claude`,
created from `main` at `524e9b4` (after the M3-3 merge) and rebased onto the tooling/brief commits `2b5f1b8`/`9a01bf4`.
Task: `Docs/STATUS.md` "Next tasks" item 1 (move the placeholder modules and lines into real rooms, add the control
station seat, positions in `Ship/ShipLayouts.cpp`). Design source: project doc sections 8.1 (lever 4, distance) and 8.3
(roles are seats).

## Behaviour changed

- **The ship has rooms with jobs.** The seven modules and four lines no longer stand in a row in the west hall. The
  clamp controller stays in the hall (the cargo hold). The power core, coolant tank, life support and engine node are
  in the L-corridor the pack itself signs "CENTRAL ENGINEERING" (X -1531..-1106 between the wall faces, Y 230..950,
  floor Z 22), with `Coolant_1` along the west wall in front of the core band and `Power_1` on the east wall. The two
  other lines are in the main corridor - `Coolant_2` on the south wall at X -3500, `Power_2` on the north wall at
  X -2700 - so a repair is a run between the hold and engineering. The galley cabinet sits in the closed west stub of
  the junction at (-1320, -20). Nav stays on the bridge. Every position was probed in PIE (walking surface Z) and
  looked at through captures; labels now face the reader (a `LabelYaw` per spec instead of the fixed -X rule).
- **The control-room seat** (`AControlStation`, `Ship/ControlStation.*`): the game mode spawns `Station_Bridge` from
  the layout inside the U of the bridge terminal at (1930, 240, 28), facing the screens. Walk up, press E: the
  server seats you (one occupant, in reach, `Seat Bridge taken by ...`), plants you on the seat point facing the
  screens with movement off, and the **control board** opens (`UI/ControlBoardWidget`: every module with type,
  status and condition, every line with its ends and fault, the section and clock, `E - stand up`). E again stands
  you up 140 cm back towards the door and frees the seat. While nobody sits, the label reads `Captain <Name> has
  the conn` (`Autopilot` when no voyage is pending). Console: `AirlockSit <StationId>` / `AirlockStand`.
- **`AirlockLook <yaw> [pitch]`**: turns the local view so a capture after a warp looks at something known. Every
  capture in the proofs from now on can be deterministic.
- **Two agents on one laptop** (on `main` already, `2b5f1b8`): `Scripts\new-worktree.ps1`, `close-editor.ps1`,
  `-NoHotReloadFromIDE` in `build-editor.bat`, `AIRLOCK_MCP_PORT` in `run-editor.bat` and `lib\mcp.ps1`. This
  whole task ran in the worktree with its own editor on port 8001 while Astra built M3-4 on port 8000.

## What the map turned out to be (so nobody plans around the vista again)

The Interiors-kit room north of the west hall (X -5600..-4206, Y 2400..3140, floor Z -70), which `STATUS.md`
called "the engineering nook off the corridor at (-4300, 1780)", is scenery. A 200 cm walkability grid (warp +
`AirlockWhere`, 99 points) shows: the hall's north strip at Z -65 (Y 2000), a Z -140 trench along Y 2100, a ledge
at Z -73 along Y 2200, a 90 cm sill with glass above at Y 2300..2360, then the room floor. The door in the main
corridor's north wall near (-3950, 1800) and the doorway pieces at X -4280 open onto nothing. Making the room
walkable needs a map edit (bridge the trench, replace one 670 cm sill + divider piece with a doorway); it is a good
future engineering bay, not something to fix in this pass.

## Files

- `Source/Airlock/Ship/ControlStation.h/.cpp` (new), `Source/Airlock/UI/ControlBoardWidget.h/.cpp` (new).
- `Source/Airlock/Ship/ShipLayouts.h/.cpp`: `FControlStationSpec`, `Stations`, `LabelYaw` on every spec, the new
  positions with a comment block that records every room's bounds and floor heights.
- `Source/Airlock/Ship/ShipModule.*`, `ShipLine.*`: labels face the spec's `LabelYaw`.
- `Source/Airlock/Core/AirlockCharacter.*`: `SeatedStation`, `ServerSitAt` / `ServerStandUp`, seated movement.
- `Source/Airlock/Core/AirlockPlayerController.*`: control board lifecycle, `AirlockSit` / `AirlockStand` /
  `AirlockLook`.
- `Source/Airlock/Core/AirlockGameMode.cpp`: spawns the stations, logs `Control station <Id> at ...`.
- `Source/Airlock/Tests/ControlStationTests.cpp` (new, 4 tests).
- `Scripts/verify-level-pass.ps1` (new proof), `AGENTS.md`, `Docs/HANDOFF.md` (sections 2, 3, 4), this report.
- No map or asset changed. Nothing under `Content/`.

## Verification

- `Scripts\build-editor.bat`: succeeded (UE 5.8.2, MSVC 14.50).
- `Scripts\run-tests.bat Airlock`: **38/38** (34 existing + `Airlock.Ship.ControlStationSpawnsFromLayout`,
  `Airlock.Ship.ControlStationSitAndStand`, `Airlock.Ship.ControlStationIsExclusive`,
  `Airlock.Ship.ControlBoardListsEveryModuleAndLine`).
- `Scripts\verify-level-pass.ps1` (worktree editor, port 8001): **exit 0 in 152 s**. Tail:

```text
> AirlockHold Coolant_2
LogAirlock: Hold Coolant_2 complete by AirlockCharacter_0
LogAirlock: Line Coolant_2 fault=None
> AirlockFault Power_2 Short  ->  Line Power_2 fault=Short
> AirlockHold Power_2  ->  Hold Power_2 complete by AirlockCharacter_0, Line Power_2 fault=None
> AirlockSit Bridge (from the door)  ->  Seat Bridge rejected: out of range (342 cm, reach 312 cm)
> AirlockWarp 1760 240 130 ; AirlockSit Bridge  ->  Seat Bridge taken by AirlockCharacter_0
> AirlockWhere  ->  Where: map=UEDPIE_0_Hauler_Mk1 pawn=AirlockCharacter_0 at X=1930.000 Y=240.000 Z=124.000
> AirlockStand  ->  Seat Bridge released by AirlockCharacter_0
> AirlockWhere  ->  Where: ... at X=1790.000 Y=240.000 Z=118.313
> AirlockWarp -7015 1560 116  ->  Arrival spawn for AirlockPlayerController_0: PlayerStart_0 at X=-5640.001
LogAirlock: Voyage E4AF24EC485132EFD0A7F9AB03BCF9BC complete
LEVEL OK: hold -> engineering (2 lines repaired) -> corridor (2 lines repaired) -> galley -> bridge seat, board, stand -> hub PlayerStart
```

  Earlier in the same run `Coolant_1` (leak, 3 s) and `Power_1` (short, 4 s) were repaired from the engineering
  walkway the same way. Captures under `Saved\AgentCaptures\LevelPass\` (ignored; rerun the script to recreate):
  `hold-cargo-clamps`, `engineering-overview` (all four engineering labels readable from the corridor's north end),
  `Coolant_1-leak` (task board `Tape leak - Coolant_1`), `Power_1-short`, `Coolant_2-leak`, `Power_2-short`,
  `galley-stub`, `bridge-station-free` (`CONTROL STATION / Captain Okonkwo has the conn` over the terminal),
  `bridge-control-board` (the board: 7 modules with live conditions, CoolantLoop at 88 % after the leaks, and 4 lines),
  `bridge-station-released`.
- `verify-loop.ps1` (**exit 0, 55 s**, `LOOP OK`), `verify-ship-graph.ps1` (**exit 0, 72 s**, `GRAPH OK`) and
  `verify-incidents.ps1` (**exit 0, 59 s**, `INCIDENTS OK`), rerun on this branch after the two later scripts' hard-coded
  line spots were moved to the new rooms (`Coolant_1` is repaired from (-1300, 430) in engineering, `Coolant_2` from
  (-3320, 1500) in the corridor). Everything ran on the worktree editor while Astra's M3-4 editor was open on port 8000.
- Reproduce: open the editor on StationHub (`Scripts\run-editor.bat`, or in the worktree with `AIRLOCK_MCP_PORT=8001`),
  then `powershell -ExecutionPolicy Bypass -File Scripts\verify-level-pass.ps1`; `Scripts\run-tests.bat Airlock`.

## Open questions / follow-ups

- Engineering's labels are dense on the east wall (life support, the wire, the engine node within 4 m); M3-6 should
  size and colour them, and the coolant tank is scaled 0.6 to fit a 4.25 m corridor - a smaller kit mesh would be
  better.
- The seat has no chair mesh (the kit has none); the terminal's U reads as the seat. A sit animation waits for a
  character mesh.
- `AControlStation::Interact` runs on the pressing client and calls the character's server RPC, like the board; the
  seat itself is server-owned. Gate the console helpers with the others before M4.
- M3-4 (Astra, in flight) places crates and clamps in the hall at X -6100..-4800; the clamp controller at
  (-6300, 1900) is clear of them.
