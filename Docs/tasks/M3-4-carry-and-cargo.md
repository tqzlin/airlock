# Task M3-4: Carry verb, volatile cargo and the delivery tally

Owner: GPT-6 Astra (Codex on stack). Reviewer: Claude. Branch: `astra/m3-4-carry-and-cargo` (from `main`).
Design source: project doc "Airlock voyage loop design", sections 3 (*Cargo Hold*, verb 2 *Carry*, *Outcome and
payoff*), 6 (the minute-by-minute run: 0-3 secure the cargo, 10-13 cargo shift, 16-19 climax, 19-20 dock) and 8.1
lever 5 (pay scales with the manifest). This brief is the contract; where the two disagree, this brief wins.
Red-teamed by a second model on 2026-09-12 before launch; its findings are folded in below.

**Parallel work (read this).** While you work, Claude is doing the level pass on `claude/level-pass` in a separate
worktree (`..\Airlock-claude`, its own editor on MCP port 8001): moving the module and line positions in
`Ship/ShipLayouts.cpp` into real rooms, dressing `Hauler_Mk1`, and adding an `AControlStation` on the bridge. So this
task must not edit `Ship/ShipLayouts.*`, any map, or anything under `Content/`. Cargo positions live in a new
`Ship/CargoLayouts.*`. Your editor is the one on port 8000 on this checkout, exactly as before. Never close editors
by process name: `Scripts\close-editor.ps1` closes only this checkout's editor (HANDOFF sections 2 and 7).

## Prompt (what the reviewer sends to Codex)

```
You are the developer on Airlock, a UE 5.8 C++ spaceship MMO MVP. Work in this repo on a new branch
astra/m3-4-carry-and-cargo (create it from main). Read AGENTS.md, Docs/HANDOFF.md (sections 2, 3 and 7 changed
today), Docs/tasks/M3-3-incident-director.md and Docs/reports/M3-3-incident-director.md (your own M3-3 work,
merged), then Docs/tasks/M3-4-carry-and-cargo.md, then `git log --oneline -30`. State the currently verified
loop and what the incident director already does in your own words before you start editing. Implement
Docs/tasks/M3-4-carry-and-cargo.md exactly as specified there.
```

## Why this task

M3-3 made the ship break on a curve and gave the crew a board of open tasks. Every task is still "walk to a pipe and
hold E". This task adds the second verb, **Carry**, and the thing worth carrying: cargo. Crates start loose in the
hold; the calm minutes are for clamping them; a cargo shift (always one of the two climax incidents, sometimes an
earlier one too) shakes a clamped crate free; a volatile crate that stays loose heats up and vents; the cold rack is
the safe place. At docking the ship counts what arrived intact and pays. After it, a run has a score and the hub
board tells you how your last run went - the Deep Rock hook, one task early.

## Definition of done

1. **Pure cargo model** in `Source/Airlock/Ship/CargoModel.h/.cpp` (plain structs and a class, no UObject state):
   - `UENUM() enum class ECargoState : uint8 { Loose, Carried, Secured, Racked, Vented }`.
   - `USTRUCT FCargoCrateState { FName Id; bool bVolatile; ECargoState State; float Temperature; FName SlotId; }`
     with `operator==` (`Temperature` 0..100; `SlotId` = the clamp or rack the crate sits in, `None` otherwise).
   - `USTRUCT FCargoSlotState { FName Id; bool bRack; FName CrateId; }` (`CrateId` `None` when empty).
   - `USTRUCT FCargoRates` with defaults, all per second, all named so tests can quote them: `LooseHeat 1.0`
     (a volatile crate that is Loose, once `VoyageElapsedSeconds >= FIncidentSchedule::CalmSeconds`, i.e. 150 s
     of voyage time, for every voyage length), `CarriedHeat 0.5` (volatile and Carried, same calm rule),
     `RackCool 4.0` (any crate that is Racked, at any time), `WarnTemperature 40`, `VentTemperature 100`.
     Secured crates neither heat nor cool. Non-volatile crates never heat.
   - `class FCargoModel`: `void Build(const FCargoLayout&)`, `bool Tick(float DeltaSeconds, double VoyageElapsedSeconds)`
     (applies the rates, clamps 0..100, turns a crate `Vented` the moment `Temperature >= VentTemperature`; returns
     true when any state changed or any crate's `FloorToInt(Temperature)` changed; `DeltaSeconds <= 0` or
     non-finite is a no-op that returns false), `bool PickUp(FName CrateId)` (Loose / Secured / Racked -> Carried,
     frees the slot), `bool PutDown(FName CrateId)` (Carried -> Loose), `bool Place(FName CrateId, FName SlotId)`
     (Carried -> Secured in a clamp / Racked in a rack; fails when the slot is unknown or occupied),
     `bool Release(FName SlotId)` (the cargo shift: the Secured crate in that clamp -> Loose; false when the slot is
     empty or a rack; unrelated to the hold verb's `AirlockRelease`), `bool SetTemperature(FName CrateId, float
     Temperature)` (proofs: volatile, not Vented, finite, 0..100 inclusive; a value of 100 vents on the next
     `Tick`), `const FCargoCrateState* FindCrate(FName) const`, `const TArray<FCargoCrateState>& Crates() const`,
     `const TArray<FCargoSlotState>& Slots() const`, `FString Summary() const` (one line per crate and slot).
     A Vented crate is inert: every operation on it returns false. Deterministic: two models built from the same
     layout and given the same calls and ticks are equal.
   - **Tally** in the same file: `USTRUCT FVoyageResult { EVoyageType Type; int32 Seed; int32 CratesTotal;
     int32 CratesIntact; int32 CratesVented; int32 CratesLoose; float ShipCondition; int32 Pay; }` and
     `static FVoyageResult FCargoModel::Tally(const FCargoModel& Cargo, const FShipGraph& Ship, const FVoyageSpec& Spec)`:
     intact = Secured or Racked (never Vented); vented = Vented; loose = Loose or Carried (unsecured at docking is
     lost); `ShipCondition` = mean module condition (0..100, unrounded); `Pay = PayPerIntactCrate (100) * intact +
     (vented == 0 && loose == 0 ? CleanManifestBonus (150) : 0) + RoundToInt(ShipCondition * HullBonusPerPoint (2.0))`.
     Worked examples the tests must reproduce: 3 Secured + 1 Racked, hull 100 -> 750; 3 intact + 1 vented, hull 50
     -> 400; 2 Secured + 2 Loose, hull 100 -> 400. `Tally` records nothing; recording is the game mode's job (7).
2. **Cargo layout** in `Source/Airlock/Ship/CargoLayouts.h/.cpp`, the same shape as `ShipLayouts`:
   `USTRUCT FCargoSlotSpec { FName Id; bool bRack; FTransform Transform; FSoftObjectPath Mesh; }`,
   `USTRUCT FCargoCrateSpec { FName Id; bool bVolatile; FTransform Transform; FSoftObjectPath Mesh; }` (the spawn
   spot, Loose), `USTRUCT FCargoLayout { FName SectionId; TArray<FCargoSlotSpec> Slots; TArray<FCargoCrateSpec> Crates; }`,
   `namespace CargoLayouts { const FCargoLayout* Find(FName SectionId); }`. One layout, `Hauler_Mk1`, the solo
   manifest of design 8.1: three clamps, one cold rack, four crates, two of them volatile. Placeholder positions in
   the west hall (walking surface Z -65, the future cargo hold), off the Y 1560 walkway and off the Y 1900 module
   row and the Y 1220 line row that the level pass is about to vacate:

   | Id | Kind | Location | Mesh (soft object path, `Package.Name` form) |
   |---|---|---|---|
   | Clamp_1 | clamp | (-6100, 1400, -65) | `/Game/ModSci_EngiProps/Meshes/SM_PlasticPallet.SM_PlasticPallet` |
   | Clamp_2 | clamp | (-5700, 1400, -65) | same |
   | Clamp_3 | clamp | (-5300, 1400, -65) | same |
   | ColdRack | rack | (-4900, 1400, -65) yaw 90 | `/Game/StarterBundle/ModularScifiProps/Meshes/SM_Shelf_A_v1.SM_Shelf_A_v1` |
   | C1 | crate | (-6000, 1700, -65) | `/Game/ModSci_EngiProps/Meshes/SM_Case_A.SM_Case_A` |
   | C2 | volatile crate | (-5600, 1700, -65) | same |
   | C3 | crate | (-5200, 1700, -65) | same |
   | C4 | volatile crate | (-4800, 1700, -65) | same |

   Check each mesh with `AssetTools.exists` and measure `SM_Case_A` (`StaticMeshTools.get_bounds`); if it is not
   roughly crate-sized (50..130 cm), use the cube fallback (`Ship/ShipVisuals.h`) scaled 0.8 x 0.8 x 0.8 and say so.
   Probe every position with `AirlockWarp x y 300` + `AirlockWhere` (HANDOFF section 3; the pattern is
   `C:\Users\tclin\Desktop\ai_game_studio\agent\done\167-probe-module-spots.ps1`) and move any spot that is not on
   the -65 floor or that blocks the walkway; the table in your report is the source of truth the reviewer will
   carry into the level pass.
3. **Actors**, spawned by the game mode right after the ship actors on any section with a cargo layout (server,
   replicated, outliner folder `Cargo`), whether or not a voyage is active (they are the scenery; only `Tick` needs
   the voyage). Only the character's server RPCs (4) ever attach, detach or teleport a crate; the game-mode Cargo
   API changes the model only; a crate released by a cargo shift stays where it is; the actors' refresh touches
   only label, light and FX and runs on clients too.
   - `Ship/CargoCrate` - `ACargoCrate`: root scene, `Body` mesh from the spec (cube fallback), `Label` text render
     `"<Id>  <State>"` plus `"  <temp>C"` (whole degrees, floored) when volatile, facing -X like the modules,
     `HeatLight` point light that is off below `WarnTemperature` and ramps to full red at `VentTemperature`,
     `VentFX` (`PS_SteamDoor`, restarted like `AShipLine::LeakFX`) active exactly while Vented. Implements
     `IInteractable` as a *press*: prompt `"Pick up <Id>"` while Loose / Secured / Racked, empty while Carried or
     Vented; `Interact` -> the character's `ServerPickUp`. `SetReplicateMovement(true)`. Collision `QueryOnly`,
     blocks Visibility only - crates never block pawns (no physics in the MVP, and a loose crate must never trap a
     player).
   - `Ship/CargoSlot` - `ACargoSlot`: root scene, `Pad` mesh from the spec (cube fallback scaled 1 x 1 x 0.1),
     `Label` `"<Id>  empty"` / `"<Id>  <CrateId>"`, `IInteractable` press: prompt `"Place crate in <Id>"` while the
     slot is empty, empty otherwise (`GetInteractPrompt` is const and pawn-less; `IInteractable` stays unchanged);
     `Interact` -> `ServerPlace`. A crate placed in a slot is teleported (by the RPC) to the slot's transform plus
     `CrateRestOffset` (a `UPROPERTY`, default Z +10). Collision as for crates.
   - Both refresh from `GameState.Cargo` on `OnShipGraphChanged` (see 5), never from the game mode.
4. **The Carry verb** on `AAirlockCharacter` (keep press-E and hold-E working exactly as they are):
   - `UPROPERTY(ReplicatedUsing=OnRep_CarriedCrate) TObjectPtr<ACargoCrate> CarriedCrate`; a `CarrySocket`
     `USceneComponent` created with `CreateDefaultSubobject` under the capsule (forward 80 cm, Z -20, a `UPROPERTY`
     offset) so clients resolve the attach parent; `UFUNCTION(Server, Reliable)` `ServerPickUp(ACargoCrate*)`,
     `ServerPutDown()`, `ServerPlace(ACargoSlot*)`.
   - Server rules: pick-up is refused with a log line when the pawn already carries (`PickUp rejected: hands full`),
     the crate is farther than `InteractReach * 1.25` (reuse `IsInHoldRange`; `out of range`), Vented (`vented`) or
     carried by anyone (`already carried`); on success the model is updated through the game mode, the crate is
     attached to `CarrySocket` (`AttachToComponent`; attachment replicates), and `Carry <Id> by <Pawn>` is logged.
     Put-down detaches and drops the crate at the pawn's location + 120 cm forward, snapped to the floor by a
     downward trace of 300 cm that ignores the crate itself (the pawn's feet if it misses); logs `PutDown <Id> by
     <Pawn>`. Place needs the slot within reach and empty; logs `Place <Id> in <Slot> by <Pawn>`. A pawn whose
     `EndPlay` reason is `Destroyed` puts its crate down first (level travel and quit leave it alone).
   - A volatile crate that reaches `VentTemperature` while Carried is detached by the server at the put-down spot,
     the carrier's `CarriedCrate` is cleared (speed restored) and `Cargo <Id> Vented` is logged; nobody is ever
     left holding a vented crate.
   - While carrying: walk speed is `BaseWalkSpeed * CarrySpeedFactor` (0.6, a `UPROPERTY`; cache the base speed once
     and *assign*, never multiply in place - RepNotifies can run more than once), applied on the server and in
     `OnRep_CarriedCrate` on the client (walk speed is not replicated; both sides must agree or the client gets
     corrected); restored on put-down/place/vent. `ServerBeginHold` is refused with `Hold rejected: hands full`.
     The E key: the interact trace and the put-down trace both `AddIgnoredActor(CarriedCrate)` (the crate hangs
     right in front of the camera); with a slot in the trace -> `ServerPlace`, otherwise -> `ServerPutDown` (any
     other interactable is ignored while carrying). `UI/CarryPromptWidget` (C++-built like `HoldProgressWidget`,
     bottom centre): while the local pawn carries, `"Carrying <Id> - E: put down, or place in a clamp / the cold rack"`.
5. **Game state and task board.** `AAirlockGameState` gains `UPROPERTY(ReplicatedUsing=OnRep_Cargo)
   TArray<FCargoCrateState> Cargo` whose `OnRep_Cargo` broadcasts the existing `OnShipGraphChanged` (no new
   delegate: widgets and actors keep one binding), `void PublishCargo(const TArray<FCargoCrateState>&)` mirroring
   `PublishShipGraph` (compare, assign only when different, `ForceNetUpdate`, then call `OnRep_Cargo()` itself -
   RepNotifies never fire on the authority, and PIE is the authority), plus `UPROPERTY(Replicated) FVoyageResult
   LastVoyageResult` and `UPROPERTY(Replicated) bool bHasLastVoyageResult` (see 7). `AAirlockGameMode` owns the
   `FCargoModel`, builds it with the graph, ticks it in the same 4 Hz timer *only while `bVoyageActive`* (passing
   the voyage's elapsed seconds), publishes through `PublishCargo` when `Tick` reported a change or an operation
   succeeded (temperatures published as whole degrees, floored), and logs `Cargo <Id> <State> temp=<n>` on every
   state change and when a crate crosses `WarnTemperature`. API for the character, the director and the console:
   `bool CargoPickUp(FName)`, `bool CargoPutDown(FName)`, `bool CargoPlace(FName Crate, FName Slot)`,
   `bool CargoRelease(FName Slot)`, `bool CargoSetTemperature(FName, float)`, `FName CargoFirstFilledClamp() const`,
   `FVoyageResult CargoTally() const` (records nothing), `FString CargoSummary() const`.
   `UTaskBoardWidget::BuildRows` gains a third parameter `const TArray<FCargoCrateState>& Cargo = {}` (the M3-3
   tests keep calling it with two). One row per crate at most, heating wins: heating crates first
   (`"Cool crate C2 - 71C"`: volatile, `Temperature >= WarnTemperature`, not Racked, not Vented; sorted hottest
   first; red at 80 or more), then Failed modules, faulted lines, Degraded modules as today, then loose crates
   last (`"Secure crate C3"`: Loose, not Vented, below `WarnTemperature` or non-volatile). Vented crates and
   Carried crates below `WarnTemperature` get no row. `All systems nominal` only when there is no row at all; the
   8-row cap and `+n more` stay.
6. **The cargo-shift incident.** `EIncidentType` gains `CargoShift` (appended after `ModuleDamage`).
   `FIncidentSchedule::Build` and `Targets` gain a trailing `const FCargoLayout* Cargo = nullptr`; with `nullptr`
   the output is exactly what it is today (the M3-3 tests keep passing untouched; the random stream is not
   consumed differently). With a cargo layout that has at least one clamp, decided rules (named constants):
   - the incident count is unchanged (a CargoShift replaces a type draw, it is never added);
   - **exactly one of the two climax entries is a CargoShift**: one extra draw, taken only on the cargo path,
     picks which of the two; the other climax entry keeps the normal type draw with CargoShift excluded (design 6:
     "a second leak and a second loose crate");
   - a non-climax CargoShift may be drawn only for entries at or after `CargoShiftMinFraction = 0.4` of the
     duration and only when the layout has at least two clamps (so the no-repeat rule can always be met); the
     first incident stays the teaching CoolantLeak; a mid-voyage shift is therefore possible, not guaranteed;
   - targets are clamp ids (never the rack); the no-consecutive-repeat rule is by `TargetId`, as in M3-3.
   Captain line: `"Captain {0} here. Hold on - cargo's shifted in the hold, {1} let go."`.
   `UIncidentDirectorComponent::StartVoyage(Spec, Layout, CrewCount, const FCargoLayout* Cargo = nullptr)`;
   the game mode passes `CargoLayouts::Find(section)`. `ApplyIncident` for a CargoShift calls `CargoRelease(Target)`
   and logs the usual `Incident CargoShift Clamp_2 (0.00) at <t>s`, or `... skipped: clamp empty` when nothing is
   secured there. `AirlockIncident CargoShift [ClampId]` works through the existing helper (add `CargoShift` to
   its type parser); an omitted target is `CargoFirstFilledClamp()` (skipped when there is none).
   `Scripts\verify-incidents.ps1` counts the climax catch-up with the regex `Incident (CoolantLeak|PowerShort|ModuleDamage) `
   (twice, lines 124 and 138); change both to `Incident (CoolantLeak|PowerShort|ModuleDamage|CargoShift) ` so
   skipped cargo shifts count. That regex is the only change allowed in that script; its `tasks-nominal` capture will now show the four
   `Secure crate` rows instead of `All systems nominal`, which is correct.
7. **Tally and the hub board.** `AAirlockGameMode::RecordVoyageResult()` returns early unless this section started a
   voyage clock; otherwise it computes `FCargoModel::Tally`, stores it on `UVoyageSubsystem` (`SetLastVoyageResult`
   / `bool GetLastVoyageResult(FVoyageResult&) const`; GameInstance, so it survives the travel) and logs
   `Voyage result: <intact>/<total> crates intact, <vented> vented, <loose> loose, hull <cond>%, pay <n>` with the
   hull as `%.2f`. It is called exactly once per voyage, immediately before each existing `ClearPendingVoyage()`
   call in `HandleTransitionRequested` (the hub-bound door) and in `ReturnToHub()` - that one-line insertion is the
   only change allowed in those functions. On the hub, `InitGameState` copies the subsystem's result into
   `GameState.LastVoyageResult` and sets `bHasLastVoyageResult`. `UVoyageBoardWidget` shows a line under the title
   when a result exists: `"Last run: 2/4 crates intact - hull 100% - pay 400 cr"` (hull rounded for display).
8. **Console helpers** on `AAirlockPlayerController` (exec -> unchecked server RPC, same caveat as the others; each
   logs what it did or why not): `AirlockCargoStatus` (one line per crate and slot from `CargoSummary`, `Cargo
   empty` on the hub), `AirlockCarry <CrateId>` (the same server path as the key, in range), `AirlockDrop`,
   `AirlockPlace <SlotId>`, `AirlockCrateHeat <CrateId> <temperature>` (`CargoSetTemperature`; 0..100 inclusive),
   `AirlockTally` (logs `CargoTally()` in the `Voyage result:` format prefixed `Tally now:`, recording nothing).
   The existing `AirlockRelease` keeps its meaning (release a hold-E); it has nothing to do with cargo.
9. **Tests** (filter `Airlock`; the existing 34 stay green and untouched apart from the defaulted parameters):
   - `Tests/CargoModelTests.cpp`: build from `Hauler_Mk1` -> 4 crates Loose, 4 slots empty; a loose volatile crate
     does not heat before `CalmSeconds` and heats at `LooseHeat` after; Carried heats at `CarriedHeat`; Secured
     stays; Racked cools at `RackCool` (also during calm) and clamps at 0; reaching `VentTemperature` -> Vented,
     stays Vented, refuses PickUp/Place/PutDown/SetTemperature; `SetTemperature` rejects non-volatile, non-finite
     and out-of-range values and accepts 100 (vents on the next tick); Place into an occupied slot fails; Release on
     an empty slot and on the rack fails; non-volatile crates never heat; zero/negative/non-finite deltas are
     no-ops; determinism; the three pay examples; `Tally` counts a Carried crate as loose.
   - `Tests/IncidentScheduleTests.cpp` (extend): with cargo, over 100 seeds: count unchanged, no CargoShift before
     40 %, never first, exactly one in the climax pair and none of the other type in that slot being CargoShift,
     targets are clamp ids, no consecutive repeats by target, deterministic; with a one-clamp layout no non-climax
     CargoShift is drawn; without cargo no CargoShift appears and the existing expectations hold.
   - `Tests/IncidentDirectorTests.cpp` (extend): a CargoShift on a filled clamp releases the crate (state Loose,
     slot empty); on an empty one it skips with `clamp empty`; the existing docking-jump test also asserts that
     `GetLastVoyageResult` is set after the return and not before.
   - `Tests/CargoInteractionTests.cpp` (spawned world; copy the harness from `ShipInteractionTests.cpp` under a new
     name, `FCargoTestWorld` - file-local helper names must be unique or unity builds collide): pick-up in range /
     rejected out of range / rejected with hands full; walk speed assigned to `Base * 0.6` and restored (also after
     two RepNotify-style applications); hold refused while carrying; put-down lands the crate near the pawn as
     Loose; place into a slot -> Secured at the slot; a second pawn cannot take a carried crate; a carried crate
     that vents is detached and the carrier freed; a carrying pawn's interact trace toward an empty slot returns
     the slot, not the crate; a pawn destroyed while carrying drops its crate; `PublishCargo` notifies once per
     change and not for an unchanged array.
   - `Tests/TaskBoardTests.cpp` (extend): heating crate row first and only one row for a hot loose crate, two
     heating crates sorted hottest first, `Secure crate` row last, vented crate no row.
10. **PIE proof** `Scripts\verify-cargo.ps1` (conventions of `verify-incidents.ps1`: file console, fresh log lines
    only, captures under `Saved\AgentCaptures\M3-4`, exit 0 and `CARGO OK` on success). The voyage clock is never
    jumped forward, so no scheduled incident fires during the proof; heat is injected. Steps: reach the ship as the
    other proofs do; `AirlockCargoStatus` -> 4 crates Loose, 4 slots empty; the task board lists `Secure crate
    C1..C4` (capture); warp beside C1, `AirlockCarry C1` -> `Carry C1`, warp beside Clamp_1, `AirlockPlace Clamp_1`
    -> `Cargo C1 Secured`; the same for C2 into Clamp_2; `AirlockIncident CargoShift Clamp_2` -> `Cargo C2 Loose`
    and the captain line; `AirlockCrateHeat C2 60` -> the board shows `Cool crate C2 - 6\dC` (capture); warp beside
    C2 (it lies at Clamp_2), `AirlockCarry C2`, warp beside the rack, `AirlockPlace ColdRack` -> `Cargo C2 Racked`
    and ten seconds later `AirlockCargoStatus` shows a lower temperature; `AirlockCrateHeat C4 100` -> `Cargo C4
    Vented` with steam (capture); `AirlockTally` -> `Tally now: 2/4 crates intact, 1 vented, 1 loose, hull 100.00%,
    pay 400`; `AirlockVoyageClock 1200` (pushes every incident 150 s away), `AirlockShipStatus` -> every module
    Nominal at 100 (repair with `AirlockRepairModule <id> 100` / `AirlockFault <id> None` if anything fired
    anyway); leave through `Airlock_ToHub` -> `Voyage result: 2/4 crates intact, 1 vented, 1 loose, hull 100.00%,
    pay 400`; on the hub `AirlockBoard` -> the board shows `Last run: 2/4 crates intact` (capture).

## Do not touch
- `Ship/ShipLayouts.*` (the level pass is editing it on another branch right now), `Ship/ShipGraph.*`,
  `Ship/AirlockDoor.*`, `Voyage/VoyageSchedule.*`, `Voyage/VoyageTypes.h`, `Core/Interactable.h`.
- `HandleTransitionRequested`, `ReturnToHub`, `TravelToSection` and `ChoosePlayerStart` beyond the single
  `RecordVoyageResult()` call described in 7.
- The expectations of the existing tests (extend the files, never rewrite a passing assertion; the defaulted
  parameters in 5 and 6 keep every existing call site compiling as it is).
- `Scripts\verify-loop.ps1`, `verify-voyage-clock.ps1`, `verify-ship-graph.ps1`; in `verify-incidents.ps1` only the
  regex named in 6.
- `Content/**`, every map, the pack folders. Everything this task adds is spawned from code.

## Edge cases (decided)
- Heating starts at `VoyageElapsedSeconds >= FIncidentSchedule::CalmSeconds` (150 s, for every voyage length; the
  calm minutes stay calm, design section 7); a loose volatile crate is safe until then, so the opening task teaches
  Carry with no pressure. Rack cooling works at any time.
- Proximity heating near hot or failed modules (design 3) is deferred to M3-6; heating is state-based only.
- The `CargoClamps` ship module and the clamp slots are not linked in M3-4 (a Failed CargoClamps module releases
  nothing); noted as an M3-6 tuning option.
- A Vented crate is inert and keeps whatever slot it vented in (the slot stays occupied); it counts as vented in
  the tally and gets no task row. A crate that vents while Carried is dropped by the server (4).
- A crate still Carried at docking counts as loose. A cargo shift on a clamp holding a non-volatile crate simply
  loosens that crate (no retargeting to a volatile one); a released crate stays on its pad until picked up.
- Seamless travel destroys crates and slots with the level; the next voyage rebuilds the manifest from the layout.
  `LastVoyageResult` is overwritten by every docking and `bHasLastVoyageResult` is false until the first one;
  a ship section entered without a booked voyage (PIE started on `Hauler_Mk1`) records nothing.
- Crew scaling of the manifest (8.1: bigger contracts carry more crates) is out of scope; four crates, fixed.

## Notes and hints
- Attachment: `AttachToComponent` on the server replicates through the actor's attachment replication when
  `bReplicateMovement` is on; `DetachFromActor` + `SetActorLocation` on put-down. `AShipLine` shows the steam
  restart pattern and `Ship/ShipVisuals.h` the soft-mesh-with-cube-fallback loader.
- The 4 Hz timer and the publish-on-change rule live in `AAirlockGameMode::TickShipGraph` / `PublishShipGraph`;
  `GetVoyageSecondsRemaining` gives you the elapsed time for the calm rule. `AirlockGameState.cpp` shows the
  server calling its own `OnRep_ShipGraph()` after a publish - do the same for cargo.
- `FShipTestWorld` (`Tests/ShipInteractionTests.cpp`) spawns the real game mode in a transient world; spawn an
  `AAirlockCharacter` in it and call the server RPC implementations directly, as those tests do. A pawn spawned
  there has not ticked, so set its movement mode to Walking yourself before asserting on walk speed.
- The file console, fresh-log-line reading and the capture rules are in `Docs/HANDOFF.md` section 3; useful
  warps: crates at Y 1700 and slots at Y 1400 are both within reach from the Y 1560 walkway; reach is
  `InteractReach * 1.25` = 312 cm, so warp beside the thing you carry from.
- Order that has served well: model + layout + tests -> actors and game-mode spawning/publishing -> the Carry verb
  and its widget -> console helpers -> task board rows -> the cargo shift -> tally and the hub board -> the proof.
  Commit after every step.

## Acceptance (reviewer runs)
`Scripts\build-editor.bat` clean; `Scripts\run-tests.bat Airlock` all green (34 existing + the new ones);
`Scripts\verify-loop.ps1`, `verify-ship-graph.ps1`, `verify-incidents.ps1` and `verify-cargo.ps1` exit 0; captures
show the loose-crate rows, the heating row, a vented crate steaming and the hub board's last-run line.

## Decisions recorded at review (Claude, 2026-09-12)
- **Positions**: the probed table in `Ship/CargoLayouts.cpp` replaces the proposed one - Clamp_1 at X -6200, Clamp_3 at
  -5200, the ColdRack at (-5000, 1300) yaw 90 (the shelf's corner pivot would otherwise reach the Y 1560 walkway), C1 at
  -6100, C3 at -5300, C4 at -5000; every centre settles at feet Z -64.85 on the -65 floor (`verify-cargo.ps1
  -ProbePositions`). The clamp controller module at (-6300, 1900) is clear of them.
- **Seat and carry on one character** (the level pass landed first): the merge keeps both verbs and adds two guards
  outside this brief - sitting is refused with a crate in hand (`Seat <Id> rejected: hands full`), picking up is
  refused while seated (`PickUp rejected: seated`).
- **The checkpoint's failing test** (`Airlock.Cargo.DirectorReleasesFilledClampAndSkipsEmpty`) raced the engine's
  dedicated log thread; the capture device is now unbuffered and the test flushes `GLog` first. The director was right.
- **The proof's calm check** counted the console echoes of `AirlockIncident`; it now anchors on the director's own
  `: Incident <Type> ` line. Nothing else in the proof changed.
- Crew scaling of the manifest, proximity heating and the CargoClamps-module link stay deferred as written above.
