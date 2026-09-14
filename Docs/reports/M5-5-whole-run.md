# M5-5 - The whole run (Claude, 2026-09-13)

Branch `claude/m5-5-whole-run` (worktree `..\Airlock-claude`, editor on port 8001). The task from
`Docs/design/refinery-floor.md` section 9: the floor proof solo and the two-client run extended to the floor, the tuning
pass from the M5-3b / M5-4b captures, and the first playtest for Tom. Everything below is on the branch; `main` is
untouched until the review.

## What changed, behaviour first

- **The crosshair prompt** (`UI/PromptWidget`): a line under the crosshair says what E does to the thing in reach - the
  target's own prompt (`Heat + (hold to turn)`, `Load C1 into Still 1`, `Squeegee the spill (hold E)`), nothing while a
  hold runs (the hold widget has the prompt) and, while carrying, only a slot, the intake or a hopper (anything else
  puts the cask down). `AAirlockCharacter::InteractPromptFor(Target, bCarrying, bHolding)` is the pure rule; the local
  pawn logs `Prompt: <text>` when it changes. `Scripts\verify-prompt.ps1` proves it on the floor.
- **A dropped cask lands on what the crew can see.** `PutDownLocation`'s floor trace looks through an interactable's
  invisible marker (a still control's box, a puddle's box, the seat) - up to four such hits - so a cask dropped in a
  puddle rests on the crude and a drop whose trace starts inside a control's marker no longer hangs at the crew's
  waist (M5-4b's landing flaw: 30 cm up in the isolated test, 96 cm up live). Hidden shapes of non-interactable actors
  (a test world's floor box) still count as ground. The `PutDown <Cask> by <Pawn>` line now ends ` at X=.. Y=.. Z=..`
  so a proof can check the landing height.
- **A partial squeegee says what is left**: `Spill S2 squeegeed by <Pawn>: 300 L off, 100 L left`; `cleared by` is
  reserved for the pass that empties the puddle.
- **The still reads from the walkway** (the tuning pass). The base label is the still's name alone (`STILL 2`), 22 cm
  at 215 cm over the deck and 125 cm in front of the tank's axis; the three gauge lines are 18 cm in **world
  centimetres** at 185 / 162 / 139 cm (`AStill::GaugeWorldOffsets`, `GaugeWorldSize`, `NameWorldHeight`), so the top
  line sits just over the tank's 170 cm rim and two read against its dark face, 23 cm apart, none touching the name.
  Before this the stack inherited the tank's 0.8 scale (14.4 cm text at 172 / 156 / 140) and the base label's second
  line (`C2 50% 149K 1.1bar`) ran into the first gauge line. An idle still with a queue reads `idle +2   cal 100%`.
  `AFloorFixture::StateLine` is unchanged (the tank's `747 L`, the bunk's `1 critical`), it just no longer appears on a
  still's base label.
- **Labels in world centimetres**: `ShipVisuals::ConfigureLabelUnscaled(Label, WorldSize, FacingYaw, Colour,
  WorldOffset)` divides the offset by the owner's scale and sets the text's relative scale to its inverse (the still
  controls' rule from M5-3b, now shared): the fixtures' labels (`AFloorFixture`), the still's name and gauges, and the
  still controls use it. The drain's label moves from 36 to 60 cm over its 0.6 pallet as a side effect; the tank's and
  the bunk's do not move (scale 1).
- **Every label is unlit.** `ShipVisuals::ConfigureLabel` gives the text the engine's `UnlitText` material
  (`/Engine/EngineMaterials/UnlitText`, the same font-sample material as the default but unlit), so a label reads the
  same white from the dark side of a still as under a work light; the M5-3b captures had grey gauges where the work
  light grazed the text from above. The default stays if the engine asset is missing (logged once). This is the one
  change that touches the ship's labels too; the legibility proof's captures were re-read (below).
- **Lever labels read their own gauge, and amber means pull me.** `RESET` / `no pop` (amber `hold E` while Popped),
  `PURGE` / `no flash` (amber `hold E` while Flash), `SCRAPE` / `coke 12%` (amber when fouled), `REPAIR` / `cond 100%`
  (amber `hold E` while Failed), `TRAP` / `cal 97%` (amber under `CalWarn`). Before this all five repeated the still's
  status word (`RESET` / `Running` x 4). Handles (`HEAT +` / `30%`) and the hopper (`HOPPER` / `1 queued`) are as before.
- **The spill label** is `SPILL` / `176 L`; the crosshair prompt carries `Squeegee the spill (hold E)`.
- **The drain's label faces the pallets** (`LabelYaw` -90 in `RefineryLayouts.cpp`): from the still walkway looking
  north it read mirrored (`NIARD`) in the first M5-5 capture, and at its new 60 cm height that showed.
- **A cask's label faces the walkway from wherever it stands.** `ACargoCrate::RefreshLabel` takes its facing from
  `CargoLayouts::LabelYawFor(current Y)` (the M3-8 walkway rule, both maps' walkway is Y 1560) instead of the spawn
  spot's fixed `Spec.LabelYaw`; the crate re-labels on every transform update, so a cask dropped north of the walkway
  turns to face south and a cask racked south of it turns to face north. In the first M5-5 squeegee capture C3, dropped
  in the crude at Y 1720, read `ƆƐ` / `ǝsooꞀ`; on the ship the racked cask had faced away from the walkway since M3-8
  (the legibility test pinned that as "keeps"; it now pins the turn).
- **Okafor's air lines lose the captain's opener.** The air and fuel patterns are shared with the ship (`Captain {0}
  here. Air's bad. ...`), so on the floor the log read `Okafor: Captain Okonkwo here. Air's bad ...`; `SpeakAirCaptain`
  strips the opener on the floor section, and the blackout line is the documented `<Pawn> is down. Get them to air.`
  Found in the floor proof's log after the pop fumed the room.
- **`AirlockFps`** (local Exec, no cheat gate, no server): `Fps: 48.3 fps, 20.7 ms/frame, map=UEDPIE_0_RefineryFloor`
  from an exponential average of the local controller's frame times over about two seconds. The exposure check before
  the playtest reads it with two stills running.
- **`verify-two-clients.ps1`** works a still from the client on the floor (C1 to the rack on the ship, then the client
  carries it into Still 1's hopper, sees its own prompt, taps the heat handle and captures its gauge).
- **`Docs/playtest/first-playtest.md`**: the checklist for Tom's first run by hand (set-up, the compressed ship leg,
  the floor by hand, what to write down, the known rough edges - updated for this pass).

## Files

`Source/Airlock/UI/PromptWidget.*` (new), `Core/AirlockCharacter.*` (the prompt rule, the put-down trace, the landing
log), `Core/AirlockPlayerController.*` (`AirlockFps`, the frame-time average), `Core/AirlockGameMode.cpp` (the squeegee
wording), `Ship/ShipVisuals.*` (the unlit material, `ConfigureLabelUnscaled`, `LabelMaterial`), `Ship/FloorFixture.cpp`,
`Ship/Still.*`, `Ship/StillControl.*` (`LeverLine`), `Ship/Spill.cpp`; tests `Tests/PromptTests.cpp` (new),
`Tests/StillTests.cpp`, `Tests/LegibilityTests.cpp`, `Tests/RefineryFloorTests.cpp`, `Tests/FloorHazardInteractionTests.cpp`;
`Scripts/verify-prompt.ps1` (new), `Scripts/verify-floor.ps1`, `Scripts/verify-two-clients.ps1`;
`Docs/playtest/first-playtest.md` (new), this report. Nothing in `Content/`, no model constants, no map.

## Tests (headless, `Scripts\run-tests.bat Airlock`)

**159 passed, 0 failed, 0 not run** on the branch's final binary (`Saved\Automation\Report\index.json`, 16:33; 158 on
`main` plus the prompt test).

- `Airlock.Prompt.RoutesByHoldCarryAndTarget` (new, M5-5): the routing rule.
- `Airlock.Floor.HazardInteraction.*` (extended): the landing on the deck through a puddle's marker and a control's
  marker, the partial-pass wording, the shorter spill label.
- `Airlock.Floor.StillGaugesReadMirrorAndWarningColours` (rewritten for the stack): the name alone on the base label at
  its world height, the three gauges' world offsets and sizes with the text at scale one, the unlit material, the stack
  clear of the rim and the name, the lines not touching, `idle +2` with a queue.
- `Airlock.Floor.StillLeverRefusalsAndDurations` (extended): every lever's second line and colour in every status, a
  fouled coil and a drifted trap amber with their readings, the lever text at scale one on a 0.35 panel.
- `Airlock.Floor.StillPressRoutesTapLoadPutDownAndRejectsDistance` (extended): `AirlockFps` logs once with the unit and
  the map.
- `Airlock.Legibility.ConfigureLabel` (extended): the unlit material, `ConfigureLabelUnscaled` on a 0.8 and a
  (0.3, 0.3, 0.15) owner at yaw -50 (the offset still turns with the owner).
- `Airlock.Floor.StillsLoadKnobsLeversAndABatch`: reads the state line through `AFloorFixture::StateLine` now that the
  still's base label is its name.

## The proofs on the branch (worktree editor, port 8001, single-player play settings forced first)

| Proof | Exit | Tail |
| --- | --- | --- |
| floor (final rerun, 312 s) | 0 | `FLOOR OK: Milk run -> C1/C3 secured, C2 racked -> floor: three casks, six fixtures, 36 controls -> C1 by hand into Still 1 -> C2 by hand into Still 2, flooded at feed 100 % (200 L into S1), popped at vent 0 after Okafor's warning (C2 lost, 466 L merged) -> C3 stumble lands on the deck (Z=-63) -> two squeegee passes (Spill S1 squeegeed by AirlockCharacter_0: 300 L off, 146 L left) -> RESET by hand, vent up by hand -> C3 by hand into Still 2 -> both batches done, 1545 L diesel, 2 spills, 1 pop, 29.8 fps with two stills running -> horn -> combined result (2/3 refined, +12 t), undocking card -> hub board, save reset` |
| legibility (71 s) | 0 | `LEGIBILITY OK: Priority hold -> north/south labels -> carried label hidden -> drop -> cargo summaries -> engineering panel -> feed steam -> refinery result -> hub, save reset` |
| prompt (36 s) | 0 | `PROMPT OK: floor direct -> Heat + (hold to turn) -> none along the row -> Hopper 0/3 - carry a cask here` |
| two-clients (87 s) | 0 | `TWO CLIENTS OK: server + client 1 on the hub -> the client crosses Dock B, both aboard -> client carries and racks C1 -> client at the seat -> client scrubs -> cheat refused at DevCheats 0 -> dock -> the client loads C1 into Still 1 and taps its heat handle on the floor -> horn, both home, the second arrival steps aside` |

The three other proofs ran on the binary before the drain, cask-facing and air-line changes; the floor proof's final rerun
ran before the air-line change (it asserts nothing about those lines). The merged tree's twelve proofs are the proof of
record (the reviewer's section below).

### `verify-floor.ps1`, rewritten to work the floor by hand

The proof books the Milk run, secures C1 / C3 and racks C2 on the ship (the clock jump to 75 s while the clamps are
empty consumes the day's skipped incidents, as in M5-4b), docks on the floor with three casks, then:

1. Still 1 preheats at heat 40 % by console (the M5-3a path stays proven); the arrival, pallets, stills, panel and east
   captures run meanwhile.
2. **C1 by hand** into Still 1's hopper (`Load C1 into Still_1 by`, `Still Still_1 Running cask=C1`).
3. **C2 by hand** into Still 2: six taps on `HEAT +`, a held handle (three or more steps in a second), ten taps on
   `FEED +` -> running.
4. **The flood by hand**: ten more taps take the feed to 100 %; five seconds later `Refinery Spill Still_2 C2
   litres=200`, `Spill S1 by Still_2: 200 L`, Okafor's flood line; the feed goes back to 50 % by console.
5. **The pop by hand**, while the charge is high: the feed eases to 25 % by console (the gain needs some feed and the
   charge must last), four taps on `HEAT +` (50 %) and `VENT -` held to 0 %; the spill capture runs while the pressure
   builds; the needle reddens (`Still Still_2 pressure 2.50 bar - warning band`), Okafor's `Pressure on Still 2`, then
   `Refinery Pop Still_2 C2`, `Still Still_2 Popped`, and the pop's spill merges into the flood's puddle (`Spill S1
   merged: <n> L`, asserted over 300 so a squeegee pass leaves something). The cask is lost - that is what a pop costs.
   The first attempt shut the vent a minute into the batch, after the spill capture and with the feed at 50 %: the
   pressure reached the warning band and stalled there (the gain falls with the charge) and C2 finished as a batch of
   147 L diesel and 612 L tar instead.
6. **Scrubbers to Hazard by hand** at the panel (`AirlockPress LifeSupport` from the pallets, `Scrub Hazard by`, `Air ..
   scrub=Hazard`): the pop fumes the room. The first rerun left the scrubbers on Normal - the board read `Hold air 58
   ppm - scrub` in red and `EXPOSURE 38%` thirteen seconds after the pop - and the crew blacked out forty seconds after
   the pop, at the calibrate hold (`Hold rejected: blacked out`). The game did what the design says; the proof now
   does what a player would.
7. **C3 into the crude**: the stumble drops it (`Stumble .. drops C3 in S1`) and the `PutDown C3 by .. at X= Y= Z=` line
   is parsed: Z within 6 cm of the deck's -65 (before the landing fix it was +31 here, on a feed handle's marker).
8. **Two squeegee passes** from the dry ring: `Spill S1 squeegeed by ..: 300 L off, <n> L left`, then `Spill S1 gone`,
   `No spills`.
9. **RESET by hand** (`Hold Still_2_Reset complete`, `Refinery Still_2 reset`, `Still Still_2 Idle`), `VENT +` held back
   up, exact knobs by console - heat **40 %** this time: C3 is a heavy cask and the second rerun ran it at the light
   cask's 30 % (C2's setting), where the feed's cooling holds the bath under the heavy cut and the batch went to tar
   (121 L diesel, 828 L tar) - then **C3 by hand** into the reset Still 2 (`Load C3 into Still_2 by`, `Still Still_2
   Running cask=C3`), the calibrate hold, a reset refused while running.
10. `AirlockFps` with two stills running; both batches complete; `AirlockTally` reads `2/3 casks refined`; the horn
   records `Floor result: floor: 2/3 casks refined, .. 2 spills, 1 pop` and the combined `Voyage result`, the undocking
   card, the hub board, the save reset.

Captures under `Saved\AgentCaptures\M5-5` (`floor-arrival`, `floor-pallets`, `floor-stills`, `floor-panel`,
`floor-east`, `floor-spill`, `floor-still-popped`, `floor-squeegee`, `floor-cleared`, `floor-still-gauges`,
`floor-still-handles`, `floor-still-running`, `floor-tank`, `hub-board-floor`), the log excerpt in `floor-log.txt`.

The fresh `LogAirlock` lines of the final rerun (`floor-log.txt`, 23:37-23:41; the second rerun's air lines, before the
opener fix, are quoted where they differ):

```text
Ship result: 3/4 crates intact, 0 vented, 1 loose, hull 87.82%, pay 626, air 0 ppm, 0 blackouts, 0 burned (3 casks to the floor)
Slot clock started: 600s endServerTime=634.632 casks=3
Load C1 into Still_1 by AirlockCharacter_0
Load C2 into Still_2 by AirlockCharacter_0
Still Still_2 Running cask=C2
Refinery Still_2 feed 20/20
Refinery Spill Still_2 C2 litres=200
Spill S1 by Still_2: 200 L (r 200 cm)
Okafor: Flood on Still 2 - crude over the lip. Feed down, and mind your footing, it's slick.
Refinery Still_2 heat 10/20
Refinery Still_2 vent 0/20
Still Still_2 pressure 2.50 bar - warning band
Okafor: Pressure on Still 2 - vent it before the relief goes.
Air 60 ppm scrub=Normal
Refinery Pop Still_2 C2 litres=0
Refinery Spill Still_2 C2 litres=300
Spill S1 merged: 466 L
Okafor: Captain Okonkwo here. Air's bad. Get those scrubbers up and keep moving.     (second rerun; now without the opener)
Okafor: Still 2's relief just blew. Vent it wide, reset the trap, and keep your face out of that cloud.
Still Still_2 Popped
Scrub Hazard by AirlockCharacter_0
Air 58 ppm scrub=Hazard
Exposure AirlockCharacter_0 30%
Stumble AirlockCharacter_0 drops C3 in S1
PutDown C3 by AirlockCharacter_0 at X=-5400.000 Y=1720.000 Z=-63.000
Okafor: Cask down - AirlockCharacter_0 went over in the crude. Get it up before it warms.
Air 19 ppm scrub=Hazard
Spill S1 squeegeed by AirlockCharacter_0: 300 L off, 146 L left
Spill S1 cleared by AirlockCharacter_0
Spill S1 gone
No spills
Hold Still_2_Reset complete by AirlockCharacter_0
Refinery Still_2 reset
Still Still_2 Idle
Refinery Still_2 vent 9/20
Refinery BatchDone Still_1 C1 diesel=750 gas=50 tar=199
Okafor: That's diesel in the tank. Keep them coming.
Load C3 into Still_2 by AirlockCharacter_0
Still Still_2 Running cask=C3
Refinery Still_2 calibrated
Hold Still_2_Reset rejected: Running
Fps: 29.8 fps, 33.5 ms/frame, map=UEDPIE_0_RefineryFloor
Refinery BatchDone Still_2 C3 diesel=682 gas=50 tar=267
Floor tally now: floor: 2/3 casks refined, 1545 L diesel, 672 L tar, 2 spills, 1 pop, 0 criticals, pay 713, fuel +12 t
Okafor: Horn. Slot's over - clear the floor.
Floor result: floor: 2/3 casks refined, 1545 L diesel, 672 L tar, 2 spills, 1 pop, 0 criticals, pay 713, fuel +12 t
Voyage result: 3/4 crates intact, 0 vented, 1 loose, hull 87.82%, pay 1339, air 0 ppm, 0 blackouts, 0 burned | floor: 2/3 casks refined, 1545 L diesel, pay 713, fuel +12 t
```

What the captures show (`Saved\AgentCaptures\M5-5`): `floor-stills` - `STILL 2` alone over `T 92 K   P 1.00 bar` /
`heat 0%  feed 0%  vent 55%` / `idle   cal 100%`, the levers `REPAIR cond 100%`, `SCRAPE coke 0%`, `PURGE no flash`,
`RESET no pop`, the trap `TRAP cal 100%`, all white and legible from the walkway with nothing overlapping;
`floor-still-popped` - the stack reads `T 214 K   P 1.00 bar` / `heat 50%  feed 25%  vent 0%` / `POPPED   cal 81%`,
`RESET` / `hold E` amber, the board `Still_2 popped - reset the trap` and `Hold air 58 ppm - scrub` in red;
`floor-still-handles` - `DRAIN` reads the right way round from the still walkway, the crosshair prompt `Feed + (hold to
turn)` under the crosshair, `VENT + 55%` .. `HEAT - 30%` along the row; `floor-still-gauges` - Still 2 running C3
after the reset (`C3 8%   cal 97%`, `RESET` / `no pop` back to white); the M3-8 captures re-read after the unlit
material (`engineering-panel`, `hold-north`): crisp white text, no glow, the same layout.

**The exposure check**: `AirlockFps` reads 29.8-32 fps (31-34 ms) in the editor's viewport PIE at 1280 x 764 on the lit
floor with two stills running, the puddle gone and a caption up - the earlier note said "about 50 FPS with one still";
this is the number to beat before the two-player playtest (the scalability pass stays on the follow-ups list).

## What I was unsure about

- **Unlit labels everywhere.** The one policy is the point of M3-8, so the unlit material went into `ConfigureLabel` for
  every label rather than the floor's alone; the ship's legibility captures were re-read and look right, but nobody has
  played the ship under it yet. Restricting it to the floor is a two-line change if the playtest dislikes the ship's.
- **A pop costs the cask.** The proof accepted `2/3 casks refined` with `1 pop` instead of M5-4b's `3/3`; that is the
  model's rule (`LoseCharge` on a pop) and the design's, not a regression, but the proof's headline number changed.
- **The pop's spill speaks the flood line.** A pop emits its own `Spill` event, so Okafor says both `relief just blew`
  and `Flood on Still 2` in the same second. Harmless; a pop's spill could skip the flood cue (a one-line change in
  `TickRefinery`, left for the audio pass).
- **The heavy and light recipes.** C3 at the light cask's 30 % heat gave 121 L of diesel; at 40 % it gave 682. The
  playtest checklist says 40 % for a batch and the light casks go through Still 2 in the proof; whether the crew can
  tell a light cask needs less heat from what they see (the label says `C2` and its state, not its cut) is a playtest
  question.
- **The put-down landing height.** `PutDown .. Z=-63` on a deck the pawn's feet read as -65: the trace hits the deck
  mesh's surface 2 cm over where the capsule settles. Within the proof's 6 cm tolerance; not investigated further.
- **Frame rate.** 30 fps in viewport PIE with the Outliner and Details panels open is not the standalone number; the
  playtest runs the same way, so it is the number Tom will feel.

## Outside the brief

Nothing. The M5-5 scope in the design note is the proofs, the tuning pass and the playtest; the unlit label material
is the one change with a reach beyond the floor (every label in the game), taken because the policy is meant to be one
policy and the legibility captures were re-read after it.

## Reproduce

From `..\Airlock-claude` in PowerShell (`$env:AIRLOCK_MCP_PORT = '8001'`): `Scripts\close-editor.ps1`,
`Scripts\build-editor.bat`, `Scripts\run-tests.bat Airlock` (read `Saved\Automation\Report\index.json`),
`Scripts\run-editor.bat`, force single-player play settings through `Scripts\lib\mcp.ps1` (HANDOFF section 5), then
`Scripts\verify-floor.ps1`, `Scripts\verify-legibility.ps1`, `Scripts\verify-prompt.ps1`, `Scripts\verify-two-clients.ps1`.

## Reviewer's verification (Claude, 2026-09-13 evening; cut short by a shutdown)

Merged into `main` as `8edd9b6` (`git merge --no-ff` from `d118dda`). Rebuilt in `Airlock\` (`Result: Succeeded`) and
the headless suite on the merged tree: **159 passed, 0 failed, 0 not run** (`Saved\Automation\Report\index.json`).

The proof batch on the merged tree (port 8000, single-player settings forced first) was run in two passes because the
editor window minimized itself between the third and fourth proofs (the MCP kept answering; every capture then failed
with `Failed to capture any editor windows` and the two-client proof's `AirlockCapture` reported `the viewport did not
render` - HANDOFF section 5's known symptom; `restore-editor-window.ps1` fixed it and the batch now restores the window
before every proof). Green on the merged tree before the machine was shut down:

| Proof | Exit | Tail |
| --- | --- | --- |
| loop | 0 | `LOOP OK: hub spawn -> booked -> Dock B -> Hauler_Mk1 PlayerStart -> exit -> hub PlayerStart, voyage complete` |
| ship-graph | 0 | `GRAPH OK: seven nominal modules -> coolant leak -> degraded coolant drifts core -> hold repairs line -> coolant restored -> hub PlayerStart, empty graph` |
| incidents | 0 | `INCIDENTS OK: solo schedule -> teaching leak and captain -> hold repair clears row -> core task -> climax -> no replay -> hub` |
| voyage-clock | 0 | `CLOCK OK: 75 seconds remaining -> one-minute captain line -> automatic return card -> StationHub PlayerStart, voyage complete` |
| level-pass | 0 | `LEVEL OK: hold -> engineering (2 lines repaired) -> corridor (2 lines repaired) -> galley -> bridge seat, board, stand -> hub PlayerStart` |
| cargo | 0 | `CARGO OK: secure two -> shift and cool C2 -> vent C4 -> 2/4 intact, pay 400 -> hub last run` |

**Still to run on the merged tree** (next session, `Airlock\`, port 8000, in this order): atmosphere, fuel, refinery,
legibility, two-clients, floor, prompt. All seven passed on the branch's own binary (legibility, prompt, two-clients and
floor in this report's table above; atmosphere, fuel and refinery are untouched by M5-5 except for the label material and
the put-down trace, which the six merged-tree proofs above already exercise on the ship). Until those seven exit 0 on
`main`, treat M5-5 as merged-and-built, not merged-and-proven.
