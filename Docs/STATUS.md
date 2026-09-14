# Airlock - where we are (kept current by the reviewing agent at every merge)

Last update: 2026-09-13, evening (Claude, after the M5-5 merge; nothing in flight - Tom's first playtest is next).
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
  `AirlockSpill`. Report: `Docs/reports/M5-4b-floor-hazards.md` (plus the reviewer's section).
- **M5-5 The whole run** (Claude, merged 2026-09-13 evening as `8edd9b6`): the crosshair prompt (a line under the
  crosshair says what E does), a dropped cask lands on the deck through the interaction markers (M5-4b's flaw),
  the partial-squeegee wording, every label unlit and scaled fixtures labelled in world centimetres, the still's
  name alone over a gauge stack clear of the rim, lever labels that read their own gauges and go amber when they
  want pulling, a cask's label facing the walkway from wherever it stands, Okafor's air lines without the ship
  captain's opener, `AirlockFps`, the two-client proof working a still from the client, `verify-floor.ps1` working
  the floor by hand (loads, a flood, a pop and its reset, the scrubbers, a stumble, two squeegee passes: 2/3
  refined, 1545 L, 2 spills, 1 pop; a pop costs the cask), and Tom's playtest checklist
  (`Docs/playtest/first-playtest.md`). Report: `Docs/reports/M5-5-whole-run.md` (the reviewer's section has the
  merged tree's batch). **159 tests, thirteen proofs.**
- **Content**: `StationHub`, `Hauler_Mk1`, `RefineryFloor` (all pack copies with code-spawned fixtures; the
  floor's walking surface Z -65, its end platforms Z 22, the east walkway ramp under Still 3's controls).
  Packs: `THIRD_PARTY_ASSETS.md` (git-ignored).

## In flight (2026-09-13, evening)
- **The merged tree's proof batch is unfinished** (Tom shut the laptop down mid-batch). On `main` at `8edd9b6`
  the build and the 159 tests are green and six proofs exited 0 (loop, ship-graph, incidents, voyage-clock,
  level-pass, cargo). **First thing next session**: from `Airlock\` run `Scripts\run-editor.bat`, force
  single-player play settings (HANDOFF section 5), then `verify-atmosphere`, `verify-fuel`, `verify-refinery`,
  `verify-legibility`, `verify-two-clients`, `verify-floor`, `verify-prompt` (about 15 minutes) and put their
  tails in the M5-5 report's reviewer section. All seven passed on the branch's binary. If the editor window
  minimizes itself again (every capture then fails with `Failed to capture any editor windows`), run
  `Scripts\restore-editor-window.ps1` before each proof.
- Both editors are otherwise free: `Airlock\` (port 8000) and the worktree `..\Airlock-claude` (port 8001, on
  `claude/m5-5-whole-run`, merged). Astra's next brief goes through `agent\queue\181`.

## Next tasks
1. **Tom's first playtest** from `Docs/playtest/first-playtest.md` (solo in the editor; the compressed ship leg
   with the clock helper, the floor by hand). Five lines of notes decide the next brief: the floor cold rack, the
   light-versus-heavy recipe (can the crew tell a light cask wants 30 % heat?), the pop's cost, the frame rate.
2. After the playtest: the map-dressing pass for the ship (the hold's props, engineering's light), the audio pass
   (the hiss rising with pressure, the pop's bang, the flash's whump, the horn; a pop's spill could skip the flood
   cue), the flash's flicker on the still, and M4-2 (exposure on the PlayerState, server-side base fuel, player
   names in logs, join mid-voyage).
3. The exposure/scalability pass: `AirlockFps` reads 30 fps in viewport PIE on the lit floor with two stills
   running (the earlier "about 50 with one still"); two PIE instances are heavier. Profile before a two-player
   playtest.

## Open follow-ups (not blocking)
- The floor's casks warm and puff like the ship's, with no rack; loading them is the cure (`Load crate C2 -
  60C` on the board). A playtest decides whether a floor cold rack is needed.
- A cask in a still at the horn is lost (`CasksLost`); the board does not warn about it yet. A pop loses the
  cask too (the model's rule; the proof now accepts 2/3).
- A pop's fumes (about 60 ppm) black out a crew that stands at the still without scrubbing within forty seconds;
  the board's red air row is the only warning. Intended, but the playtest should say whether it reads as fair.
- Two crew on the floor with puddles (shrink and removal on the client, owner-only speed, late-join pop
  baselines, the medical row) is proven only by automation and the single-player proof; the two-client proof
  works a still from the client but is not a floor-hazard scenario.
- Floor fumes and medical consequences want a longer floor playtest; a critical is the cumulative blackout count,
  nobody is carried to the bunk.
- The literal dry squeegee ring beyond a 250 cm pop puddle is 47 cm (the hold reach is a sphere from the origin
  96 cm up), not the brief's 62; the 220 cm cleanup viewpoint works. Changing reach or puddle constants needs a
  brief.
- Leaving the ship through its own airlock skips the floor (an abort) and still pays the flat 10 t per intact
  cask; a playtest may want the door to dock too.
- Solo schedules can draw three coolant leaks out of four incidents; type variety. Today's Milk-run schedule
  targets Clamp_3 with a skipped cargo shift that a docking jump consumes: the floor proof jumps the ship clock
  to 75 s while the clamps are empty.
- The unlit label material is game-wide (one policy); the ship's legibility captures were re-read and look right,
  but nobody has played the ship under it. Restricting it to the floor is a two-line change.
- `PutDown .. Z=-63` on a deck the pawn's feet read as -65: the trace hits the deck mesh 2 cm over where the
  capsule settles; within tolerance, not investigated.
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
- Proof batches from Claude Code run detached (`Start-Process powershell -File <batch>.ps1` with the output
  redirected, a pid file, a Bash monitor that `iconv`s the UTF-16 log): the Bash tool's ten-minute cap would kill
  a batch mid-PIE. The redirect folder must exist first, a comma list passed through `-File` arrives as one
  string, and the batch's `$ErrorActionPreference` must be `Continue` around the proof loop or the first proof's
  stderr ends the batch (M5-5's four restarts).
- Review from Claude Code: merge, `close-editor` -> `build-editor` -> `run-tests` -> `run-editor` -> force the play
  settings -> the proofs in sequence (thirteen: loop, ship-graph, incidents, voyage-clock, level-pass, cargo,
  atmosphere, fuel, refinery, legibility, two-clients, floor, prompt; about 30 minutes), `git merge --no-ff`, one
  docs commit, push.
- Branch rule: one task per `<agent>/<task>` branch; reviewer (Claude) merges `--no-ff` after tests + proofs.
