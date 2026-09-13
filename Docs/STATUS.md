# Airlock - where we are (kept current by the reviewing agent at every merge)

Last update: 2026-09-12, evening (Claude: the refining run adopted as the MVP mission; Astra launched on M3-5). Read
`AGENTS.md` (what the game is, code map) and `Docs/HANDOFF.md` (how to build, drive the editor, prove a change)
first; this file is the *what next*.

## The mission (decided 2026-09-12)
The MVP voyage is **the refining run** (`Docs/design/refining-run.md`): a hauler lifts cryogenic hydrocarbon
crude ("sludge") from the Vell Extractor, the crude puffs nitriles and spills until the scrubbers are on and the
casks are racked, a feed valve fails mid-run and the crew limps to the Halder Solar Refinery by burning casks
through the crude bypass, the refinery refuels the ship and pays partly in fuel the base cannot make. The three
voyage types become three contract tiers of that run. Names are placeholders until Tom picks. The plan: M3-5
atmosphere + scrubbers (Astra, in flight), M3-6 fuel + bypass (brief written, launches after M3-5 merges), M3-7
refinery dock + base fuel + hub reskin (Claude), M3-8 legibility, then M4.

## Verified on `main`
- **M2 loop** (`verify-loop.ps1`): hub PlayerStart -> book a voyage at the board -> Dock B -> `Hauler_Mk1` PlayerStart ->
  exit airlock -> hub PlayerStart, voyage complete. Gated dock doors, transition card, airlock chambers, space sky.
- **M3-1 voyage clock** (`verify-voyage-clock.ps1`): replicated clock, captain announcements, automatic return.
- **M3-2 ship systems graph** (`verify-ship-graph.ps1`): 7 modules, 4 lines, leak -> coolant -> core -> brownout
  propagation, replicated module/line actors with lights, labels and steam, hold-E repair with progress bar,
  console helpers.
- **M3-3 Incident Director + task board** (`verify-incidents.ps1`): seeded, paced incident schedule (teaching leak
  at 150 s, min gap 90 s, two-incident climax at 78..85 %), server director, captain lines, top-left task board,
  `AirlockIncident` / `AirlockIncidentSchedule`. Report: `Docs/reports/M3-3-incident-director.md`.
- **Level pass + control seat** (`verify-level-pass.ps1`): modules and lines in rooms (hold, corridor, engineering
  L-corridor, galley stub, bridge), `AControlStation` seat + control board, `AirlockSit`/`AirlockStand`/`AirlockLook`.
  Report: `Docs/reports/level-pass.md`. Two-agent tooling: `Docs/HANDOFF.md` section 7.
- **M3-4 Carry verb + volatile cargo + delivery tally** (`verify-cargo.ps1`; Astra, reviewed by Claude): casks,
  clamps and a cold rack in the hold; carry / put down / place; volatile casks warm, vent at 100 C; the `CargoShift`
  climax incident; heating and loose rows on the task board; `Voyage result:` at docking and `Last run:` on the hub
  board. **56 tests.** Report: `Docs/reports/M3-4-carry-and-cargo.md`.
- **Content**: `StationHub` (Interiors demo hall + docks/chambers/sky), `Hauler_Mk1` (Engineer demo hall + doors +
  the bridge). Packs: `THIRD_PARTY_ASSETS.md` (git-ignored).

## In flight (2026-09-12, evening)
- **Astra: M3-5 Hazard atmosphere, scrubbers, crew exposure** on `astra/m3-5-hazard-atmosphere`, from the brief
  `Docs/tasks/M3-5-hazard-atmosphere.md` (red-teamed by a second reader before launch; findings folded in). Launched
  by `agent\queue\175-launch-astra-m3-5.ps1`; log in `agent\results\175-*`. Owns the checkout `Airlock\` and the
  editor on port 8000. Expected: `FShipAtmosphere` + puffs/rupture sources and scrub sinks, the life-support scrub
  control, per-crew exposure with the 30 s blackout, `Hold air` rows, clean-air bonus and blackout fines,
  `verify-atmosphere.ps1`.
- Claude: reviewer; while Astra runs, the M3-6 brief (`Docs/tasks/M3-6-fuel-and-bypass.md`, written, to be
  red-teamed before its launch) and docs. The worktree `..\Airlock-claude` (editor on port 8001) is idle on
  `claude/level-pass`; merge `main` in before using it.

## Next tasks
1. Review and merge M3-5 (diff vs main, do-not-touch, rebuild, 56+ tests, the six proofs + `verify-atmosphere`).
2. Red-team and launch **M3-6 Fuel, the feed leak and the crude bypass** (brief written): pure `FFuelSystem`, the
   scripted fuel-leak set piece at 40-55 % of the run, the feed valve hold, the bypass press, the intake that burns
   a carried cask (+15 fuel, Engines -10/-20, soot, `Burned` state), the injector-clear hold, exhaustion = a late
   dock, soot and late fines, `verify-fuel.ps1`. The mixture minigame is deferred to the timing-calls milestone.
3. **M3-7 Refinery dock + base fuel + hub reskin** (Claude, content + code): the exit becomes the refinery dock with
   its own card; fuel credit per intact cask and `BaseFuelStock` persisted (SaveGame) with a base line on the
   departure board; three contract tiers (4/8/12 casks) instead of three voyage types; lore signage without renaming
   sections; the hold's dressing (cryo rack fixture, intake hatch). Brief to write after M3-6 launches.
4. M3-8 legibility pass (the follow-ups below), then M4 multiplayer. Deferred: parts / cannibalize slots, the
   power-lobbying board and timing calls (design note 8.1 levers 2-3), fires.

## Open follow-ups (not blocking)
- Every console helper is an unchecked Server RPC: gate behind a dev-cheats flag before M4.
- Solo schedules can draw three coolant leaks out of four incidents (seed 20708 did); type variety in M3-8.
- Slate accessibility snapshots omit the PIE HUD; proofs must use captures for widget checks.
- Exposure/scalability pass before any playtest (PIE was 2-6 FPS in one period).
- Steam on leaking lines and vented crates is faint in captures; the red heat light is what reads (M3-8).
- The carried crate's label fills the view; crate and pad labels face -X only (M3-8).
- Engineering's east-wall labels crowd; the coolant tank is scaled 0.6; the seat has no chair mesh (M3-8).
- Cargo positions are placeholders on the hall floor; the hold's dressing waits for M3-7.
- `StarfieldFree` creator name still to fill in `THIRD_PARTY_ASSETS.md`; industrial packs get Nanite on demand.
- Cascade particles are deprecated; convert with the Cascade-to-Niagara plugin when convenient.
- Remote: `origin` = `https://github.com/tqzlin/airlock.git` (private); `main` and task branches are pushed after
  each merge.

## Agent loop facts
- Runner: `ai_game_studio\agent\runner.bat` polls `agent\queue\*.ps1` in name order, logs to `agent\results\`,
  moves scripts to `agent\done\`. Next free script number: **176**. Staged inputs under `agent\staged\handoffNN\`
  (next: `handoff20`; `handoff19` holds the M3-5 prompt and launch script). Scripts must be CRLF + ASCII. From
  Claude Code the review runs by hand (merge, `close-editor` -> `build-editor` -> `run-tests` -> `run-editor` ->
  the proofs in sequence, `git merge --no-ff`, the docs commit); the runner only launches Astra.
- Astra: Codex CLI on stack, `gpt-6-astra`, launched by a queue script that pipes the prompt on stdin and waits on
  the codex *process* (pattern: script 175). M3-3: 35 min / 231k tokens; M3-4: 39 min / 259k tokens, cut short by a
  physical Escape on its Computer Use tool - the M3-5 prompt forbids GUI tools and points at HANDOFF section 5 for
  the one dialog that can appear.
- Review pattern: `agent\done\172` for the runner, by hand from Claude Code otherwise; merge pattern: `agent\done\173`.
- Branch rule: one task per `<agent>/<task>` branch; reviewer (Claude) merges `--no-ff` after tests + proofs.
