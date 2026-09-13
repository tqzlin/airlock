# Airlock - where we are (kept current by the reviewing agent at every merge)

Last update: 2026-09-13, morning (Claude, after the M5-1 and M5-2 merges). Read `AGENTS.md` (what the game is, code
map) and `Docs/HANDOFF.md` (how to build, drive the editor, prove a change) first; this file is the *what next*.

## The mission (decided 2026-09-12; the floor decided 2026-09-13)
The MVP voyage is **the refining run** (`Docs/design/refining-run.md`): a hauler lifts cryogenic hydrocarbon
crude ("sludge") from the Vell Extractor, the crude puffs nitriles and spills until the scrubbers are on and the
casks are racked, a feed valve fails mid-run and the crew limps to the Halder Solar Refinery by burning casks
through the crude bypass - and since 2026-09-13 the run has a second half: **the floor** (`Docs/design/refinery-floor.md`),
one walkable room at the refinery where the crew turns the delivered casks into diesel at three stills with
heat / feed / vent knobs and a drifting calibration, with spills, fumes, pops, flashes and coke as the risks.
Names are final (Vell, Halder, the Rig, the Still, MV Corrin, diesel, Okafor); hazards are quasi-realistic with no
concern for rating (Tom, 2026-09-13). The three offers are three contract tiers of the run.

## Verified on `main`
- **M2 loop** through **M4-1** (two-client run, the cheat gate): see the earlier entries in `AGENTS.md`; every
  proof still exits 0 on today's binary. Reports under `Docs/reports/`.
- **M3-8 Legibility** (`verify-legibility.ps1`; Astra, reviewer-tuned, merged 2026-09-13 early): labels, summaries,
  readouts, local lights. Report: `Docs/reports/M3-8-legibility.md`.
- **M4-1 Dev-cheats gate, per-instance console, the first two-client run** (`verify-two-clients.ps1`; Claude,
  merged 2026-09-13 early). Report: `Docs/reports/M4-1-dev-cheats-and-two-clients.md`.
- **M5-1 The refinery model** (pure; Astra, merged 2026-09-13 morning): three stills, integer knobs, the bath,
  the yield bell, the valve-like pressure and the pop, floods, flashes, coke, drift, the floor tally; 26 tests
  that pin the measured figures. Report: `Docs/reports/M5-1-refinery-model.md` (plus the reviewer's section).
  Astra's run: 32 min, clean. Not wired to the floor yet.
- **M5-2 The refinery floor** (`verify-floor.ps1`; Claude, merged 2026-09-13 morning): the third map
  (`RefineryFloor`, a copy of the Interiors pack's second hall), the ship's docking lands the crew there with the
  run's intact casks on the staging spots, a 600 s slot with Okafor's lines, the floor's own air and scrubber
  panel, six placeholder fixtures, three signs, the slot's end undocks everyone home with a combined result on the
  hub board. Report: `Docs/reports/M5-2-refinery-floor.md`. **136 tests, twelve proofs.**
- **Content**: `StationHub`, `Hauler_Mk1`, `RefineryFloor` (all pack copies with code-spawned fixtures; the
  floor's walking surface Z -65, its end platforms Z 22). Packs: `THIRD_PARTY_ASSETS.md` (git-ignored).

## In flight (2026-09-13, morning)
- Nothing. Both editors are open and idle on `main`'s binary: port 8000 on `Airlock\` (StationHub, no PIE) and
  port 8001 on the worktree `..\Airlock-claude` (switch it to a fresh branch from `main` before the next task).

## Next tasks (the M5 plan, `refinery-floor.md` section 9)
1. **M5-3a The model on the floor** (Claude, next): the floor section owns an `FRefineryModel` built from the
   delivered casks, ticks it at 4 Hz while the slot runs, adds its ppm to the atmosphere, replicates
   `FRefineryState` on the game state (`OnRep` -> `OnShipGraphChanged`), exposes the verbs as a server API on the
   game mode (`RefineryLoadHopper`, `RefineryStepKnob`, `RefineryClearPop/Purge/Scrape/Repair/Calibrate`) with
   console helpers behind the cheat gate, fills the tank fixture's label and the task board's floor rows from the
   replicated state, and turns `FFloorResult` into the run's floor fields and base fuel (replacing the flat 15 t
   credit). A headless test drives one cask through a still by console.
2. **M5-3b Stills and knobs** (Astra, after 3a): `AStill` replacing the still fixtures (hopper press with a carried
   cask, `-` / `+` handles per knob with a repeat-while-held press, the gauge stack from `StillReadout` +
   `BatchProgress`, the lever holds `RESET` / `PURGE` / `SCRAPE` / `REPAIR`, `CALIBRATE` at the trap gauge), the
   product tank's gauge, Okafor's warning lines (pressure high, drift, fouled), the task board header on the floor
   (`to horn`). Proof: a cask refined by hand in PIE.
3. **M5-4 Floor hazards** (either): puddles (spawn from `Spill` events, growth, slip, the stumble drop, the
   squeegee hold, boil-off ppm), pop and flash FX and sounds, the critical rule and the bunk, a cold rack on the
   floor if the puffing casks prove too much.
4. **M5-5 The whole run** (Claude): `verify-floor.ps1` extended through a refined cask, the two-client run on the
   floor, tuning from captures, the first two-person playtest. Then the map-dressing pass (the hold's props, the
   floor's lighting) and M4-2 (exposure on the PlayerState, server-side base fuel, player names in logs, join
   mid-voyage) from the earlier list.

## Open follow-ups (not blocking)
- The task board header says `to dock` on the floor (M5-3b); the still labels sit close to the tank's rim.
- The floor's casks warm and puff like the ship's, with no rack to cool them (a real pressure; M5-4 judges it).
- Leaving the ship through its own airlock skips the floor (an abort); a playtest may want the door to dock too.
- The floor is dark in its pack dressing; the map-dressing pass adds light over the stills and pallets.
- Solo schedules can draw three coolant leaks out of four incidents; type variety.
- Exposure/scalability pass before any playtest (PIE was 2-6 FPS in one period; two PIE instances are heavier).
- The save game is per machine and unversioned (`AirlockSaveReset` clears it); server-side in M4-2.
- `StarfieldFree` creator name still to fill in `THIRD_PARTY_ASSETS.md`; Cascade steam to Niagara.
- Remote: `origin` = `https://github.com/tqzlin/airlock.git` (private); `main` and task branches are pushed after
  each merge.

## Agent loop facts
- Runner: `ai_game_studio\agent\runner.bat` polls `agent\queue\*.ps1` in name order, logs to `agent\results\`,
  moves scripts to `agent\done\`. Next free script number: **179**. Staged inputs under `agent\staged\handoffNN\`
  (next: `handoff23`; `handoff22` holds the M5-1 prompt and launch script). Scripts must be CRLF + ASCII. The launch
  guard stops on *any* untracked or modified file in `Airlock\`: commit everything before queueing.
- Astra: Codex CLI on stack, `gpt-6-astra`, launched by a queue script that pipes the prompt on stdin and waits on
  the codex *process* (pattern: script 178). M3-8: 38 min; M5-1: 32 min, clean, and it pinned the brief's
  arithmetic where the prose disagreed instead of tuning - the behaviour we want. Red-team every brief before
  launch (the M5-1 reader simulated the numbers and found the pressure model bang-bang).
- Two agents share the laptop: never stop processes by script name; filter by checkout path or pid.
- Building a map from a pack hall: `duplicate` leaves the copy **unsaved**, so `load_level` refuses it and any
  surgery lands in the pack's map in memory. Save the duplicate right after duplicating, then load it (the floor
  build hit this and recovered by pattern 108: back up the pack file, save, duplicate, restore the pack file).
- Review from Claude Code: merge, `close-editor` -> `build-editor` -> `run-tests` -> `run-editor` -> the proofs in
  sequence (twelve now: loop, ship-graph, incidents, voyage-clock, level-pass, cargo, atmosphere, fuel, refinery,
  legibility, two-clients, floor; about 25 minutes), `git merge --no-ff`, one docs commit, push.
- Branch rule: one task per `<agent>/<task>` branch; reviewer (Claude) merges `--no-ff` after tests + proofs.
