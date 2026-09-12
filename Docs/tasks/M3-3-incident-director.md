# Task M3-3: Incident Director and the task board

Owner: GPT-6 Astra (Codex on stack). Reviewer: Claude. Branch: `astra/m3-3-incident-director` (from `main`, after
M3-2 is merged). Design source: project doc "Airlock voyage loop design", sections 3 (Incident Director), 6 (the
minute-by-minute run) and 8.2 (conflict pairs). This brief is the contract.

## Prompt (what the reviewer sends to Codex)

```
You are the developer on Airlock, a UE 5.8 C++ spaceship MMO MVP. Work in this repo on a new branch
astra/m3-3-incident-director (create it from main). Read AGENTS.md, Docs/HANDOFF.md, Docs/tasks/M3-2-ship-graph.md
(what you are building on) and Docs/tasks/M3-3-incident-director.md before touching code, then
`git log --oneline -30`. Implement Docs/tasks/M3-3-incident-director.md exactly as specified there.
```

## Why this task

M3-2 gave the ship a model that degrades when lines fault, and a verb that repairs them. Nothing raises a fault
yet except the console. This task adds the captain's brain: a seeded, paced schedule of incidents that hits the
graph during the voyage, a captain line for each, and a task board so every player can see what is open and
where. After it, a solo run is already a game: things break on a curve, you fix them, the ship docks in better or
worse shape.

## Definition of done

1. **Pure schedule** in `Source/Airlock/Voyage/IncidentSchedule.h/.cpp`:
   - `enum class EIncidentType : uint8 { CoolantLeak, PowerShort, ModuleDamage }` (cargo shift and calibration
     arrive with M3-4/M3-5; do not stub them).
   - `USTRUCT FIncidentSpec { float AtSeconds; EIncidentType Type; FName TargetId; float Magnitude; FText CaptainLine; }`
     (`Magnitude` is the damage for `ModuleDamage`, unused otherwise).
   - `FIncidentSchedule::Build(const FVoyageSpec& Spec, const FShipLayout& Layout, int32 CrewCount)` returns an
     ordered `TArray<FIncidentSpec>`, deterministic from `Spec.Seed` (use `FRandomStream`), with these rules
     (all in named constants so tests can quote them):
     - nothing before `CalmSeconds = 150` and nothing after `Duration - DockSeconds (60)`;
     - count = `BaseIncidents (4) + (CrewCount - 1)`, clamped to `[4, 8]`; crew count clamps to `[1, 5]` so the
       examples below hold (1 -> 4, 4 -> 7, 9 -> 8). *(Reviewer note 2026-09-12: the brief originally said `[1, 4]`,
       which contradicted the `9 -> 8` test; Astra implemented the test examples and the reviewer kept that.)*
     - the first incident is always a `CoolantLeak` (the teaching fault); targets are drawn from the layout
       (coolant lines for leaks, power lines for shorts, `PowerCore`/`LifeSupport`/`CargoClamps`/`Engines` for damage
       with magnitude 30..50), no two consecutive incidents on the same target;
     - spacing: incidents are spread over the window with a minimum gap of `MinGapSeconds = 90`, except the
       **climax pair**: the last two incidents are 10..20 s apart and land at 78..85 % of the duration;
     - each entry carries a captain line built from a small table per type ("Pressure drop on the coolant loop -
       find the leak", "Breaker trip on the {Target} feed", "{Target} is taking damage, get on it") with the
       captain's name where the M3-1 lines use it.
   - `FIncidentSchedule::FindNextUnfired(...)` in the same spirit as `FVoyageSchedule::FindNextUnfiredEntry` so
     the game mode can drive both from timers.
2. **Director on the server.** A `UIncidentDirectorComponent` (`Voyage/IncidentDirectorComponent.h/.cpp`) created
   by `AAirlockGameMode` on ship sections when the voyage clock starts (after the graph is built). It builds the
   schedule with `CrewCount = max(1, number of player controllers at that moment)`, arms one timer at a time
   (like the captain announcements), and on fire applies the incident through the M3-2 game-mode API
   (`ShipSetLineFault` / `ShipDamageModule`), sends the captain line through `ClientCaptainSays` to every
   controller, and logs `LogAirlock: Incident <Type> <Target> (<mag>) at <t>s`. `AirlockVoyageClock` moving the
   clock must reschedule incidents exactly the way it reschedules announcements (past ones fire once, in order).
   If a target is already faulted / already Failed when its incident fires, skip it and log `skipped`.
3. **Task board.** `UI/TaskBoardWidget` (C++-built like the others): a top-left panel, visible on ship sections
   while a voyage is active, listing open tasks derived *only* from the replicated game state on
   `OnShipGraphChanged` (no extra replication): one row per faulted line (`"Tape leak - Coolant_1"` /
   `"Patch short - Power_2"`) and one per module that is not Nominal (`"PowerCore - Degraded 42%"`, red for
   Failed), plus the voyage time remaining on top. Empty state: `"All systems nominal"`. Rows sorted: Failed
   modules, then faulted lines, then Degraded modules. Max ~8 rows, then `"+n more"`.
4. **Console helpers** on `AAirlockPlayerController` (exec -> server RPC, dev-cheat caveat as the others):
   `AirlockIncident <CoolantLeak|PowerShort|ModuleDamage> [TargetId] [Magnitude]` (fires one now; omitted target
   picks the first valid one from the layout), `AirlockIncidentSchedule` (logs every entry of the built schedule:
   time, type, target).
5. **Tests** in `Tests/IncidentScheduleTests.cpp` (filter `Airlock`): determinism (same spec + crew -> equal
   arrays; different seed -> different); ordering and window (sorted, none before `CalmSeconds`, none after
   `Duration - DockSeconds`); count by crew size (1 -> 4, 4 -> 7, 9 -> 8 after clamping); first is a leak;
   targets exist in the layout and match the type; no consecutive repeats; climax pair spacing and placement;
   captain name present in every line. The M3-2 and earlier tests stay green.
6. **PIE proof** `Scripts\verify-incidents.ps1` (same conventions as `verify-ship-graph.ps1`): reach the ship;
   `AirlockIncidentSchedule` -> the log lists 4 entries for a solo run, first a `CoolantLeak`; `AirlockVoyageClock
   <Duration-160>` -> within 15 s the first incident fires (`Incident CoolantLeak ...`), the captain line shows,
   the task board shows the tape-leak row (capture); repair it with `AirlockWarp` + `AirlockHold` as in the M3-2
   proof -> the row disappears (capture); `AirlockIncident ModuleDamage PowerCore 45` -> the board shows
   `PowerCore - Degraded`; `AirlockVoyageClock 70` -> the climax pair has fired (two `Incident` lines within 20 s
   of each other in the log); return to the hub. Print `INCIDENTS OK`.

## Do not touch
- `Ship/AirlockDoor.*`, the transition checks, `TravelToSection`, `ReturnToHub`.
- `Ship/ShipGraph.*`, `Ship/ShipLayouts.*` (read them; if a change seems needed, stop and report).
- `Voyage/VoyageSchedule.*` (the captain's fixed announcements stay as they are; incidents are a second chain).
- `Content/**`, pack folders.

## Edge cases (decided)
- Crew count is sampled once when the voyage clock starts; players joining later do not change the schedule.
- `Duration < CalmSeconds + DockSeconds + 30` (tests, short clocks): the window collapses to the middle 50 % of
  the duration and the minimum gap scales down proportionally; the count rules stay.
- An incident whose target no longer exists (layout mismatch) is skipped with a log line, never a crash.
- The task board reads the clock from the game state exactly like the caption widget does; no new RPCs.

## Notes and hints
- Timer chains: `ScheduleNextVoyageEvent` / `TickVoyageClock` in `AirlockGameMode.cpp` are the pattern; keep the
  two chains independent so `AirlockVoyageClock` can reschedule both.
- `FRandomStream(Spec.Seed ^ 0x1NC1DENT)`-style derivation keeps the incident stream independent of any other use
  of the seed.
- Widgets: `UI/CaptainLineWidget` for the show/hide pattern, `UI/HoldProgressWidget` (M3-2) for reading the graph.
- Everything a proof needs already exists after M3-2: `AirlockShipStatus`, `AirlockHold`, the file console.

## Acceptance (reviewer runs)
Build clean; `run-tests.bat Airlock` all green (24 existing + the new schedule tests); `verify-loop.ps1`, `verify-ship-graph.ps1` and
`verify-incidents.ps1` exit 0; captures show the task board with and without open tasks.
