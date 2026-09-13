# Task M5-4: Floor hazards - spills with bodies, a pop you can see, criticals on the board

Two parts, one brief. **M5-4a** (Claude, worktree, done on `claude/m5-4a-floor-hazards-model`): the pure spill
model and its tests. **M5-4b** (Astra, after the M5-3b, M5-2b and M5-4a merges): the puddle actors, the crew's
slip and stumble, the squeegee, the pop burst on the still, the board rows, the proof. Design source:
`Docs/design/refinery-floor.md` section 6 (the failure table). Tom's tone note applies: quasi-realistic hazards,
no concern for rating. Part b was red-teamed before launch; where it disagrees with the design note, the brief
is final.

## Why this task

Since M5-3a the model spills, pops, flashes, fouls and fails, and the log and Okafor say so - but nothing on the
floor changes: a flood is a line in the log and 200 L that vanish. The design's risk is *physical*: crude on the
deck slows the crew and drops the cask they carry, a pop throws a cloud. This task gives the model's events
bodies and consequences, so "things can go wrong" is something a player sees, walks into, and cleans up.

## M5-4a: the pure model (Claude, done)

`Ship/FloorHazards.h/.cpp` and `Tests/FloorHazardsTests.cpp` (4 tests, 142 total; report
`Docs/reports/M5-4a-floor-hazards-model.md`): `FFloorHazardRates` (`EvaporationLps` 1, `PpmPerLitreEvaporated`
0.05, radius `50 + 0.75 x litres` capped at 250 cm, `MergeDistanceCm` 250, `ZToleranceCm` 60, and the owner's
rules `SlowFactor` 0.6, `StumbleSeconds` 4, `SqueegeeSeconds` 6, `SqueegeeLitres` 300), `FFloorSpillState { Id
S1.., StillId, Location, Litres, RadiusCm }`, `FFloorHazardState { Spills, SpillsTotal, LitresSpilled,
LitresCleared, Stumbles }`, `FFloorHazards::Reset / Spill(Still, Location, Litres) -> Id / Tick(dt, OutPpm) /
Squeegee(Id) / Contains(Feet) / CountStumble / RadiusFor / Find / Summary`. Evaporation is linear (a 4 Hz tick
and a 10 s tick agree exactly); `Tick` returns true when a whole litre, a whole centimetre or a removal would
publish; a puddle within the merge distance at deck height takes new litres and keeps its centre.

## M5-4b: the bodies (Astra)

Read first: `Ship/FloorHazards.h`, `Ship/Still.h/.cpp` and `Ship/StillControl.*` (M5-3b), `Ship/FuelValve.cpp`
(steam FX restart, lines around `OnSystemFinished`), `Ship/FloorFixture.cpp` (`SetLine2`, the retry-bound mirror
refresh), `Core/AirlockCharacter.cpp` (`Tick`'s authority branch, `TickCrewExposure`, `OnRep_Exposure ->
ApplyWalkSpeed`, `PutDownLocation`, `ServerPutDown`), `Core/AirlockGameMode.cpp` (`TickRefinery`,
`PublishRefinery`, `ApplyRefineryEvents`, `SpeakFloorLine`, `NotifyBlackout`), `Tests/StillTests.cpp` (the test
world with `CrewAt` / `Carry`).

1. **The game mode owns `FFloorHazards`** on the floor: built in `BuildRefinery`, reset there and again in the
   clock-start block next to `AtmosphereModel.Reset()`. **Only `ERefineryEvent::Spill` reaches it** - the model
   emits its own `Spill` event after every `Pop` with the pop's litres (`RefineryModel.cpp`, the pop branch), so
   feeding `Pop` too would double or zero the puddle; `Pop` only triggers the still's burst (item 4). The spill
   point: `FFloorFixtureSpec` gains `FVector SpillOffset` (stills: `(0, -230, 0)`), `RefineryLayouts::SpillPoint(Spec)
   = Location + Rotation.RotateVector(SpillOffset)` (never `TransformPosition` - the still's transform carries
   scale 0.8), added to `-ProbePositions` as `Still_<n>_Spill`. Consequence to keep, not fix: a 200 L puddle at
   (-5400, 1670, -65) covers the handle row and the hopper (the design's point - the crew works in the crude
   until it is cleared); Still 1's and Still 2's points are 400 cm apart and never merge. `TickShipGraph` ticks
   the hazards after `TickRefinery()` and before `TickFuel()`, the ppm through `AtmosphereModel.AddPpm` +
   `PublishAtmosphere()` exactly like the stills' fumes.
   API on the game mode: `FName HazardSpill(FName Still, float Litres)` (resolves the spill point; the event
   path and the console both use it; `Spill rejected: no such still`), `bool HazardSqueegee(FName Id, APawn* By)`,
   `FName HazardContains(const FVector& Feet) const`, `FString HazardSummary() const`. Logs: `Spill S1 by Still_2:
   200 L (r 200 cm)`, `Spill S1 merged: 400 L` (when the returned id already existed), `Spill S1 cleared by
   <Pawn>` (a hold), `Spill S1 gone` (evaporated), `Stumble <Pawn> drops <Cask> in S1`.
2. **The mirror**: `AAirlockGameState` gains `UPROPERTY(ReplicatedUsing=OnRep_FloorHazards) FFloorHazardState
   FloorHazards;`, `PublishFloorHazards(const FFloorHazardState&)`, `OnRep_FloorHazards()` ->
   `OnShipGraphChanged`, a `DOREPLIFETIME` line - the `PublishRefinery` pattern. The game mode's
   `PublishFloorHazards()` floors `Litres` and `RadiusCm` (`FMath::FloorToFloat`, the model's own `Published`),
   leaves `Location` and the totals as they are, and diffs the old mirror's ids against the new: a new id spawns
   an `ASpill`, a missing id destroys it and logs `gone`. `PublishFloorHazards` (server) is the only place an
   `ASpill` is spawned or destroyed. Also `UPROPERTY(Replicated) int32 FloorCriticals` on the game state: the
   game mode's `BlackoutCount` published from `NotifyBlackout` when `IsFloorSection()`, reset in the clock-start
   block (it is already the `Criticals` of `RefineryTally`).
3. **`ASpill` actors** (`Ship/Spill.h/.cpp`, `AActor` + `IInteractable`, folder `Floor/Spills`, spawned with
   `Params.Name = Id` and `SetActorLabel(Id)` under `WITH_EDITOR` so `AirlockHold S1` finds it): the engine's
   `/Engine/BasicShapes/Cylinder.Cylinder` (100 x 100 x 100, origin centred) at scale `(r / 50, r / 50, 0.02)`
   and relative Z +1 so it clears the deck, with a dynamic instance of `/Engine/BasicShapes/BasicShapeMaterial`
   (`SetVectorParameterValue("Color", FLinearColor(.01f, .015f, .03f))`, `SetScalarParameterValue("Roughness",
   .15f)`) - load both with `LoadObject` like the cube fallback, not `ShipVisuals::LoadBody` (its fallback raises
   the mesh 50 cm); an invisible `UBoxComponent` marker of extent `(r, r, 30)` (the `AStillControl` pattern:
   QueryOnly, Visibility only) so the E trace has something to hit from eye height, the mesh Visibility-only
   too; the crates' steam (`PS_SteamDoor`, scaled 2.5, `OnSystemFinished` restart, the `FuelValve.cpp` lines)
   while the puddle exists; a two-line label through `ConfigureLabel(Label, 14, -90, White, (0, 0, 40))`:
   `SPILL` / `<litres> L - squeegee (hold E)`. Litres and radius come from the mirror with the retry-bound
   refresh (`FloorFixture.cpp`'s `BindState`); the disc and the marker rescale on every refresh. The hold: 6 s
   (`FFloorHazardRates().SqueegeeSeconds`), prompt `Squeegee the spill (hold E)`, `OnHoldComplete` ->
   `HazardSqueegee(Id, Pawn)`. Squeegeeing from inside the puddle is allowed (slowed, not refused); the hold reach
   is 312 cm from the actor's centre, so a fresh 250 cm pop puddle leaves a 62 cm ring to stand in dry. An actor
   destroyed inside its own hold is safe (the character reads the label before `ResetHold`; an evaporated target
   is dropped by the `IsValid` guard) - the test ticks once more before asserting the actor count.
4. **The crew in the crude** (`Core/AirlockCharacter.*`): `UPROPERTY(ReplicatedUsing=OnRep_InSpill) FName InSpill`
   with `COND_OwnerOnly`; the server sets it every frame in `Tick`'s authority branch next to `TickCrewExposure`,
   from `GM->HazardContains(Feet)` while `GM->RefineryHasFloor() && GS->bVoyageActive`, where `Feet =
   GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight())` (the capsule's half
   height is 96: the actor's origin stands 96 cm above the deck and `ZToleranceCm` is 60); on a change:
   `ApplyWalkSpeed(); ForceNetUpdate();`; `OnRep_InSpill` calls `ApplyWalkSpeed`, which gains
   `* (InSpill.IsNone() ? 1.f : FFloorHazardRates().SlowFactor)`. The stumble: a dwell clock that runs only while
   `InSpill` is set, `CarriedCrate` is valid and `!bBlackedOut`, resets on leaving the puddle and after each drop;
   at `StumbleSeconds` log `Stumble <Pawn> drops <Cask> in S1`, then `ServerPutDown()` (the cask lands 120 cm
   ahead, floor-snapped - usually still in the crude, which is the design's punishment: "get it up before it
   warms"), then the game mode's `CountStumble` + publish and Okafor through `SpeakFloorLine(TEXT("Stumble"),
   ...)`: `Cask down - {Pawn} went over in the crude. Get it up before it warms.` Holds and presses are not
   refused in a puddle.
5. **The pop on the still** (`Ship/Still.*`, no RPC, no game-mode change): `AStill` caches `LastStatus`; in its
   `Refresh`, a transition `Running -> Popped` plays a 0.5 s white point-light burst at the tank top and one large
   steam puff (actor timers, both sides); the first refresh only sets the baseline so a late joiner gets no
   burst. The flash's flicker is deferred (a follow-up in the report); no audio assets exist - list the cues
   wanted (the hiss, the bang) as a follow-up.
6. **The board** (`UI/TaskBoardWidget::BuildRows`, new *defaulted trailing* parameters `const FFloorHazardState*
   Hazards = nullptr, int32 FloorCriticals = 0` - the do-not-touch `RefineryFloorTests.cpp` calls it with seven
   arguments): the criticals row first (`1 critical - 200 cr medical`, red, persistent - the count is a total and
   nobody is carried to the bunk in the MVP), then the still block (popped / flash / failed / pressure), then one
   `Spill by <Still> - squeegee` per puddle (white), then the fuel and air rows and the rest as today. The bunk
   fixture's second line through the existing `AFloorFixture::SetLine2` (find it by `GetFixtureId() == "Bunk"`;
   it replicates): `<n> critical` / `empty`. No edit to `FloorFixture.cpp`.
7. **Console** (cheat-gated Server RPCs, the count in the every-RPC test 32 -> 34): `AirlockFloor` (the hazard
   summary and `FloorCriticals`), `AirlockSpill <Still> [Litres]` (`HazardSpill`, default 200; bypasses the
   model, so it does not count in the floor result's `spills`).
8. **Tests** (`Tests/FloorHazardInteractionTests.cpp`): copy `FFloorTestWorld` as `FHazardTestWorld` (unity
   builds; the original is in a do-not-touch file's anonymous namespace) with `CrewAt` / `Carry` from
   `StillTests.cpp`; one spill through the model (heat 6 / feed 20 / load C2, `Tick(6)` after `Running`) proves
   the event wiring and the actor at the spill point; the rest through `Mode->HazardSpill("Still_2", 200)`: a
   pawn whose feet are placed in the puddle (`CrewAt(SpillPoint + (0, 0, 96))`, the world's gravity-free pawn
   pattern) slows, and carrying, drops its cask after `Tick(4.25)` (`CarriedCrate == nullptr`,
   `FloorHazards.Stumbles == 1`); `Crew->ServerBeginHold(Spill); Tick(6.25)` clears it and one more tick
   destroys the actor; the board rows; `AStill`'s burst on a `Popped` transition does not crash headless.
9. **Proof** - `Scripts\verify-floor.ps1`, captures to `M5-4b`:
   - Ship leg, after `Cargo C2 Racked`: `AirlockWarp -5330 1700 116`, `AirlockCarry C3`, `AirlockWarp -5430 1400
     116`, `AirlockPlace Clamp_3` -> `Cargo C3 Secured` (C3 is heavy: it never vents on the floor). Assertions:
     `Ship result:` contains `3 casks to the floor`, `Slot clock started` `casks=3`, `Cargo C3 Loose` on the
     floor; delete the `if (Get-FloorProofLines ': Cargo C3 ')` throw.
   - Floor, unchanged through the M5-3b by-hand batch up to `Still Still_2 Running cask=C2` (about 75 s into the
     slot, well before a loose light cask would vent at ~250 s). Then at once: `AirlockKnob Still_2 Feed 20` ->
     within 6 s `Refinery Spill Still_2 C2 litres=200`, `Spill S1 by Still_2: 200 L`, Okafor's flood line;
     `AirlockKnob Still_2 Feed 10`; capture `floor-spill` from (-5400, 1450) looking +Y with the board's `Spill by
     Still_2 - squeegee` in frame. The M5-3b calibrate hold, reset refusal and gauge captures follow as they are
     (the pawn stands in the puddle: slowed, not carrying, its holds unaffected).
   - Then `AirlockWarp -5920 1560 116`, `AirlockCarry C3` (row one's third spot), `AirlockWarp -5400 1600 116`
     (70 cm from S1's centre) -> `Stumble ` ... `drops C3 in S1` within 6 s, then `PutDown C3 by`; `AirlockWarp
     -5400 1450 116` (outside the puddle, inside hold reach), `AirlockHold S1` -> `Hold S1 complete by`, `Spill S1
     cleared by`; `AirlockStillLoad Still_1 C3` (Still 1 queues it and starts it the tick after C1's `BatchDone`).
   - Waits: the existing `Tank 3[0-9][0-9] L`, `BatchDone Still_1 C1`, `BatchDone Still_2 C2` (early: 200 L less),
     plus `BatchDone Still_1 C3`, all before ~300 s of the 600. Tally and horn assertions become `3/3`; `Floor
     result:` must contain `1 spill, 0 pops`; keep the `$litres -lt 600` check on the tank total (do not assert
     C2's yield: its bath drops 30 K under the boil point after the flood and makes tar for ~30 s). Update the
     `FLOOR OK` message.
   **Mandatory proofs on your binary**: `verify-floor.ps1` (`FLOOR OK`), `verify-cargo.ps1` (carry and put-down),
   `verify-two-clients.ps1` (walk speed and put-down over the network). The reviewer runs the twelve.

## Do not touch (4b)
`Ship/RefineryModel.*`, `Ship/FloorHazards.*` (4a's facts; wire, do not tune), `Ship/CargoModel.*`,
`Ship/FloorFixture.*`, the maps, `Tests/RefineryFloorTests.cpp`, `Tests/RefineryModelTests.cpp`,
`Tests/FloorHazardsTests.cpp`, `Tests/StillTests.cpp`.

## Allowed files (4b)
New: `Ship/Spill.*`, `Tests/FloorHazardInteractionTests.cpp`, `Docs/reports/M5-4b-floor-hazards.md`. Edit:
`Core/AirlockGameMode.*`, `Core/AirlockGameState.*`, `Core/AirlockCharacter.*`, `Core/AirlockPlayerController.*`,
`Ship/Still.*` (the burst), `Ship/RefineryLayouts.*` (`SpillOffset`, `SpillPoint`), `UI/TaskBoardWidget.*` (the
two rows and the defaulted parameters), `Tests/DevCheatsTests.cpp` (the two RPC lines and the count),
`Tests/TaskBoardTests.cpp` (the rows), `Scripts/verify-floor.ps1`.

## Reports
`Docs/reports/M5-4a-floor-hazards-model.md` (Claude, done), `Docs/reports/M5-4b-floor-hazards.md` (Astra: what
the crew can now walk into and clean up, the probed spill points, the judgement calls, the test names, the
FLOOR OK tail with the spill, the stumble and the squeegee, the captures, the follow-ups).
