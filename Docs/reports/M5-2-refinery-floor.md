# M5-2 report: the refinery floor - the map, the layouts, the section wiring

Claude, 2026-09-13 (morning), branch `claude/m5-2-refinery-floor` from `main` at f4c4d96, in the worktree
`..\Airlock-claude` with its editor on port 8001 while Astra ran M5-1 (the pure refinery model) on port 8000. Brief:
`Docs/tasks/M5-2-refinery-floor.md`; design: `Docs/design/refinery-floor.md` sections 2, 4 and 7.

## Behaviour changed

- **A third section exists: the floor.** `Content/Maps/RefineryFloor.umap` is a copy of the Interiors pack's
  second dressed hall (`ModSciInteriors/Maps/Example_2_Dynamic`, the hub's footprint in a cooler dressing) with
  the pack's doors and player starts stripped, `Airlock_ToHub` in the west doorway (target `StationHub`), `Door_East`
  sealed in the east one, `PlayerStart_Floor` just inside the airlock at (-6660, 1560), the space sky, the thinned
  fog and the exposure clamp of the other two maps. The Engineer pack's second hall and the three industrial packs
  were scouted first: the Engineer one is an L of corridors, the industrial maps are prop showcases on a plane.
- **The ship docks on the floor.** When the voyage clock runs out on the ship, the game mode keeps the ship tally on
  the subsystem as the **ship result** (`Ship result: 2/4 crates intact, ... (2 casks to the floor)`) and travels
  everyone to the floor under `DOCKING - HALDER SOLAR REFINERY` / `cargo off - your slot on the floor starts now`,
  the pending voyage intact. The ship's own airlock (leaving early) still goes straight home as before, so nine of
  the eleven existing proofs are untouched.
- **The floor runs a slot.** On `RefineryFloor` with a pending voyage the same clock fields run a 600 s slot
  (`Slot clock started: 600s ... casks=2`), Okafor speaks at 0, 300, 540 and 600 s (`Okafor: Okafor, floor three.
  Your casks are on the pallets. ...`), the run's intact casks stand on the staging spots as loose crates with the
  light ones volatile (`CargoLayouts::BuildFloor` from the ship result's `IntactCasks` / `IntactLightCasks`), the
  floor has air and the scrubber panel (a one-module ship layout: `LifeSupport`, so the press verb, the brownout
  rules and the board's air row work unchanged), six placeholder fixtures (`AFloorFixture`: three stills, the
  product tank, the drain, the bunk, kit meshes with two-line labels) and three signs. No incident director, no set
  pieces, no fuel system on the floor.
- **The slot's end undocks everyone.** `ReturnToHub` on the floor combines the ship result with the floor's
  (M5-2: nothing refined yet - every delivered cask goes home as crude at 40 cr), stores it with
  `SetLastVoyageResult` (which saves and credits base fuel as before), clears the pending voyage and travels home
  under `UNDOCKING - HOME TO THE VELL EXTRACTOR` / `the Rig's board has your pay`. `FVoyageResult` carries the floor
  (`bHasFloor`, totals, diesel litres, floor pay) and its summary appends ` | floor: 0/2 casks refined, 0 L diesel,
  pay 80`; the hub board's `Last run:` line appends ` - floor: 0/2 refined`.
- **Names.** The captain's departure line opens `Captain Tanaka, MV Corrin.`; the blackout line is `<Pawn> is down.
  Get them to air.` (no med-bot).

## Files

- New: `Ship/RefineryLayouts.h/.cpp` (fixture table, `FloorSectionId`), `Ship/FloorFixture.h/.cpp`,
  `Voyage/FloorSchedule.h/.cpp`, `Tests/RefineryFloorTests.cpp`, `Scripts/verify-floor.ps1`, `Content/Maps/RefineryFloor.umap` (LFS).
- Edited: `Core/AirlockGameMode.h/.cpp` (`IsShipSection`, `IsFloorSection`, `DockAtFloor`, `RecordShipResult`,
  `BuildFloorFixtures`, `SpeakerName`, the floor branches in `BeginPlay`, `BuildCargo`, `ReturnToHub`,
  `RecordVoyageResult`), `Ship/CargoLayouts.h/.cpp` (`BuildFloor`, `Find` knows the floor), `Ship/CargoModel.h/.cpp`
  (the result's cask lists and floor fields, the summary), `Ship/ShipLayouts.cpp` (the floor's one module),
  `Station/StationSign.cpp` (three floor signs), `Voyage/VoyageSubsystem.h/.cpp` (the ship result),
  `Voyage/VoyageSchedule.cpp` (the hull name), `UI/VoyageBoardWidget.cpp` (one line),
  `Scripts/verify-voyage-clock.ps1` and `Scripts/verify-two-clients.ps1` (the floor hop after the docking, as the
  brief allowed; no assertion weakened).
- Untouched: `Ship/RefineryModel.*` and its tests (Astra's), every other existing test and proof, the packs, the two
  existing maps.

## Verification

- `Scripts\build-editor.bat` clean (first build). `Scripts\run-tests.bat Airlock`: **110 / 110** (104 + six
  `Airlock.Floor.*`: layouts and signs, Okafor's schedule, the ship result and the combined tally, the floor
  section beginning play from a three-cask ship result, an empty floor without one, the ship section unchanged).
- `Scripts\verify-floor.ps1 -ProbePositions` first: the deck's walking surface is Z -65 (the ship's hall's number),
  the walkway trench -70, both end platforms Z 22, and the westmost staging column at X -6400 lands on the west
  platform's railing - the tables were set from that probe (stills and spots at -65, tank and bunk at 22, drain
  at -70, the staging rows from X -6240).
- `Scripts\verify-floor.ps1` on port 8001: **FLOOR OK** on the first full run. Tail:

```text
Ship result: 2/4 crates intact, 0 vented, 2 loose, hull 92.80%, pay 536, air 0 ppm, 0 blackouts, 0 burned (2 casks to the floor)
Transition card: DOCKING - HALDER SOLAR REFINERY / cargo off - your slot on the floor starts now
Section RefineryFloor: voyage underway (type=0 ship=Hauler_Mk1 captain=Okonkwo)
Slot clock started: 600s endServerTime=630.291 casks=2
Floor fixtures: 6
Okafor: Okafor, floor three. Your casks are on the pallets. Ten minutes on the slot - make them count.
Where: map=UEDPIE_0_RefineryFloor pawn=AirlockCharacter_0 at X=-6660.000 Y=1560.000 ...
Cargo C1 Loose temp=0 volatile=0 slot=None / Cargo C2 Loose temp=0 volatile=1 slot=None   (no C3, no C4)
Air 0 ppm scrub=Normal brownout=0 disabled=0 sources=0 / Ship LifeSupport Nominal cond=100.00
Okafor: Horn. Slot's over - clear the floor.
Voyage result: 2/4 crates intact, 0 vented, 2 loose, hull 92.80%, pay 616, air 0 ppm, 0 blackouts, 0 burned | floor: 0/2 casks refined, 0 L diesel, pay 80, fuel +20 t
Transition card: UNDOCKING - HOME TO THE VELL EXTRACTOR / the Rig's board has your pay
Base fuel 280 t (+20 this run), runs 9 -> Save reset: base fuel 200 t, runs 0
FLOOR OK
```

- The eleven existing proofs on this branch's binary: see the table at the end (run after the floor proof).

## Captures (`Saved\AgentCaptures\M5-2\`)

- `floor-arrival` - from the airlock: the hall with `C1` / `Loose` and `C2` / `Loose  0C` on the deck, the stills'
  tanks along the north side, `SHIP TASKS` with `Secure crate C1` / `C2` and `09:43 to dock` (the header's wording
  is M5-3's to change).
- `floor-pallets` - the staging spots from the walkway (`C2` / `Loose  0C` facing the camera).
- `floor-stills` - `STILL 2` / `idle` on its tank (the label sits close to the tank's rim; M5-3's still has its own
  gauge stack), the `NO NAKED FLAME - METHANE` sign above.
- `floor-panel` - the scrubber panel with `LifeSupport` / `Nominal  100%` / `Scrubbers Normal` under the fans.
- `floor-east` - the east platform with the product tank and the sealed door.
- `hub-board-floor` - `Last run: 2/4 crates intact - hull 93% - pay 616 cr - air clean - floor: 0/2 refined`.

## Decisions and open questions

- **Leaving the ship early skips the floor.** The ship's own airlock still travels straight home (the M2 loop):
  it reads as aborting the run, keeps nine proofs untouched, and the docking through the clock is the only way
  onto the floor. If a playtest wants the door to dock too, `HandleTransitionRequested`'s hub-bound branch can
  call `DockAtFloor` on the ship section.
- **The floor's casks warm and puff** like the ship's after the calm minutes (the M3-4 rule) - a real time pressure
  the design wants, with no rack to cool them; M5-4 decides whether the floor gets a cold rack of its own.
- **Lighting.** The hall is dark in its pack dressing (cool teal, red accents); the fixtures read by their labels.
  The map-dressing pass should add working light over the stills and the pallets.
- The task board's header says `to dock` on the floor; `FTaskBoardWidget` is M5-3's (Astra's next) to re-word.
- Not proven: two clients on the floor beyond the arrival (`verify-two-clients.ps1` checks both instances land on
  the floor and come home; the carry there is M5-3's proof); a floor entered with zero intact casks in PIE (the
  test covers the empty floor).

## Reproduce

From the repo root with the editor open on a clean StationHub on the port in `AIRLOCK_MCP_PORT`:

```powershell
Scripts\run-tests.bat Airlock
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-floor.ps1 -ProbePositions
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-floor.ps1
```
