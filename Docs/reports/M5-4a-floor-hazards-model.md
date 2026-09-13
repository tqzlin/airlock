# Report M5-4a: The pure spill model

Claude, 2026-09-13 (midday, in parallel with Astra's M5-3b). Branch `claude/m5-4a-floor-hazards-model` from `main`
at f92eb5c. Brief: `Docs/tasks/M5-4-floor-hazards.md` (part a). Pure C++ only: `Ship/FloorHazards.h/.cpp`,
`Tests/FloorHazardsTests.cpp`. Nothing Astra's M5-3b touches.

## What the model does

`FFloorHazards` is the crude on the deck. `Spill(Still, Location, Litres)` puts litres down at a point: a puddle
within `MergeDistanceCm` 250 (and within `ZToleranceCm` 60 of height - a platform 87 cm up is not the deck) takes
the litres and keeps its centre, otherwise a new puddle `S1, S2, ...` appears. Radius = `50 + 0.75 x litres`,
capped at 250 cm (a 200 L flood is a 2 m puddle, a 300 L pop a 2.5 m one). `Tick(dt, OutPpm)` evaporates
`EvaporationLps` 1 L/s per puddle and returns `0.05 ppm` per litre gone (a flood fumes 10 ppm over its 200 s);
a dry puddle is removed; the return value says whether a published (whole-litre, whole-centimetre) figure
changed. `Squeegee(Id)` takes up to `SqueegeeLitres` 300 off one puddle (a merged 400 L puddle takes two holds).
`Contains(Feet)` names the puddle a point stands in (XY distance within the radius, height within tolerance).
`CountStumble` and the totals (`SpillsTotal`, `LitresSpilled`, `LitresCleared`, `Stumbles`) are the record for the
floor result. `Summary()`: `No spills` or `Spills 2: S1 by Still_2 200 L r=200; S2 by Still_1 300 L r=250`.

The rates the owner applies rather than the model (`SlowFactor` 0.6, `StumbleSeconds` 4, `SqueegeeSeconds` 6)
live in `FFloorHazardRates` so 4b reads them from one place.

## Decisions

- **Linear evaporation, no sub-steps.** The refinery model sub-steps because its rates couple; a puddle's litres
  fall linearly, so a 4 Hz tick and a 10 s tick agree exactly (a test pins it). The brief's "sub-stepped like the
  refinery's" was not needed.
- **Merging keeps the first centre.** Two floods at one still make one bigger puddle where the first landed; the
  actor's disc grows in place instead of two discs overlapping.
- **`Contains` is the model's, the slow and the stumble are the owner's.** The character's walk-speed composition
  and the put-down verb already exist; 4b multiplies the factor in and calls `ServerPutDown` on the dwell clock.
- **Spill points are 4b's.** The model takes a location; where a still overflows (in front of its hopper) is a
  layout-table decision that needs a probe.

## Tests
`Airlock.Floor.Hazards.RadiusMergeIdsAndRejections`, `EvaporationFumesAndPublication`,
`SqueegeeContainmentAndStumbles`, `DeterministicOverAThousandTicks` - **142 / 142** with the rest of the suite on
the worktree build (`Scripts\run-tests.bat Airlock`).

## Nothing player-visible
No PIE proof: the model has no actors until 4b; the twelve proofs are unaffected (no shared file changed).

## Outside the brief
Nothing. `Docs/tasks/M5-4-floor-hazards.md` (both parts) is on this branch so 4b's brief travels with the model.
