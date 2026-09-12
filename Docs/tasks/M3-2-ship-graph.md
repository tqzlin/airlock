# Task M3-2: ship systems graph, modules and lines, hold-E repair

Owner: GPT-6 Astra (Codex on stack). Reviewer: Claude. Branch: `astra/m3-2-ship-graph` (from `main`).
Design source: project doc "Airlock voyage loop design", sections 3, 4 and 8 (the *Ship Systems Graph* and the
first of the five verbs). This brief is the contract; where the two disagree, this brief wins.

## Prompt (what the reviewer sends to Codex; kept here so the task is reproducible)

```
You are the developer on Airlock, a UE 5.8 C++ spaceship MMO MVP. Work in this repo on a new branch
astra/m3-2-ship-graph (create it from main). Read AGENTS.md, Docs/HANDOFF.md and Docs/tasks/M3-2-ship-graph.md
before touching code, then `git log --oneline -30`. State the currently verified loop in your own words before
you start editing. Implement Docs/tasks/M3-2-ship-graph.md exactly as specified there. Server-authoritative C++,
no Blueprint or UMG assets, automation tests for the pure model, small commits with gameplay-effect messages.
```

## Why this task

M3-1 gave the voyage a clock and a captain. The ship itself is still scenery. This task gives the ship a *model*:
a small graph of modules and the pipe/cable lines between them, with condition that degrades when a line faults
and propagates (leak -> coolant -> core -> brownout), plus the first player verb that fights it: walk up to a
leaking pipe and hold E to tape it. The Incident Director (M3-3) will raise faults against this model on a
schedule; cargo, carrying and calibration (M3-4/5) build on the same graph. So the model must be pure, tested and
replicated; the actors must be legible from ten metres; and every fault must be raisable and provable from the
file console.

## Definition of done

1. **Pure model** in `Source/Airlock/Ship/ShipGraph.h/.cpp` (no UObject state; plain structs and a class):
   - `enum class EShipModuleType : uint8 { PowerCore, CoolantLoop, LifeSupport, CargoClamps, Engines, Nav, Galley }`
   - `enum class EShipModuleStatus : uint8 { Nominal, Degraded, Failed }` derived from `Condition`:
     Nominal >= 60, Degraded 20..59.99, Failed < 20.
   - `enum class EShipLineKind : uint8 { Coolant, Power }`, `enum class EShipLineFault : uint8 { None, Leak, Short }`
   - `USTRUCT FShipModuleState { FName Id; EShipModuleType Type; float Condition (0..100); EShipModuleStatus Status; }`
   - `USTRUCT FShipLineState { FName Id; EShipLineKind Kind; FName FromModule; FName ToModule; EShipLineFault Fault; }`
   - `USTRUCT FShipGraphRates` with defaults (per second): `LeakDrain 1.0` (CoolantLoop condition per leaking
     coolant line), `ShortDrain 1.0` (the shorted power line's `ToModule`), `CoreDriftDegraded 0.5` /
     `CoreDriftFailed 1.5` (PowerCore, while CoolantLoop is Degraded / Failed), `BrownoutDegraded 0.25` /
     `BrownoutFailed 0.75` (LifeSupport and CargoClamps each, while PowerCore is Degraded / Failed).
   - `class FShipGraph`: `void Build(const FShipLayout&)`, `TArray<FName> Tick(float DeltaSeconds)` (applies the
     rates above, clamps to 0..100, recomputes statuses; returns the ids of modules whose Status changed, in layout
     order), `bool SetLineFault(FName LineId, EShipLineFault)`, `bool DamageModule(FName, float)`,
     `bool RepairModule(FName, float)`, `const FShipModuleState* FindModule(FName) const`,
     `const FShipLineState* FindLine(FName) const`, `const TArray<FShipModuleState>& Modules() const`,
     `const TArray<FShipLineState>& Lines() const`, `FString Summary() const` (one line per module and line, used
     by the log and the console helper). No self-recovery: condition only rises through `RepairModule`.
     Deterministic: two graphs built from the same layout and ticked with the same deltas are equal.
2. **Layout table** in `Source/Airlock/Ship/ShipLayouts.h/.cpp`: `USTRUCT FShipModuleSpec { FName Id; EShipModuleType
   Type; FTransform Transform; FSoftObjectPath Mesh; }`, `USTRUCT FShipLineSpec { FName Id; EShipLineKind Kind; FName
   From; FName To; FTransform Transform; FSoftObjectPath Mesh; }`, `USTRUCT FShipLayout { FName SectionId;
   TArray<FShipModuleSpec> Modules; TArray<FShipLineSpec> Lines; }` and `namespace ShipLayouts { const FShipLayout*
   Find(FName SectionId); }`. One layout, `Hauler_Mk1`, with exactly these entries (placeholder positions in the
   first hall of the ship; the reviewer will move them in the level pass, so keep the table the single place they
   live). Z is the walking surface of that hall (20). Yaw 0 unless given.

   | Id | Type / Kind | Location | Mesh (soft path; fall back to `/Engine/BasicShapes/Cube` scaled 1x1x1.5 if it does not load) |
   |---|---|---|---|
   | PowerCore | PowerCore | (-6300, 1900, 20) yaw 180 | `/Game/ModSciInteriors/Meshes/SM_PowerCells_Wall` |
   | CoolantLoop | CoolantLoop | (-5900, 1900, 20) yaw 180 | `/Game/ModSciInteriors/Meshes/SM_WVC_Tank_A` |
   | LifeSupport | LifeSupport | (-5500, 1900, 20) yaw 180 | `/Game/ModSciInteriors/Meshes/SM_ElectricDistribBox` |
   | CargoClamps | CargoClamps | (-5100, 1900, 20) yaw 180 | `/Game/ModSci_Engineer/Meshes/SM_StorageAreaNetwork` |
   | Engines | Engines | (-4700, 1900, 20) yaw 180 | `/Game/ModSciInteriors/Meshes/SM_EnergyNode_A` |
   | Nav | Nav | (-6300, 1220, 20) | `/Game/ModSciInteriors/Meshes/SM_WVD_Server` |
   | Galley | Galley | (-4700, 1220, 20) | `/Game/StarterBundle/ModularScifiProps/Meshes/SM_Cabinet_A` |
   | Coolant_1 | Coolant, CoolantLoop -> PowerCore | (-6100, 1220, 20) | `/Game/ModSci_Engineer/Meshes/SM_Pipe_Medium` |
   | Coolant_2 | Coolant, CoolantLoop -> Engines | (-5700, 1220, 20) | `/Game/ModSci_Engineer/Meshes/SM_Pipe_Medium` |
   | Power_1 | Power, PowerCore -> LifeSupport | (-5300, 1220, 20) | `/Game/ModSci_EngiProps/Meshes/SM_Wire_Wall_A` |
   | Power_2 | Power, PowerCore -> CargoClamps | (-4900, 1220, 20) | `/Game/ModSci_EngiProps/Meshes/SM_Wire_Wall_A` |

   Check each mesh path with `AssetTools.exists` once before you rely on it; if a pack path is wrong, fix the path
   (the mesh names are right, the folder may differ) and say so in the report. Never edit the packs.
3. **Authority and replication.** The server game mode owns the `FShipGraph` (`AAirlockGameMode`: build it in
   `BeginPlay` when `ShipLayouts::Find(CurrentSectionId)` returns a layout - i.e. on `Hauler_Mk1`, never on the hub -
   tick it on a 4 Hz timer *only while `GameState.bVoyageActive`*, and mirror it into `AAirlockGameState`:
   `TArray<FShipModuleState> ShipModules` and `TArray<FShipLineState> ShipLines`, replicated, written only when
   something changed, with `OnRep` firing a `DECLARE_MULTICAST_DELEGATE(FOnShipGraphChanged)` `OnShipGraphChanged`
   that the server also broadcasts after each write. Game-mode API used by actors, console helpers and M3-3:
   `bool ShipSetLineFault(FName, EShipLineFault)`, `bool ShipRepairLine(FName)` (= fault None),
   `bool ShipDamageModule(FName, float)`, `bool ShipRepairModule(FName, float)`, `FString ShipSummary() const`.
   Logging: `LogAirlock: Ship <ModuleId> <Status> cond=<n>` whenever a module's status changes,
   `LogAirlock: Line <LineId> fault=<None|Leak|Short>` on every fault change, `LogAirlock: Ship graph built:
   <n> modules, <m> lines (section <id>)` once.
4. **World actors**, spawned by the game mode right after the graph is built (server; replicated to clients), one
   per layout entry, in the world outliner folder `Ship`:
   - `Ship/ShipModule` - `AShipModule`: root scene, `Body` static mesh from the spec (soft load; cube fallback),
     `StatusLight` point light (green Nominal / amber Degraded / red Failed, ~300 cd, radius ~250, placed 120 cm
     above the origin), `Label` text render (`"<Id>  <Status>  <cond>%"`, 30 cm letters, faces -X in the table
     above, i.e. towards the corridor). Refreshes from the game state on `OnShipGraphChanged`. Collision:
     `QueryOnly`, blocks Visibility (so the interaction trace can hit it), no pawn blocking.
   - `Ship/ShipLine` - `AShipLine`: root scene, `Body` mesh from the spec, `Label` text render as above, a
     `UParticleSystemComponent LeakFX` (template soft-loaded from `/Game/ModSciInteriors/Particles/PS_SteamDoor`,
     auto-activate off) that is active exactly while `Fault == Leak`, and a `UPointLightComponent ShortLight`
     (orange, flickering by timer between 0 and ~400 cd) active exactly while `Fault == Short`. Implements
     `IInteractable`: prompt `"Tape the leak (hold E)"` / `"Patch the short (hold E)"` / empty when no fault;
     hold time 3 s for a leak, 4 s for a short; completing the hold calls `ShipRepairLine` on the server. Collision
     as for modules.
5. **Hold-E interaction** (the verb; keep press-E working for the voyage board):
   - `Core/Interactable.h`: add `BlueprintNativeEvent float GetInteractHoldSeconds()` (default 0 = a press) and
     `BlueprintNativeEvent void OnHoldComplete(APawn* Pawn)`. `Interact` keeps its meaning for press interactables.
   - `Core/AirlockCharacter`: on the interact action *started*, trace as today; if the hit actor's hold seconds
     are > 0 call `ServerBeginHold(AActor*)`, otherwise the existing press path. On the action *completed*
     (key released) call `ServerEndHold()`. Server side the character keeps `HoldTarget`, `HoldElapsed`, ticks
     while the target is valid, still faulted/interactable and within `InteractReach * 1.25` of the pawn; out of
     range pauses the hold (no reset); release resets it. `HoldProgress` (0..1, `COND_OwnerOnly`) replicates
     to the owning client. On completion the server calls `IInteractable::Execute_OnHoldComplete(Target, this)`,
     logs `LogAirlock: Hold <ActorLabel> complete by <PawnName>` and resets.
   - `UI/HoldProgressWidget` - C++-built (like `CaptainLineWidget`): bottom-centre prompt text plus a progress
     bar, visible while the local pawn has a hold target; the prompt text comes from `GetInteractPrompt`.
6. **Console helpers** on `AAirlockPlayerController` (exec -> unchecked server RPC, same caveat as the others;
   all log what they did): `AirlockFault <LineId> <None|Leak|Short>`, `AirlockDamage <ModuleId> <amount>`,
   `AirlockRepairModule <ModuleId> <amount>`, `AirlockShipStatus` (logs `ShipSummary()` one line per entry),
   `AirlockHold <ActorLabel>` (starts a hold on that actor exactly as the key would - same server path - if it is
   within `InteractReach * 1.25` of the pawn, else logs why not), `AirlockRelease`.
7. **Tests** in `Source/Airlock/Tests/ShipGraphTests.cpp` (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`, filter `Airlock`):
   build from `Hauler_Mk1` gives 7 modules and 4 lines, all Nominal at 100 and no faults; a leak drains
   CoolantLoop at `LeakDrain` and nothing else until it is Degraded; CoolantLoop Degraded drifts PowerCore and
   Failed drifts it faster; PowerCore Degraded browns out LifeSupport and CargoClamps and no other module; a short
   drains only the line's `ToModule`; clearing a fault stops the drain on the next tick; `RepairModule` clamps at
   100 and `DamageModule` at 0; `Tick` returns exactly the modules whose status changed, in layout order; two
   graphs ticked identically are equal (determinism); unknown ids return false. Existing 9 tests stay green.
8. **PIE proof** `Scripts\verify-ship-graph.ps1` (model it on `verify-voyage-clock.ps1`: file console, fresh log
   lines only, exit 0 on success, captures under `Saved\AgentCaptures\`): reach the ship the way `verify-loop.ps1`
   does; `AirlockShipStatus` -> 7 modules Nominal; `AirlockFault Coolant_1 Leak` -> `Line Coolant_1 fault=Leak`;
   `AirlockDamage CoolantLoop 50` -> `Ship CoolantLoop Degraded`; wait 12 s -> `AirlockShipStatus` shows PowerCore
   below 100; `AirlockWarp -6100 1400 116` (in front of the line) + `AirlockHold Coolant_1` -> within 6 s
   `Hold ... complete` and `Line Coolant_1 fault=None`; `AirlockRepairModule CoolantLoop 100` -> `Ship CoolantLoop
   Nominal`; capture the line with the steam on and the module labels; back to the hub through the exit. Print
   `GRAPH OK` on success.

## Do not touch
- `Ship/AirlockDoor.*`, the checks in `HandleTransitionRequested`, `TravelToSection`, `ReturnToHub`.
- `Voyage/VoyageSchedule.*`, `Voyage/VoyageSubsystem.*`, `Voyage/VoyageTypes.h`, the existing tests' expectations.
- `Content/**` - no map or asset edits; everything this task adds is spawned from code.
- Pack folders (`Content/ModSci*`, `Content/StarterBundle`, `Content/StarfieldFree`, `Content/SD_Art`,
  `Content/Construction_VOL2`, `Content/Warehouse_Storage`).

## Edge cases (decided)
- The graph exists on the ship whether or not a voyage is active (the actors are the scenery); it only *ticks*
  while `bVoyageActive`. On the hub nothing is built and the arrays stay empty.
- Seamless travel destroys the actors with the level; the next arrival rebuilds from the layout at full health.
  Persisting damage across sections is out of scope.
- A hold on a line whose fault was cleared meanwhile (another player, the console) ends silently.
- Two players holding the same line: both progress independently; the first completion clears it; the other
  hold ends silently.
- `Tick` with `DeltaSeconds <= 0` or non-finite is a no-op that returns an empty array.
- Mesh soft paths that fail to load fall back to the cube and log one warning; the task still passes.
- The 4 Hz timer runs on the server game mode only; clients never tick a graph.

## Notes and hints
- `IInteractable` today: `Core/Interactable.h`, used by `Station/VoyageBoard`. The character's trace is in
  `Core/AirlockCharacter.cpp` (`Interact`, `InteractReach`). Enhanced Input actions are built in C++ on the
  controller; `IA_Interact` already exists - bind `Started` and `Completed` triggers for it.
- Replication patterns: `GetLifetimeReplicatedProps` in `AirlockGameState.cpp`; RPC pattern:
  `AirlockVoyageClock` -> `ServerSetVoyageClock`.
- Soft mesh loading with a primitive fallback: `Ship/AirlockDoor.cpp` does exactly this.
- Text render and point light components need no assets. Keep every colour and size as a `UPROPERTY` so the
  legibility pass (M3-6) can tune them.
- `PS_SteamDoor` is a Cascade system (still supported in 5.8). `UParticleSystemComponent` lives in `Engine`.
- Widgets are built in `RebuildWidget` (`UI/VoyageBoardWidget`, `UI/CaptainLineWidget` are the patterns).
- The file console (`Saved\AgentConsole\commands.txt`) and `Get-FreshLine` rules are in `Docs/HANDOFF.md`.

## Acceptance (reviewer runs)
`Scripts\build-editor.bat` clean; `Scripts\run-tests.bat Airlock` shows 19+ passing; `Scripts\verify-loop.ps1`
exit 0; `Scripts\verify-ship-graph.ps1` exit 0 with `GRAPH OK` and the captures showing lit labels and steam.
