# Airlock - where we are (kept current by the reviewing agent at every merge)

Last update: 2026-09-13, late morning (Claude, after the M5-3a merge; M5-3b handed to Astra). Read `AGENTS.md` (what
the game is, code map) and `Docs/HANDOFF.md` (how to build, drive the editor, prove a change) first; this file is
the *what next*.

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
- **M3-8 Legibility** (`verify-legibility.ps1`; Astra, reviewer-tuned, merged 2026-09-13 early). Report:
  `Docs/reports/M3-8-legibility.md`.
- **M4-1 Dev-cheats gate, per-instance console, the first two-client run** (`verify-two-clients.ps1`; Claude,
  merged 2026-09-13 early). Report: `Docs/reports/M4-1-dev-cheats-and-two-clients.md`.
- **M5-1 The refinery model** (pure; Astra, merged 2026-09-13 morning): three stills, integer knobs, the bath,
  the yield bell, the valve-like pressure and the pop, floods, flashes, coke, drift, the floor tally; 26 tests
  that pin the measured figures. Report: `Docs/reports/M5-1-refinery-model.md` (plus the reviewer's section).
- **M5-2 The refinery floor** (`verify-floor.ps1`; Claude, merged 2026-09-13 morning): the third map
  (`RefineryFloor`), the ship's docking lands the crew there with the run's intact casks on the staging spots, a
  600 s slot with Okafor's lines, the floor's own air and scrubber panel, fixtures, signs, the slot's end undocks
  everyone home with a combined result on the hub board. Report: `Docs/reports/M5-2-refinery-floor.md` (its
  addendum records the merge: a unity-build name collision, the editor's persisted two-client play settings).
- **M5-3a The model on the floor** (`verify-floor.ps1` refines a cask by console; Claude, merged 2026-09-13 late
  morning): the game mode owns and ticks the model on the slot, the state is replicated, the verbs are a server
  API with console helpers, the still and tank labels and the board's floor rows read the mirror, the run's floor
  pay and base fuel come from the model's tally; the `Loaded` cargo state. Report:
  `Docs/reports/M5-3a-refinery-on-the-floor.md`. **138 tests, twelve proofs.**
- **Content**: `StationHub`, `Hauler_Mk1`, `RefineryFloor` (all pack copies with code-spawned fixtures; the
  floor's walking surface Z -65, its end platforms Z 22). Packs: `THIRD_PARTY_ASSETS.md` (git-ignored).

## In flight (2026-09-13, late morning)
- **M5-3b Stills and knobs** (Astra, `astra/m5-3b-stills-and-knobs`, brief `Docs/tasks/M5-3b-stills-and-knobs.md`,
  launched by agent script 179): `AStill` (a subclass of the fixture) with a gauge stack, control actors per still
  (a hopper for the carried cask, `-` / `+` handles per knob with a repeating hold, four lever holds and a
  calibrate hold), Okafor's pressure and drift warnings, `AirlockPress <Label>`, a by-hand batch in
  `verify-floor.ps1`. Astra owns `Airlock\` and port 8000 while it runs; Claude reviews from the worktree.
- Claude's worktree `..\Airlock-claude` (port 8001) is idle on `main`; switch it to a fresh branch before the next
  task.

## Next tasks (the M5 plan, `refinery-floor.md` section 9)
1. **M5-3b** (Astra, in flight) - then the reviewer's pass: merge, the twelve proofs, the docs.
2. **M5-4 Floor hazards** (either): puddles (spawn from `Spill` events, growth, slip, the stumble drop, the
   squeegee hold, boil-off ppm), pop and flash FX and sounds, the critical rule and the bunk, a cold rack on the
   floor if the puffing casks prove too much. The model's pops, flashes and spills already fire and are logged;
   this task gives them bodies.
3. **M5-5 The whole run** (Claude): `verify-floor.ps1` through two stills by hand, the two-client run on the
   floor, tuning from captures, the first two-person playtest. Then the map-dressing pass (the hold's props, the
   floor's lighting) and M4-2 (exposure on the PlayerState, server-side base fuel, player names in logs, join
   mid-voyage) from the earlier list.

## Open follow-ups (not blocking)
- The floor's casks warm and puff like the ship's, with no rack to cool them; loading them is the only cure
  (`Load crate C2 - 60C` on the board). M5-4 judges whether that is the right pressure.
- A cask in a still at the horn is lost (`CasksLost`); the board does not warn about it yet.
- Leaving the ship through its own airlock skips the floor (an abort) and still pays the flat 10 t per intact
  cask; a playtest may want the door to dock too.
- The floor is dark in its pack dressing; the map-dressing pass adds light over the stills and pallets.
- Solo schedules can draw three coolant leaks out of four incidents; type variety.
- Exposure/scalability pass before any playtest (PIE was 2-6 FPS in one period; two PIE instances are heavier).
- The save game is per machine and unversioned (`AirlockSaveReset` clears it); server-side in M4-2.
- `StarfieldFree` creator name still to fill in `THIRD_PARTY_ASSETS.md`; Cascade steam to Niagara.
- Remote: `origin` = `https://github.com/tqzlin/airlock.git` (private); `main` and task branches are pushed after
  each merge.

## Agent loop facts
- Runner: `ai_game_studio\agent\runner.bat` polls `agent\queue\*.ps1` in name order, logs to `agent\results\`,
  moves scripts to `agent\done\`. Next free script number: **180**. Staged inputs under `agent\staged\handoffNN\`
  (next: `handoff24`; `handoff23` holds the M5-3b prompt and launch script). Scripts must be CRLF + ASCII. The
  launch guard stops on *any* untracked or modified file in `Airlock\`: commit everything before queueing.
- Astra: Codex CLI on stack, `gpt-6-astra`, launched by a queue script that pipes the prompt on stdin and waits on
  the codex *process* (pattern: scripts 178, 179). M3-8: 38 min; M5-1: 32 min, clean. Red-team every brief before
  launch (the M5-1 reader found the pressure model bang-bang; the M5-3b reader's findings are folded in).
- Two agents share the laptop: never stop processes by script name; filter by checkout path or pid - and a
  filter on a command line matches the shell that runs the filter too, so build the pattern from pieces. Do not
  compile one checkout while the other's editor is starting.
- Every proof but two-clients needs single-player play settings; force them through the MCP before a batch
  (HANDOFF section 5). A merge is rebuilt and tested before it is called merged (the M5-1 + M5-2 unity collision).
- Review from Claude Code: merge, `close-editor` -> `build-editor` -> `run-tests` -> `run-editor` -> force the play
  settings -> the proofs in sequence (twelve: loop, ship-graph, incidents, voyage-clock, level-pass, cargo,
  atmosphere, fuel, refinery, legibility, two-clients, floor; about 28 minutes), `git merge --no-ff`, one docs
  commit, push.
- Branch rule: one task per `<agent>/<task>` branch; reviewer (Claude) merges `--no-ff` after tests + proofs.
