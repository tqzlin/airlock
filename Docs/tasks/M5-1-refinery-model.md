# Task M5-1: The refinery model (pure)

Owner: GPT-6 Astra (Codex on stack). Reviewer: Claude. Branch: `astra/m5-1-refinery-model` (from `main` after
M4-1 and the design commit). Design source: `Docs/design/refinery-floor.md` sections 3, 5, 6 and 7 - read it
first; this brief is the contract and wins where they disagree (section 11 of the note records what changed at
brief time, after a second reader simulated the numbers). **Pure code only**: a deterministic model and its tests.
No actors, no widgets, no maps, no game-mode changes, no console helpers, no PIE proof (nothing a player can see
changes); the acceptance is the build, the tests and `verify-loop.ps1` as the smoke check that the editor still
boots the run.

**Parallel work (read this).** Claude builds the refinery floor map and the section wiring (M5-2) in the worktree
`..\Airlock-claude` (editor on port 8001) while you run, touching `Core/**`, `Voyage/**`, `Ship/RefineryLayouts.*`,
`Ship/FloorFixture.*`, `Ship/CargoLayouts.*`, `Ship/CargoModel.*`, `Station/**`, `Content/Maps/**` and `Scripts/`.
So this task must not edit any of those. Everything below lives in `Ship/RefineryModel.h`,
`Ship/RefineryModel.cpp` and `Tests/RefineryModelTests.cpp`. Your editor is the one on port 8000 on this checkout;
never touch the one on 8001; close only your own (`Scripts\close-editor.ps1`). `Scripts\build-editor.bat` passes
`-NoHotReloadFromIDE`, so the other checkout's open editor does not block your build; if `Build.bat` still refuses
with only 8001 open, report it, never close that editor. No Computer Use / GUI tool.

## Why this task

The refinery is the second half of the run and the half that pays (`refinery-floor.md` section 1). Everything on
the floor - the three stills, the knobs, the yields, the spills, pops, flashes and coke - is one deterministic
model that the actors (M5-3), the hazards (M5-4) and the floor section (M5-2) will read and drive. Written first
and pure, it can be tested to the litre before a single actor exists, like `FShipGraph`, `FCargoModel`,
`FShipAtmosphere` and `FFuelSystem` before it (read `Ship/FuelSystem.h/.cpp` and `Ship/ShipAtmosphere.h/.cpp` for
the house style: rates struct, state struct, `Reset/Tick/...`, `Summary`, floored values on publish, no UObject
state, no randomness).

## Definition of done

1. **Types** (`Ship/RefineryModel.h`; house style: `USTRUCT()` with `UPROPERTY()` on every field, `UENUM()` with
   `enum class ... : uint8`, `AIRLOCK_API` on every struct and the class; floats throughout - 2,400 ticks is fine):
   - `EStillStatus { Idle, Running, Popped, Flash, Failed }` (fouled is **not** a status - it is the coke value; see
     item 2) and `ERefineryKnob { Heat, Feed, Vent }`.
   - `ERefineryEvent { BatchDone, Spill, Pop, Flash, Fouled, Failed }` - only what a tick produces; the verbs return
     bool and the owner logs them. `FRefineryEvent { Type, StillId, CaskId, Litres, DieselL, GasL, TarL }`: `Litres`
     is the spilled litres for `Spill` and the processed litres (`DieselL + GasL + TarL`) for `BatchDone`.
   - `FRefineryRates` (`UPROPERTY(EditAnywhere)` with these defaults): `CaskLitres` 1000, `MaxFeedLps` 20,
     `HeatBaseK` 92, `HeatSpanK` 260, `HeatTauSeconds` 10, `FeedCoolKPerL` 0.12, `HeavyIdealK` 183, `LightIdealK`
     157, `BellHalfWidthK` 22, `YieldMax` 0.85, `GasBase` 0.05, `GasSlope` 0.6, `GasStartK` 225, `GasSpanK` 60,
     `BoilStartK` 130, `PressureGain` 0.002, `PressureFloorBar` 1, `VentBleedPerS` 0.3, `TrapLimit` 0.6,
     `RoomPpmPerVentExcess` 40, `RoomPpmPerGasL` 0.05, `WarnBar` 2.5, `PopBar` 3.0, `PopPpm` 60, `PopSpillL` 300,
     `FlashK` 320, `FlashRoomPpmMin` 40, `FlashPpm` 40, `FlashConditionHit` 30, `CokeHotK` 290, `CokeHotRate` 0.6,
     `CokeDryRate` 0.3, `CokeDryHeat` 0.5, `FouledCoke` 60, `FailedCoke` 100, `RepairCoke` 40, `CalDriftPerS`
     0.004, `CalWarn` 0.8, `FloodFeed` 0.8, `FloodAfterSeconds` 5, `FloodSpillL` 200, `FloodCoolK` 30, `HopperCasks`
     3, `KnobSteps` 20, `MaxStepSeconds` 0.25, `SlotSeconds` 600, `DieselCrPerL` 0.5, `CrudeSaleCr` 40,
     `TarFeeCrPer100L` 10, `MedicalFineCr` 200, `RefuelUnitsPerL` 0.1, `BaseFuelTonnesPer100L` 1.
   - `FStillState { FName Id; EStillStatus Status; int32 HeatSteps, FeedSteps, VentSteps (0..KnobSteps; the knob
     value is Steps / KnobSteps - integers so every threshold comparison and every printed percentage is exact);
     float TempK; float PressureBar; float Calibration (0..1); float Coke (0..100); bool bFouled (mirror of
     `Coke >= FouledCoke`); float Condition (0..100); float ChargeL; FName CurrentCask; bool bCurrentLight;
     TArray<FName> Hopper; float BatchDieselL, BatchGasL, BatchTarL; int32 BatchesDone; float FloodSeconds; }` with
     `operator==`. A still starts Idle at `HeatBaseK`, `PressureFloorBar`, heat 0, feed 0, vent 11 steps (55 %),
     calibration 1, coke 0, condition 100, empty.
   - `FRefineryCask { FName Id; bool bLight; }` (`bLight` is the ship manifest's `bVolatile`; M5-2 maps it),
     `FRefineryState { TArray<FStillState> Stills; TArray<FRefineryCask> Waiting; int32 CasksDelivered; float
     TankDieselL, TotalGasL, TotalTarL, SpilledL; int32 CasksRefined, CasksLost, Spills, Pops, Flashes; }`, both with
     `operator==`.
   - `FFloorResult { int32 CasksTotal, CasksRefined, CasksUnrefined, CasksLost; float DieselL, GasL, TarL; int32
     Spills, Pops, Flashes, Criticals; float RefuelUnits, SurplusL; int32 BaseFuelTonnes; int32 Pay; FString
     Summary() const; }` - the summary reads `floor: 9/12 casks refined, 6420 L diesel, 1100 L tar, 1 spill, 0 pops,
     1 critical, pay 2980, fuel +54 t` (litres floored, one line, counts plural as in `FVoyageResult::Summary`).
2. **The model** (`class AIRLOCK_API FRefineryModel`, no UObject state, `FRefineryRates Rates` public):
   - `void Build(int32 StillCount, const TArray<FRefineryCask>& Casks)` - stills `Still_1..N`, the casks on the
     waiting list in the given order, `CasksDelivered` = their count, totals zero. `Rebuild()` builds again with the
     same inputs (every hopper's casks return to `Waiting` in build order).
   - Accessors: `const FRefineryState& State() const`, `const FStillState* FindStill(FName) const`,
     `float BatchProgress(FName) const` (`1 - ChargeL / CaskLitres` while a cask is loaded, else 0).
   - `bool StepKnob(FName Still, ERefineryKnob Knob, int32 Steps)` (clamped to `0..KnobSteps`; false for an unknown
     still or zero steps) and `bool SetKnob(FName Still, ERefineryKnob Knob, int32 Steps)` (clamped; false for an
     unknown still).
   - `bool LoadHopper(FName Still, FName Cask)` - moves a waiting cask into the still's hopper (false when the cask
     is not waiting, the hopper already holds `HopperCasks`, or the still is unknown); allowed in every status (a
     queue is a queue; a still can hold four casks - three in the hopper plus the one in the separator).
   - `bool ClearPop(FName Still)` (Popped -> Idle, pressure to the floor; the hopper is untouched),
     `bool Purge(FName Still)` (Flash -> Idle: pressure to the floor, the temperature is kept - nitrogen puts the fire
     out, the bath stays warm), `bool Scrape(FName Still)` (Idle with no cask and no charge - the hopper may hold
     casks - coke to 0), `bool Repair(FName Still)` (Failed -> Idle, coke `RepairCoke`, condition 100),
     `bool Calibrate(FName Still)` (any status but Failed: calibration 1). Each returns false when its precondition
     fails and never changes anything else.
   - `bool Tick(float DeltaSeconds, float RoomPpm, float& OutRoomPpm, TArray<FRefineryEvent>& OutEvents)` - advances
     every still, adds the ppm to put into the room this tick to `OutRoomPpm` (vent excess, gas cut, pops, flashes)
     and returns whether any floored / published value changed (compare a floored copy before and after, as
     `FFuelSystem::Tick` does). A non-positive or non-finite delta is ignored (false, 0 ppm); a non-finite `RoomPpm`
     reads as 0. A delta above `MaxStepSeconds` is sub-stepped into slices of at most `MaxStepSeconds` so the result
     does not depend on the caller's tick (a 1 s call equals four 0.25 s calls). Per still, per slice, in this order
     (`Heat`, `Feed`, `Vent` below are the knob values `Steps / KnobSteps`; the bell reads the temperature at the
     start of the slice):
     1. **Take the next cask**: a still that is Idle, or Running with `ChargeL == 0`, and has a non-empty hopper,
        pops the first hopper cask into `CurrentCask` / `bCurrentLight`, `ChargeL = CaskLitres`, batch litres 0,
        status Running.
     2. **Feed**: while Running, `Fed = min(ChargeL, Feed * MaxFeedLps * dt)`. The litres fed are processed at the
        current temperature: `Bell = max(0, 1 - ((TempK - Ideal) / BellHalfWidthK)^2)` with `Ideal` by the cask's
        light flag; `YDiesel = YieldMax * Bell * (0.5 + 0.5 * Calibration) * (bFouled ? 0.5 : 1)`; `YGas = min(1 -
        YDiesel, GasBase + GasSlope * clamp((TempK - GasStartK) / GasSpanK, 0, 1))`; `YTar = 1 - YDiesel - YGas`.
        Diesel goes to the batch and to `TankDieselL` at once (the tank gauge climbs continuously; a pop loses only
        the unfed charge), gas and tar to the batch and the totals; `ChargeL -= Fed`; `OutRoomPpm += Fed * YGas *
        RoomPpmPerGasL` (a poisoned batch fumes).
     3. **Temperature**: a first-order lag toward `Target = HeatBaseK + Heat * HeatSpanK`: `TempK += (Target - TempK)
        * min(1, dt / HeatTauSeconds) - Fed * FeedCoolKPerL`, floored at `HeatBaseK` (crude cannot cool the bath
        below itself). Every status heats, Failed included - a failed still's coils are cut: `HeatSteps` is set to 0
        when a still fails. At feed 50 % the bath settles `FeedLps * FeedCoolKPerL * HeatTauSeconds` = 12 K under
        its target, so the recipes are heat 40 % (196 K target, 184 K bath) for a heavy cask and 30 % (158 K) for a
        light one; an empty still with the heat up climbs to its target.
     4. **Flood**: while Running and `Feed > FloodFeed`, `FloodSeconds += dt`; at `FloodAfterSeconds` a `Spill` of
        `min(ChargeL, FloodSpillL)` litres leaves the charge, the temperature drops `FloodCoolK` (floored), the
        counter resets, `Spills++`, `SpilledL` grows. The counter resets to 0 below the flood feed and whenever the
        still leaves Running; a flood never changes the status.
     5. **Pressure**: while a cask is loaded (`ChargeL > 0`) and `TempK > BoilStartK`: `PressureBar += PressureGain *
        (TempK - BoilStartK) * (ChargeL / CaskLitres) * (Fed > 0 ? 1 : 0.3) * dt`; then the valve: `PressureBar -=
        Vent * VentBleedPerS * (PressureBar - PressureFloorBar) * dt`, floored at `PressureFloorBar` (so the still
        settles at `1 + PressureGain * (T - 130) * charge / (Vent * VentBleedPerS)`: about 1.6 bar at the recipe, 2.8
        at vent 20 %, a pop below vent 15 % - the needle is in the red (`WarnBar`) for tens of seconds first). Vent
        above the trap limit while boil-off exists adds `RoomPpmPerVentExcess * (Vent - TrapLimit) * clamp((TempK -
        BoilStartK) / 100, 0, 1) * dt` to `OutRoomPpm`. At `PressureBar >= PopBar` the still **pops**: status
        Popped, pressure to the floor, `PopPpm` added, a `Spill` of `min(ChargeL, PopSpillL)`, the rest of the charge
        lost (`ChargeL = 0`, `CurrentCask` cleared, `CasksLost++` - diesel already made stays made), `Pops++`,
        `Spills++`; then nothing but heat and coke until `ClearPop`.
     6. **Flash**: while Running with a charge, `TempK > FlashK` and `RoomPpm >= FlashRoomPpmMin`: status Flash,
        `FlashPpm` added, the charge lost (`CasksLost++`), `Condition -= FlashConditionHit`, `Flashes++`; at
        `Condition <= 0` the still is Failed instead (`Failed` event as well as `Flash`).
     7. **Coke**: `+CokeHotRate * dt` while `TempK > CokeHotK` (any status but Failed - a popped or flashed still's
        coils are still on), `+CokeDryRate * dt` while `Heat > CokeDryHeat` and nothing was fed this slice; `bFouled`
        mirrors `Coke >= FouledCoke` and `Fouled` is emitted on each upward crossing (again after a `Scrape` or
        `Repair` has brought it back under); reaching `FailedCoke` makes the still Failed (charge lost, `CasksLost++`,
        `Failed` event, heat cut).
     8. **Calibration** drifts `-CalDriftPerS * dt` while `Fed > 0` (floored at 0): a loaded cask waiting through a
        preheat does not drift.
     9. **Batch done**: when `ChargeL` reaches 0 on a Running still with a cask, emit `BatchDone` with the batch
        litres, `BatchesDone++`, `CasksRefined++`, clear the cask; the still stays Running (it takes the next hopper
        cask on the next slice's step 1, so there is no dry slice between casks; with an empty hopper it goes Idle).
   - `FFloorResult Tally(int32 Criticals, float ShipFuelUnitsMissing) const` (const; records nothing):
     `CasksTotal` = delivered, `CasksRefined` and `CasksLost` from the totals, `CasksUnrefined` = waiting + every
     hopper (a cask mid-batch is lost, not crude); `ShipFuelUnitsMissing` clamped to >= 0 (non-finite reads 0);
     `RefuelUnits = min(missing, TankDieselL * RefuelUnitsPerL)`; `SurplusL = TankDieselL - RefuelUnits /
     RefuelUnitsPerL`; `BaseFuelTonnes = floor(SurplusL / 100 * BaseFuelTonnesPer100L)`; `Pay = max(0,
     round(DieselCrPerL * DieselL) + CrudeSaleCr * CasksUnrefined - TarFeeCrPer100L * floor(TarL / 100) -
     MedicalFineCr * Criticals)`. Worked example the test asserts exactly: 12 delivered, 9 refined, 2 unrefined, 1
     lost, 6420 L diesel, 1100 L tar, 1 critical, 100 units missing -> RefuelUnits 100, SurplusL 5420,
     BaseFuelTonnes 54, Pay 3210 + 80 - 110 - 200 = 2980.
   - `FString Summary() const` (one line per still plus the tank: `Still_2 Running T=184 P=1.6 heat=40% feed=50%
     vent=55% cal=82% coke=12 cond=100 cask=C4 charge=420` with `cask=none` when empty, then `Tank diesel=6420 gas=900
     tar=1100 spilled=200 refined=9 lost=1 waiting=2`) and `FString StillReadout(FName Still) const` for the gauge
     label: `T 184 K  P 1.6 bar  feed 50%  cal 82%` (temperature and calibration floored, pressure to one decimal,
     knob percentages exact from the integer steps; M5-3 composes the batch yield line from `BatchProgress` and the
     batch litres).
3. **Tests** `Tests/RefineryModelTests.cpp`, `Airlock.Refinery.Model.*`, all at 0.25 s ticks. Exact where the value
   is a count, a status, an event count, a spill (200 / 300 L) or a `ChargeL` on the 2.5 L grid; a tolerance
   (`TestEqual(..., 0.01f)` or 0.5 L, as `FuelSystemTests` does) for accumulated floats: calibration, batch litres,
   coke, pressure. `Summary` / `StillReadout` are asserted from a constructed state, not a ticked one. Every yield
   test uses the recipe **set the knobs, tick 60 s with the still empty (it reaches its target), then load** - a
   cold start yields 6 % and a loaded cask does not drift during the preheat. At least:
   - build and the waiting order; `CasksDelivered`; `Rebuild` returns hopper casks in build order;
   - knob steps and clamps and the rejections; the readout prints exact percentages;
   - a heavy cask at heat 40 % / feed 50 % / vent 55 %, preheated, never recalibrated, yields at least 75 % diesel
     and finishes in 100 s plus or minus one tick; the same with `Rates.CalDriftPerS = 0` yields at least 83 %; a
     light cask at heat 30 % yields more than at heat 40 %;
   - the bath settles 12 K under the target at feed 50 % (tolerance 1 K after 100 s) and an empty still reaches
     its target; the temperature never goes below `HeatBaseK` (feed 90 % through two floods);
   - a hot still (heat 80 %, preheated to 300 K) turns a cask into about 0 / 650 / 350 L (0.5 L tolerance on the
     split at the end), fumes through `RoomPpmPerGasL`, keeps feeding after `Fouled` (emitted once) and ends with
     coke between 90 and 95;
   - calibration drifts only while feeding and `Calibrate` restores the yield;
   - feed 90 % spills 200 L after 5 s and cools 30 K; the counter resets when the still leaves Running;
   - vent 90 % fumes while boiling and 50 % adds nothing; vent 65 % fumes less than 90 %;
   - the pressure settles about 1.6 bar at the recipe; vent 20 % settles under `PopBar` and above `WarnBar`; vent
     10 % at heat 50 % pops once (status, +60 ppm, a 300 L spill, the charge lost, `CasksLost` 1), does nothing but
     heat and coke until `ClearPop`, and `ClearPop` on a Running still is refused;
   - a flash needs both the temperature (heat 90 %, preheated) and `RoomPpm >= 40`, costs 30 condition and the
     charge, and `Purge` clears it keeping the bath warm; with both coke rates zeroed, four flashes fail the still
     and cut its heat;
   - coke from heat with no feed; `Fouled` at 60 halves the yield and the still keeps running; Failed at 100 stops
     it and cuts the heat; `Scrape` needs an idle empty still (a loaded hopper is fine); `Repair` leaves coke 40 and
     re-arms the `Fouled` crossing;
   - the hopper holds three, refuses the fourth and an unknown cask, and a still runs four casks back to back with
     no dry slice (four `BatchDone`, `CasksRefined` 4);
   - litres are conserved: `TankDieselL + TotalGasL + TotalTarL + SpilledL + (charge still loaded) == CaskLitres x
     casks taken` (0.5 L);
   - the tally on the worked example above, exactly; `CasksUnrefined` after a partial run (one waiting, one in a
     hopper, one mid-batch -> 2); negative and non-finite missing units;
   - `Tick(1.0)` equals four `Tick(0.25)` (state equality) and a 1 s call over a flood still spills exactly once;
   - determinism: two models given the same scripted inputs at fixed ticks (loads, knob changes, a `ClearPop`, the
     `RoomPpm` argument) over 2,400 ticks are equal in state and in every event;
   - the summary and readout strings (constructed state); non-finite and negative deltas ignored (false, 0 ppm).
4. **Report** `Docs/reports/M5-1-refinery-model.md` (HANDOFF section 6 format): what the model does in your words,
   every constant you had to interpret and how, the test list with results, the build log lines, `LOOP OK` from
   `verify-loop.ps1`, open questions for the M5-2 / M5-3 authors (anything the actors will need that the API lacks).

## Do not touch
- Everything outside the three files above. In particular `Core/**`, `Voyage/**`, `Ship/*` other than the two new
  files, `UI/**`, `Station/**`, `Scripts/**`, every existing test, every proof, `Content/**`, every map, the packs.
- No console helpers, no replication, no timers - the model is called by whoever owns it.

## Edge cases (decided)
- Litres are floats inside the model; only `Summary`, `StillReadout` and `FFloorResult::Summary` floor them.
- A still with heat 0 sinks back to `HeatBaseK` through the lag; pressure never rises below `BoilStartK`.
- `LoadHopper` on a Failed or Popped still is allowed (the cask just waits there); it counts as unrefined.
- Not modelled (the design's section 6 lists them; M5-3 must not add them at the actor): a spill from feeding a
  popped or failed still (step 2 feeds only Running), a spill from an overfilled hopper (the fourth cask is refused).
- Events are appended in the order the steps above produce them; a slice can emit several for one still (a spill
  and a pop, say). Nothing is emitted twice for one cause.
- `Tally` is const and records nothing; the floor section owns the clock and calls it once at the end.
- Constants are starting values; do not tune them - say in the report what felt off. If a number cannot produce
  the behaviour this brief describes, implement the brief's number, prove the contradiction with a test that
  documents the actual figure, and say so.

## Acceptance (reviewer runs)
`Scripts\build-editor.bat` clean; `Scripts\run-tests.bat Airlock` all green (104 + yours); `verify-loop.ps1` exits 0.
