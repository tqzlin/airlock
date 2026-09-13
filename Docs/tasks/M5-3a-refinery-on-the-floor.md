# Task M5-3a: The refinery model on the floor - the wiring, the verbs, the result

Owner: Claude (worktree `..\Airlock-claude`, editor on port 8001). Branch: `claude/m5-3a-refinery-wiring` (from
`main` at 65f6ae2, after the M5-1 and M5-2 merges). Design source: `Docs/design/refinery-floor.md` sections 4-7 and
M5-1's `Ship/RefineryModel.*`. Done by the reviewer so that M5-3b (Astra: the still actors) starts against a proven
server API instead of inventing one. Everything a player sees here ships with a PIE proof.

## Why this task

M5-1 built the stills as a pure model and M5-2 built the room they stand in; nothing connected them. This task makes
the floor section own an `FRefineryModel`, tick it on the slot clock, replicate its state, expose every verb the
actors and the console need, and pay the run from its tally. After it, a cask can be refined end to end by console
and the floor's pay and base fuel are the model's, not M5-2's placeholders.

## Definition of done

1. **The model lives in the game mode.** `BuildRefinery()` after `BuildCargo()` on the floor: `Build(StillCount,
   Casks)` from the cargo model's crates (light = the ship's volatile) with as many stills as the fixture table has
   `Still` entries (`RefineryLayouts::StillCount`, ids `Still_1..N` matching the fixtures); logs
   `Refinery: <n> stills, <m> casks delivered`. `TickRefinery()` in the 4 Hz systems tick while `bVoyageActive`:
   `Tick(0.25, room ppm, out ppm, events)`, the returned ppm goes into the floor's air through `AddPpm`, events are
   logged (`Refinery BatchDone|Spill|Pop|Flash|Fouled|Failed <Still> [<Cask>] ...`) and Okafor speaks each once per
   20 s per cue (the first batch: `That's diesel in the tank. Keep them coming.`).
2. **The mirror.** `AAirlockGameState::Refinery` (`FRefineryState`, `OnRep_Refinery` -> `OnShipGraphChanged`),
   `PublishRefinery` floored like the cargo (temperature to 1 K, pressure to 0.05 bar, calibration to 1 %, litres
   to 10 L per still and 1 L for the tank), with log lines on transitions only: `Still <Id> <Status>[ cask=<Cask>]`,
   `Still <Id> pressure <bar> bar - warning band|back under the warning band`, `Still <Id> fouled|clean`,
   `Still <Id> calibration <pct>% - drifted|calibrated`, `Tank <n> L diesel` on each 100 L. `RefineryHasFloor()` on
   the game state and the game mode = the section's fixture table has stills.
3. **The verbs** (server API on the game mode; each publishes and logs): `RefineryLoadHopper(Still, Cask)` - a Loose
   or Carried cask on the floor into the hopper (`Refinery <Still> loaded <Cask> (hopper n/3)`; refusals
   `Refinery load rejected: <reason> (<Cask> into <Still>)`); the cask becomes the new cargo state **`Loaded`**
   (hidden and inert like Burned, off the puff list, out of the carrier's hands in place, `PickUp rejected: loaded
   into a still`, neither intact nor loose in a tally). `RefinerySetKnob` / `RefineryStepKnob(Still, Knob, n)`
   (`Refinery <Still> heat|feed|vent <k>/20`). `RefineryLever(Still, Reset|Purge|Scrape|Repair|Calibrate)`
   (`Refinery <Still> reset|purged|scraped|repaired|calibrated`; `... rejected: <Status>`). `RefineryTally()` =
   `Tally(blackouts on the floor, the ship result's fuel gap)`; `FVoyageResult::FuelUnitsAtDock` (new, from
   `FCargoModel::Tally` with fuel data) is what the gap is measured from.
4. **Console helpers** behind the cheat gate (`AirlockRefinery`, `AirlockKnob <Still> <Heat|Feed|Vent> <0..20>`,
   `AirlockKnobStep <Still> <Knob> <delta>`, `AirlockStillLoad <Still> <Cask>` with no reach, `AirlockStill <Still>
   <Reset|Purge|Scrape|Repair|Calibrate>`); `AirlockTally` adds `Floor tally now: <FFloorResult::Summary>` on the
   floor; the four new Server RPCs join the every-RPC cheat test.
5. **What the crew sees.** The still fixtures' second line from the mirror (`idle 92K`, `idle +2 92K`, `C1 42% 184K
   1.4bar`, `POPPED`, `FLASH`, `FAILED`, ` fouled` appended) and the tank's (`747 L`), on both sides, through
   `AFloorFixture::StateLine`; the task board on the floor: `FLOOR TASKS`, `mm:ss to horn`, rows `<Still> failed -
   repair` / `flash - purge` / `popped - reset the trap` / `<bar> bar - open the vent` (red) first, then the air
   row, then `fouled - scrape when empty`, `drifted <pct>% - calibrate`, `Load a still - <n> casks waiting`, warm
   casks as `Load crate <Id> - <t>C`, and no `Secure` rows (there are no clamps).
6. **The result.** `RecordVoyageResult` on the floor takes `FloorCasksTotal/Refined/Unrefined`, `FloorDieselL`,
   `FloorPay` and the pay from `FFloorResult` (`Floor result: <summary>` logged), `FuelCredit` = the refuel units and
   `FuelSurplus` = the model's base-fuel tonnes (the flat 10 t per intact cask now only applies to a ship that left
   through its own airlock without docking).
7. **Tests** (`Tests/RefineryFloorTests.cpp`): a world test drives the M5-1 recipe by world ticks (240 s preheat at
   heat 8, C1 at feed 10, one batch in 100 s, 747 L, the fixture labels, the recorded result and the banked
   surplus, restored after); a pure test pins the board rows, the readout and the fixture lines. 138 tests.
8. **Proof**: `Scripts\verify-floor.ps1` refines C1 by console after the arrival checks (`FLOOR OK`).

## Do not touch
`Ship/RefineryModel.*` (M5-1's constants and behaviour are facts; wire, do not tune), the maps, the ship's cargo
tables, `IncidentSchedule`/`Director`, `FuelSystem`.

## Report
`Docs/reports/M5-3a-refinery-on-the-floor.md`.
