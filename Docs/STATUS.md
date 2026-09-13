# Airlock - where we are (kept current by the reviewing agent at every merge)

Last update: 2026-09-13, early afternoon (Claude, after the M5-3b, M5-2b and M5-4a merges; M5-4b handed to Astra).
Read `AGENTS.md` (what the game is, code map) and `Docs/HANDOFF.md` (how to build, drive the editor, prove a
change) first; this file is the *what next*.

## The mission (decided 2026-09-12; the floor decided 2026-09-13)
The MVP voyage is **the refining run** (`Docs/design/refining-run.md`): a hauler lifts cryogenic hydrocarbon
crude ("sludge") from the Vell Extractor, the crude puffs nitriles and spills until the scrubbers are on and the
casks are racked, a feed valve fails mid-run and the crew limps to the Halder Solar Refinery by burning casks
through the crude bypass - and the run has a second half: **the floor** (`Docs/design/refinery-floor.md`), one
walkable room at the refinery where the crew turns the delivered casks into diesel at three stills with heat /
feed / vent knobs and a drifting calibration, with spills, fumes, pops, flashes and coke as the risks. Names are
final (Vell, Halder, the Rig, the Still, MV Corrin, diesel, Okafor); hazards are quasi-realistic with no concern
for rating (Tom, 2026-09-13). The three offers are three contract tiers of the run.

## Verified on `main`
- **M2 loop** through **M4-1** (two-client run, the cheat gate): see the earlier entries in `AGENTS.md`; every
  proof still exits 0 on today's binary. Reports under `Docs/reports/`.
- **M5-1 The refinery model** (pure; Astra), **M5-2 The refinery floor** (the map and the section; Claude),
  **M5-3a The model on the floor** (the wiring, the verbs, the result; Claude) - all merged 2026-09-13 morning;
  reports `Docs/reports/M5-1-refinery-model.md`, `M5-2-refinery-floor.md`, `M5-3a-refinery-on-the-floor.md`.
- **M5-3b Stills and knobs** (Astra, 55 min, merged 2026-09-13 early afternoon): `AStill` with a gauge stack,
  twelve control actors per still (hopper, six handles with a repeating hold, four lever holds and calibrate),
  Okafor's pressure and drift warnings, `AirlockPress`; the floor proof refines the light cask by hand alongside
  the console batch. Report: `Docs/reports/M5-3b-stills-and-knobs.md` (plus the reviewer's section).
- **M5-2b Work lights on the floor** (content only; Claude): nine warm lights over the stills, the tank, the
  pallets, the panel and the walkway. Report: `Docs/reports/M5-2b-floor-lighting.md`.
- **M5-4a The pure spill model** (Claude): puddles by litres and point, merging, evaporation into the room, the
  squeegee, containment; not wired yet. Report: `Docs/reports/M5-4a-floor-hazards-model.md`.
  **150 tests, twelve proofs.**
- **Content**: `StationHub`, `Hauler_Mk1`, `RefineryFloor` (all pack copies with code-spawned fixtures; the
  floor's walking surface Z -65, its end platforms Z 22, the east walkway ramp under Still 3's controls).
  Packs: `THIRD_PARTY_ASSETS.md` (git-ignored).

## In flight (2026-09-13, early afternoon)
- **M5-4b Floor hazards, the bodies** (Astra, `astra/m5-4b-floor-hazards`, brief `Docs/tasks/M5-4-floor-hazards.md`
  part b, launched by agent script 180): the puddle actors from the model's Spill events at each still's spill
  point, the crew's slip and stumble in the crude, the squeegee hold, the pop burst on the still from the
  replicated status change, the criticals and spill rows on the board, `AirlockFloor` / `AirlockSpill`, a proof
  that floods Still 2, drops a third cask in the puddle and clears it. Astra owns `Airlock\` and port 8000 while
  it runs; Claude reviews from the worktree.
- Claude's worktree `..\Airlock-claude` (port 8001, editor closed) sits on `claude/m5-4a-floor-hazards-model`
  (merged); switch it to a fresh branch from `main` before the next task.

## Next tasks (the M5 plan, `refinery-floor.md` section 9)
1. **M5-4b** (Astra, in flight) - then the reviewer's pass: merge, the twelve proofs, the docs.
2. **M5-5 The whole run** (Claude): the tuning pass from the captures (the gauge stack inherits the still's 0.8
   scale and overlaps the base label's second line; the still labels front-lit; the lever labels' second line),
   `verify-floor.ps1` through the hazards by hand, the two-client run on the floor (two crew at two stills), a
   playtest checklist for Tom. Then the map-dressing pass for the ship (the hold's props, engineering's light)
   and M4-2 (exposure on the PlayerState, server-side base fuel, player names in logs, join mid-voyage).

## Open follow-ups (not blocking)
- Press prompts (`Load C2 into Still 2`, `Heat + (hold to turn)`) are API text until a crosshair prompt widget
  exists; only running holds show their prompt. Worth a small task before the playtest.
- No audio: the hiss rising with pressure, the pop's bang, the flash's whump, the horn - an asset pass.
- The flash's flicker on the still and the bunk's role are deferred from M5-4b.
- The floor's casks warm and puff like the ship's, with no rack; loading them is the cure (`Load crate C2 -
  60C` on the board). A playtest decides whether a floor cold rack is needed.
- A cask in a still at the horn is lost (`CasksLost`); the board does not warn about it yet.
- Leaving the ship through its own airlock skips the floor (an abort) and still pays the flat 10 t per intact
  cask; a playtest may want the door to dock too.
- Solo schedules can draw three coolant leaks out of four incidents; type variety.
- Exposure/scalability pass before any playtest (PIE runs about 50 FPS on the lit floor with one still; two PIE
  instances are heavier).
- The save game is per machine and unversioned (`AirlockSaveReset` clears it); server-side in M4-2.
- `StarfieldFree` creator name still to fill in `THIRD_PARTY_ASSETS.md`; Cascade steam to Niagara.
- Remote: `origin` = `https://github.com/tqzlin/airlock.git` (private); `main` and task branches are pushed after
  each merge.

## Agent loop facts
- Runner: `ai_game_studio\agent\runner.bat` polls `agent\queue\*.ps1` in name order, logs to `agent\results\`,
  moves scripts to `agent\done\`. Next free script number: **181**. Staged inputs under `agent\staged\handoffNN\`
  (next: `handoff25`; `handoff24` holds the M5-4b prompt and launch script). Scripts must be CRLF + ASCII. The
  launch guard stops on *any* untracked or modified file in `Airlock\`: commit everything before queueing.
- Astra: Codex CLI on stack, `gpt-6-astra`, launched by a queue script that pipes the prompt on stdin and waits on
  the codex *process* (pattern: scripts 178-180). M3-8: 38 min; M5-1: 32 min; M5-3b: 55 min, nine commits, the
  brief followed to the letter including its wrong gauge offsets (it recorded the geometry instead of tuning -
  the behaviour we want). Red-team every brief before launch; the M5-3b and M5-4b readers each found 16-22
  real problems.
- Two agents share the laptop: never stop processes by script name; filter by checkout path or pid, build the
  pattern from pieces (a filter on a command line matches the shell that runs it), and leave `zenserver.exe`
  alone. Do not compile one checkout while the other's editor is starting.
- Every proof but two-clients needs single-player play settings; force them through the MCP before a batch
  (HANDOFF section 5) - the two-client proof's ini restore does not hold across an editor restart. A merge is
  rebuilt and tested before it is called merged; `git merge -F` takes a file, not `-`.
- Review from Claude Code: merge, `close-editor` -> `build-editor` -> `run-tests` -> `run-editor` -> force the play
  settings -> the proofs in sequence (twelve: loop, ship-graph, incidents, voyage-clock, level-pass, cargo,
  atmosphere, fuel, refinery, legibility, two-clients, floor; about 30 minutes), `git merge --no-ff`, one docs
  commit, push.
- Branch rule: one task per `<agent>/<task>` branch; reviewer (Claude) merges `--no-ff` after tests + proofs.
