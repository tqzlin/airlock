# The Still: the refinery floor (design note, 2026-09-13)

Claude, from Tom's decisions of 2026-09-13 on the three open questions in `refining-run.md` section 7: names are
mine to pick, hazards stay quasi-realistic with no concern for rating, and the refinery becomes **a single
walkable room with refining units** where the crew dumps crude, turns knobs and gets a varying yield of diesel,
with real risk and skill in the process (spills, fumes that put people down, units that fail when miscalibrated or
fed at the wrong rate). This note is the design and the plan; it becomes the contract for the M5 briefs.

## 1. The verdict first

The refinery is the second half of every run and the half that pays. The ship leg gets casks to Halder intact;
the floor leg turns those casks into diesel. A crew that hauls twelve casks and refines them badly earns less
than a crew that hauls four and refines them well, so the two halves pull on each other: careful in the hold,
skilled at the still. The floor is one room, three stills, ten minutes on the clock, and three knobs per still
that fight each other - heat, feed and vent - on top of a calibration that drifts. Everything that can go wrong
is a consequence of a knob left wrong for too long, it announces itself before it happens (a needle climbing, a
hiss, frost on the floor), and it is fixed by walking over and doing something with your hands. Solo, you run one
still and keep it alive; with three, one cooks, one loads and cleans, one keeps the air breathable and the traps
calibrated.

## 2. Decisions taken (final for the MVP; ids in code never change)

**Names.** The moon is **Vell**, the gas giant **Halder**, the slurry **Vell crude** ("sludge" to crews), the hub
is the **Vell Extractor** ("the Rig"), the destination is the **Halder Solar Refinery** ("the Still" to crews), the
ship class is the **Hauler Mk1** and this hull is the **MV Corrin**; the refined fuel cut is **diesel** - what every
crew and every label calls the refinery's clean fuel, though a chemist would say liquefied methane with the ethane
blended back for density, methalox grade. The refinery's floor boss is **Okafor**, heard on the floor speakers the
way Captain Tanaka is heard on the ship. The Rig's operator is the **Vell Extraction Co-op**; the refinery is run
by **Halder Solar**. These are now the names in the code strings, not placeholders.

**Tone.** Quasi-realistic. Nitrile fumes give headache, dizziness, a stumble, then collapse; a crew member down in
bad air keeps taking exposure and a second collapse is **critical**: out for the rest of the session, carried to
the bunk, a medical fine, and the boss says so plainly. Spilled crude is a cryogenic liquid: frost, boiling fumes,
a burn if you stand in it carrying nothing between you and the floor. Nothing is played for gore; nothing is
softened either - no med-bot line, no cartoon fainting.

**The refinery is a place.** Docking at Halder now lands the crew on **the floor**: a third section
(`RefineryFloor`), one large room, walkable, with its own clock. The card reads `DOCKING - HALDER SOLAR REFINERY` /
`cargo off - your slot on the floor starts now`; the return leg to the Rig stays a card.

## 3. What refining Vell crude actually is (and why the knobs are what they are)

Vell crude arrives at about 92 K: a slurry of liquid methane and ethane with dissolved nitriles and suspended
tholin solids. Refining it is not distilling a heavy oil; it is **warming it gently so the fuel boils off, catching
that vapour cold, and leaving the poison and the tar behind**:

| Temperature of the separator | What comes off | In the game |
|---|---|---|
| under 140 K | almost nothing - the fuel stays in the sludge | low yield, tar-heavy, the batch takes forever |
| 150-200 K (the window; 160 K for light casks, 180 K for heavy) | methane then ethane: the **diesel cut**, re-liquefied in the cold trap | the yield band; where a good cook keeps the needle |
| from 230 K | **nitriles** (hydrogen cyanide boils at 299 K but comes off the slurry well before) carry into the vapour | the product is poisoned (yield counts as gas, not diesel) and the trap vents it: room ppm climbs |
| from 290 K | the tholins **char** on the heater coils | coke builds, the still fouls, then fails |
| from 320 K with gas in the room | methane in air burns between 5 and 15 % | the **flash**: a short fire on the still, the batch lost, the unit damaged |

The three controls follow from that:

- **Heat** sets how hard the coils drive the separator. The temperature chases the setting slowly (it is a bath of
  cryogenic liquid), and every litre of cold crude fed in pulls it down.
- **Feed** meters crude from the hopper into the separator. Too fast and the bath floods and crashes cold: the
  separator overflows through the weir and crude hits the floor (the **spill**). Too slow with the heat up and the
  coils run dry: coke.
- **Vent** bleeds the boil-off to the cold trap. The trap has a rate limit: vent faster than it can condense and
  the excess goes to the room (fumes); vent slower than the boil-off and pressure climbs to the relief valve (the
  **pop**: a burst of gas into the room and the still shut down).
- **Calibration** is the trap's condenser set point, which drifts as it heat-soaks; a drifted trap loses diesel to
  gas even at a perfect temperature. Re-centring it is a three-second hold at the trap gauge.

Honesty box: real separators are continuous, not batch, and a real cold trap is a column; the game's still is a
batch flash with one vapour line, and one room has one uniform air number, as on the ship.

## 4. The floor

One room, roughly the Engineer hall's size (about 30 x 12 m, 4.8 m clear), walkable end to end, no vista rooms.
From the dock airlock (the way in, and the card home) along the long axis:

- **Cask staging** (near the airlock): pallets where the dock crew have put the run's *intact* casks - as many as
  the tally counted at the ship's dock, light and heavy marked (`C2` / `light`). Vented and burned casks never
  arrive. This is the coupling: the ship leg decides how much there is to refine.
- **Three stills** in a row along one wall, three metres apart, each an island a crew member can walk around:
  the **hopper** on the walkway side (place a carried cask: press E; holds up to three casks), the **knob panel**
  with `HEAT`, `FEED` and `VENT` (each a pair of handles, `-` and `+`; press E for one step of 5 %, hold E to keep
  stepping), the **gauge stack** above the panel (temperature, pressure, feed, yield of the current batch, the
  trap's calibration needle with a `CALIBRATE` hold), a **reset lever** (`RESET` hold after a pop, `PURGE` hold
  during a flash, `SCRAPE` hold to clear coke when idle, `REPAIR` hold when failed), the separator's **weir lip**
  where an overflow spills, and the relief stack where a pop vents.
- The **product tank** at the far end: one big level gauge (`DIESEL 6,420 L`) and the ship's **refuel line** off
  it (the ship takes its fill first, the rest is the surplus that goes home as base fuel).
- The **scrubber panel** on the wall opposite the stills: the floor's life support, the same press verb as the
  ship's (`Scrubbers: Normal - E for hazard mode`), sized for a refinery - Hazard mode here is strong, but three
  stills venting to the room out-run it.
- The **floor drain** in the middle of the walkway: a spill is cleared by a hold at the puddle (`Squeegee to the
  drain (hold E)`, 5 s), or it boils off on its own in two minutes, feeding the room fumes the whole time.
- The **bunk** by the airlock: where a critical crew member ends up, and where the session's medical fine is
  read out. A speaker grille over it is Okafor.
- Signs, code-spawned like the Rig's: `HALDER SOLAR - FLOOR 3` over the airlock, `NO NAKED FLAME - METHANE` over
  the stills, `SCRUBBERS` over the panel.

Two crew can work one still without bumping (walkway 3 m); three stills fit three crew at three stills or one
cook running between two while the third crew member loads and cleans.

## 5. The still: one batch, start to finish

Numbers are starting values for the pure model (`FRefineryModel`, ticked at 4 Hz like the other models); the
reviewer tunes from proofs and the first playtest.

- **Charge.** A cask is 1,000 L. The hopper meters it into the separator at `Feed x 20 L/s` (feed 50 % = 10 L/s = a
  cask in 100 s). Three stills at a sensible feed clear twelve casks in about seven of the ten minutes - if nothing
  goes wrong.
- **Temperature.** `T` chases `T_set = 92 + Heat x 260` (heat 35 % is 183 K) at 2 K/s, minus `FeedFlow x 1.2 K/s`
  of cooling while crude comes in; an empty separator with the heat up climbs past the window on its own, which is
  how a still left running dry cokes.
- **Yield.** Every litre processed at temperature `T` splits by `Y_diesel = 0.85 x bell(T, T*, 22 K) x (0.5 + 0.5 x Cal)`,
  `Y_gas = 0.05 + 0.6 x clamp((T - 225) / 60)` (nitriles and cracked light gas, worthless) and `Y_tar = 1 - Y_diesel -
  Y_gas` (tholin sludge, a disposal fee). `T*` is 180 K for a heavy cask and 160 K for a light one; the bell is 1 at
  `T*` and 0 at plus or minus 22 K. A perfect batch is 85 % diesel; a sloppy one 40-60 %; a hot one is mostly gas.
- **Pressure.** Boil-off makes pressure: `dP/dt = 0.02 x max(0, T - 130) x (Charge / 1000) - Vent x 3.0` bar/s, floor
  1 bar. The trap condenses up to `Vent 60 %` worth; the excess above that goes to the room at `40 ppm/s x
  (Vent - 0.6)`. At `P >= 3.0 bar` the relief **pops**.
- **Calibration** drifts `-0.4 %/s` while the still runs; the `CALIBRATE` hold (3 s) resets it to 100 %. A trap at
  50 % loses a quarter of the diesel to gas.
- **Coke** accrues `+0.6/s` while `T > 290 K`, and `+0.3/s` while the coils are up (`Heat > 0.5`) with nothing being
  fed; at 60 the still is **Fouled** (yield halved); at 100 **Failed** (stops). `SCRAPE` (8 s, idle still) clears it;
  `REPAIR` (12 s) brings a failed still back with coke at 40.
- A batch's yield is announced when its cask is done (`Still 2 batch 3: 640 L diesel, 180 L gas, 180 L tar`) and
  the tank's gauge climbs. The run's diesel total is what pays.

## 6. What goes wrong, how it reads, how it is fixed

| Failure | Cause | Warning before it | The event | Fix |
|---|---|---|---|---|
| **Spill** | feed above 80 % for 5 s, or feeding a popped / failed still, or the hopper overfilled | the feed needle in the red, the bath temperature crashing, frost at the weir | crude overflows the weir: a **puddle** actor on the floor (radius grows with litres, 250 cm max), frost, a boiling haze; standing in it slows you to 60 % and, carrying a cask, a stumble every few seconds drops it; the puddle adds `0.02 ppm/s per litre` to the room while it lasts | `Squeegee to the drain (hold E)` 5 s at the puddle, or wait two minutes |
| **Fumes** | venting past the trap's limit, a pop, a flash, a spill, a poisoned batch | the scrubber row on the board (`Floor air 34 ppm - scrub`), Okafor's line at 20 and 50 ppm, coughing at 60 % exposure | the ship's exposure rules: slow at 60 %, **collapse** at 100 % (30 s down, exposure back to 50); a crew member down while the room is above 50 ppm keeps taking exposure, and a second collapse within 90 s of getting up is **critical**: they are carried to the bunk for the rest of the slot (spectator), and the session pays a 200 cr medical fine | scrubbers to Hazard, stop the venting source, clear the spill; drag is not in the MVP |
| **Pop** | pressure reaches 3 bar (vent too low for the boil-off) | the pressure needle in the red for ten seconds, a rising hiss | the relief bursts: `+60 ppm` to the room at once, the still shuts down, the charge in the separator drains to the floor as a spill | `RESET` hold 4 s, then start again with the vent higher |
| **Flash** | `T > 320 K` while the room holds gas (ppm above 40) | the temperature needle far past the window, the room already bad | a short fire on the still (three seconds of flame, `+40 ppm`), the batch lost, the still's condition takes 30 %; a still at 0 % condition is Failed | `PURGE` hold 5 s (floods the separator with nitrogen: the batch is gone, the fire is out) |
| **Coke / Fouled / Failed** | heat up with no feed, or cooking above 290 K | the coke gauge climbing, yields falling, the coils glowing | Fouled halves yield; Failed stops the still until repaired | `SCRAPE` 8 s (idle), `REPAIR` 12 s (failed) |
| **Drift** | time | the calibration needle off centre, Okafor's "trap two's drifting" | diesel quietly becomes gas | `CALIBRATE` 3 s |
| **Out of time** | the ten-minute slot | Okafor at five minutes and one minute | unrefined casks are sold as crude at 40 cr each; a batch mid-process is lost | none - plan the batches |

Every warning is something you can see or hear from the walkway without the HUD: the needles (the gauge stack is
a label like the ship's, `STILL 2` / `T 176 K  P 1.9 bar  feed 50%  cal 82%`, red words when out of band), the
hiss (a placeholder sound loop that rises with pressure), frost and haze on the floor, the coils' glow, and
Okafor's lines. The task board lists what is open on the floor the way it does on the ship: `Still 2 - pressure
high` (red), `Spill by still 1 - squeegee`, `Floor air 34 ppm - scrub`, `Still 3 - calibrate`, `Still 1 - fouled`.

## 7. Time, pay, fuel

- **The slot** is 600 s on a replicated clock like the voyage's (`FloorEndServerTime`), started when the floor
  section begins play, with Okafor's lines at 0, 300, 540 and 600 s. At 0 the card `UNDOCKING - HOME TO THE VELL
  EXTRACTOR` takes everyone to the hub.
- **Pay** replaces the run's flat manifest pay for delivered casks with the refinery's price for what was made:
  `0.5 cr per litre of diesel` (a perfect cask is 850 L = 425 cr; a poor one 400 L = 200 cr), plus 40 cr per cask
  sold unrefined, minus `10 cr per 100 L of tar` disposal, minus 200 cr per critical crew member, minus the ship
  leg's existing fines (soot, late dock, blackouts). The hull bonus and the clean-air bonus stay on the ship leg.
  A twelve-cask Priority run refined well is worth about 4,500 cr; hauled and sold as crude, 480 cr.
- **Fuel.** The ship refuels from the product tank first (it needs about 80 units, 1 unit per 10 L, so one good
  cask); every litre after that is surplus and goes home as **base fuel** at 1 t per 100 L. The M3-7 flat credit
  (15 t per intact cask) goes away: the base's stock now depends on how well the floor was run.
- The tally line grows: `Voyage result: ... | floor: 9/12 casks refined, 6,420 L diesel, 1,100 L tar, 1 spill, 0 pops,
  1 critical, pay 3,180, fuel +54 t`. The hub board's `Last run:` shows the diesel litres and the pay.

## 8. Crew, skill and why it stays interesting

- **Solo**: one still, feed 50 %, heat 35 %, vent 55 %, calibrate every 90 s, carry a cask every 100 s. Clean but
  slow: four or five casks in the slot. A second still is the solo gamble.
- **Two**: a cook on two stills and a loader who also handles spills and the scrubbers.
- **Three**: three stills, or two stills plus one crew member on air and calibration for all three.
- **Skill** is reading needles and pacing: the three knobs are coupled (feed cools, heat boils, boil-off needs
  vent, vent past the trap makes fumes), light and heavy casks want different windows, the trap drifts, and the
  room's air is a shared budget between stills. The first mistake is always recoverable; the second one on top of
  it is what ends a session on the bunk.
- **Legibility** is the M3-8 policy applied: two-line labels on every gauge and handle, summaries on the board,
  and the failures each with a light, a sound and a shape on the floor.

## 9. Milestones (M5, "The Still"), owners, and what runs in parallel

| Task | Owner | Scope |
|---|---|---|
| **M5-1 Refinery model** | Astra | `Ship/RefineryModel.*`: pure, deterministic, 4 Hz; stills, knobs, batches, pressure, calibration, coke, the six failures as events, spills as litres at a still, the floor tally; ~20 tests. No actors, no maps. |
| **M5-2 The floor** | Claude | the `RefineryFloor` map (a copy of a pack hall with the dock airlock, signs, the drain), `Ship/RefineryLayouts` (still, tank, panel, pallet, bunk spots - probed), the section wiring in the game mode (the floor clock, delivered casks from the tally, the return card, the extended result and pay, base fuel from diesel), the names in every string. |
| **M5-3 Still actors and verbs** | Astra | `AStill` (hopper, handles, gauge stack, lever), the repeat-while-held press for handles, `AProductTank`, task-board rows, Okafor's lines, labels per the M3-8 policy. |
| **M5-4 Floor hazards** | Astra or Claude | puddles (spawn, growth, slip, stumble, squeegee, boil-off), the pop and flash FX and sounds, the critical rule and the bunk, the floor's scrubber panel. |
| **M5-5 The whole run** | Claude | `verify-floor.ps1` solo and the two-client run extended to the floor; tuning from the captures; the first two-person playtest. |

M5-1 runs first on Astra's side while Claude builds M5-2 (map and wiring); M5-3 and M5-4 follow the model. The
ship leg is untouched except for the tally and the docking card.

## 10. Risks

- **Ten minutes of knobs** could be tedious if the dynamics are too slow or too twitchy; the model's constants
  are in one struct and the first playtest tunes them. The floor slot length is a constant too.
- **A third map** is a level pass: reuse the Engineer hall like the ship did, keep the props off the walkway (the
  hold's lesson), probe every spot.
- **Balance shifts to the floor.** The ship leg still matters (intact casks are the charges, and a burned or vented
  cask never reaches the still), but a bad cook wastes a good haul; the pay curve above is a first guess.
- **Sound placeholders** matter more here than anywhere so far (the hiss is the pressure warning); the first pass
  can be engine tones.

## 11. Changes at brief time (2026-09-13, after a second reader simulated the numbers)

Recorded so the note and the M5-1 brief never disagree; the brief wins.

- **Pressure is a valve, not a switch.** The first draft's gain and bleed made the still either pinned at 1 bar or
  through 3 bar in two seconds, with no warning and no way to run hot at all. Now `dP = 0.002 x (T - 130) x
  (charge fraction) x (feeding ? 1 : 0.3) - Vent x 0.3 x (P - 1)` per second: about 1.6 bar at the recipe, 2.8 bar
  at vent 20 %, a pop below vent 15 % after tens of seconds in the red (`WarnBar` 2.5). A hot still can now run
  at vent 65 % while fuming, which is the coupling section 3 wants.
- **The bath is a first-order lag** (10 s), so feeding cold crude really pulls it 12 K under the setting at feed
  50 %. The recipes are therefore heat **40 %** for a heavy cask (184 K bath) and **30 %** for a light one (158 K),
  and the ideals sit on the knob grid: 183 K heavy, 157 K light. Knobs are integer 5 % steps.
- **A perfect batch is 85 %; a good one is 75 %.** Trap drift over a 100 s batch costs the difference; the design's
  own 4,500 cr for twelve casks already assumed 75 %. Drift only runs while crude is being fed.
- **Fouled is a coke reading, not a state.** A still that fouls mid-batch keeps running at half yield (section 6)
  instead of stopping; only Failed stops it. A failed still's coils are cut.
- The gas cut fumes into the room (0.05 ppm per litre of gas), the vent excess scales with how hard the still is
  boiling, purging keeps the bath warm, the tank gauge climbs continuously (a pop loses only the unfed charge), a
  cask mid-batch at the horn is lost rather than sold as crude, and the tally's worked example is now exact:
  12 delivered, 9 refined, 2 unrefined, 1 lost, 6,420 L diesel, 1,100 L tar, 1 critical -> pay 2,980, fuel +54 t.
- The hopper holds three casks; with the one in the separator a still holds four.
