# Task M3-1: voyage clock, auto-return, captain announcements

Owner: GPT-6 Astra (Codex on stack). Reviewer: Claude. Branch: `astra/m3-voyage-clock`.

## Prompt (paste this into the Codex session opened on `C:\Users\tclin\Desktop\ai_game_studio\Airlock`)

```
You are the developer on Airlock, a UE 5.8 C++ spaceship MMO MVP. Work in this repo on a new branch
astra/m3-voyage-clock (create it from main). Read AGENTS.md, Docs/HANDOFF.md and Docs/tasks/M3-voyage-clock.md
before touching code, then `git log --oneline -25`. Do not start until you can state the currently verified
loop (AGENTS.md "The MVP loop as it stands") in your own words in your first message.

Implement Docs/tasks/M3-voyage-clock.md exactly as specified there (definition of done, do-not-touch list,
verification). Server-authoritative C++, no Blueprint or UMG assets, automation tests for the pure logic,
small commits with gameplay-effect messages. Close the editor before building (HANDOFF section 2), reopen it
after, run the tests and Scripts\verify-loop.ps1, then the PIE proof in the brief.

Stop and ask (in your report, then wait) if: the build fails twice on the same error, the task seems to need a
change to Ship/AirlockDoor.* or to the checks in AAirlockGameMode::HandleTransitionRequested, a pack asset
under Content/ModSci* or Content/StarfieldFree would need editing, or any map would need to change.

Finish with the report format from Docs/HANDOFF.md section 6.
```

## Why this task
The M2 loop (book -> Dock B -> Hauler_Mk1 -> exit -> hub) is verified. The voyage currently has no duration and the
captain is a name on a card. This task makes a voyage *end*: a replicated clock, a handful of captain lines, and an
automatic return through the existing transition path when time runs out. It is bounded, pure gameplay code, and
touches nothing the door/travel work depends on.

## Definition of done
1. **Clock.** `AAirlockGameState` replicates the voyage end as server world time (`VoyageEndServerTime`, 0 when no
   voyage is active) plus `bVoyageActive`. Any client can compute seconds remaining from
   `GetServerWorldTimeSeconds()`. On a ship section with a pending voyage (`UVoyageSubsystem::GetPendingVoyage`), the
   game mode starts the clock in `BeginPlay` from `FVoyageSpec::DurationSeconds` (1200 s today). On the hub nothing
   starts.
2. **Auto-return.** When the clock reaches zero the server travels everyone back to the hub through the same path a
   player takes by walking through `Airlock_ToHub`: the "RETURNING TO STATION" card, `TravelFadeDelay`, `ServerTravel`,
   voyage cleared, arrival at `PlayerStart_Hub`. Implement it by extracting the travel part of
   `AAirlockGameMode::HandleTransitionRequested` into a reusable method (suggested:
   `void TravelToSection(const TSoftObjectPtr<UWorld>& Level, FName SectionId, const FText& Title, const FText& Subtitle)`)
   and calling it from both places. The gating checks in `HandleTransitionRequested` (booking required, hub target
   clears the voyage, `bTravelPending`) must keep their exact behaviour. The hub level to return to: the level of the
   door in this map whose `TargetSectionId == AAirlockGameMode::HubSectionId`; fall back to
   `/Game/Maps/StationHub` if no such door exists.
3. **Captain announcements.** A pure, testable schedule: `FVoyageSchedule::Build(const FVoyageSpec&)` (new files under
   `Source/Airlock/Voyage/`) returning an ordered array of `{ float AtSeconds; FText Line; }` with at least four
   entries: departure (0 s), a mid-voyage line specific to the voyage type (mining / pirates / derelict each get their
   own text), "one minute to dock" (Duration - 60), and docking (Duration). The captain's name from the spec appears
   in the lines. The game mode fires each entry on a timer and sends `ClientCaptainSays(const FText& Line)` (new
   Client RPC on `AAirlockPlayerController`) to every controller; the controller shows it in a small C++-built widget
   (`UI/CaptainLineWidget`, top-centre text, hides itself after ~6 s, replaced by the next line). Log every line as
   `LogAirlock: Captain <name>: <line>`.
4. **Console helper for testing.** `UFUNCTION(Exec) void AirlockVoyageClock(float SecondsRemaining)` on the player
   controller -> server RPC that moves the clock so `SecondsRemaining` are left and reschedules the not-yet-fired
   announcements accordingly (entries whose time has passed fire immediately once, in order). Log the change.
5. **Tests** in `Source/Airlock/Tests/` (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`, filter `Airlock`): the schedule is
   sorted, starts at 0, ends at `DurationSeconds`, contains the captain's name, and every `EVoyageType` yields a
   distinct mid-voyage line. Existing four tests stay green.
6. **PIE proof** (attach to the report): `Scripts\verify-loop.ps1` passes; then, in a PIE run: book, cross Dock B,
   after arrival `AirlockVoyageClock 75` -> the "one minute" line appears in the viewport within ~15 s (capture it),
   then the RETURNING TO STATION card, then `AirlockWhere` shows `X=-5640` in `StationHub` and the log has
   `Voyage ... complete`. Include the `LogAirlock` lines from `Section Hauler_Mk1` to the hub arrival.

## Do not touch
- `Source/Airlock/Ship/AirlockDoor.h/.cpp` and the door instances in the maps.
- The checks in `HandleTransitionRequested` (extraction of the travel part is expected; behaviour identical).
- `Content/**` - no map or asset edits are needed for this task.
- The existing tests' expectations, the `TodaySeed` maths, `MakeVoyageSpec` determinism.
- Anything under `Content/ModSci*` or `Content/StarfieldFree` (pack assets).

## Edge cases (answers to the questions Astra raised in its read-through)
- Durations under 60 s (tests, `AirlockVoyageClock`): the "one minute" entry sits at `max(0, Duration - 60)`; entries with equal times keep list order; nothing is dropped.
- `AirlockVoyageClock`: clamp the argument to `[0, DurationSeconds]`; ignore NaN/inf (log and return). Entries whose time is already past after the move fire once, in order, on the next timer tick.
- Re-booking aboard a ship (`ServerChooseVoyage` is not gated by section today) is out of scope: the clock follows the voyage that was pending when the level started; do not change `ServerChooseVoyage` or `RequestVoyage` in this task, note it in the report if it bites.
- `Scripts\verify-loop.ps1` only trusts log lines stamped after the run starts; if you extend it, keep that rule.

## Notes and hints
- `UVoyageSubsystem` (GameInstance subsystem) survives travel and holds the pending voyage; `ClearPendingVoyage` is
  what `HandleTransitionRequested` calls for a hub target - the auto-return must end up in the same state.
- The transition card is `UAirlockGameInstance::ShowTransition/HideTransition`; controllers already end the card
  1 s after `BeginPlay` on arrival. Reuse; do not add a second overlay.
- Widgets are built in C++ (`UVoyageBoardWidget::RebuildWidget` is the pattern; `UMG` is already a module dependency).
- Replicated properties: `GetLifetimeReplicatedProps` in `AirlockGameState.cpp` is the pattern.
- Timers: `GetWorldTimerManager()`; the game mode only exists on the server, which is what you want.
- Keep the hub untouched: `bVoyageActive` false there, no timers, no captain.

## Acceptance (reviewer runs)
`Scripts\build-editor.bat` clean; `Scripts\run-tests.bat Airlock` shows 6+ passing; `Scripts\verify-loop.ps1` exit 0;
the `AirlockVoyageClock 75` scenario reproduces from the report's commands.
