# Task M3-6: Fuel, the feed leak and the crude bypass

Owner: GPT-6 Astra (Codex on stack). Reviewer: Claude. Branch: `astra/m3-6-fuel-and-bypass` (from `main`, after
M3-5 has merged). Design source: `Docs/design/refining-run.md` sections 2 (methane is fuel; the bypass, fouling
and soot), 3 (minutes 9-12 "the fuel beat" and 12-16 "dirty burn"), 4 (the soot fine) and 5 (starting numbers).
This brief is the contract; where the two disagree, this brief wins. Design cut made here: the mixture-needle
minigame from the note is deferred to the timing-calls milestone (design note `airlock-voyage-loop-design`, 8.1
lever 3); in this task the bypass burns each cask on its own and the cost is fouling, soot and the fine.
Red-teamed by a second reader on 2026-09-12; 35 findings folded in. Re-check the M3-5 names it relies on against
`Docs/reports/M3-5-hazard-atmosphere.md` before launch.

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
   - `USTRUCT FFuelRates` with defaults, named so tests can quote them (all `float` unless noted): `Capacity 100.0`
     (units), `StartFuel 90.0` (the Rig short-fills: 90 units at the nominal burn is 22.5 min, a 150 s reserve),
     `NominalBurnPerMinute 4.0`, `LeakPerSecond = 40.f / 60.f` (a failed feed valve bleeds 40 units in 60 s),
     `CaskFuel 15.0`, `CaskBurnSeconds 20.0`, `SootPpmPerCask 15.0` (added to the atmosphere evenly over the burn),
     `FoulingPerCask 10.0` and `FoulingPerCaskFouled 20.0` (Engines condition lost per cask fed, the second while
     the injectors are fouled), `int32 CasksPerFouling 2` (the injectors foul when the second, fourth, ... burn
     completes), `LowFuelPercent 30.0`, `CriticalFuelPercent 15.0`, `LateDockSeconds 120.0`.
   - `USTRUCT FFuelState { float Units = 90.f; bool bFeedLeak = false; bool bBypassOpen = false; FName BurningCrate;
     int32 BurnSecondsLeft = 0; int32 CasksBurned = 0; bool bInjectorsFouled = false; bool bExhausted = false; }`
     with `operator==` (`Units` published floored; `BurnSecondsLeft` published as `CeilToInt` of the remaining burn,
     0 when nothing burns). The defaults equal the reset state.
   - `class FFuelSystem`: `void Reset()`; `bool Tick(float DeltaSeconds)` (burns `NominalBurnPerMinute / 60` per
     second plus `LeakPerSecond` while `bFeedLeak`; advances the cask burn, and when it completes credits `CaskFuel`,
     increments `CasksBurned` and sets `bInjectorsFouled` when `CasksBurned % CasksPerFouling == 0`; clamps
     `0..Capacity`; sets `bExhausted` the tick `Units` reaches 0 and never clears it; returns true when the floored
     units, the published burn seconds, a flag, the burning crate or the count changed; invalid deltas are no-ops
     that return false); `void OpenLeak()`; `bool CloseLeak()` (false when there is none); `bool OpenBypass()` (false
     when already open; it never closes again this voyage); `bool FeedCask(FName CrateId)` (refuses `NAME_None`,
     a closed bypass and a burn in progress; starts a `CaskBurnSeconds` burn); `bool ClearInjectors()` (false when
     not fouled); `float SootPerSecond() const` (`SootPpmPerCask / CaskBurnSeconds` while burning, else 0); `float
     FoulingOnFeed() const` (`FoulingPerCaskFouled` while fouled, else `FoulingPerCask` - what the *next* feed costs
     the engines); `float SecondsOfFuelLeft() const` (at the nominal burn, ignoring a leak); `bool IsShort(float
     VoyageSecondsRemaining) const` (`SecondsOfFuelLeft() < VoyageSecondsRemaining`); `const FFuelState& State()
     const`; `FString Summary() const` (one line: `Fuel <n>% leak=<0|1> bypass=<open|closed> burning=<CrateId|none>
     burned=<n> fouled=<0|1> exhausted=<0|1>`).
   - The engine damage is applied by the game mode (item 4) from `FoulingOnFeed()` read before the feed: with the
     defaults the first two casks cost 10 each, the injectors foul when the second burn completes, a third cask fed
     while fouled costs 20. Deterministic; no randomness in the model; tests compare fuel with a 0.01 tolerance.
2. **Cargo side.** `ECargoState` gains `Burned` (appended after `Vented`), terminal and inert exactly like `Vented`:
   `FCargoModel::Tick` skips it (temperature frozen), `PickUp` / `PutDown` / `Place` / `SetTemperature` / `Release`
   refuse it, `StateName` knows it, the crate's prompt is empty, `ServerPickUp` logs `PickUp rejected: burned`, the
   task board's heating filter excludes it, and M3-5's puff qualification (`ShipAtmosphere.cpp`) and the crate's
   puff-FX rule (`CargoCrate.cpp`) exclude it the way they exclude `Vented`, with its puff clock dropped (both files
   are yours to edit for this). `FCargoModel` gains `bool Consume(FName CrateId)` (a `Carried` crate -> `Burned`;
   false otherwise; the caller detaches the actor only after `Consume` succeeded, see item 3). `Tally` counts
   `CratesBurned` (new `UPROPERTY() int32 CratesBurned` on `FVoyageResult`), a Burned cask counts neither intact,
   vented nor loose (`CratesTotal` still counts it), and **the clean-manifest bonus now also requires
   `CratesBurned == 0`** whether or not a fuel state is passed - a burned cask is not a clean manifest. A Burned
   cask's actor is hidden by its `Refresh` (`SetActorHiddenInGame(true)`, collision off, label off); nothing is
   destroyed. Add `Burned` to the M3-4 inert-state test as a second inert state. `FCargoLayouts` is untouched.
3. **Layout and actors** in `Source/Airlock/Ship/FuelLayouts.h/.cpp`: `USTRUCT FFuelFixtureSpec { FName Id;
   FTransform Transform; FSoftObjectPath Mesh; float LabelYaw = 180.f; FVector Scale = FVector(1); }`, `USTRUCT
   FFuelLayout { FName SectionId; FFuelFixtureSpec FeedValve; FFuelFixtureSpec BypassValve; FFuelFixtureSpec
   Intake; }`, `namespace FuelLayouts { const FFuelLayout* Find(FName); }`, `Hauler_Mk1` only, all three in
   engineering (the "CENTRAL ENGINEERING" L-corridor: X -1531..-1106 between the wall faces, Y 230..950, floor Z 22,
   ceiling 480; the west wall is taken by the core band for its whole length, the east side has the coolant tank
   centred (-1192, 420) with a 86 cm radius, `Power_1` on the east wall at (-1110, 450, 100), life support
   wall-mounted at (-1106, 760, 110) with a 49 cm wide box, and the engine node at (-1160, 880); `Coolant_1` runs
   along the west wall at Y 250; the walkway is X -1300). Proposed, to be probed: the feed valve `FeedValve` at
   (-1130, 280, 22) and the bypass valve `BypassValve` at (-1130, 700, 22), both the kit standpipe valve
   `/Game/ModSci_Engineer/Meshes/SM_PipeValve_A.SM_PipeValve_A` (31 x 38 x 478 tall; scale 0.3 -> 143 cm) with the
   wheel facing the walkway; the intake `FuelIntake` at (-1200, 600, 22), the pallet
   `/Game/ModSci_EngiProps/Meshes/SM_PlasticPallet.SM_PlasticPallet` at scale 0.6 (115 cm square: clear of the
   tank, which ends at Y 506, and of the walkway). Check each mesh with `AssetTools.exists` (cube fallback
   otherwise, say so); probe every spot and the four walkway spots below with `AirlockWarp` + `AirlockWhere`
   (`-1300 280 118` for the feed valve, `-1300 700 118` for the bypass valve, `-1300 600 118` for the intake,
   `-1300 880 118` for the engine node - all within the 312 cm reach) and record the final table in your report;
   positions live in `FuelLayouts.cpp`, never in a map.
   - `Ship/FuelValve` - `AFuelValve` (the feed valve): replicated, `IInteractable` *hold* (3 s) with prompt `Shut the
     feed valve (hold E)` while `GameState.Fuel.bFeedLeak`, empty otherwise; `OnHoldComplete` -> the game mode's
     `FuelCloseLeak()`; a red `LeakLight` and the steam FX (the `AShipLine` pattern) while leaking; label `FEED
     VALVE  leak | shut`.
   - `Ship/BypassValve` - `ABypassValve`: replicated, `IInteractable` *press* with prompt `Open the crude bypass`
     while closed, empty once open; `Interact` -> the character's new `UFUNCTION(Server, Reliable)
     ServerOpenBypass(ABypassValve*)` (in reach, not blacked out) -> `FuelOpenBypass()`, logs `Bypass opened by
     <Pawn>` / `Bypass rejected: <reason>`; an amber `BypassLight` on while open; label `BYPASS  closed | OPEN`.
   - `Ship/FuelIntake` - `AFuelIntake`: replicated, `IInteractable` *press* whose (pawn-less, const) prompt is
     `Feed a cask to the engines` while the bypass is open and nothing burns, `Bypass closed` while closed, and empty
     while burning; `Interact` -> the character's new `ServerFeedCask(AFuelIntake*)`. **The carry branch of
     `AAirlockCharacter::Interact` changes** (today it routes every E with a crate in hand to place or put down):
     `ACargoSlot` -> `ServerPlace`, `AFuelIntake` -> `ServerFeedCask`, anything else -> `ServerPutDown`;
     `UCarryPromptWidget::UpdateCarry` now reads `Carrying {0} - E: put down, place in a clamp / the cold rack, or
     feed to the intake`. `ServerFeedCask` validates first (in reach of the intake, not blacked out, carrying, bypass
     open, nothing burning), calls the game mode's `FuelFeedCask(CrateId)` and only on `true` detaches the crate
     (`DetachCarriedCrateAt` at the intake's transform); on `false` it keeps carrying and logs `Feed rejected:
     <reason>`; on success `Feed <Id> by <Pawn>`. Label `INTAKE  <CrateId> <n> s | ready | bypass closed`.
   - All three spawn from the game mode after the cargo actors, folder `Fuel`, names = the fixture ids, spec
     replicated `COND_InitialOnly`; they read `GameState.Fuel` on `OnShipGraphChanged`. The engineering-overview
     capture of `verify-level-pass.ps1` will now show them; that is expected.
4. **The game mode owns it**: an `FFuelSystem` reset with the cargo model on any section with a fuel layout, reset
   again when the voyage clock starts, ticked in the 4 Hz timer *only while `bVoyageActive`*, after the atmosphere
   tick; while a cask burns it calls the M3-5 `FShipAtmosphere::AddPpm(SootPerSecond() * 0.25)` each tick.
   Publication: `AAirlockGameState` gains `UPROPERTY(ReplicatedUsing=OnRep_Fuel) FFuelState Fuel`, `PublishFuel`
   (the `PublishCargo` pattern) and `OnRep_Fuel` broadcasting `OnShipGraphChanged`; every `Fuel*` mutator publishes
   immediately. Logs `Fuel <n>% leak=<0|1> bypass=<open|closed>` on flag changes and on crossing `LowFuelPercent`
   or `CriticalFuelPercent` either way, `Fuel feed <Id> started` / `Fuel feed <Id> done: +15` / `Injectors fouled` /
   `Injectors cleared` / `Fuel exhausted: docking 120 s late`. API: `bool FuelHasSystem() const`
   (`FuelLayouts::Find(section) != nullptr`), `FuelOpenLeak()`, `FuelCloseLeak()`, `FuelOpenBypass()`,
   `FuelFeedCask(FName)` (in order: `FoulingOnFeed()` read, `FeedCask` on the model, then `Consume` on the cargo
   model, then `ShipDamageModule(TEXT("Engines"), thatFouling)`; all-or-nothing - if `FeedCask` or `Consume` refuses
   nothing else happens), `FuelClearInjectors()`, `FuelSet(float)` (proofs), `FuelSummary()` (`No fuel system on
   this section` on the hub).
   - **Clearing the injectors** is a hold (4 s) on the Engines module: `AShipModule` (an `IInteractable` since M3-5)
     returns hold seconds 4 and prompt `Clear the injectors (hold E)` for the module of type `Engines` while
     `GameState.Fuel.bInjectorsFouled` (0 and an empty prompt otherwise); `OnHoldComplete` -> `FuelClearInjectors()`;
     it does not repair the condition (that stays the M3-2 repair path). This supersedes M3-5's two sentences
     ("`GetInteractHoldSeconds` stays 0", "every other module type ... ignores `Interact`") for the Engines module
     only; M3-5's tests assert them on unfouled modules, which stays true.
   - **Exhaustion**: the tick `bExhausted` becomes true the game mode extends the voyage once:
     `SetVoyageSecondsRemaining(GS->GetVoyageSecondsRemaining() + LateDockSeconds)` (the game state holds the
     remaining time; the clamp to the duration only bites before 120 s elapsed, unreachable in play), logs `Fuel
     exhausted: docking 120 s late`, and speaks `"Captain {0} here. We're dry. Coasting in on momentum - we dock
     late."` The engines' condition is not touched. Feeding casks afterwards refills fuel normally; the extension
     is never undone. Consequences to accept: already-fired captain entries do not replay (cursor), the cargo and
     atmosphere elapsed time (`Duration - remaining`) steps back 120 s.
5. **The set piece.** The fuel leak is scripted, not drawn. `Voyage/VoyageSetPieces.h/.cpp` (pure, tested):
   `enum class ESetPieceKind : uint8 { FuelLeak }`, `struct FVoyageSetPiece { float AtSeconds; ESetPieceKind Kind;
   FText CaptainLine; }`, `struct FVoyageSetPieces { static TArray<FVoyageSetPiece> Build(const FVoyageSpec&); }`
   returning one entry with `AtSeconds = Duration * FRandRange(0.40, 0.55)` (for 1200 s: minute 8 to 11) drawn from
   `FRandomStream(Seed ^ 0x5E7B1ECE)` - time first, then the cause index - with the line chosen by cause:
   `"Captain {0} here. Feed valve's failed open - we're bleeding methane. Engineering, shut it."`,
   `"Captain {0} here. Fuel line's split aft of the tank. Shut the feed, now."`, `"Captain {0} here. The Rig
   short-filled us and now the feed's stuck open. Shut it before we lose the rest."`. The game mode builds the list
   when the voyage clock starts (the hub has none) and folds its cursor into the existing timer:
   `ScheduleNextVoyageEvent` waits for the sooner of the next captain entry and the next unfired set piece;
   `TickVoyageClock` fires due captain entries first, then due set pieces (so the leak line is the caption left on
   screen), each consumed once; `SetVoyageSecondsRemaining` already reschedules through those two, so it is *not*
   changed: past entries fire once in order on a forward jump, a rewind never replays; a jump straight to docking
   fires it before the docking caption and the return; a voyage that ends before it fires never fires it. When it
   fires: `FuelOpenLeak()`, the captain line, and the log `SetPiece FuelLeak at <t>s elapsed=<e>s`. A **follow-up**
   timer (real time) fires 10 s later or the moment the leak is shut, whichever is sooner, once, only while
   `bVoyageActive` and no travel is pending: if `IsShort(remaining)` -> `"Captain {0} here. At this burn we dock
   short. Sludge is methane - open the crude bypass and feed me casks."`; else if the leak is shut -> `"Captain {0}
   here. Feed's shut. We're still good for the refinery."`; else -> `"Captain {0} here. Shut that feed. We're still
   good for the refinery - for now."`. `FIncidentSchedule` and the director are untouched; in the frame where a
   forward jump lands on both the set piece and the climax pair their order is unspecified.
6. **Task board.** `BuildRows` gains a fifth parameter `const FFuelState* Fuel = nullptr`; `RefreshRows` passes it
   only on sections with a fuel system. Rows, in this order at the top (above the M3-5 air row): `Fuel leak - shut
   the feed valve` (red) while `bFeedLeak`; otherwise, while `Units < Capacity * LowFuelPercent / 100`: `Fuel 18% -
   open the bypass and feed casks` while the bypass is closed, `Fuel 18% - feed casks to the intake` while it is
   open and nothing burns, and no fuel row while a cask burns (the intake label carries the countdown) - red while
   `Units < Capacity * CriticalFuelPercent / 100`; `Clear the injectors` while fouled (after the air row, before the
   heating crates). Existing rows keep their order; the cap and the nominal text are unchanged. The control board
   gains a header line `FUEL <n>%  BYPASS <closed|open>[  BURNING <Id> <n> s][  INJECTORS FOULED]`.
7. **Tally and the hub board.** `FVoyageResult` gains `UPROPERTY() int32 CratesBurned = 0` and `UPROPERTY() bool
   bLateDock = false`; `FCargoModel` gains `static constexpr int32 SootFine = 40` and `LateDockFine = 100`; `Tally`
   gains one trailing parameter `const FFuelState* Fuel = nullptr`: with it, `bLateDock = Fuel->bExhausted`, `Pay`
   subtracts `SootFine` per burned cask and `LateDockFine` when late (never below 0). The burned count and the
   manifest rule of item 2 apply with or without `Fuel`. Worked examples the tests must reproduce (intact 100 each;
   manifest bonus 150 only with nothing vented, loose or burned; hull = round(mean condition x 2); M3-5 clean air 150
   under 10 ppm; blackouts 50 each): 3 Secured + 1 Burned, hull 90, air 4 ppm, 0 blackouts, not late -> 300 + 0
   (one burned voids the manifest) + 180 + 150 - 40 = 590, and the same manifest with `Fuel` omitted -> 630 (no fine,
   the bonus still voided); 2 Secured + 1 Burned + 1 Loose, hull 100, air 4 ppm, 0 blackouts, late -> 200 + 0 + 200
   + 150 - 40 - 100 = 410. `Summary` appends `, <n> burned[, late]` only when `Fuel` was given (after M3-5's air
   fields); `DescribeResult` appends ` - <n> burned` when `CratesBurned > 0` and ` - late` when late, after the air
   text: `Last run: 0/4 crates intact - hull 100% - pay 270 cr - air clean - 2 burned`. Existing strings and the
   M3-4 / M3-5 examples are unchanged when the new parameter is omitted.
8. **Console helpers**: `AirlockFuel` (the summary line), `AirlockFuelSet <units>`, `AirlockFuelLeak` (fires the set
   piece now if it is still pending - consuming it - else just `FuelOpenLeak()` with the log `SetPiece FuelLeak
   (console)` and no captain line), `AirlockBypass` (open it, no reach check, logs like the key), `AirlockFeed
   <CrateId>` (the same server path as the key: the pawn must carry that cask - `Feed rejected: carrying <Other>`
   otherwise - and be in reach of the intake). Clearing the injectors uses the existing `AirlockHold Engines`.
9. **Tests** (the existing tests stay green and untouched apart from defaulted parameters):
   - `Tests/FuelSystemTests.cpp`: reset state; nominal burn (90 -> 86 after 60 s, tolerance 0.01); leak rate (40
     more in 60 s); `CloseLeak` stops it; bypass opens once; `FeedCask` refused while closed, busy or `NAME_None`,
     credits 15 after exactly 20 s, `SootPerSecond` during the burn only, `CasksBurned` and fouling on the second
     completed burn, `FoulingOnFeed` 10, 10, then 20 while fouled; `ClearInjectors`; exhaustion latches; `IsShort`;
     clamp; invalid deltas; determinism; publication only on a whole-unit, whole-second, flag, crate or count change.
   - `Tests/VoyageSetPieceTests.cpp`: one entry, within 40-55 % over 100 seeds, deterministic, the line names the
     captain, all three causes appear across seeds.
   - `Tests/FuelInteractionTests.cpp` (spawned world; `FFuelTestWorld` copies `FIncidentTestWorld` from
     `IncidentDirectorTests.cpp`, which books a voyage so the clock, director, atmosphere and fuel all start): the
     three fixtures spawn in engineering; the feed valve hold shuts the leak and only while leaking; the bypass press
     opens once; feeding a carried cask detaches it, makes it `Burned` (hidden), starts the burn, damages Engines by
     10, a second cask by 10 and fouls on completion, a third by 20; a refused feed keeps the pawn carrying; the
     Engines hold clears the injectors; exhaustion: `SetVoyageSecondsRemaining(600)`, `FuelSet(0)`, one timer tick
     -> remaining within 1 s of 720, `bExhausted`, and no second extension after a refill and a second `FuelSet(0)`;
     a blacked-out pawn is refused; `PublishFuel` notifies once per change; the hub has no fuel system.
   - `Tests/TaskBoardTests.cpp` and `CargoModelTests.cpp` (extend): the fuel rows and their order and wording, the
     three pay examples, the manifest rule with a burned cask and no fuel state, `Summary` / `DescribeResult` with
     and without fuel data, `Burned` as a second inert state.
10. **PIE proof** `Scripts\verify-fuel.ps1` (conventions of `verify-cargo.ps1`, captures under
    `Saved\AgentCaptures\M3-6`, `FUEL OK`; HUD text is capture-only, every wait is on a log line): reach the ship;
    on `Incident director started` send `AirlockVoyageClock 1200`; `AirlockFuel` -> `Fuel (89|90)% leak=0
    bypass=closed`; `AirlockFuelLeak` -> `SetPiece FuelLeak`, the captain's feed-valve line, then within 12 s the
    follow-up `open the crude bypass` line (90 units is 22.5 min; any leak makes the run short within seconds);
    warp to `-1300 280 118`, `AirlockLook 0`, capture `feed-leak` (the valve's steam and red light, the red board row);
    `AirlockHold FeedValve` -> `Hold FeedValve complete`, `Fuel .* leak=0`; `AirlockFuelSet 25` -> `Fuel 25%` (the
    low-fuel row `Fuel 2\d% - open the bypass and feed casks`); warp to `-1300 700 118`, `AirlockBypass` -> `Bypass
    opened by`, `bypass=open` (capture `bypass-open`: the amber light, the row now `feed casks to the intake`);
    `AirlockVoyageClock 1200` again (the loose casks must not reach the calm boundary during the feeds); warp to
    the hold, `AirlockCarry C3`, warp to `-1300 600 118`, `AirlockFeed C3` -> `Feed C3 by`, `Fuel feed C3 started`
    (capture `intake-burning`: the intake label counting down), `Fuel feed C3 done: +15` after 20 s;
    `AirlockShipStatus` -> `Ship Engines Nominal cond=90\.00`; `AirlockCarry C1` from the hold, `AirlockFeed C1` ->
    after 20 s `Fuel feed C1 done: +15` and `Injectors fouled`; `AirlockShipStatus` -> `Ship Engines Nominal
    cond=80\.00`; warp to `-1300 880 118`, `AirlockHold Engines` -> `Hold Engines complete`, `Injectors cleared`;
    `AirlockScrub Hazard` and wait for `AirlockAir` under 10 ppm; `AirlockTally` -> `Tally now: 0/4 crates intact, 0
    vented, 2 loose, hull 97\.14%, pay 264, air \d ppm, 0 blackouts, 2 burned` (0 + 0 + 194 + 150 - 80);
    `AirlockRepairModule Engines 100`, `AirlockVoyageClock 1200`, `AirlockShipStatus` -> seven Nominal at 100; leave
    -> `Voyage result: 0/4 crates intact, 0 vented, 2 loose, hull 100\.00%, pay 270, air \d ppm, 0 blackouts, 2
    burned`; on the hub `AirlockFuel` -> `No fuel system on this section`, `AirlockBoard`, capture `hub-last-run-fuel`
    (`Last run: 0/4 crates intact - hull 100% - pay 270 cr - air clean - 2 burned`). The exhaustion path is proven
    headless (test), not in PIE.

## Do not touch
- `Ship/ShipLayouts.*`, `Ship/ControlStation.*`, `Ship/ShipGraph.*`, `Ship/AirlockDoor.*`, `Ship/CargoLayouts.*`,
  `Voyage/VoyageSchedule.*`, `Voyage/IncidentSchedule.*`, `Voyage/IncidentDirectorComponent.*`, `Voyage/VoyageTypes.h`,
  `Voyage/VoyageSubsystem.*`, `Core/Interactable.h`, `Core/AgentConsoleSubsystem.*`.
- `HandleTransitionRequested`, `ReturnToHub`, `TravelToSection`, `ChoosePlayerStart`, `SetVoyageSecondsRemaining`
  (called, never changed).
- The expectations of every existing test; the seven existing proofs (`verify-loop`, `verify-voyage-clock`,
  `verify-ship-graph`, `verify-incidents`, `verify-level-pass`, `verify-cargo`, `verify-atmosphere`) must exit 0
  on your branch without edits. Worked out: `verify-incidents.ps1` jumps to 1040 s remaining (160 s elapsed, before
  the set piece's 480-660 s window) and later to 70 s remaining, which fires the set piece in the same frame as
  the climax pair (a leak, one captain line, a red row; the follow-up 10 s later); its counts anchor on `Incident
  (...)` lines and `Incident schedule: 4 entries`, which `SetPiece` lines do not match, and it leaves through the
  door long before exhaustion. `verify-voyage-clock.ps1` jumps to 75 s remaining: the set piece and the mid-voyage
  line fire together, the follow-up at +10 s, `One minute to dock` at +15 s; its waits are pattern-specific and it
  docks with ~30 units left. `verify-cargo.ps1`, `verify-atmosphere.ps1`, `verify-level-pass.ps1` (its
  engineering-overview capture now shows the three fixtures), `verify-ship-graph.ps1`, `verify-loop.ps1`: no forward
  jumps, no fuel-sensitive assertions, no end anchors on the tally regexes. If a proof fails on your branch, the
  model is wrong, not the proof.
- `Content/**`, every map, the pack folders.

## Edge cases (decided)
- Only sections with a fuel layout have a fuel system (today: `Hauler_Mk1`); the hub has none; `BuildRows` gets
  `nullptr` there and the helpers say so.
- The fuel system ticks only while `bVoyageActive`, after cargo and atmosphere. A ship entered without a booking
  never ticks it.
- A cask fed to the intake is gone: `Burned`, hidden, never counted intact, counted in `CratesBurned`, voids the
  manifest bonus, fined at the dock. Feeding a non-volatile cask works the same (all sludge burns).
- Nothing burns while something burns: the intake refuses a second cask for 20 s (the prompt and label say so).
- The bypass, once open, stays open; the set piece fires once per voyage; the console `AirlockFuelLeak` can reopen
  the leak afterwards for proofs.
- Exhaustion extends the clock exactly once by `LateDockSeconds` and marks the result late; fuel can be refilled
  after it; the engines' condition is untouched by exhaustion.
- A blacked-out pawn (M3-5) can neither open the bypass nor feed the intake; the existing hold refusals cover the
  valve and the injectors.
- `StartFuel` 90 at 4 per minute is 22.5 min against a 20 min run: a leak left open for 60 s (40 units) leaves the
  ship about 30 units short - two casks; shut in 20 s it is about 3 short - one cask. That is the intended tension,
  and a clean run without a leak never comes near exhaustion (float accumulation included).

## Notes and hints
- M3-5's `FShipAtmosphere::AddPpm(float)` is the soot hook; the atmosphere is owned by the game mode next to the
  cargo model, so the fuel tick reaches it directly. Check its exact semantics in the M3-5 report.
- `TryClampRemainingSeconds` clamps the *remaining* time to `[0, Duration]`, so `remaining + 120` is only clamped
  before 120 s elapsed; `ScheduleNextVoyageEvent` / `TickVoyageClock` cope with a later end time (elapsed simply
  shrinks). Read them before folding the set-piece cursor in.
- `FIncidentTestWorld` (`Tests/IncidentDirectorTests.cpp`) books a voyage before `BeginPlay`; `FCargoTestWorld` does
  not (no clock, no director) - the fuel tests need the former.
- `PublishShipGraph` logs `Ship <Id> <Status> cond=` only on a status change; `AirlockShipStatus` prints every
  module in the `ShipSummary` format (`Ship Engines Nominal cond=90.00`).
- Order: model + tests -> layout and actors -> game-mode ownership and publication -> the set piece -> task board
  and control board -> tally and hub board -> console helpers -> the proof. Commit after every step.

## Acceptance (reviewer runs)
`Scripts\build-editor.bat` clean; `Scripts\run-tests.bat Airlock` all green; all seven existing proofs and
`verify-fuel.ps1` exit 0; captures show the leaking valve, the open bypass, the intake counting down, the fouled
row and the hub board's `burned` text.
