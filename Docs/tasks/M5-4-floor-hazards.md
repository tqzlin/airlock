# Task M5-4: Floor hazards - spills with bodies, pops and flashes you can see, criticals on the board

Two parts, one brief. **M5-4a** (Claude, worktree, in parallel with Astra's M5-3b): the pure spill model and its
tests - no actors, no game-mode changes, nothing Astra's M5-3b touches. **M5-4b** (Astra, after the M5-3b and
M5-4a merges): the puddle actors, the crew's slip and stumble, the squeegee, the pop and flash bodies on the
still, the board rows, the proof. Design source: `Docs/design/refinery-floor.md` section 6 (the failure table).
Tom's tone note applies: quasi-realistic hazards, no concern for rating.

## Why this task

Since M5-3a the model spills, pops, flashes, fouls and fails, and the log and Okafor say so - but nothing on the
floor changes: a flood is a line in the log and 200 L that vanish. The design's risk is *physical*: crude on the
deck slows the crew and drops the cask they carry, a pop throws a cloud and a bang, a flash burns for three
seconds. This task gives the model's events bodies and consequences, so "things can go wrong" is something a
player sees, walks into, and cleans up.

## M5-4a: the pure model (Claude)

`Ship/FloorHazards.h/.cpp` in the house style (rates struct, state struct, deterministic ticks, no UObject
state, `operator==` on the replicated struct), `Tests/FloorHazardsTests.cpp`:

- `FFloorHazardRates`: `EvaporationLps` 1 (a 200 L flood lasts 200 s), `PpmPerLitreEvaporated` 0.05 (a 200 L flood
  fumes 10 ppm over its life; a pop's 300 L 15 ppm on top of the model's own `PopPpm` 60), `RadiusBaseCm` 50,
  `RadiusPerLitre` 0.75, `RadiusMaxCm` 250 (200 L -> 200 cm, 300 L -> 250 cm capped), `MergeDistanceCm` 250 (a
  second spill at the same still joins the first: litres add, the centre stays), `SlowFactor` 0.6 (a pawn whose
  feet are inside a puddle walks at 60 %, multiplied with the carry and exposure factors), `StumbleSeconds` 4 (a
  pawn carrying a cask inside a puddle drops it every 4 s of dwell; the clock resets on leaving),
  `SqueegeeSeconds` 6 (the hold), `SqueegeeLitres` 300 (one completed hold removes up to that; a merged puddle
  can take two).
- `FFloorSpillState { Id (S1, S2, ... in spawn order), StillId, Location (from the owner - the model keeps the
  point it was given), Litres, RadiusCm }`, `FFloorHazardState { Spills, SpillsTotal, LitresSpilled, LitresCleared,
  Stumbles }`.
- `FFloorHazards::Reset / Spill(StillId, Location, Litres) -> Id / Tick(dt, OutPpm) (evaporation; an empty puddle
  is removed) / Squeegee(Id) / Contains(Location) -> Id or None / Summary()` (`Spills 1: S1 by Still_2 200 L r=200`
  ... or `No spills`), `RadiusFor(Litres)` pure. Ticks are sub-stepped like the refinery's so a 4 Hz tick and a
  headless 1 s tick agree; ppm out is per tick, added by the owner through `AddPpm` like the stills' fumes.
- Tests: radius and cap, merge by distance, evaporation to removal with the ppm sum equal to litres x rate,
  squeegee partial and full, `Contains` on the edge, determinism over 1000 ticks, `Summary`.

Nothing else in 4a: the game mode, the character, the actors and the board are 4b's, after M5-3b merges (they
share files with it).

## M5-4b: the bodies (Astra)

1. **The game mode owns `FFloorHazards`** on the floor (built with the refinery, reset with it): every
   `ERefineryEvent::Spill` and `::Pop` from the model tick calls `Spill(Still, point, Litres)` at the still's
   **spill point** (a new column in the `RefineryLayouts` still table or a fixed offset `(0, -230, 0)` from the
   still, probed: on the deck in front of the hopper's row, where the crew walks). The 4 Hz tick evaporates and
   adds the ppm to the floor's air. The state is mirrored on the game state (`Hazards`, `OnRep_Hazards` ->
   `OnShipGraphChanged`) with log lines `Spill S1 by Still_2: 200 L (r 200 cm)`, `Spill S1 merged: 400 L`,
   `Spill S1 cleared by <Pawn>` / `... gone` (evaporated), `Stumble <Pawn> drops <Cask> in S1`. API:
   `HazardSqueegee(Id)`, `HazardSummary()`, `HazardContains(Location)`; console `AirlockFloor` (the hazard
   summary, plus each crew member's `inSpill=<Id|none>`), `AirlockSpill <Still> [Litres]` (a spill now, for
   proofs; cheat-gated).
2. **`ASpill` actors** (`Ship/Spill.h/.cpp`, folder `Floor/Spills`, spawned by the game mode per spill id and
   destroyed when the mirror drops the id): a flat disc on the deck (the engine's basic cylinder at radius / 50,
   height 0.02, with a dynamic instance of the basic-shape material tinted near-black with a blue sheen), a
   steam puff like the crates' (scaled 2.5, restarted while the puddle exists), a two-line label `SPILL` /
   `<litres> L - squeegee (hold E)`, radius following the mirror. It implements `IInteractable` as a 6 s hold
   (`Squeegee the spill (hold E)`) -> `HazardSqueegee`. Collision: Visibility only.
3. **The crew in the crude**: the server ticks each character's feet against `HazardContains` (a cheap loop, at
   most a few puddles); `bInSpill` replicated (owner-only is enough), `ApplyWalkSpeed` multiplies
   `SlowFactor` in, and a carrying pawn inside a puddle drops its cask through `ServerPutDown` every
   `StumbleSeconds` (`Stumble <Pawn> drops <Cask> in S1`; Okafor: `Cask down - {Pawn} went over in the crude.
   Get it up before it warms.`, 20 s cooldown). Blacked out in a puddle: nothing more (the exposure rules
   already cover the air).
4. **The pop and the flash on the still** (`AStill` from M5-3b): the game mode calls a multicast on the still for
   each event (`MulticastPop()`: a 0.5 s white point-light burst at the tank top and a large steam puff;
   `MulticastFlash()`: three seconds of orange flicker light and a puff at the tank top, then off). No audio
   assets exist; the report lists the sound cues wanted (a hiss rising with pressure, the bang, the whump) as a
   follow-up for the asset pass.
5. **Criticals on the board**: the game state gains `FloorCriticals` (the floor's blackout count, published by
   the game mode); the task board's floor rows add `Crew down - 1 critical (200 cr)` (red) while the count is
   above zero and `Spill by Still_2 - squeegee` per puddle after the still rows; the bunk fixture's second line
   reads `<n> down` / `empty`.
6. **Tests** (`Tests/FloorHazardInteractionTests.cpp`): the spill event spawns an actor at the still's point and
   the mirror carries it; a pawn placed in the puddle slows and, carrying, drops its cask after `StumbleSeconds`
   of world ticks; the squeegee hold clears it and the actor goes; the board rows; the pop multicast reaches a
   still (no crash headless).
7. **Proof**: `verify-floor.ps1` books the Milk run and secures **three** casks (C1 clamp, C2 rack, C3 clamp);
   on the floor, after C1's batch: `AirlockKnob Still_2 Feed 20` with C2 loaded -> `Refinery Spill Still_2 C2
   litres=200`, `Spill S1 by Still_2: 200 L`, the board's `Spill by Still_2 - squeegee`, capture; `AirlockCarry
   C3`, warp into the puddle -> `Stumble ... drops C3 in S1` within 5 s; warp out, `AirlockHold S1` ->
   `Hold S1 complete`, `Spill S1 cleared`; the horn -> `1 spill` in the floor result. The mandatory proofs on
   the binary: floor, cargo, fuel, atmosphere, two-clients; the reviewer runs the twelve.

## Do not touch (4b)
`Ship/RefineryModel.*`, `Ship/FloorHazards.*` (4a's facts; wire, do not tune), `Ship/CargoModel.*`, the maps,
`Tests/RefineryFloorTests.cpp`, `Tests/RefineryModelTests.cpp`, `Tests/FloorHazardsTests.cpp`.

## Allowed files (4b)
New: `Ship/Spill.*`, `Tests/FloorHazardInteractionTests.cpp`, `Docs/reports/M5-4b-floor-hazards.md`. Edit:
`Core/AirlockGameMode.*`, `Core/AirlockGameState.*`, `Core/AirlockCharacter.*`, `Core/AirlockPlayerController.*`,
`Ship/Still.*`, `Ship/FloorFixture.cpp` (the bunk line), `Ship/RefineryLayouts.*` (the spill point),
`UI/TaskBoardWidget.*` (the two rows), `Tests/DevCheatsTests.cpp` (the new RPC lines), `Tests/TaskBoardTests.cpp`
or the floor tests' board test (the rows), `Scripts/verify-floor.ps1`.

## Reports
`Docs/reports/M5-4a-floor-hazards-model.md` (Claude), `Docs/reports/M5-4b-floor-hazards.md` (Astra).
