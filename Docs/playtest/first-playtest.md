# The first playtest: a refining run by hand (2026-09-13)

For Tom (and a second player when there is one). Everything below is playable on `main` in the editor; the console
helpers are the same ones the proofs use, typed into the editor's console box (`~` in PIE, or the `Cmd` box under the
viewport). Nothing here needs a build.

## 1. Set up (two minutes)

- Open the project (`Scripts\run-editor.bat`), let it settle on `StationHub`, press **Play** (Selected Viewport).
- Solo: leave the play settings alone (standalone, one player). Two on one laptop is impractical (one keyboard);
  for a second player on the same machine use *Editor Preferences > Play > Multiplayer Options*: Net Mode
  **Play As Listen Server**, Number of Players **2**, Run Under One Process - the second window is the client.
  The proofs restore these to standalone / 1 afterwards; set them back by hand if you change them.
- Controls: WASD, mouse, **E** interacts (a press or a hold - the bar under the prompt shows a hold), the line
  under the crosshair says what E will do. Carrying a cask slows you to 60 %; E again puts it down.
- Cheats that make a 20-minute run fit a lunch break (they need `airlock.DevCheats 1`, the editor's default):
  `AirlockVoyageClock <seconds>` sets the time left on the ship or the floor; `AirlockWarp x y z` teleports;
  `AirlockWhere` prints where you are; `AirlockBook 0|1|2` books a run from anywhere on the hub.

## 2. The ship leg (aim for 8-10 minutes with the clock helper)

1. At the departure board (Dock B platform): book the **Milk run** (4 casks, two of them light). The Dock B
   light turns green; walk through the airlock.
2. In the hold: carry each cask (E) to a clamp (E at the pallet) - the two **light** casks warm up on a clamp and
   start puffing nitriles; put those in the **cold rack** instead. Watch the task board top-left and the air row.
3. Engineering: the scrubber panel (the life-support box) - E cycles Normal / Hazard. Hazard mode clears the
   hold's air; the captain says so.
4. Skip ahead: `AirlockVoyageClock 660` puts you at the feed-valve failure window. Wait for the captain's line,
   go to engineering, hold E on the **feed valve** (3 s), then decide: open the **crude bypass** (E) and feed a
   cask into the **intake** (carry it there, E) - the burned cask costs pay but keeps you off a late dock.
5. `AirlockVoyageClock 20` to dock. The card says `DOCKING - HALDER SOLAR REFINERY`.

## 3. The floor (the ten-minute slot is the real one)

1. You arrive by the airlock. Okafor speaks. Your intact casks stand on the pallets by the door; three stills
   along the north wall; the tank at the east end; the scrubber panel on the south wall.
2. A batch by hand at Still 1: pick a cask up, walk to the **hopper** (the pallet in front of the still), E.
   Set **HEAT +** to 40 % (eight taps, or hold E), **FEED +** to 50 %. Watch the gauge: T climbs toward 180-185 K,
   the cask's progress counts up, diesel appears on the tank's label. A batch is about 100 s.
3. Do it wrong on Still 2 on purpose: FEED to 100 % - after five seconds crude floods over the lip: a puddle on the
   deck, steam, Okafor. Walk into it carrying a cask: you slow down and drop it. Hold E on the puddle (6 s) with
   the squeegee. Then VENT down to 10 % on a running still: the pressure needle goes red, Okafor warns, the relief
   **pops**: RESET (hold E, 4 s) and start again with the vent up.
4. Drift: after one batch the trap reads under 80 % - hold E on the **CALIBRATE** panel (3 s) or the next batch
   loses its cut to tar.
5. Air: floods and pops fume the room. Above 20 ppm the board says scrub; the panel's Hazard mode clears it; at
   60 % exposure you slow down, at 100 % you black out for 30 s (a critical: 200 cr off the pay).
6. The horn at 0:00 takes everyone home. The hub board shows the run: casks refined, litres, pay, the fuel the Rig
   banked.

## 4. What to write down (five lines is plenty)

- Where you did not know what to do next, or what a label / gauge / line meant.
- Anything you could not read from where you stood (the labels' size, the lighting, the board).
- The moment you were most engaged, and the moment you were bored or waiting.
- Anything that felt unfair (a spill you could not see coming, a pop with no warning) or too safe.
- How long the floor took you, and whether ten minutes felt long or short for two casks.

## 5. Known rough edges (no need to report these)

- A cask dropped in a puddle lands on the crude, but the case's pivot is at its base, so a cask put down on a
  ramp or a sill can clip the deck; pick it up again.
- No sound yet: the pop's bang, the hiss, the horn are lines and lights only.
- The flash (a fire on an overheated still in a gassy room) has no flicker yet, only the log and Okafor.
- Nobody carries a blacked-out crewmate to the bunk yet; the bunk is a label.
- Two players on the floor works (proven with a script); a second human on the same laptop needs a second
  keyboard and mouse, or a second machine on the LAN (not set up yet).
