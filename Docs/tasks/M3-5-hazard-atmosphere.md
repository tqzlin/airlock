# Task M3-5: Hazard atmosphere, scrubbers and crew exposure

Owner: GPT-6 Astra (Codex on stack). Reviewer: Claude. Branch: `astra/m3-5-hazard-atmosphere` (from `main`).
Design source: `Docs/design/refining-run.md` (the refining run, adopted 2026-09-12), sections 2 (why the crude
off-gases nitriles and why scrubbers are the answer), 3 (minutes 3-6 "first puff" and 19-20 "under 10 ppm"), 4
(crew exposure, the clean-air bonus, blackouts) and 5 (starting numbers). This brief is the contract; where the two
disagree, this brief wins. Red-teamed by a second reader on 2026-09-12 before launch; 27 findings folded in.

**Parallel work (read this).** Claude reviews from Claude Code and may work on the M3-6 brief and docs while you run;
nobody else drives the editor on port 8000. Your editor is the one on port 8000 on this checkout, exactly as before.
Never close editors by process name: `Scripts\close-editor.ps1` closes only this checkout's editor (HANDOFF sections
2 and 7). Do not use a Computer Use / GUI tool for anything: the file console, the MCP client and the scripts cover
every step, and if a modal dialog blocks the editor after a forced close, HANDOFF section 5 says what to do.

## Prompt (what the reviewer sends to Codex)

```
You are the developer on Airlock, a UE 5.8 C++ spaceship MMO MVP. Work in this repo on a new branch
astra/m3-5-hazard-atmosphere (create it from main). Read AGENTS.md, Docs/HANDOFF.md (sections 2, 3, 5 and 7),
Docs/design/refining-run.md (the mission this task serves), Docs/tasks/M3-4-carry-and-cargo.md and
Docs/reports/M3-4-carry-and-cargo.md (your own M3-4 work, merged and reviewed), then
Docs/tasks/M3-5-hazard-atmosphere.md, then `git log --oneline -30`. State the currently verified loop and what
the cargo model already does in your own words before you start editing. Implement
Docs/tasks/M3-5-hazard-atmosphere.md exactly as specified there.
```

## Why this task

M3-4 gave the hold volatile casks that warm, vent and are lost. In the refining run the boil-off is the danger:
a warm cask's relief valve puffs nitrile-laden gas into the hold, a ruptured cask dumps it, and the crew breathes
it. This task adds the second pressure of the run - **the air** - and the answer to it - **the scrubbers** - plus
the thing that makes it personal: a per-crew **exposure** meter that slows you and, at the limit, puts you on the
deck for thirty seconds. Docking with clean air pays; passing out costs. Nothing here is a fail state: the run
always docks, as the design decided.

## Definition of done

1. **Pure atmosphere model** in `Source/Airlock/Ship/ShipAtmosphere.h/.cpp` (plain structs and a class, no UObject
   state). Forward-declare `struct FCargoCrateState;` here and `struct FAtmosphereState;` in `CargoModel.h`; include
   the other header only in the `.cpp` files (the two headers must not include each other).
   - `UENUM() enum class EScrubMode : uint8 { Off, Normal, Hazard }`.
   - `USTRUCT FAtmosphereRates` with defaults, all named so tests can quote them: `PuffRate 2.0` (ppm per second
     while a cask puffs), `PuffSeconds 10.0` (a puff lasts this long), `PuffPeriodSeconds 20.0` (a new puff starts
     this long after the previous one started, while the cask stays warm), `RuptureRate 5.0` (ppm per second),
     `RuptureSeconds 30.0`, `ScrubNormal 0.5` and `ScrubHazard 3.0` (ppm removed per second), `BrownoutFactor 0.5`,
     `WarnPpm 20.0`, `DangerPpm 50.0`, `CleanPpm 10.0`, `MaxPpm 500.0`, `ExposureRateAtWarn 1.0` (exposure percent
     per second at exactly `WarnPpm`, scaled by `Ppm / WarnPpm` above it), `ExposureRecovery 0.5` (percent per
     second below `WarnPpm`), `SlowAtExposure 60.0`, `SlowFactor 0.7`, `BlackoutSeconds 30.0`,
     `ExposureAfterBlackout 50.0`.
   - `USTRUCT FAtmosphereState { float Ppm = 0.f; EScrubMode ScrubMode = EScrubMode::Normal; bool bBrownout = false;
     bool bScrubDisabled = false; int32 ActiveSources = 0; }` with `operator==` (`Ppm` published as a whole number,
     floored; `ActiveSources` = casks *adding ppm this tick*). The defaults equal the reset state, so a game state's
     default equals a fresh model and the build-time publish broadcasts nothing.
   - `class FShipAtmosphere`: `void Reset()` (0 ppm, `Normal`, no flags, no clocks); setters `bool
     SetScrubMode(EScrubMode)`, `bool SetBrownout(bool)`, `bool SetScrubDisabled(bool)` (a Failed life support),
     `bool SetPpm(float)` (finite and within `0..MaxPpm`, else false; puff clocks untouched), `bool AddPpm(float)`
     (finite; clamps; for M3-6's soot) - every setter updates `State()` at once and returns true when something
     changed; `bool Tick(float DeltaSeconds, const TArray<FCargoCrateState>& Cargo)` (sources and sinks below, clamps
     `Ppm` to `0..MaxPpm`; returns true only for what the tick itself changed: `FloorToInt(Ppm)`, `ActiveSources` or
     the `PuffingCrates()` list; `DeltaSeconds <= 0` or non-finite is a no-op that returns false); `float GetPpm()
     const`; `const FAtmosphereState& State() const`; `bool IsPuffing(FName CrateId) const` and `TArray<FName>
     PuffingCrates() const` (in cargo order: every cask whose puff cycle is *running* - qualifying, in its on or its
     off phase - or bursting, so the FX runs continuously while a warm cask is unracked); `FString Summary() const`
     (one line: `Air <ppm> ppm scrub=<Off|Normal|Hazard> brownout=<0|1> disabled=<0|1> sources=<n>`); and the pure
     exposure step `static float TickExposure(float Exposure, float Ppm, float DeltaSeconds, const FAtmosphereRates&
     Rates)` (rises by `ExposureRateAtWarn * (Ppm / WarnPpm)` per second when `Ppm >= WarnPpm`, falls by
     `ExposureRecovery` per second below it, clamped `0..100`, invalid deltas return the input unchanged).
   - **Sources**, decided: a cask puffs when it is `bVolatile`, `Temperature >= FCargoRates().WarnTemperature` (40),
     and its state is neither `Racked` nor `Vented` (a clamped warm cask keeps puffing - the rack is the safe place,
     the clamp is not). Each puffing cask has its own clock keyed by crate id: a puff adds `PuffRate` for the first
     `PuffSeconds` of every `PuffPeriodSeconds` cycle, the cycle starts at 0 the tick the cask first qualifies, and
     stops (clock discarded) the tick it stops qualifying; a cask that qualifies again starts a fresh cycle. A cask
     the model sees as `Vented` for the first time bursts: `RuptureRate` for `RuptureSeconds` from that tick, then
     nothing more, ever. Cargo ids that vanish from the array drop their clocks.
   - **Sinks**, decided: `Off` removes nothing; `Normal` removes `ScrubNormal`; `Hazard` removes `ScrubHazard`;
     with `bBrownout` the removal is multiplied by `BrownoutFactor`; with `bScrubDisabled` it is 0. Sinks apply after
     sources in the same tick. `Ppm` never goes below 0.
   - Deterministic: two models given the same calls and the same cargo arrays are equal, tick for tick.
2. **The game mode owns it** (`AAirlockGameMode`). A section *has an atmosphere* iff
   `CargoLayouts::Find(GameState.CurrentSectionId) != nullptr` - a pure test usable on clients, in widgets and in
   tests; the game mode exposes it as `bool AirHasAtmosphere() const`. The model is `Reset` with the cargo model on
   such a section, reset again when the voyage clock starts, and ticked in the same 4 Hz timer *only while
   `bVoyageActive`* and *after* the cargo tick (so sources see this tick's temperatures and states; pass the model
   the cargo model's `Crates()`). Before the cargo tick the game mode derives the flags from the ship graph and sets
   the rack factor: `SetScrubDisabled(LifeSupport.Status == Failed)`, `SetBrownout(PowerCore.Status != Nominal ||
   LifeSupport.Status == Degraded)`, and the rack pays for hazard scrubbing: `FCargoModel` gains `void
   SetRackCoolFactor(float)` (default 1, clamped `0..1`, multiplies `RackCool`; `Summary` unchanged) which the game
   mode sets every tick to `(CoolantLoop.Status == Nominal ? 1 : 0.5) * (ScrubMode == Hazard && PowerCore.Status !=
   Nominal ? 0.5 : 1)`. Publication: `AAirlockGameState` gains `UPROPERTY(ReplicatedUsing=OnRep_Atmosphere)
   FAtmosphereState Atmosphere` and `UPROPERTY(ReplicatedUsing=OnRep_Atmosphere) TArray<FName> PuffingCrates`,
   `void PublishAtmosphere(const FAtmosphereState&, const TArray<FName>&)` mirroring `PublishCargo` (compare both,
   assign only when either differs, `ForceNetUpdate`, call `OnRep_Atmosphere()` itself on the authority) and
   `OnRep_Atmosphere` broadcasts the existing `OnShipGraphChanged` (no new delegate). The game mode publishes after
   every tick that reported a change, and every `Air*` mutator below publishes immediately (like the Cargo API).
   It logs `Air <ppm> ppm scrub=<Mode>[ brownout][ disabled]` on every mode or flag change and every time the floored
   ppm crosses `WarnPpm`, `DangerPpm` or `CleanPpm` in either direction (never more often). API for the character,
   actors and console: `bool AirSetScrubMode(EScrubMode)` (false without an atmosphere; with the *current* mode it
   returns true and publishes nothing), `bool AirSetPpm(float)` (the model's `SetPpm`; proofs), `EScrubMode
   AirGetScrubMode() const`, `FString AirSummary() const` (`No atmosphere on this section` on the hub). The scrub
   mode is `Normal` when the voyage starts.
3. **Captain lines**, from the game mode through `ClientCaptainSays` to every controller: rising through `WarnPpm`:
   `"Captain {0} here. Nitriles in the hold air. Scrubbers to hazard mode - the life-support panel."`; rising through
   `DangerPpm`: `"Captain {0} here. Air's bad. Get those scrubbers up and keep moving."`; falling back through
   `WarnPpm`: `"Captain {0} here. Air's clearing. Good work."`; a crew blackout: `"Captain {0} here. {1} is down -
   med-bot's on it."` (`{1}` = the pawn's name). Each line has its own 20 s cooldown; when one publish crosses several
   thresholds upward (an `AirlockAirSet 80` from 0) only the highest threshold's line is spoken. Log each as today's
   captain lines are logged (`Captain <Name>: <line>`).
4. **The scrubber control** on the life-support module. `AShipModule` gains `FName GetModuleId() const` and
   `EShipModuleType GetModuleType() const` and implements `IInteractable` as a *press* (`GetInteractHoldSeconds`
   stays 0): for the module whose type is `LifeSupport` the prompt is `"Scrubbers: Normal - E for hazard mode"` /
   `"Scrubbers: Hazard - E for normal mode"` / `"Scrubbers: Off - E to start"` (from `GameState.Atmosphere.ScrubMode`;
   empty while `bScrubDisabled` or on a section without an atmosphere); `Interact` calls the character's new
   `UFUNCTION(Server, Reliable) ServerSetScrubMode(EScrubMode)` with the next mode (`Off -> Normal`, `Normal ->
   Hazard`, `Hazard -> Normal`). The character does the work in one server-side member `bool
   SetScrubModeServer(EScrubMode Mode, bool bRequireReach)`: refused while blacked out, refused when `bRequireReach`
   and the life-support module actor is not within `IsInHoldRange`, otherwise `AirSetScrubMode`; logs `Scrub <Mode>
   by <Pawn>` or `Scrub rejected: <reason>`. `ServerSetScrubMode_Implementation` calls it with `true`; the console
   path (item 9) with `false`. Every other module type returns an empty prompt and ignores `Interact`. Note the
   M3-4 rule: E with a crate in hand puts the crate down before anything else, so the panel is used with empty hands
   (say so in the prompt only if you like; the reviewer will not). The label of the life-support module gains a
   second line `Scrubbers <Mode>`. The control board (`UControlBoardWidget`, level pass) gains one header line under
   the clock: `AIR <ppm> ppm  SCRUBBERS <Mode>[ BROWNOUT]` from the game state - a display, not a control;
   `BuildRows` and its test are untouched.
5. **Crew exposure** on `AAirlockCharacter`: `UPROPERTY(ReplicatedUsing=OnRep_Exposure) float Exposure` (owner-only,
   like `HoldProgress`), `UPROPERTY(ReplicatedUsing=OnRep_BlackedOut) bool bBlackedOut` (to everyone) and
   `UPROPERTY(Replicated) double BlackoutEndServerTime` (owner-only). On the server, in `Tick`, while
   `AirHasAtmosphere()` and `bVoyageActive`: `Exposure = FShipAtmosphere::TickExposure(Exposure,
   GameState.Atmosphere.Ppm, DeltaSeconds, Rates)`; logs `Exposure <Pawn> <n>%` when the floored value crosses 30,
   60 or 100 upwards, and `Cough <Pawn>` every 5 s while `Exposure >= 60` (the audio placeholder). At 100:
   **blackout** - the server sets `bBlackedOut` and `BlackoutEndServerTime`, puts the carried crate down
   (`ServerPutDown`, which already restores speed), leaves any seat (`LeaveSeat(false)`), resets any hold, applies
   `MOVE_None`, logs `Blackout <Pawn>`, tells the game mode (`AAirlockGameMode::NotifyBlackout(APawn*)` counts it for
   the tally and speaks the captain line), and after `BlackoutSeconds` restores walking, sets `Exposure =
   ExposureAfterBlackout`, clears `bBlackedOut` and logs `Recovered <Pawn>`. Movement mode is one predicate on both
   sides: `ApplyLocalMovement()` sets `MOVE_None` iff `SeatedStation || bBlackedOut`, else `MOVE_Walking` (with
   `StopMovementImmediately`), called by the server after every seat or blackout change and by `OnRep_SeatedStation`
   and `OnRep_BlackedOut` on the owning client (this replaces the body of `ApplySeatedMovement`; the seat tests must
   stay green). While blacked out `Move` and `Interact` return early and every server RPC that acts (`ServerPickUp`,
   `ServerPlace`, `ServerBeginHold`, `ServerSitAt`, `ServerSetScrubMode`) is refused with `<Verb> rejected: blacked
   out`. `AirlockWarp` keeps working (proofs). Walk speed becomes one function: `ApplyWalkSpeed()` assigns
   `BaseWalkSpeed * (carrying ? CarrySpeedFactor : 1) * (Exposure >= SlowAtExposure ? SlowFactor : 1)` from the cached
   base and is called by `OnRep_CarriedCrate`, `OnRep_Exposure` and wherever the server changes either input (assign,
   never multiply in place - the M3-4 rule).
   Local UI `UI/ExposureWidget` (C++-built like `HoldProgressWidget`; the local pawn creates it in `Tick` and removes
   it in `EndPlay`): a full-viewport `UBorder` tinted `(0.35, 0.05, 0.02)` whose opacity is `0` below 30 % exposure
   and rises linearly to `0.55` at 100 % (`HitTestInvisible`), a bottom-right label `EXPOSURE 62%` visible from 30 %
   (red from 60 %), and while blacked out the label reads `BLACKED OUT - 18 s` (whole seconds remaining, from
   `BlackoutEndServerTime` and the game state's server time) at opacity `0.85`.
6. **Task board.** `UTaskBoardWidget::BuildRows` gains a fourth parameter `const FAtmosphereState* Air = nullptr`
   (existing call sites compile as they are). With air at or above `WarnPpm` the first row is `"Hold air 32 ppm -
   scrub"` (mode `Off` or `Normal`), `"Hold air 32 ppm - scrubbing"` (mode `Hazard`) or `"Hold air 32 ppm - repair
   life support"` (`bScrubDisabled`), red at or above `DangerPpm`; below `WarnPpm` there is no air row. Everything
   else keeps its order (heating crates, Failed modules, faulted lines, Degraded modules, loose crates); the 8-row
   cap and `All systems nominal` are unchanged. `RefreshRows` passes `&State->Atmosphere` on sections with an
   atmosphere and `nullptr` elsewhere.
7. **Puffs you can see.** `ACargoCrate` reads `GameState.PuffingCrates` in its `Refresh`: while its id is listed and
   it is not Vented, the existing `VentFX` runs (the same restart pattern) and the `HeatLight` flickers on a 0.4 s
   timer by *assigning* `HeatIntensity * Heat * 0.6` and `HeatIntensity * Heat * 1.0` alternately (never scaling the
   current value); while Vented the M3-4 behaviour (steam, full red) is unchanged. Labels are unchanged.
8. **Tally and the hub board.** `FVoyageResult` gains `float DockPpm = -1.f`, `int32 Blackouts = 0`, `bool
   bCleanAir = false`. `FCargoModel` gains `static constexpr int32 CleanAirBonus = 150` and `BlackoutDock = 50` next
   to `PayPerIntactCrate`, and `Tally` gains trailing `const FAtmosphereState* Air = nullptr, int32 Blackouts = 0`:
   with `Air` the result records `DockPpm = Air->Ppm`, `bCleanAir = Air->Ppm < CleanPpm`, and `Pay` adds
   `CleanAirBonus` when clean and subtracts `BlackoutDock` per blackout (`Pay` may not go below 0); without `Air` the
   M3-4 result is exactly what it is today (the M3-4 tests keep passing untouched). Worked examples the tests must
   reproduce: 3 Secured + 1 Racked, hull 100, air 4 ppm, 0 blackouts -> 900 (the design note's "750" forgot the
   M3-4 clean-manifest bonus; 900 is right and M3-6 will re-base on it); the same at 30 ppm -> 750; 2 Secured +
   2 Loose, hull 100, air 4 ppm, 1 blackout -> 500; 1 Racked + 3 Loose, hull 100, air 4 ppm, 1 blackout -> 400.
   `FVoyageResult::Summary` appends `, air <ppm> ppm, <n> blackouts` only when `DockPpm >= 0` (whole ppm, floored),
   so the M3-4 log formats and proof regexes still match. The game mode's `CargoTally` and `RecordVoyageResult` pass
   the live atmosphere and the blackout count (`AirlockTally` shows the same fields). `UVoyageBoardWidget::
   DescribeResult` appends ` - air clean` when `bCleanAir`, else ` - air <ppm> ppm`, only when `DockPpm >= 0` (the
   existing exact-text test passes a result without air data and must stay green).
9. **Console helpers** on `AAirlockPlayerController` (exec -> unchecked server RPC, same caveat as the others; each
   logs what it did or why not): `AirlockAir` (the atmosphere summary line plus one `Crew <Pawn> exposure=<n>%
   blackedOut=<0|1>` line per crew pawn - a different prefix from the crossing log on purpose), `AirlockScrub
   <Off|Normal|Hazard>` (the character's `SetScrubModeServer(Mode, false)`: no reach check, same logs),
   `AirlockAirSet <ppm>` (`AirSetPpm`), `AirlockExpose <pct>` (sets the calling controller's pawn's `Exposure`,
   `0..100`, finite; it takes effect through the exposure tick, so 100 blacks out on the next tick only while that
   tick runs - a section with an atmosphere and an active voyage; elsewhere it just sets the number).
10. **Tests** (filter `Airlock`; the existing 56 stay green and untouched apart from the defaulted parameters):
    - `Tests/ShipAtmosphereTests.cpp`: no sources -> 0 ppm forever and `Tick` returns false; a warm loose volatile
      cask puffs `PuffRate * PuffSeconds` per period minus scrubbing (quote the numbers), the second period starts
      at `PuffPeriodSeconds`; a warm Secured cask puffs, a Racked one does not, a non-volatile one never does; a
      cask below `WarnTemperature` stops mid-puff and restarts a fresh cycle when it warms again; `PuffingCrates`
      lists a cask through its off phase and drops it when it stops qualifying; a Vented cask bursts exactly
      `RuptureRate * RuptureSeconds` once and never again; `Off` / `Normal` / `Hazard` sinks, brownout halves,
      disabled removes nothing; `SetPpm` / `AddPpm` validation and clamping at `MaxPpm` and 0; setters report a
      change and `Tick` reports only its own changes; invalid deltas are no-ops; determinism over 2000 ticks with a
      scripted cargo array; `TickExposure` rises at 1 %/s at 20 ppm, 2.5 %/s at 50 ppm, recovers at 0.5 %/s, clamps.
    - `Tests/CrewExposureTests.cpp` (spawned world; copy the `FCargoTestWorld` harness under a new name,
      `FAirTestWorld`): exposure rises only while `bVoyageActive`; walk speed is `Base * SlowFactor` at 60 % and
      `Base * CarrySpeedFactor * SlowFactor` while also carrying, restored on recovery, unchanged after repeated
      `OnRep` calls; blackout at 100 drops the carried crate as Loose, vacates a taken seat, sets `MOVE_None`,
      refuses pick-up / hold / sit / scrub, recovers after `BlackoutSeconds` with `Exposure ==
      ExposureAfterBlackout` and walking restored; `ApplyLocalMovement` gives `MOVE_None` for seated, blacked out
      or both and `MOVE_Walking` for neither; `NotifyBlackout` counts once per blackout; the life-support module's
      prompt cycles the modes, `ServerSetScrubMode` out of reach is refused and the console path is not; a Failed
      life support empties the prompt and disables scrubbing; `PublishAtmosphere` notifies once per change (also
      when only `PuffingCrates` changed) and not for an unchanged state; the hub has no atmosphere (`AirSummary` says
      so, helpers reject, `AirHasAtmosphere` is false).
    - `Tests/TaskBoardTests.cpp` (extend): air row first and only from `WarnPpm`, the three wordings, red from
      `DangerPpm`, no row below `WarnPpm`, the cap still holds with the extra row.
    - `Tests/CargoModelTests.cpp` (extend): the four pay examples above, `Summary` with and without air data,
      `SetRackCoolFactor` halves cooling and clamps.
    - `Tests/CargoInteractionTests.cpp` or `CrewExposureTests.cpp`: `DescribeResult` with air data appends the air
      text and without it is the M3-4 string.
11. **PIE proof** `Scripts\verify-atmosphere.ps1` (conventions of `verify-cargo.ps1`: file console, fresh log lines
    only, captures under `Saved\AgentCaptures\M3-5`, exit 0 and `ATMOSPHERE OK` on success; anchor every log count
    on the category boundary, HANDOFF section 5). HUD text (`Hold air` rows, the vignette, the countdown, the board's
    `air clean`) is **capture-only** - Slate snapshots omit the PIE HUD - so every wait below is on a log line; the
    captures are looked at by the reviewer. The proof never jumps the clock forward; it rewinds twice so the 150 s
    calm never ends while it runs (the teaching leak would spoil the tally). Steps:
    - Reach the ship as `verify-cargo.ps1` does; on `Incident director started` send `AirlockVoyageClock 1200`;
      `AirlockAir` -> `Air 0 ppm scrub=Normal brownout=0 disabled=0 sources=0`.
    - Warp beside C2 from the west (`Move-BesideCargo` in the cargo proof), `AirlockCrateHeat C2 45` -> `Cargo C2 Loose
      temp=45` (a warm loose cask puffs at once, +2 ppm/s for 10 s of every 20 s, minus 0.5 ppm/s scrubbing).
      Within 40 s: the crossing log `Air 20 ppm scrub=Normal` and the captain's `Nitriles in the hold air` line;
      `AirlockAir` >= 20 ppm. Capture `first-puff` (`AirlockLook` at C2: steam and the flickering heat light; the
      board's `Hold air \d+ ppm - scrub` row).
    - `AirlockAirSet 80` -> `Air 80 ppm scrub=Normal` and the captain's `Air's bad` line (the board row turns red).
    - Warp to the engineering walkway in reach of life support: probe (-1300, 760, 118) with `AirlockWarp` +
      `AirlockWhere` first (life support is wall-mounted at (-1106, 760, 110); the reach is 312 cm) and `AirlockLook 0`
      at it; `AirlockScrub Hazard` -> `Scrub Hazard by AirlockCharacter_0` and `Air \d+ ppm scrub=Hazard`; capture
      `scrubbing` within the next 8 s (air about 50-65 ppm, the row reads `- scrubbing`).
    - Warp back beside C2, `AirlockCarry C2` -> `Carry C2 by`; warp beside the rack, `AirlockPlace ColdRack` -> `Cargo
      C2 Racked`. Within 60 s of the `AirlockScrub Hazard` command (not of the place - the air may already have
      crossed 20 on its way down): the crossing log `Air 19 ppm scrub=Hazard` and the captain's `Air's clearing`
      line; then `AirlockAir` below 20 ppm.
    - `AirlockAirSet 120` -> `Air 120 ppm scrub=Hazard` (the `Air's bad` line again if its 20 s cooldown has passed;
      do not wait on it); `AirlockExpose 90` -> `Exposure AirlockCharacter_0 100%` and `Blackout AirlockCharacter_0`
      within 10 s (6 %/s at 120 ppm); capture `blackout` at once (the vignette and `BLACKED OUT - <n> s`); `Recovered
      AirlockCharacter_0` within 40 s of the blackout line; `AirlockVoyageClock 1200` again; wait until `AirlockAir`
      shows `Air [0-9] ppm scrub=Hazard` (hazard scrubbing clears 120 ppm in 40 s with C2 racked).
    - `AirlockScrub Hazard` -> `Scrub Hazard by AirlockCharacter_0` (already hazard: accepted, nothing published).
    - `AirlockTally` -> `Tally now: 1/4 crates intact, 0 vented, 3 loose, hull 100\.00%, pay 400, air \d ppm, 1
      blackouts`; `AirlockShipStatus` -> seven `Nominal cond=100\.00`; leave through `Airlock_ToHub` -> `Voyage
      result: 1/4 crates intact, 0 vented, 3 loose, hull 100\.00%, pay 400, air \d ppm, 1 blackouts`.
    - On the hub: `AirlockAir` -> `No atmosphere on this section`; `AirlockBoard` -> `Voyage board opened`; capture
      `hub-last-run-air` (`Last run: 1/4 crates intact - hull 100% - pay 400 cr - air clean`).

## Do not touch
- `Ship/ShipLayouts.*`, `Ship/ControlStation.*`, `Ship/ShipGraph.*`, `Ship/AirlockDoor.*`, `Voyage/VoyageSchedule.*`,
  `Voyage/VoyageTypes.h`, `Voyage/VoyageSubsystem.*`, `Voyage/IncidentSchedule.*` (no new incident type in this task),
  `Core/Interactable.h`, `Core/AgentConsoleSubsystem.*`.
- `HandleTransitionRequested`, `ReturnToHub`, `TravelToSection`, `ChoosePlayerStart` (unchanged; `RecordVoyageResult`
  is where the tally changes live).
- The expectations of the existing 56 tests (extend the files, never rewrite a passing assertion; the defaulted
  parameters in 6 and 8 keep every existing call site compiling and every existing string identical). The seat tests
  exercise `ApplySeatedMovement` through `ServerSitAt` / `ServerStandUp` only; the `ApplyLocalMovement` refactor must
  keep them green.
- `Scripts\verify-loop.ps1`, `verify-voyage-clock.ps1`, `verify-ship-graph.ps1`, `verify-incidents.ps1`,
  `verify-level-pass.ps1`, `verify-cargo.ps1` - all six must still exit 0 on your branch without edits. Worked out
  from the measured runs: `verify-cargo.ps1` ruptures C4 and docks 16 s later - about 80 ppm and 34 % exposure at
  the exit, no blackout, no clean-air bonus, `pay 400` unchanged, its `Voyage result:` / `Tally now:` regexes have no
  end anchor so the appended air fields are fine (its `vented-C4` capture will carry an `Air's bad` caption, which
  is correct). `verify-voyage-clock.ps1` jumps to 75 s remaining, so both volatile casks warm and puff for the last
  40 s: `Cough` lines and captions appear, no regex breaks. `verify-incidents.ps1`: the casks reach about 30 C before
  the exit (no puff) and its `ModuleDamage PowerCore 45` step now also logs `Air ... brownout`. `verify-level-pass`
  and `verify-ship-graph`: unaffected. If any of the six fails on your branch, the model or the numbers are wrong,
  not the proof.
- `Content/**`, every map, the pack folders. Everything this task adds is spawned from code.

## Edge cases (decided)
- Only sections with a cargo layout have an atmosphere (today: `Hauler_Mk1`); the hub has none and the helpers say
  so. One atmosphere per section (the whole ship is one volume in the MVP).
- The atmosphere ticks only while `bVoyageActive`, after the cargo tick, from the cargo model's live crates; a ship
  section entered without a booking never ticks it.
- Puff clocks belong to the model and are keyed by crate id; nothing about puffs is replicated except the
  `PuffingCrates` list the actors read for their FX.
- A Vented cask bursts from the tick the model first sees it Vented (also if it is already Vented when the voyage
  clock starts). A cask that vents while Carried bursts in place after the server drops it (M3-4 behaviour).
- Exposure lives on the pawn. It ticks only on a section with an atmosphere while `bVoyageActive`; seamless travel
  spawns a fresh pawn, so a crew member arriving on the hub starts at 0 there; nothing is saved.
- A blackout drops the crate at the put-down spot (not the feet: the existing `ServerPutDown`), releases the seat
  without the exit teleport (`LeaveSeat(false)`), and ignores movement input; `AirlockWarp` still moves the pawn
  (proofs rely on it). A pawn destroyed while blacked out needs no extra handling (`EndPlay` already drops cargo
  and vacates seats).
- The clean-air bonus reads the ppm at the moment of `RecordVoyageResult` (the docking or the hub-bound door),
  floored. Blackouts are counted per pawn per event on the game mode (`NotifyBlackout`), reset when the voyage
  clock starts.
- The scrub mode resets to `Normal` whenever the voyage clock starts; `Off` exists for tests and the console, the
  key never selects it.
- Life support Failed disables scrubbing entirely (prompt empty, `disabled` in the summary, the board row says
  `repair life support`); repairing it restores the previously selected mode.

## Notes and hints
- `AAirlockGameMode::TickShipGraph` is the 4 Hz timer; `PublishCargo` / `AAirlockGameState::PublishCargo` are the
  publish pattern (compare, assign, `ForceNetUpdate`, call the RepNotify yourself on the authority);
  `ShipGraph.Modules()` gives module status by id; `CargoModel.Crates()` the live crate states.
- `AShipModule` keeps its spec (id and type) privately since M3-2 - hence the two getters in item 4; adding
  `IInteractable` to it follows `ACargoSlot`. The life-support module is at (-1106, 760, 110) on the engineering
  east wall; the walkway in front of it is around X -1300 (the level-pass proof repairs `Coolant_1` from (-1250, 430)
  and `Power_1` from (-1400, 450)) - probe your spot.
- `AAirlockCharacter::LeaveSeat(bool)`, `ApplySeatedMovement(bool)` and `ResetHold()` are private members of the
  same class - call them directly from the blackout code. `OnRep_SeatedStation` shows how the owning client mirrors
  a movement mode today; `ApplyLocalMovement` generalises it.
- `UCaptainLineWidget` shows whatever `ClientCaptainSays` sends; the incident director shows how the game mode
  formats a captain line with `FText::Format` and the captain's name from `ActiveVoyage.Captain`.
- The cargo proof warps beside things from the west (`spot.X - 230`) so labels facing -X are readable; do the same,
  and use `AirlockLook <yaw>` before every capture (HANDOFF section 3).
- Order that has served well: model + tests -> game-mode ownership, publication and the flags -> the life-support
  control and captain lines -> exposure, blackout and the widget -> task board and control board lines -> tally
  and hub board -> console helpers -> the proof. Commit after every step.

## Acceptance (reviewer runs)
`Scripts\build-editor.bat` clean; `Scripts\run-tests.bat Airlock` all green (56 existing + the new ones); all six
existing proofs and `verify-atmosphere.ps1` exit 0; captures show the puff, the `Hold air` row in the `scrub` and
`scrubbing` wordings, the blackout vignette with its countdown, and the hub board's `air clean` text.
