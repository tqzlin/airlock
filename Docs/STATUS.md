# Airlock - where we are (kept current by the reviewing agent at every merge)

Last update: 2026-09-13, afternoon (Claude, after the M5-4b merge; M5-5 in flight in Claude's worktree).
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
  squeegee, containment. Report: `Docs/reports/M5-4a-floor-hazards-model.md`.
- **M5-4b Floor hazards with bodies** (Astra, 48 min, merged 2026-09-13 afternoon): a flood or a pop leaves a
  dark steaming puddle (`ASpill`) at the still's spill point that shrinks as it evaporates into the room's air;
  crew in the crude walk at 60 % and drop a carried cask after four seconds (the stumble, Okafor's line); a
  six-second squeegee hold clears 300 L; a running still that pops flashes white with steam; the board leads with
  the medical total and one squeegee row per puddle, the bunk counts the criticals; `AirlockFloor` /
  `AirlockSpill`; the floor proof delivers three casks, floods Still 2, drops C3 in the crude and squeegees it
  (3/3 refined, 1853 L). Report: `Docs/reports/M5-4b-floor-hazards.md` (plus the reviewer's section).
  **158 tests, twelve proofs.**
- **Content**: `StationHub`, `Hauler_Mk1`, `RefineryFloor` (all pack copies with code-spawned fixtures; the
  floor's walking surface Z -65, its end platforms Z 22, the east walkway ramp under Still 3's controls).
  Packs: `THIRD_PARTY_ASSETS.md` (git-ignored).

## In flight (2026-09-13, afternoon)
- **M5-5 The whole run** (Claude, `claude/m5-5-whole-run` in the worktree `..\Airlock-claude`, port 8001, editor
  closed; branched from `main` before the M5-4b merge - merge `main` in first, the two branches both touch
  `Core/AirlockCharacter.*`). Landed on the branch so far: the **crosshair prompt** (`UI/PromptWidget`: a line
  under the crosshair says what E does to the thing in reach, logged `Prompt: <text>` on change; 1 test;
  `Scripts\verify-prompt.ps1`), `verify-two-clients.ps1` working a still from the client on the floor, and the
  first playtest checklist for Tom (`Docs/playtest/first-playtest.md`). Still to do: the tuning pass from the
  captures (the gauge stack inherits the still's 0.8 scale and overlaps the base label's second line; the still
  labels front-lit; the lever labels' second line), the put-down landing surface (a cask dropped in a puddle
  rests on the spill marker's top 30 cm up, and on a control marker 96 cm up in the live viewpoint - the
  put-down trace should ignore interaction markers), the partial-squeegee log wording (`cleared by` on a 300 L
  pass that leaves a puddle), `verify-floor.ps1` through the hazards by hand where the console still drives them,
  then the exposure/scalability check before the playtest.
- `Airlock\` (port 8000) is free: Astra's M5-4b run is over and the editor there was restarted by the reviewer for
  the proof batch. Astra's next brief goes through `agent\queue\181`.

## Next tasks (the M5 plan, `refinery-floor.md` section 9)
1. **M5-5** (Claude, in flight) - then Tom's first playtest from `Docs/playtest/first-playtest.md`.
2. After the playtest: the map-dressing pass for the ship (the hold's props, engineering's light), the audio pass
   (the hiss rising with pressure, the pop's bang, the flash's whump, the horn), the flash's flicker on the still,
   and M4-2 (exposure on the PlayerState, server-side base fuel, player names in logs, join mid-voyage).

## Open follow-ups (not blocking)
- The floor's casks warm and puff like the ship's, with no rack; loading them is the cure (`Load crate C2 -
  60C` on the board). A playtest decides whether a floor cold rack is needed.
- A cask in a still at the horn is lost (`CasksLost`); the board does not warn about it yet.
- Two crew on the floor with puddles (shrink and removal on the client, owner-only speed, late-join pop
  baselines, the medical row) is proven only by automation and the single-player proof; the two-client proof is
  a regression proof, not a floor-hazard scenario.
- Floor fumes and medical consequences want a longer floor playtest: a single cleaned-up 200 L spill stayed at
  0 published ppm under the existing scrubbers; a critical is the cumulative blackout count, nobody is carried to
  the bunk.
- The literal dry squeegee ring beyond a 250 cm pop puddle is 47 cm (the hold reach is a sphere from the origin
  96 cm up), not the brief's 62; the 220 cm cleanup viewpoint works. Changing reach or puddle constants needs a
  brief.
- Leaving the ship through its own airlock skips the floor (an abort) and still pays the flat 10 t per intact
  cask; a playtest may want the door to dock too.
- Solo schedules can draw three coolant leaks out of four incidents; type variety. Today's Milk-run schedule
  targets Clamp_3 with a skipped cargo shift that a docking jump consumes: the floor proof now jumps the ship
  clock to 75 s while the clamps are empty.
- Exposure/scalability pass before any playtest (PIE runs about 50 FPS on the lit floor with one still; two PIE
  instances are heavier).
- The save game is per machine and unversioned (`AirlockSaveReset` clears it); server-side in M4-2.
- `StarfieldFree` creator name still to fill in `THIRD_PARTY_ASSETS.md`; Cascade steam to Niagara (the puddle's
  and the pop's steam are the same scaled sprites and read mostly by their lights).
- Remote: `origin` = `https://github.com/tqzlin/airlock.git` (private); `main` and task branches are pushed after
  each merge.

## Agent loop facts
- Runner: `ai_game_studio\agent\runner.bat` polls `agent\queue\*.ps1` in name order, logs to `agent\results\`,
  moves scripts to `agent\done\`. Next free script number: **181**. Staged inputs under `agent\staged\handoffNN\`
  (next: `handoff25`; `handoff24` holds the M5-4b prompt and launch script). Scripts must be CRLF + ASCII. The
  launch guard stops on *any* untracked or modified file in `Airlock\`: commit everything before queueing.
- Astra: Codex CLI on stack, `gpt-6-astra`, launched by a queue script that pipes the prompt on stdin and waits on
  the codex *process* (pattern: scripts 178-180). M3-8: 38 min; M5-1: 32 min; M5-3b: 55 min; M5-4b: 48 min, eleven
  commits, the brief followed to the letter (it recorded the marker-box landing and the 47 cm ring as measured
  facts with tests instead of tuning them - the behaviour we want) and two proof-setup corrections declared in
  the report (the clock jump that consumes the skipped incidents, the queued cask's `Running cask=C3` in place of
  `Idle`). Red-team every brief before launch; the M5-3b and M5-4b readers each found 16-22 real problems.
- Two agents share the laptop: never stop processes by script name; filter by checkout path or pid, build the
  pattern from pieces (a filter on a command line matches the shell that runs it), and leave `zenserver.exe`
  alone. Do not compile one checkout while the other's editor is starting, and do not build in the worktree
  while a proof batch runs on the other editor (one GPU; the proofs are timing-sensitive).
- Every proof but two-clients needs single-player play settings; force them through the MCP before a batch
  (HANDOFF section 5) - the two-client proof's ini restore does not hold across an editor restart. A merge is
  rebuilt and tested before it is called merged; `git merge -F` takes a file, not `-`. The headless suite's
  counts are in `Saved\Automation\Report\index.json`, not in `run-tests.bat`'s stdout.
- Review from Claude Code: merge, `close-editor` -> `build-editor` -> `run-tests` -> `run-editor` -> force the play
  settings -> the proofs in sequence (twelve: loop, ship-graph, incidents, voyage-clock, level-pass, cargo,
  atmosphere, fuel, refinery, legibility, two-clients, floor; about 30 minutes), `git merge --no-ff`, one docs
  commit, push.
- Branch rule: one task per `<agent>/<task>` branch; reviewer (Claude) merges `--no-ff` after tests + proofs.
