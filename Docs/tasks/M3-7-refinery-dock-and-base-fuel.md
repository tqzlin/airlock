# Task M3-7: Contract tiers, the refinery dock, base fuel and the Rig's identity

Owner: Claude (Claude Code, worktree `..\Airlock-claude`, editor on port 8001). Reviewer: Claude with Tom's
read of the report. Branch: `claude/m3-7-refinery-dock` (from `main` after M3-5; M3-6 lands in parallel and the
reviewer merges the two). Design source: `Docs/design/refining-run.md` sections 2 (the Rig, the refinery, the
fuel loop), 3 (minute 19-20), 4 (fuel credit, contract tiers) and 6 (M3-7).

## Why this task

The run now has its hazards (air, cargo, soon fuel) but not its frame: the board still sells "mining, pirates,
derelicts", the ship still "returns to station", and nothing the crew brings back changes anything. This task
puts the fiction on the surfaces the player reads - the departure board, the transition cards, the captain's
mouth - makes the three offers three sizes of the same run, docks the ship at the refinery, and gives the Rig a
fuel stock that only crews raise. It is content and glue, deliberately code-spawned so nothing in a map moves.

## Definition of done

1. **Contract tiers** (`Voyage/VoyageTypes.h`, `VoyageSubsystem.cpp`, `VoyageSchedule.cpp`): `EVoyageType` values
   become the tiers `MilkRun` ("Milk run"), `Standard` ("Standard run"), `Priority` ("Priority run") in that order
   (so `AirlockBook 0` is still the small run every proof books). `FVoyageSpec` gains `int32 CaskCount` (4 / 8 / 12)
   and `int32 VolatileCount` (2 / 4 / 6); `ShipClassId` is `Hauler_Mk1` for every tier; `CrewCapacity` is 2 / 4 / 6;
   `MakeVoyageSpec` stays deterministic from (Type, Seed) with the captain drawn as today. The offer line becomes
   `{Tier}  -  {n} casks  -  Capt. {Captain}  -  crew {cap}  -  20 min`. The captain's mid-voyage line per tier:
   MilkRun `"Captain {0} here. Light load today. Keep the casks cold and we're home for supper."`, Standard
   `"Captain {0} here. Halfway to the refinery. Check the rack, check the air."`, Priority `"Captain {0} here.
   Priority manifest - twelve casks and the refinery's counting. Nobody sleeps."`; departure `"Captain {0} here.
   We're underway. Sludge casks are loose on the deck - clamp them, or rack them, the rack's cold."`; docking
   `"Captain {0} here. Docking at the refinery. Their crew's coming aboard - then home."`. The tests that name the
   old types (`AirlockSmokeTests`, `VoyageScheduleTests`) are updated to the tiers; that is a recorded decision,
   not a drift.
2. **Manifest by tier** (`Ship/CargoLayouts.*`): `CargoLayouts::Find(SectionId)` keeps returning today's four-cask,
   three-clamp layout (every existing test and proof books tier 0 and must see exactly it). New
   `FCargoLayout CargoLayouts::Build(FName SectionId, int32 CaskCount, int32 VolatileCount)` returns the layout
   for a tier: crates `C1..Cn` on probed spots (the first four are today's), volatile = every second cask up to
   `VolatileCount`, clamps `Clamp_1..Clamp_m` with `m = CaskCount * 3 / 4` (3 / 6 / 9) on probed spots (the first
   three are today's), one `ColdRack`. Twelve crate spots and nine clamp spots are probed in PIE (`AirlockWarp` +
   `AirlockWhere`, all on the hall's Z -65 floor, off the Y 1560 walkway and clear of the clamp controller at
   (-6300, 1900)) and recorded in the report. The game mode builds the cargo model from `Build(section,
   Pending.CaskCount, Pending.VolatileCount)` when a voyage is pending and from `Find` otherwise (a ship entered
   unbooked keeps the four). `FIncidentSchedule` and the director take the built layout as they do today.
3. **The refinery dock** (`Core/AirlockGameMode.cpp`, strings only): the hub-bound transition card reads
   `DOCKING - HALDER SOLAR REFINERY` / `cargo off, fuel on - then home to the Vell Extractor` for both the door and
   the automatic return (the docking caption is the captain's line above); boarding stays `CYCLING AIRLOCK`. The
   `Voyage result:` log and `RecordVoyageResult` are unchanged except for item 4.
4. **Base fuel** (`Voyage/VoyageSubsystem.*`, new `Core/AirlockSaveGame.*`): `UAirlockSaveGame : USaveGame` with
   `int32 BaseFuelTonnes = 200` and `int32 RunsCompleted = 0`, slot `Airlock`, user index 0; the subsystem loads it
   in `Initialize` (creating it when absent) and saves it in `SetLastVoyageResult`. `FVoyageResult` gains
   `UPROPERTY() int32 FuelCredit` (`FuelCreditPerIntactCask` 15 x intact, tonnes of refined methane the refinery
   credits) and `UPROPERTY() int32 FuelSurplus` (`SurplusPerIntactCask` 10 x intact - what is left after the trip
   home), both filled by `Tally`; `SetLastVoyageResult` adds the surplus to `BaseFuelTonnes`, increments
   `RunsCompleted`, saves, and logs `Base fuel <n> t (+<surplus> this run), runs <k>`. `AAirlockGameState` gains
   `UPROPERTY(Replicated) int32 BaseFuelTonnes` and `RunsCompleted`, copied on the hub in `InitGameState` next to
   the last result. `FVoyageResult::Summary` appends `, fuel +<surplus> t` last (after every earlier suffix; the
   proof regexes have no end anchor). Console: `AirlockBaseFuel` (logs the stock and runs), `AirlockBaseFuelSet <n>`
   (proofs), `AirlockSaveReset` (deletes the slot and reloads defaults).
5. **The Rig's identity** (`UI/VoyageBoardWidget.cpp`, `Core/AirlockGameMode.cpp`, new `Station/StationSign.*`):
   the board's title reads `VELL EXTRACTOR  -  DOCK B DEPARTURES`, the hint `Pick a contract, then proceed to
   Dock B.`, and a line above the offers `Base fuel {n} t  -  runs {k}` (with `  (+{surplus} this run)` when a last
   result exists). `AStationSign` is a code-spawned text-render actor (kit-free, two lines, faces a yaw) placed by
   the game mode on the hub from a small table in `Station/StationSigns.cpp` (`VELL EXTRACTOR` / `DOCK B - HALDER
   SOLAR REFINERY RUNS` beside the board at Dock B, `VELL EXTRACTOR` / `DOCK A - NO VESSEL DOCKED` at Dock A;
   positions probed) - lore on the walls without touching a map. The refinery's mirror field in the hub's sky is
   left to the map pass (M3-8 or later), noted in the report.
6. **Tests** (extend, plus deliberate updates of the type-naming ones): offers cover the three tiers with 4 / 8 / 12
   casks and 2 / 4 / 6 volatile; `Build` produces unique ids, distinct positions, the right clamp count, and
   `Build(section, 4, 2)` equals `Find`; the save game round-trips through a fresh subsystem (`Initialize` twice);
   `Tally` fills the fuel fields (3 intact -> credit 45, surplus 30); `SetLastVoyageResult` adds the surplus and
   counts the run once; the board text with and without a last result; the tier lines name the captain.
7. **PIE proof** `Scripts\verify-refinery.ps1` (conventions of `verify-cargo.ps1`, captures under
   `Saved\AgentCaptures\M3-7`, `REFINERY OK`): `AirlockSaveReset` -> `AirlockBaseFuel` -> `Base fuel 200 t`;
   `AirlockBook 2` (Priority) -> on the ship `AirlockCargoStatus` shows C1..C12 and nine clamps (capture the hold from
   the west end: twelve cases on the floor); carry C1 to Clamp_1 and C2 to the rack (`Cargo C2 Racked`);
   `AirlockVoyageClock 1200`; leave -> the card `DOCKING - HALDER SOLAR REFINERY` (capture during the fade), `Voyage
   result: 2/12 crates intact, ... fuel +20 t`, `Base fuel 220 t (+20 this run), runs 1`; on the hub `AirlockBoard`
   -> `VELL EXTRACTOR - DOCK B DEPARTURES`, `Base fuel 220 t - runs 1 (+20 this run)`, the three tier offers (capture);
   book tier 0, leave at once -> `Base fuel 220 t (+0 this run), runs 2` (nothing intact, the stock does not fall);
   `AirlockSaveReset` at the end so the next proof starts clean. All existing proofs stay green unedited: they book
   tier 0, which is today's layout, and their `Voyage result:` / `Last run:` texts gain only trailing fields.

## Do not touch
- `Ship/ShipGraph.*`, `Ship/AirlockDoor.*`, `Ship/ShipLayouts.*`, `Voyage/IncidentSchedule.*`,
  `Voyage/IncidentDirectorComponent.*`, `Core/Interactable.h`, `Core/AgentConsoleSubsystem.*`, the M3-6 files
  (`Ship/FuelSystem.*`, `Ship/FuelLayouts.*`, `Voyage/VoyageSetPieces.*`) until M3-6 has merged.
- `HandleTransitionRequested` / `ReturnToHub` / `TravelToSection` / `ChoosePlayerStart` beyond the card strings.
- Maps and `Content/**`: every sign, crate and clamp is spawned from code; positions live in the layout tables.
- Existing proofs: unedited. Existing test expectations: only the type-naming ones change, recorded in the report.

## Edge cases (decided)
- The save game is per machine and unversioned in the MVP; `AirlockSaveReset` is the escape hatch and every proof
  that touches the stock ends with it. A server-side stock replaces it in M4.
- The stock never falls in M3-7 (the Rig's daily burn is a later mechanic); a run with nothing intact adds 0.
- A voyage without a pending spec (PIE started on the ship) builds the four-cask layout and records nothing.
- Twelve casks with three volatile at once is the Priority tier's difficulty; the incident count still scales by
  crew only (M3-3).
- The tier rename changes log text `type=<n>` nowhere (it logs the enum's integer); captain lines that the clock
  proof waits for (`One minute to dock`) are unchanged.

## Acceptance
`Scripts\build-editor.bat` clean in the worktree; `Scripts\run-tests.bat Airlock` all green; every existing proof
and `verify-refinery.ps1` exit 0 on the worktree editor (port 8001); captures show the twelve-cask hold, the
refinery card, and the board with the base line and the tier offers.
