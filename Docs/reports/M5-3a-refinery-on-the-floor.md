# Report M5-3a: The refinery model on the floor

Claude, 2026-09-13 (late morning). Branch `claude/m5-3a-refinery-wiring` from `main` at 65f6ae2 (after the M5-1 and
M5-2 merges and the unity-build name fix). Brief: `Docs/tasks/M5-3a-refinery-on-the-floor.md`. Worktree
`..\Airlock-claude`, editor on port 8001.

## What changed (behaviour first)

- **A cask can be refined on the floor.** After the dock, `AirlockKnob Still_1 Heat 8`, `AirlockKnob Still_1
  Feed 10` and `AirlockStillLoad Still_1 C1` run M5-1's heavy recipe in the real slot: the crate vanishes from the
  pallet (`Cargo C1 Loaded`), the still reads `Still_1 Running cask=C1`, its label shows `C1 42% 184K 1.4bar`, the
  tank label counts up, `Tank 100 L diesel` ... `Refinery BatchDone Still_1 C1 diesel=7xx gas=xx tar=1xx` about
  100 s later, Okafor says `That's diesel in the tank. Keep them coming.`, and at the horn the run is paid from the
  model: `Floor result: floor: 1/2 casks refined, 7xx L diesel, ...`, `Voyage result: ... | floor: 1/2 casks
  refined, 7xx L diesel, pay 4xx, fuel +7 t`, the Rig's board shows the surplus.
- **The floor section owns the model.** `BuildRefinery()` builds `StillCount` stills from the delivered casks
  (light = volatile); `TickRefinery()` ticks it in the 4 Hz systems tick while the slot runs, feeds the room's ppm
  in and the stills' fumes out into the floor's air, logs every model event and has Okafor speak them (spill, pop,
  flash, coke, failure, the first batch) with a 20 s cooldown per cue.
- **The state is replicated** (`AAirlockGameState::Refinery`, floored on publish like the cargo), with log lines
  on transitions only (status and cask changes, the pressure warning band both ways, fouled / clean, calibration
  drifted / calibrated, the tank per 100 L). Actors and widgets read the mirror, never the game mode.
- **The verbs are a server API** on the game mode (`RefineryLoadHopper`, `RefinerySetKnob`, `RefineryStepKnob`,
  `RefineryLever`, `RefineryTally`, `RefinerySummary`, `RefineryStillReadout`) with five cheat-gated console
  helpers (`AirlockRefinery`, `AirlockKnob`, `AirlockKnobStep`, `AirlockStillLoad`, `AirlockStill`) and the floor
  tally appended to `AirlockTally`. M5-3b's actors call the same API.
- **A new cargo state, `Loaded`**: a cask in a hopper is off the deck (hidden, no collision, no puffing, no
  pick-up, out of the carrier's hands in place), like Burned but not a fine - it is crude being refined.
  `FCargoModel::IsConsumed` names the pair; `FCargoModel::Load` takes Loose or Carried.
- **What the crew sees**: the still fixtures' second line and the tank's litres come from the mirror on both
  sides (`AFloorFixture::StateLine`); the task board on the floor is `FLOOR TASKS` / `mm:ss to horn` with still
  rows (failed / flash / popped / pressure in red first, then fouled, drifted, `Load a still - n casks waiting`),
  `Load crate C2 - 60C` for a warm cask, and no `Secure` rows.
- **The result**: `RecordVoyageResult` on the floor fills the floor fields and adds the model's pay; the base fuel
  banked at the Rig is the model's surplus tonnes (`FuelSurplus`), the ship's refuel the units (`FuelCredit`),
  measured from the new `FVoyageResult::FuelUnitsAtDock`. The flat 10 t per intact cask now only applies to a
  ship that leaves through its own airlock without docking (the abort path; a follow-up in STATUS).
- **Okafor's event lines** (quasi-realistic, per Tom's tone note): `Flood on Still 1 - crude over the lip. Feed
  down, and mind your footing, it's slick.`, `Still 1's relief just blew. Vent it wide, reset the trap, and keep
  your face out of that cloud.`, `Flash on Still 1! Kill the heat and purge it. Anyone who breathed that sits down
  before they fall down.`, `Still 1's coking up - you're getting half the cut. Scrape it when the hopper's empty.`,
  `Still 1's dead - coked solid. Get it repaired or write it off.` The pressure-band and drift warnings are
  M5-3b's (they belong with the gauges).

Files: `Core/AirlockGameMode.h/.cpp` (the model, the tick, the publish, the events, the verbs, the tally, the
result), `Core/AirlockGameState.h/.cpp` (the mirror), `Core/AirlockPlayerController.h/.cpp` (the helpers),
`Core/AirlockCharacter.cpp` (`Loaded` frees the hands, refuses pick-up), `Ship/CargoModel.h/.cpp` (`Loaded`,
`Load`, `IsConsumed`, `FuelUnitsAtDock`), `Ship/CargoCrate.cpp` and `Ship/ShipAtmosphere.cpp` (consumed = hidden
and silent), `Ship/RefineryLayouts.h/.cpp` (`ERefineryLever`, `StillCount`, the name helpers),
`Ship/FloorFixture.h/.cpp` (`StateLine`, the state binding), `UI/TaskBoardWidget.h/.cpp` (the floor rows, the
horn), `Tests/RefineryFloorTests.cpp` (+2), `Tests/DevCheatsTests.cpp` (+4 RPCs), `Scripts/verify-floor.ps1`.

## Decisions worth knowing

- **`Loaded` instead of reusing `Burned`.** Reusing would have logged `Cargo C1 Burned` on the floor and counted
  a soot fine in any cargo tally. The new state touches six files but every place that treats a cask as gone now
  says so through `IsConsumed`.
- **The publish is floored** (1 K, 0.05 bar, 1 % calibration, 10 L per still) so a running still replicates a few
  times a second, not sixty; the board rebuild rides on the same event as the cargo.
- **Knob logs print the value after the step** (`Refinery Still_1 heat 8/20`), so a clamped step at the end is
  visible as a repeat of the same value; the model refuses a zero step and that is logged `rejected`.
- **The event lines are Okafor's, the band warnings are not yet.** The events come out of the model tick; the
  pressure band and the drift are transitions the gauges show, so they go with the gauges in M5-3b (two lines in
  `PublishRefinery`, brief item 4).
- **The test world ticks by hand** with `GFrameCounter` advanced per frame - without that the timer manager fires
  once for the whole loop and the still never heats (found the first time the test ran: `Preheated` 92 K).

## How it was verified

- `Scripts\run-tests.bat Airlock` on the branch: **138 / 138** (the first run was 137 / 138: the world test's
  still never heated because the timer manager ticks once per `GFrameCounter` value; the test world's `Tick` now
  advances it per frame).
- `Scripts\verify-floor.ps1` on port 8001: **FLOOR OK** in 199 s (the first run failed on the wait for `Refinery
  Still_1 loaded C1 (hopper 1/3)` - the log query treats its pattern as a regex and the parentheses became a
  group; the hopper count moved to the wildcard check).
- The other eleven proofs on the branch binary (port 8001, play settings forced single-player first):

  | Proof | Result |
  | --- | --- |
  | `verify-loop.ps1` | LOOP OK (53 s) |
  | `verify-ship-graph.ps1` | GRAPH OK (72 s) |
  | `verify-incidents.ps1` | INCIDENTS OK (58 s) |
  | `verify-voyage-clock.ps1` | CLOCK OK (117 s, with the floor hop) |
  | `verify-level-pass.ps1` | LEVEL OK (151 s) |
  | `verify-cargo.ps1` | CARGO OK (88 s) |
  | `verify-atmosphere.ps1` | ATMOSPHERE OK (123 s) |
  | `verify-fuel.ps1` | FUEL OK (147 s) |
  | `verify-refinery.ps1` | REFINERY OK (66 s) |
  | `verify-legibility.ps1` | LEGIBILITY OK (70 s) |
  | `verify-two-clients.ps1` | TWO CLIENTS OK (76 s, with the floor hop) |
- Captures under `Saved\AgentCaptures\M5-2\` on the worktree: `floor-still-running.png` (Still 1's label while C1
  runs), `floor-tank.png` (the tank after the batch, the board's floor rows), plus M5-2's set.

Proof tail:

```
Ship result: 2/4 crates intact, 0 vented, 2 loose, hull 92.80%, pay 536, air 0 ppm, 0 blackouts, 0 burned (2 casks to the floor)
Slot clock started: 600s endServerTime=629.358 casks=2
Tank diesel=0 gas=0 tar=0 spilled=0 refined=0 lost=0 waiting=2
Refinery Still_1 heat 8/20
Refinery Still_1 feed 10/20
Refinery Still_1 Reset rejected: Idle
Refinery load rejected: no such cask (C9 into Still_1)
Refinery Still_1 loaded C1 (hopper 1/3)
Cargo C1 Loaded temp=0
Still Still_1 Running cask=C1
Tank 300 L diesel
Refinery BatchDone Still_1 C1 diesel=754 gas=50 tar=195
Okafor: That's diesel in the tank. Keep them coming.
Still Still_1 Idle
Tank diesel=754 gas=50 tar=195 spilled=0 refined=1 lost=0 waiting=1
Floor tally now: floor: 1/2 casks refined, 754 L diesel, 195 L tar, 0 spills, 0 pops, 0 criticals, pay 407, fuel +6 t
Floor result: floor: 1/2 casks refined, 754 L diesel, 195 L tar, 0 spills, 0 pops, 0 criticals, pay 407, fuel +6 t
Voyage result: 2/4 crates intact, 0 vented, 2 loose, hull 92.80%, pay 943, air 0 ppm, 0 blackouts, 0 burned | floor: 1/2 casks refined, 754 L diesel, pay 407, fuel +6 t
Base fuel 206 t (+6 this run), runs 2
FLOOR OK: Milk run -> C1 secured, C2 racked -> clock docks at the refinery -> floor: two casks on the pallets, Okafor, air, scrubber panel, six fixtures -> Still 1 at heat 40 % / feed 50 %, C1 loaded, batch done, 754 L in the tank -> horn -> combined result (1/2 refined, +6 t), undocking card -> hub board, save reset
```

Read from the captures: the board reads `FLOOR TASKS` / `AIR 0 ppm | 09:09 to horn` / `Load a still - 1 cask
waiting` on arrival; Still 1's label reads `C1 6% 188K 1.5bar` seconds into the batch and `idle 188K` after it,
when the board adds `Still_1 drifted 60% - calibrate` (one 100 s batch drifts the trap by 0.4 - M5-1's
`CalDriftPerS`; a second cask without calibrating would cut the yield, which is the skill the design asks for) and
Okafor's caption sits top-centre. The pay: 754 L x 0.5 = 377, plus 40 for the unrefined cask, minus 10 for the
195 L of tar = 407. The fuel: the clock jump to the dock fires the pending feed-leak set piece first (the M3-6
rule), so the ship docked at 89 units (`Fuel 89% leak=1` in the log) and the result carried `FuelUnitsAtDock`
89; the floor refuels that 11-unit gap with 110 L before banking, 644 L remain and `floor(6.44)` = 6 t reaches
the Rig (`Base fuel 206 t (+6 this run)`). Correct by the design: refuel the ship, then bank the rest.

## Unsure about / open for M5-3b and M5-4

- The heavy recipe from a 30 s preheat yields less than the 240 s recipe (the bath is still climbing when the feed
  opens); the proof asserts at least 600 L and reports the litres. M5-3b's gauges make the preheat visible.
- The light casks on the floor keep warming with no rack; loading them is the cure and the board says so. If a
  playtest shows the pallets puffing before anyone reaches a still, M5-4's cold rack is the answer.
- `FloorCasksLost` is not a field of the run result yet; a cask still in a still at the horn shows only in the
  `Floor result:` log line.

## Outside the brief
Nothing outside the brief's list. `Tests/DevCheatsTests.cpp` gained the four RPC lines the brief asks for.

## Reproduce
```
Scripts\close-editor.ps1 ; Scripts\build-editor.bat ; Scripts\run-tests.bat Airlock
Scripts\run-editor.bat  (port 8000, or AIRLOCK_MCP_PORT=8001 on the worktree)
powershell -ExecutionPolicy Bypass -File Scripts\verify-floor.ps1
```

## Merged tree (Claude, 2026-09-13 late morning)

Merged `--no-ff` into `main` at 244c6eb (after the M5-1 / M5-2 docs commit). On `Airlock\` with port 8000: the build
clean, **138 / 138** tests, and the twelve proofs green in one batch with the play settings forced single-player
(loop 53 s, ship-graph 72, incidents 58, voyage-clock 117, level-pass 149, cargo 88, atmosphere 125, fuel 146,
refinery 67, legibility 69, two-clients 75, floor 198 with the refined cask). M5-3b's brief and Astra's launch
script (179) follow this merge.
