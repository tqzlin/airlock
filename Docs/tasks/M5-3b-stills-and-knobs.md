# Task M5-3b: Stills and knobs - the still actors, their controls, the gauges, the by-hand proof

Owner: GPT-6 Astra (checkout `Airlock\`, editor on port 8000). Branch: `astra/m5-3b-stills-and-knobs` from `main`
(after the M5-3a merge). Design source: `Docs/design/refinery-floor.md` sections 4-6 (the still, the knobs, the
levers, the failures) and M5-3a's server API (`Docs/tasks/M5-3a-refinery-on-the-floor.md`, `Core/AirlockGameMode.h`
"The refinery model on the floor"). Everything a player sees here ships with a PIE proof through the file console.
This brief was red-teamed before launch; where it disagrees with the design note's numbers, the brief is final.

## Why this task

Since M5-3a a cask can be refined on the floor **by console only**: the model runs in the game mode, its state is
replicated, the verbs exist (`RefineryLoadHopper`, `RefinerySetKnob`, `RefineryStepKnob`, `RefineryLever`), and
the placeholder still fixtures show a one-line state. This task gives the crew hands: a hopper to load a carried
cask into, `-` / `+` handles for the three knobs, a gauge stack per still, four lever holds and a calibrate hold,
and Okafor's two warning lines - so a player standing on the floor can do what the proof does by command. Nothing
in the model or its numbers changes.

## What exists (read before designing)

- `FRefineryModel` (M5-1): integer knobs 0..20 (`KnobSteps`), statuses Idle / Running / Popped / Flash / Failed,
  `bFouled`, `Calibration`, `PressureBar` (`WarnBar` 2.5, `PopBar` 3), `ChargeL` (1000 L per cask), `Hopper` (3;
  the running cask is `CurrentCask`, not in the hopper - the count drops by one the tick after a load). The verb
  rules (`Ship/RefineryModel.cpp`): `ClearPop` needs Popped, `Purge` needs Flash, `Repair` needs Failed, `Scrape`
  needs Idle with no cask in the still (an idle still with a queued hopper turns Running on the next tick, so in
  practice scrape when nothing is queued), `Calibrate` refuses only Failed.
- The mirror `AAirlockGameState::Refinery` (`FRefineryState`; `OnRep_Refinery` broadcasts `OnShipGraphChanged`),
  published floored (1 K, 0.05 bar, 1 % calibration, 10 L). Actors read the mirror, never the game mode's model.
- The game mode's server API, each call publishing and logging: `RefineryLoadHopper(Still, Cask)` (a Loose or
  Carried cask -> `Loaded`, the crate hides, the carrier's hands are freed in place by `CheckCarriedCargo`),
  `RefinerySetKnob` / `RefineryStepKnob(Still, ERefineryKnob, n)` -> `Refinery <Still> heat|feed|vent <k>/20`
  (`Refinery <Still> heat rejected` for a zero or impossible step), `RefineryLever(Still, ERefineryLever)` ->
  `Refinery <Still> reset|purged|scraped|repaired|calibrated` or `... rejected: <Status>`, `RefineryHasFloor()`,
  `RefineryStillReadout(Still)`, `GetRefineryModel().BatchProgress(Still)` (server only; clients compute
  `1 - ChargeL / CaskLitres` from the mirror).
- `AFloorFixture` (M5-2/M5-3a): the still placeholder (kit tank `SM_WVC_Tank_A` at 0.8 - a cylinder 287 cm
  across, 212 tall at scale 1, origin centred in XY at its base: radius 115 cm and 170 cm tall at 0.8 - label at
  Z 250 on the axis, facing -Y toward the walkway, second line from `AFloorFixture::StateLine`). Stills stand at
  (-5800 | -5400 | -5000, 1900, -65); the deck's walking surface is Z -65, the walkway trench at Y 1560 is Z -70
  (`verify-floor.ps1 -ProbePositions`). Its members are private and its `Refresh` is not virtual - see item 1.
- Interaction: `IInteractable` (`Interact` press, `GetInteractPrompt`, `GetInteractHoldSeconds` > 0 = a hold,
  `OnHoldComplete`); the character's E traces the first actor in `InteractReach` and, while carrying, routes by
  class (`ACargoSlot` -> place, `AFuelIntake` -> `ServerFeedCask`, else put down) - `Core/AirlockCharacter.cpp`
  `Interact()`. A hold target is never pressed: `Interact()` calls `ServerBeginHold`, the release `ServerEndHold`
  (which resets the hold), and the server ticks `HoldElapsed` in `Tick`. `ServerBeginHold` refuses blacked out,
  hands full, a non-interactable, zero hold seconds or an empty prompt (`Hold <Label> rejected: no active fault/hold
  interaction`) and range. `ServerFeedCask` is the pattern for "a carried cask into a fixture". `GetInteractPrompt`
  is only rendered by the hold widget while a hold runs: press prompts are API text until a crosshair prompt widget
  exists (not this task).
- Console helpers exist for every verb (`AirlockKnob`, `AirlockKnobStep`, `AirlockStillLoad`, `AirlockStill`,
  `AirlockRefinery`); `AirlockHold <ActorLabel>` begins a hold by label and never ends it (`AirlockRelease` does).
  There is no press-by-label helper yet.
- `Docs/kit-mesh-bounds.csv` has every kit mesh's bounds. `SM_PipeValve_A` is 31 x 38 x 478 (a standpipe),
  `SM_WVD_Panel_A` 120 x 22 x 78 with Z centred and its back face at Y 0, `SM_PlasticPallet` is not listed (measure
  it or pick another). Handles that are thin sticks cannot be traced by hand from the walkway: every control gets
  an invisible box for the E trace (item 2).

## Definition of done

1. **`AStill : public AFloorFixture`** (`Ship/Still.h/.cpp`), spawned by `BuildFloorFixtures` for every `Still`
   fixture instead of the base class (one line in the game mode: the class by `Spec.Kind`; `Floor fixtures: 6`
   stays). To make the subclass possible, `Ship/FloorFixture.h` takes a three-line edit and nothing more:
   `Refresh()` becomes `protected virtual`, `bVisualsReady` and `FloorState` become protected, and a
   `const FFloorFixtureSpec& GetSpec() const` accessor is added. `AStill` keeps the body and the base label (the
   M5-2/M5-3a tests that count and read `AFloorFixture`s still pass) and adds the **gauge stack**: three
   `UTextRenderComponent`s in front of the tank's face, above its top and below the base label - relative offsets
   (0, -125, 215), (0, -125, 195), (0, -125, 175), 18 cm, the spec's `LabelYaw` - refreshed from the mirror on both
   sides in the overridden `Refresh`: `T <K> K   P <bar> bar` (red while `PressureBar >= WarnBar`, white
   otherwise), `heat <%>  feed <%>  vent <%>` and `<cask> <progress>%   cal <%>` (amber while `Calibration <
   CalWarn`; `idle` / `POPPED` / `FLASH` / `FAILED` in place of the cask while not running). Progress =
   `1 - ChargeL / FRefineryRates().CaskLitres` from the mirror.
2. **Controls as actors**: `AStillControl : public AActor, public IInteractable` (`Ship/StillControl.h/.cpp`; the
   `AFuelIntake` pattern: a replicated `FStillControlSpec` `COND_InitialOnly`, `Initialize()` under deferred
   construction, its own retry-bound mirror refresh; folder `Floor`, actor name and label `<Still>_<Control>`,
   e.g. `Still_1_HeatPlus`; `GetStillId()`, `GetControlKind()`), spawned by the game mode right after the stills
   from a table in `Ship/RefineryLayouts.*` (`EStillControl { Hopper, HeatMinus, HeatPlus, FeedMinus, FeedPlus,
   VentMinus, VentPlus, Reset, Purge, Scrape, Repair, Calibrate }`, `FStillControlSpec { Kind, Offset (X, Y, Z
   relative to the still, in cm, unscaled), Yaw (added to the still's), Mesh, Scale (the control's own - never
   composed with the still's 0.8), Label }`; `RefineryLayouts::StillControls(const FFloorFixtureSpec& Still)`
   returns the world transforms: location = `Still.Transform.GetLocation() + Still.Transform.GetRotation().RotateVector(Offset)`,
   rotation = the still's yaw + the table yaw, scale = the table's). Log `Still controls: <n>` (36 for three
   stills). Every control has an invisible `UBoxComponent` `Marker` (the `AControlStation` pattern: QueryOnly,
   blocks Visibility only, hidden, about 20 x 20 x 45 cm, centred at 60 cm up for the handles and levers, flat
   over the pallet for the hopper) so the E trace has something to hit; the mesh is cosmetic and blocks Visibility
   only, never pawns. Per still: a **hopper** (`Hopper`: a pallet at 0.6, label `HOPPER` / `<n> queued`), six
   **handles** (the standpipe valve `SM_PipeValve_A` at non-uniform scale (0.3, 0.3, 0.15) - about 9 x 11 x 72 cm;
   labels `HEAT -` / `HEAT +` etc. over the current value, e.g. `40%`), four **levers** (`SM_WVD_Panel_A` at 0.35
   - 42 x 8 x 27 cm - labels `RESET` / `PURGE` / `SCRAPE` / `REPAIR`) and **`Calibrate`** (the same panel, label
   `TRAP` / `CALIBRATE`). Starting offsets - the table is what you tune, and the tank's radius is 115 cm so every
   XY offset with `sqrt(X^2 + Y^2) <= 120` is inside it: handles at X -120, -80 | -20, +20 | +80, +120 on Y -200,
   Z 0 (a row 40 cm inside each pair, 100 cm between pairs); the hopper at (-200, -200, 0) beside the row (the
   next still's hopper is then 80 cm from this still's `VentPlus`); the levers on the tank's front face at Y -130,
   Z 110, X -75, -25, +25, +75 (the panel's back face toward the tank); `Calibrate` on the east flank at (150,
   -60, 110). Probe every control with `verify-floor.ps1 -ProbePositions` (extend its spot table) **and** with a
   capture from the walkway - the feet-Z probe does not detect a mesh inside another.
3. **The verbs on the controls** (all through the game mode API, never the model):
   - The hopper: while the local pawn carries a cask, E routes a `Cast<AStillControl>(Target)` whose kind is
     `Hopper` to a new `ServerLoadStill(AStillControl* Hopper)` on the character (the `ServerFeedCask` pattern:
     blacked out, range `InteractReach * 1.25`, hands, `RefineryHasFloor`; then `RefineryLoadHopper(StillId,
     CaskId)`; `DetachCarriedCrateAt` as the safety net; log `Load <Cask> into <Still> by <Pawn>` / `Load
     rejected: <reason>`). This branch goes before the put-down `else`. Pressing E at any other control while
     carrying puts the cask down - that is intended ("put the cask down before working the knobs"), not a bug.
     Prompts: `Load <Cask> into Still 1` while carrying, `Hopper 1/3 - carry a cask here` otherwise.
   - The handles: a **tap** steps the knob by one and a **held key repeats the step every 0.25 s**. Implement the
     repeat once, in the character's hold, server side: a new `IInteractable` native event `IsRepeatingHold()`
     (default false). In `ServerBeginHold`, after the existing checks pass and before `HoldTarget` is set, a
     repeating target gets one `OnHoldComplete` immediately (the tap's step); in `Tick`'s completion branch a
     repeating target gets `OnHoldComplete`, then `HoldElapsed` and `HoldProgress` reset to 0 with `HoldTarget`
     kept (release still ends it through `ServerEndHold`; out of range still pauses it). A tap is therefore
     BeginHold + EndHold = one step; a held key = 1 + 4 steps per second. `GetInteractHoldSeconds()` returns 0.25
     for handles. The per-step `Hold <Label> complete by <Pawn>` line is suppressed for repeating holds (log the
     begin and the release only; `Refinery <Still> heat <k>/20` is the evidence), and the handle's
     `OnHoldComplete` does nothing when the mirror already shows 0 or 20 for that knob (no `rejected` spam at the
     end of the range). The hold widget shows the prompt as for any hold (`Heat + (hold to turn)`); its bar
     cycling four times a second is fine.
   - The levers: holds through `RefineryLever` - `Reset` 4 s, `Purge` 5 s, `Scrape` 8 s, `Repair` 12 s,
     `Calibrate` 3 s (the design note's numbers; final). A lever's hold seconds and prompt are non-empty in every
     state; the refusal on the wrong state happens **before the hold starts** through a second new native event
     `FText GetHoldRefusal() const` (default empty = allowed) that `ServerBeginHold` checks after the range check
     and logs as `Hold <Label> rejected: <Refusal>` (the same family as the existing `rejected: out of range` /
     `no active fault/hold interaction` lines, a distinct tail). The lever returns the still's status name from
     the mirror (`Running`, `Idle`) so the proof line `Hold Still_2_Reset rejected: Running` holds; prompts name
     the action and its condition: `Reset the trap (hold E)` while Popped, `Reset - the still is not popped`
     otherwise; `Purge - the still has not flashed`; `Scrape - the still is running` / `Scrape the coke (hold E)`;
     `Repair - the still has not failed`; `Calibrate the trap (hold E)` (refused only while Failed).
4. **Okafor's warning lines** in `PublishRefinery`'s transitions (the game mode; the M5-3a event lines stay):
   on a still entering the warning band `Pressure on {Still} - vent it before the relief goes.`, on a still's
   calibration dropping under `CalWarn` `{Still}'s trap has drifted - you're losing the cut. Calibrate it.`;
   `SpeakFloorLine(cue, line)` with its 20 s cooldown **per cue, not per still** (a second still entering the
   band within 20 s is silent; the tests assert one line per cue), `{Still}` printed as `Still 1`.
5. **A press-by-label console helper**: `AirlockPress <ActorLabel>` -> `ServerPress` behind `CheatAllowed` (one
   line in the every-RPC cheat test; the count goes 31 -> 32): find the actor by label (the `ServerHoldByLabel`
   loop), then: carrying -> the carry branch by class (a hopper loads, a slot places, anything else puts down);
   else hold seconds > 0 -> `ServerBeginHold` immediately followed by `ServerEndHold` (a tap: one step on a
   repeating target, the refusal log on a lever); else `Execute_Interact`. Logs `Press <Label> rejected: <reason>`
   when out of reach or unknown. `ServerLoadStill` (the key's own path) is not behind `CheatAllowed`, like
   `ServerFeedCask`.
6. **Tests** (`Tests/StillTests.cpp`, `Airlock.Floor.Still*`): copy `FFloorTestWorld` from `RefineryFloorTests.cpp`
   (it lives in that file's anonymous namespace) and add `CrewAt()` / `Carry()` from `FuelInteractionTests.cpp`;
   drive holds with `Crew->ServerBeginHold(Control)` + `S.Tick(seconds)` (0.25 s frames: N frames held = 1 + N
   steps) and `Crew->ServerEndHold()`. Cover: the floor spawns 3 stills and 36 controls at the table's transforms;
   a carried cask into the hopper loads it (crate hidden, hands free, `Refinery.Waiting` shrinks) and the
   empty-handed press does nothing; a handle tap steps once, a held handle steps every frame and stops on release,
   and a held handle at 20 stays silent; a lever refuses on the wrong status before starting (`GetHoldTarget()`
   stays null, the `rejected: Idle` log) and completes on the right one - drive a pop through the API with M5-1's
   pop test as the model: heat 10 / feed 10 / vent 2, 240 ticks of preheat, load a cask, then at most 80 ticks
   (assert the `Pressure on Still 1` cue before the `Pop` line); the drift line: heat 8 / feed 10, load, about 220
   ticks; the gauge lines from a hand-built mirror. Keep each world test under a few hundred ticks.
7. **Proof**: extend `Scripts\verify-floor.ps1` so the by-hand batch on Still 2 with the light cask C2 runs **in
   parallel with** M5-3a's console batch, not after it - a loose light cask on the floor vents at about 250 s
   elapsed (`LooseHeat` after `CalmSeconds`), and the console batch alone ends around 190 s. Right after the line
   `Still Still_1 Running cask=C1`: `AirlockWarp -6080 1560 116`, `AirlockCarry C2` (C2 stands at (-6080, 1400)),
   `AirlockWarp -5400 1600 116` (every Still 2 control is within hold range from there), `AirlockPress
   Still_2_Hopper` -> `Load C2 into Still_2 by`, `Refinery Still_2 loaded C2`, `Cargo C2 Loaded`; `AirlockPress
   Still_2_HeatPlus` x 6 -> `Refinery Still_2 heat 6/20` (the light recipe, heat 30 %); `AirlockHold
   Still_2_HeatPlus`, `Start-Sleep 1`, `AirlockRelease` -> at least three more `Refinery Still_2 heat <n>/20`
   lines; `AirlockKnob Still_2 Heat 6` back; `AirlockPress Still_2_FeedPlus` x 10 -> `feed 10/20` and `Still
   Still_2 Running cask=C2`; `AirlockHold Still_2_Calibrate` -> `Hold Still_2_Calibrate complete`, `Refinery
   Still_2 calibrated`; `AirlockHold Still_2_Reset` -> `Hold Still_2_Reset rejected: Running`. Then the existing
   waits continue for Still 1; add the wait for `Refinery BatchDone Still_2 C2` before `AirlockTally`, and change
   the tally and horn assertions to `2/2` (`Floor tally now: floor: 2/2 casks refined`, `Floor result: floor: 2/2
   casks refined`, `| floor: 2/2 casks refined,`), the `FLOOR OK` message with them. Captures of the gauge stack
   and the handles from the walkway (`floor-still-gauges`, `floor-still-handles`). `-ProbePositions` reports every
   control spot. If the slot budget worries you, `AirlockVoyageClock 590` before the by-hand batch is legal (a
   rewind never replays Okafor's lines).
   **Mandatory proofs on your binary** (the code paths this task touches): `verify-floor.ps1` (`FLOOR OK`),
   `verify-ship-graph.ps1` (the line hold), `verify-fuel.ps1` (the valve and injector holds, the intake carry
   branch), `verify-cargo.ps1` (carry and put-down), `verify-voyage-clock.ps1` (the floor hop),
   `verify-two-clients.ps1` (client input through the hold). The reviewer runs the full twelve.

## Do not touch
`Ship/RefineryModel.*` (facts, not tunables), `Ship/FloorFixture.cpp`, `Ship/CargoModel.*`, `UI/TaskBoardWidget.*`,
`Voyage/*`, the maps, `Tests/RefineryFloorTests.cpp`, `Tests/RefineryModelTests.cpp`, the incident and fuel
systems. If the table's control positions collide with the tank mesh, move the controls, not the stills.

## Allowed files
New: `Ship/Still.*`, `Ship/StillControl.*`, `Tests/StillTests.cpp`, `Docs/reports/M5-3b-stills-and-knobs.md`.
Edit: `Ship/FloorFixture.h` (the three-line edit of item 1 only), `Ship/RefineryLayouts.*` (the control enum,
spec and table), `Core/AirlockGameMode.cpp` (the spawn class, the control spawns, the two lines),
`Core/AirlockCharacter.*` (the hopper branch, `ServerLoadStill`, the repeating hold, the refusal check),
`Core/Interactable.h` (`IsRepeatingHold`, `GetHoldRefusal`), `Core/AirlockPlayerController.*` (`AirlockPress` /
`ServerPress`), `Tests/DevCheatsTests.cpp` (one RPC line and the count), `Scripts/verify-floor.ps1`.

## Report
`Docs/reports/M5-3b-stills-and-knobs.md`: what changed (behaviour first), the control table with the probed
positions and a capture per still face, how it was verified (the proof tail, the captures, the test count, the
mandatory proofs' OK lines), what you were unsure about, anything touched outside the list, the exact commands.
Commit small: the still and its gauges, the control table and actors, the character verbs, the helper, the
tests, the proof, the report.
