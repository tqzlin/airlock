# Task M5-2: The refinery floor - the map, the layouts, the section wiring

Owner: Claude (worktree `..\Airlock-claude`, editor on port 8001). Branch: `claude/m5-2-refinery-floor` (from
`main` at f4c4d96). Design source: `Docs/design/refinery-floor.md` sections 2, 4 and 7. Runs in parallel with
Astra's M5-1 (the pure refinery model, three files under `Ship/RefineryModel.*` and its tests) - this task touches
nothing Astra touches and does not depend on the model: the stills are placeholder fixtures until M5-3 wires
`FRefineryModel` into them. Everything a player sees here ships with a PIE proof.

## Why this task

The floor is a third section: the crew must arrive there from the ship's dock with the run's intact casks waiting
on pallets, spend a timed slot in one room, and leave for the hub with a result that includes the floor. Building
that skeleton (map, travel, clock, casks, tally, signs, the floor's own air and scrubbers) first means M5-3 can put
working stills into a place that already exists and is already proven end to end.

## Definition of done

1. **The map** `Content/Maps/RefineryFloor.umap`: a copy of a pack hall (chosen from the scouting captures; the
   Engineer hall like the ship if none of the industrial packs reads better), one walkable room, the pack's own
   doors and player starts stripped, one `AAirlockDoor` `Airlock_ToHub` (target `StationHub`, `TargetLevel` the hub
   map) in the doorway nearest the room's short end, every other doorway slot a plain door with no target,
   `PlayerStart_Floor` just inside the airlock, the space sky / fog / exposure settings of the other two maps if the
   hall has windows, Nanite on the pack's opaque kit meshes if the pack is new (`THIRD_PARTY_ASSETS.md` rule). The
   walkway between the airlock and the far end must be clear of pack props (the hold's lesson); props that stand on
   the fixture spots are removed from the copy, never from the pack.
2. **Layouts** (code tables; positions probed like the hold's):
   - `Ship/RefineryLayouts.*`: `FFloorFixtureSpec { Id, Kind (Still, Tank, Drain, Bunk), Location, Yaw, Mesh, Scale,
     LabelYaw, Line1, Line2 }` and `RefineryLayouts::Find("RefineryFloor")` -> three stills `Still_1..3` in a row
     along one wall three metres apart (kit tank meshes), the `Tank` at the far end, the `Drain` mid-walkway, the
     `Bunk` by the airlock; a new `Ship/FloorFixture.*` actor (`AFloorFixture`: kit mesh with the cube fallback, a
     two-line label through the M3-8 policy, replicated spec `COND_InitialOnly`, collision blocks Visibility only)
     that the game mode spawns from the table (folder `Floor`). M5-3 replaces the still fixtures with `AStill`.
   - `ShipLayouts::Find("RefineryFloor")`: one module, `LifeSupport` (the floor's scrubber panel on the wall
     opposite the stills, kit mesh `SM_ElectricDistribBox`), no lines, no stations - so the existing life-support
     press verb, the atmosphere brownout rules and the task board's air row all work on the floor unchanged.
   - `CargoLayouts::BuildFloor(const TArray<FName>& Casks, const TArray<FName>& LightCasks)`: crate spots on the
     staging pallets by the airlock (a 4 x 3 grid, 120 cm pitch, probed), the light casks marked volatile so they
     read `light` on the label and warm like the ship's; no clamps, no rack. `CargoLayouts::Find` is unchanged.
   - `StationSigns::Find("RefineryFloor")`: `HALDER SOLAR - FLOOR 3` over the airlock, `NO NAKED FLAME - METHANE`
     over the stills, `SCRUBBERS` over the panel.
3. **The result carries the casks.** `FVoyageResult` gains `IntactCasks` and `IntactLightCasks` (`TArray<FName>`),
   filled by `FCargoModel::Tally` (Secured or Racked casks, in manifest order); `Summary()` is unchanged.
4. **The ship docks at the floor, not the hub.** On a ship section (one whose section id is the pending voyage's
   `ShipClassId`), `ReturnToHub()` and the hub-bound door both record the ship tally into the subsystem as the
   **ship result** (`UVoyageSubsystem::SetShipResult` / `GetShipResult`, stored only - no save, no base fuel, no run
   count) and travel to `/Game/Maps/RefineryFloor` (section `RefineryFloor`) with the card `DOCKING - HALDER SOLAR
   REFINERY` / `cargo off - your slot on the floor starts now`; the pending voyage stays pending. `FloorLevel` is a
   soft path constant on the game mode (no door leads there).
5. **The floor section.** On `RefineryFloor` with a pending voyage and a ship result: `InitGameState` and
   `BeginPlay` start the **slot clock** on the same replicated fields (`bVoyageActive`, `VoyageEndServerTime`) with
   `FFloorSchedule::SlotSeconds` 600, build the floor's cargo from the ship result's intact casks
   (`CargoLayouts::BuildFloor`), reset the atmosphere (the floor has air: `AirHasAtmosphere` is true wherever the
   cargo layout is non-empty, which it is even with zero casks - the pallets exist), spawn the fixtures and signs,
   build the one-module ship graph, and speak **Okafor's** lines from `Voyage/FloorSchedule.*` (pure, like
   `FVoyageSchedule`): 0 s `Okafor, floor three. Your casks are on the pallets. Ten minutes on the slot - make them
   count.`, 300 s `Halfway. Anything still on a pallet at the horn goes home as crude.`, 540 s `One minute.`, 600 s
   `Horn. Slot's over - clear the floor.` No incident director, no set pieces, no fuel system on the floor.
6. **Leaving the floor.** At the slot's end (`ReturnToHub` from the floor) or through `Airlock_ToHub`: combine the
   ship result with the floor's tally into the run's `FVoyageResult` (for M5-2 the floor part is `FFloorResult`
   with zero casks refined and every delivered cask unrefined: `CasksUnrefined` = casks on the floor; pay adds
   `40 cr` per unrefined cask; `FuelSurplus` stays the ship leg's for now - the diesel path is M5-3's), store it
   with `SetLastVoyageResult` (which saves, counts the run and credits base fuel as today), clear the pending
   voyage, and travel to the hub with `UNDOCKING - HOME TO THE VELL EXTRACTOR` / `the Rig's board has your pay`.
   `FVoyageResult` gains `FFloorResult Floor; bool bHasFloor`; `Summary()` appends ` | ` + the floor summary when
   `bHasFloor`; the hub board's `Last run:` line appends ` - floor: <n>/<m> refined` when present.
7. **Names in strings.** `MV Corrin` in the captain's departure line (`Captain Tanaka, MV Corrin. ...`), the
   med-bot blackout line becomes `<Pawn> is down. Get them to air.`; nothing else renamed.
8. **Tests** `Tests/RefineryFloorTests.cpp`: `BuildFloor` spots (count, grid, light flags, no slots); the floor's
   ship layout is one LifeSupport module; the sign table; `FFloorSchedule` (four lines, in order, `SlotSeconds`);
   `Tally` fills the intact cask ids in manifest order; the subsystem's ship result round trip and `SetLastVoyageResult`
   after it; a spawned `RefineryFloor` test world with a pending voyage and a ship result of three casks: clock at
   600, three crates on pallets with the right light flags, air present, one module, no director, four fixtures;
   the combined result's summary and the board's line.
9. **PIE proof** `Scripts/verify-floor.ps1` (`FLOOR OK`; captures under `Saved/AgentCaptures/M5-2`): book the Milk
   run, cross Dock B, secure C1 and rack C2 (`Cargo C1 Secured`, `Cargo C2 Racked`), `AirlockVoyageClock 5` ->
   `Transition card: DOCKING - HALDER SOLAR REFINERY`, `Section RefineryFloor: ...`, `Where` on
   `UEDPIE_0_RefineryFloor` at `PlayerStart_Floor`, `Slot clock started: 600s`, Okafor's first line,
   `AirlockCargoStatus` -> `Cargo C1 Loose` and `Cargo C2 Loose` on the pallets (and nothing else), `AirlockAir` ->
   an atmosphere line, `AirlockShipStatus` -> `Ship LifeSupport Nominal`, captures `floor-arrival` (the pallets and
   casks, the airlock sign), `floor-stills` (the three fixtures and the tank), `floor-panel` (the scrubber panel);
   `AirlockVoyageClock 5` -> Okafor's horn, `Voyage result: ... | floor: 0/2 casks refined ...`, `Transition card:
   UNDOCKING - HOME TO THE VELL EXTRACTOR`, the hub, the board's `Last run:` with `floor: 0/2 refined`. Then the
   existing eleven proofs unedited (the ship's docking now lands on the floor: `verify-voyage-clock`, `-cargo`,
   `-atmosphere`, `-fuel`, `-refinery`, `-legibility` and `-two-clients` all wait for a hub arrival after the
   docking; they need the floor's second hop - **allowed edit**: each waits for the floor's `Section RefineryFloor`
   line, sends `AirlockVoyageClock 5` once more, then waits for the hub as before; the assertions themselves do
   not change).

## Do not touch
- `Ship/RefineryModel.*`, `Tests/RefineryModelTests.cpp` (Astra's), `UI/**` except the one `Last run:` line,
  every pack folder, the two existing maps.

## Edge cases (decided)
- A ship docking with zero intact casks still goes to the floor (an empty slot: Okafor says so at 0 s and the
  crew can leave through the airlock at once).
- A floor session started without a ship result (PIE from the floor map) builds an empty floor, logs it, and the
  slot runs; leaving records nothing.
- The slot clock uses the voyage clock's fields so the task board, `AirlockVoyageClock` and the arrival card logic
  work unchanged; `TaskBoardWidget`'s header still says `to dock` - M5-3 re-words it on the floor.
- The floor's casks warm and puff like the ship's after the calm minutes (the M3-4 rule) - a real time pressure the
  design wants; the rack cooling factor has no rack to apply to.

## Acceptance
`Scripts\build-editor.bat` clean; `Scripts\run-tests.bat Airlock` all green; `verify-floor.ps1` and the eleven
existing proofs exit 0 on port 8001.
