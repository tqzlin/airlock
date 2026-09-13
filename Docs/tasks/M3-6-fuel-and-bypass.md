# Task M3-6: Fuel, the feed leak and the crude bypass

Owner: GPT-6 Astra (Codex on stack). Reviewer: Claude. Branch: `astra/m3-6-fuel-and-bypass` (from `main`, after
M3-5 has merged). Design source: `Docs/design/refining-run.md` sections 2 (methane is fuel; the bypass, fouling
and soot), 3 (minutes 9-12 "the fuel beat" and 12-16 "dirty burn"), 4 (the soot fine) and 5 (starting numbers).
This brief is the contract; where the two disagree, this brief wins. Design cut made here: the mixture-needle
minigame from the note is deferred to the timing-calls milestone (design note `airlock-voyage-loop-design`, 8.1
lever 3); in this task the bypass burns each cask on its own and the cost is fouling, soot and the fine.

**Parallel work (read this).** Claude reviews from Claude Code; nobody else drives the editor on port 8000. Your
editor is the one on port 8000 on this checkout. Never close editors by process name (`Scripts\close-editor.ps1`
only). No Computer Use / GUI tool for anything (HANDOFF section 5 covers the one dialog that can appear).

## Prompt (what the reviewer sends to Codex)

```
You are the developer on Airlock, a UE 5.8 C++ spaceship MMO MVP. Work in this repo on a new branch
astra/m3-6-fuel-and-bypass (create it from main). Read AGENTS.md, Docs/HANDOFF.md (sections 2, 3, 5 and 7),
Docs/design/refining-run.md, Docs/tasks/M3-5-hazard-atmosphere.md and Docs/reports/M3-5-hazard-atmosphere.md
(your own M3-5 work, merged and reviewed), then Docs/tasks/M3-6-fuel-and-bypass.md, then
`git log --oneline -30`. State the currently verified loop and what the cargo and atmosphere models already do in
your own words before you start editing. Implement Docs/tasks/M3-6-fuel-and-bypass.md exactly as specified there.
```

## Why this task

The run needs a reversal in its middle. Nine minutes out, a feed valve fails and the hauler bleeds methane; at the
burn rate that follows it docks short. The fix is the thing the crew is paid to deliver: sludge is methane, the
yard fitted a crude bypass, and a cask fed to the intake is fifteen units of fuel that will never be sold. Which
cask to burn is a decision (the warm one that will not survive anyway is the smart burn), and burning has a cost:
soot into the hold air, injectors that foul until someone clears them, a fine at the dock. A ship that does run
dry does not fail; it coasts and docks late, for less.

## Definition of done

1. **Pure fuel model** in `Source/Airlock/Ship/FuelSystem.h/.cpp` (plain structs and a class, no UObject state):
   - `USTRUCT FFuelRates` with defaults, named so tests can quote them: `Capacity 100.0` (units), `StartFuel 80.0`
     (a voyage starts with 80 % - the Rig short-fills), `NominalBurnPerMinute 4.0`, `LeakPerSecond 0.6667` (a
     failed feed valve bleeds about 40 units in 60 s), `CaskFuel 15.0`, `CaskBurnSeconds 20.0`, `SootPpmPerCask 15.0`
     (added to the atmosphere evenly over the burn), `FoulingPerCask 10.0` (Engines condition lost per cask
     burned), `FoulingPerCaskFouled 20.0` (per cask burned while the injectors are already fouled),
     `CasksPerFouling 2` (every second cask burned fouls the injectors), `LowFuelPercent 30.0`,
     `LateDockSeconds 120.0`.
   - `USTRUCT FFuelState { float Fuel; bool bFeedLeak; bool bBypassOpen; FName BurningCrate; float BurnRemaining;
     int32 CasksBurned; bool bInjectorsFouled; bool bExhausted; }` with `operator==` (`Fuel` published as a whole
     number, floored).
   - `class FFuelSystem`: `void Reset()` (`StartFuel`, no leak, bypass closed, nothing burning, 0 burned, not
     fouled, not exhausted), `bool Tick(float DeltaSeconds)` (burns `NominalBurnPerMinute / 60` per second, plus
     `LeakPerSecond` while `bFeedLeak`; advances a cask burn and credits `CaskFuel` when `BurnRemaining` reaches 0;
     clamps `0..Capacity`; sets `bExhausted` the tick `Fuel` reaches 0 and never clears it; returns true when the
     floored fuel, a flag, the burning crate or the count changed; invalid deltas are no-ops that return false),
     `void OpenLeak()`, `bool CloseLeak()` (false when there is none), `bool OpenBypass()` (false when already open;
     it never closes again this voyage), `bool FeedCask(FName CrateId)` (requires the bypass open and nothing
     burning; starts a `CaskBurnSeconds` burn), `bool ClearInjectors()` (false when not fouled), `float
     SootPerSecond() const` (`SootPpmPerCask / CaskBurnSeconds` while burning, else 0), `int32
     FoulingOnFeed() const` (what the next cask will cost the engines), `float SecondsOfFuelLeft() const` (at the
     nominal burn, ignoring a leak), `bool IsShort(float VoyageSecondsRemaining) const` (`SecondsOfFuelLeft() <
     VoyageSecondsRemaining`), `const FFuelState& State() const`, `FString Summary() const` (one line: `Fuel <n>%
     leak=<0|1> bypass=<open|closed> burning=<CrateId|none> burned=<n> fouled=<0|1> exhausted=<0|1>`).
   - Fouling bookkeeping lives in the model: `FeedCask` increments `CasksBurned` when the burn completes (not when
     it starts); when `CasksBurned % CasksPerFouling == 0` at that moment, `bInjectorsFouled` becomes true.
     The engine damage itself is applied by the game mode (item 3) from `FoulingOnFeed()` read *before* the feed.
   - Deterministic; no randomness anywhere in the model.
2. **Cargo side.** `ECargoState` gains `Burned` (appended after `Vented`): inert like `Vented`, never counted intact,
   never puffs. `FCargoModel` gains `bool Consume(FName CrateId)` (a `Carried` crate -> `Burned`, its carrier's job
   to detach first; false otherwise) and `Tally` counts `CratesBurned` (new `FVoyageResult` field). A Burned cask's
   actor is hidden (`SetActorHiddenInGame(true)`, collision off, label off) by its `Refresh` when the state says
   so; nothing is destroyed. Task board: a Burned crate gets no row. `FCargoLayouts` is untouched.
3. **Layout and actors** in `Source/Airlock/Ship/FuelLayouts.h/.cpp` (the `CargoLayouts` shape):
   `USTRUCT FFuelLayout { FName SectionId; FTransform FeedValve; FTransform BypassValve; FTransform Intake;
   FSoftObjectPath ValveMesh; FSoftObjectPath IntakeMesh; }`, `namespace FuelLayouts { const FFuelLayout*
   Find(FName); }`, `Hauler_Mk1` only, all three in engineering (the "CENTRAL ENGINEERING" L-corridor: X -1531..-1106
   between the wall faces, Y 230..950, floor Z 22; the west wall is taken by the core band for its whole length, the
   east wall has the coolant tank at (-1192, 420), life support at (-1106, 760, 110) and the engine node at (-1160,
   880); `Coolant_1` runs along the west wall at Y 250, `Power_1` climbs the east wall at (-1110, 450)). Proposed:
   the feed valve on the east wall at (-1150, 600, 110) yaw 180, the bypass valve next to it at (-1150, 680, 110)
   yaw 180, the intake pad on the floor at (-1200, 560, 22) yaw 0; meshes `SM_Valve_A` from
   `/Game/ModSci_Engineer/Meshes/` for the valves (check with `AssetTools.exists`, pick the nearest valve/wheel mesh
   the kit has, cube fallback otherwise) and the pallet for the intake. Probe every spot with `AirlockWarp` +
   `AirlockWhere` from the walkway spot (-1300, 600, 118) and record the final table in your report; positions live
   in `FuelLayouts.cpp`, never in a map.
   - `Ship/FuelValve` - `AFuelValve` (the feed valve): replicated, `IInteractable` *hold* (3 s) with prompt `Shut the
     feed valve (hold E)` while `GameState.Fuel.bFeedLeak`, empty otherwise; `OnHoldComplete` -> the game mode's
     `FuelCloseLeak()`; a red `LeakLight` and the steam FX (the `AShipLine` pattern) while leaking.
   - `Ship/BypassValve` - `ABypassValve`: replicated, `IInteractable` *press* with prompt `Open the crude bypass`
     while closed, empty once open; `Interact` -> the character's new `UFUNCTION(Server, Reliable)
     ServerOpenBypass(ABypassValve*)` (in reach, not blacked out) -> `FuelOpenBypass()`; an amber `BypassLight` on
     while open; label `BYPASS  closed|OPEN`.
   - `Ship/FuelIntake` - `AFuelIntake`: replicated, `IInteractable` *press*: prompt `Feed <Id> to the engines`
     while the local pawn carries a cask and the bypass is open and nothing is burning, `Bypass closed` while the
     bypass is closed, `Burning <Id> - <n> s` (no interaction) while burning, empty otherwise; `Interact` ->
     the character's new `ServerFeedCask(AFuelIntake*)`: in reach, not blacked out, carrying, bypass open, nothing
     burning; the server detaches the crate (the M3-4 `DetachCarriedCrateAt` at the intake's transform), calls the
     game mode's `FuelFeedCask(CrateId)` (which applies `Consume`, the engine damage and starts the burn), logs
     `Feed <Id> by <Pawn>` or `Feed rejected: <reason>`; label `INTAKE  <CrateId> <n> s | ready | bypass closed`.
   - All three spawn from the game mode after the cargo actors, folder `Fuel`, names `FeedValve`, `BypassValve`,
     `FuelIntake`, spec replicated `COND_InitialOnly`; they read `GameState.Fuel` on `OnShipGraphChanged`.
4. **The game mode owns it**: an `FFuelSystem` reset with the cargo model on any section with a fuel layout, reset
   again when the voyage clock starts, ticked in the 4 Hz timer *only while `bVoyageActive`*, after the atmosphere
   tick; while a cask burns it adds `SootPerSecond() * 0.25` to the atmosphere through the M3-5
   `FShipAtmosphere::AddPpm`. Publication: `AAirlockGameState` gains `UPROPERTY(ReplicatedUsing=OnRep_Fuel)
   FFuelState Fuel`, `PublishFuel` (the `PublishCargo` pattern) and `OnRep_Fuel` broadcasting `OnShipGraphChanged`.
   Logs `Fuel <n>% leak=<0|1> bypass=<open|closed>` on flag changes and on crossing `LowFuelPercent` either way,
   `Fuel feed <Id> started` / `Fuel feed <Id> done: +15` / `Injectors fouled` / `Injectors cleared` / `Fuel
   exhausted: docking <n> s late`. API: `FuelOpenLeak()`, `FuelCloseLeak()`, `FuelOpenBypass()`,
   `FuelFeedCask(FName)`, `FuelClearInjectors()`, `FuelSet(float)` (proofs), `FuelSummary()`.
   - **Engine cost**: on each feed the game mode calls `ShipDamageModule(TEXT("Engines"), FoulingOnFeed())`
     (10, or 20 while fouled). Clearing the injectors is a hold (4 s) on the Engines module: `AShipModule` (already
     `IInteractable` since M3-5) returns hold seconds 4 and prompt `Clear the injectors (hold E)` for the module of
     type `Engines` while `GameState.Fuel.bInjectorsFouled`; `OnHoldComplete` -> `FuelClearInjectors()`; it does
     not repair the condition (that stays the M3-2 repair path). Other module types keep their M3-5 behaviour.
   - **Exhaustion**: the tick `bExhausted` becomes true the game mode extends the voyage once:
     `SetVoyageSecondsRemaining(GetVoyageSecondsRemaining() + LateDockSeconds)`, logs `Fuel exhausted: docking
     120 s late`, and `ShipDamageModule(TEXT("Engines"), 0)` is *not* called (the engines are starved, not broken).
     Feeding casks after exhaustion refills fuel normally; the extension is never undone.
5. **The set piece.** The fuel leak is scripted, not drawn: the game mode keeps a small `FVoyageSetPieces` list
   (pure, in `Voyage/VoyageSetPieces.h/.cpp`, tested): `Build(Spec)` returns one entry `{ AtSeconds, Kind =
   FuelLeak, CaptainLine }` with `AtSeconds = Duration * FRandRange(0.40, 0.55)` from `FRandomStream(Seed ^
   0x5E7B1ECE)` (for 1200 s that is minute 8 to 11), and a cause chosen from the same stream for the line:
   `"Captain {0} here. Feed valve's failed open - we're bleeding methane. Engineering, shut it."`,
   `"Captain {0} here. Fuel line's split aft of the tank. Shut the feed, now."`, `"Captain {0} here. The Rig
   short-filled us and now the feed's stuck open. Shut it before we lose the rest."`. The game mode fires it by
   timer like the captain schedule (`ScheduleNextVoyageEvent` pattern), consumed once, `SetVoyageSecondsRemaining`
   moves it like the others (past entries fire once in order; a rewind never replays), and it logs `SetPiece
   FuelLeak at <t>s elapsed=<e>s`. When it fires: `FuelOpenLeak()` and the captain line. Ten seconds later, or as
   soon as the leak is shut if that is sooner, the captain follows up once: `"Captain {0} here. At this burn we
   dock short. Sludge is methane - open the crude bypass and feed me casks."` if `IsShort(remaining)` is true at
   that moment, else `"Captain {0} here. Feed's shut. We're still good for the refinery."`. `FIncidentSchedule` and
   the director are untouched; the set piece is a second, separate list.
6. **Task board.** `BuildRows` gains a fifth parameter `const FFuelState* Fuel = nullptr`. Rows, in this order at the
   top (above the M3-5 air row): `Fuel leak - shut the feed valve` (red) while `bFeedLeak`; `Fuel 18% - open the
   bypass and feed casks` (red below 15 %) while `Fuel/Capacity < LowFuelPercent` and no leak; `Clear the injectors`
   while fouled (after the air row, before heating crates); nothing while a cask burns except the intake's own
   label. Existing rows keep their order; the cap and the nominal text are unchanged. The control board gains a
   header line `FUEL <n>%  BYPASS <closed|open>[  BURNING <Id>][  INJECTORS FOULED]`.
7. **Tally and the hub board.** `FVoyageResult` gains `int32 CratesBurned`, `bool bLateDock`; `Tally` gains
   trailing `const FFuelState* Fuel = nullptr, bool bLateDock = false`: with `Fuel`, `Pay` subtracts `SootFine (40)`
   per cask burned and `LateDockFine (100)` when late (never below 0); a burned cask counts neither intact, vented
   nor loose (`CratesTotal` still counts it). Worked examples: 3 Secured + 1 Burned, hull 90, air 4 ppm, 0
   blackouts, not late -> 300 + 0 + 180 + 150 - 40 = 590; 2 Secured + 1 Burned + 1 Loose, hull 100, air 4 ppm, late
   -> 200 + 200 + 150 - 40 - 100 = 410. `Summary` appends `, <n> burned[, late]` only when a fuel state was given;
   `DescribeResult` appends ` - <n> burned` when `CratesBurned > 0` and ` - late` when late. Existing strings and
   the M3-4 / M3-5 examples are unchanged when the new parameters are omitted.
8. **Console helpers**: `AirlockFuel` (the summary line), `AirlockFuelSet <units>`, `AirlockFuelLeak` (fires the
   set piece now, consuming it if pending), `AirlockBypass` (open it, no reach check), `AirlockFeed <CrateId>`
   (the same server path as the key: the pawn must carry that cask and be in reach of the intake),
   `AirlockClearInjectors` (the hold, by the same path as `AirlockHold Engines`).
9. **Tests** (the existing tests stay green and untouched apart from defaulted parameters):
   - `Tests/FuelSystemTests.cpp`: reset state; nominal burn (80 -> 76 after 60 s); leak rate (40 in 60 s on top);
     `CloseLeak` stops it; bypass opens once; `FeedCask` refused while closed or busy, credits 15 after exactly
     20 s, soot per second during the burn, `CasksBurned` and fouling every second cask, `FoulingOnFeed` 10 then 20;
     `ClearInjectors`; exhaustion latches; `IsShort`; clamp; invalid deltas; determinism; publication only on a
     whole-unit or flag change.
   - `Tests/VoyageSetPieceTests.cpp`: one entry, within 40-55 % over 100 seeds, deterministic, the line names the
     captain, three distinct causes appear across seeds.
   - `Tests/FuelInteractionTests.cpp` (spawned world, `FFuelTestWorld`): actors spawn in engineering; the feed
     valve hold shuts the leak and only while leaking; the bypass press opens once; feeding a carried cask detaches
     it, makes it `Burned` (hidden), starts the burn, damages Engines by 10 then 20 when fouled; the Engines hold
     clears the injectors; exhaustion extends the clock once; a blacked-out pawn is refused; `PublishFuel`
     notifies once per change; the hub has no fuel (`FuelSummary` says `No fuel system on this section`).
   - `Tests/TaskBoardTests.cpp` and `CargoModelTests.cpp` (extend): the three fuel rows and their order, the pay
     examples, `Summary` / `DescribeResult` with and without fuel data.
10. **PIE proof** `Scripts\verify-fuel.ps1` (conventions of `verify-cargo.ps1`, captures under
    `Saved\AgentCaptures\M3-6`, `FUEL OK`): reach the ship; `AirlockFuel` -> `Fuel 80% leak=0 bypass=closed`;
    `AirlockFuelLeak` -> `SetPiece FuelLeak`, the captain's line, `Fuel leak - shut the feed valve` on the board
    (capture from the walkway with the valve's steam); `AirlockFuelSet 30` (so the follow-up says "dock short");
    warp beside the feed valve, `AirlockHold FeedValve` -> `Hold FeedValve complete`, `Fuel .* leak=0`, the
    "open the crude bypass" line; `AirlockBypass` -> `bypass=open` (capture the amber light and the board's
    `Fuel 2\d% - open the bypass` row gone or reworded); warp to the hold, `AirlockCarry C3`, warp beside the
    intake, `AirlockFeed C3` -> `Feed C3 by`, `Fuel feed C3 started`, the intake label counting down (capture),
    `Fuel feed C3 done: +15` after 20 s, `Ship Engines .* cond=90`; `AirlockCarry C1` / `AirlockFeed C1` -> the
    second feed fouls: `Injectors fouled`, `Clear the injectors` on the board; `AirlockHold Engines` ->
    `Injectors cleared`; `AirlockScrub Hazard` and wait under 10 ppm; `AirlockTally` -> `2/4 crates intact`?
    no: C1 and C3 burned, C2 and C4 loose -> `Tally now: 0/4 crates intact, 0 vented, 2 loose, hull 9\d\.\d\d%,
    pay \d+, air \d ppm, 0 blackouts, 2 burned`; `AirlockVoyageClock 1200`, repair anything Degraded
    (`AirlockRepairModule Engines 100`), leave -> `Voyage result: ... 2 burned`; hub board shows ` - 2 burned`
    (capture). The exhaustion path is proven headless (test), not in PIE.

## Do not touch
- `Ship/ShipLayouts.*`, `Ship/ControlStation.*`, `Ship/ShipGraph.*`, `Ship/AirlockDoor.*`, `Ship/CargoLayouts.*`,
  `Voyage/VoyageSchedule.*`, `Voyage/IncidentSchedule.*`, `Voyage/IncidentDirectorComponent.*`, `Voyage/VoyageTypes.h`,
  `Voyage/VoyageSubsystem.*`, `Core/Interactable.h`, `Core/AgentConsoleSubsystem.*`.
- `HandleTransitionRequested`, `ReturnToHub`, `TravelToSection`, `ChoosePlayerStart`; `SetVoyageSecondsRemaining`
  is called, not changed.
- The expectations of every existing test; the seven existing proofs (`verify-loop`, `verify-voyage-clock`,
  `verify-ship-graph`, `verify-incidents`, `verify-level-pass`, `verify-cargo`, `verify-atmosphere`) must exit 0
  on your branch without edits. Note `verify-incidents.ps1` jumps the clock to 1040 s remaining (160 s elapsed):
  the set piece fires somewhere in minute 8-11, i.e. only after a further jump, and `AirlockVoyageClock 70` lands at
  1130 s elapsed - past the set piece, which then fires once (a leak, a captain line, a red board row) *before*
  the climax pair; that proof counts `Incident ...` lines only, so it is unaffected, and `verify-cargo.ps1` never
  jumps forward. If a proof fails on your branch, the model is wrong, not the proof.
- `Content/**`, every map, the pack folders.

## Edge cases (decided)
- Only sections with a fuel layout have a fuel system (today: `Hauler_Mk1`); the hub has none; helpers say so.
- The fuel system ticks only while `bVoyageActive`, after cargo and atmosphere. A ship entered without a booking
  never ticks it.
- A cask fed to the intake is gone: `Burned`, hidden, never counted intact, counted in `CratesBurned`, fined at the
  dock. Feeding a non-volatile cask works the same (the fuel value is the same; the lore says all sludge burns).
- Nothing burns while something burns: the intake refuses a second cask for 20 s (the prompt says so).
- The bypass, once open, stays open; the leak, once shut, stays shut; the set piece fires once per voyage.
- Exhaustion extends the clock exactly once by `LateDockSeconds` and marks the result late; fuel can be refilled
  after it; the engines' condition is not touched by exhaustion.
- A blacked-out pawn (M3-5) can neither open the bypass nor feed the intake; the existing hold refusals cover the
  valve and the injectors.
- `StartFuel` 80 and `NominalBurnPerMinute` 4 leave no margin for the leak: unshut for 60 s the ship is short by
  about 40 units, i.e. three casks. Shut in 20 s it is short by one. That is the intended tension.

## Notes and hints
- M3-5 added `FShipAtmosphere::AddPpm(float)` for exactly this soot; the atmosphere is owned by the game mode next
  to the cargo model, so the fuel tick can reach it directly.
- `SetVoyageSecondsRemaining` (M3-1) clamps to the duration; extending past the original duration is therefore
  done by moving `VoyageEndServerTime` and the schedule the way that function does internally - read it first and
  extend it with a `bAllowBeyondDuration` parameter rather than duplicating it (that one signature change is
  allowed).
- The captain schedule's timer pattern (`ScheduleNextVoyageEvent`, `FindNextUnfiredEntry`) is the model for the set
  piece list; keep the set piece's own cursor.
- Order: model + tests -> layout and actors -> game-mode ownership and publication -> the set piece -> task board
  and control board -> tally and hub board -> console helpers -> the proof. Commit after every step.

## Acceptance (reviewer runs)
`Scripts\build-editor.bat` clean; `Scripts\run-tests.bat Airlock` all green; all seven existing proofs and
`verify-fuel.ps1` exit 0; captures show the leaking valve, the open bypass, the intake counting down, the fouled
row and the hub board's `burned` text.
