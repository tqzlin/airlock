# Airlock - where we are (kept current by the reviewing agent at every merge)

Last update: 2026-09-12, late night (Claude, after the M3-6 and M3-7 merges). Read `AGENTS.md` (what the game is,
code map) and `Docs/HANDOFF.md` (how to build, drive the editor, prove a change) first; this file is the *what next*.

## The mission (decided 2026-09-12)
The MVP voyage is **the refining run** (`Docs/design/refining-run.md`): a hauler lifts cryogenic hydrocarbon
crude ("sludge") from the Vell Extractor, the crude puffs nitriles and spills until the scrubbers are on and the
casks are racked, a feed valve fails mid-run and the crew limps to the Halder Solar Refinery by burning casks
through the crude bypass, the refinery refuels the ship and pays partly in fuel the base cannot make. The three
offers are three contract tiers of that run. Names are placeholders until Tom picks. **The refining run is now
playable end to end on `main`**: M3-5 air, M3-6 fuel and M3-7 tiers / refinery dock / base fuel all merged tonight.

## Verified on `main`
- **M2 loop** (`verify-loop.ps1`): hub PlayerStart -> book a contract at the board -> Dock B -> `Hauler_Mk1`
  PlayerStart -> exit airlock -> hub PlayerStart, voyage complete. Gated dock doors, transition card, airlock
  chambers, space sky.
- **M3-1 voyage clock** (`verify-voyage-clock.ps1`): replicated clock, captain announcements, automatic return.
- **M3-2 ship systems graph** (`verify-ship-graph.ps1`): 7 modules, 4 lines, leak -> coolant -> core -> brownout
  propagation, replicated module/line actors, hold-E repair with progress bar, console helpers.
- **M3-3 Incident Director + task board** (`verify-incidents.ps1`): seeded, paced incidents (teaching leak at 150 s,
  min gap 90 s, two-incident climax at 78..85 %), captain lines, top-left task board. Report: `Docs/reports/M3-3-incident-director.md`.
- **Level pass + control seat** (`verify-level-pass.ps1`): modules and lines in rooms, `AControlStation` seat +
  control board. Report: `Docs/reports/level-pass.md`. Two-agent tooling: `Docs/HANDOFF.md` section 7.
- **M3-4 Carry verb + volatile cargo + delivery tally** (`verify-cargo.ps1`; Astra): casks, clamps, a cold rack;
  carry / put down / place; casks warm and vent; the `CargoShift` climax; heating and loose rows; `Voyage result:`
  and `Last run:`. Report: `Docs/reports/M3-4-carry-and-cargo.md`.
- **M3-5 Hazard atmosphere, scrubbers, crew exposure** (`verify-atmosphere.ps1`; Astra): warm unracked casks puff
  nitriles, a vented cask bursts once, scrubbers at Normal/Hazard at the life-support panel, captain lines at 20 and
  50 ppm, exposure that slows at 60 % and blacks out for 30 s at 100 %, clean-air bonus and blackout fines. Report:
  `Docs/reports/M3-5-hazard-atmosphere.md`.
- **M3-6 Fuel, the feed leak and the crude bypass** (`verify-fuel.ps1`; Astra, reviewed by Claude, merged
  2026-09-12): a 90-unit fuel system burning 4 per minute, the scripted feed-leak set piece at 40-55 % riding the
  captain timer with a 10 s follow-up and a once-per-voyage `Reserve's gone` call the tick the ship is short
  (the reviewer's fix for the brief's contradiction Astra flagged), the feed valve hold, the bypass press, the
  intake that burns a carried cask (+15, soot, fouling, a hold to clear the injectors), exhaustion as a 120 s late
  dock, Burned casks, soot and late fines, fuel rows and the control board's fuel line. Report:
  `Docs/reports/M3-6-fuel-and-bypass.md` (Astra's, plus the reviewer's section). Astra's run: 72 min, no GUI tool.
- **M3-7 Contract tiers, the refinery dock, base fuel, the Rig's signs** (`verify-refinery.ps1`; Claude, merged
  2026-09-12): Milk run / Standard / Priority = 4 / 8 / 12 casks from a probed tiered hold (pallets at 0.75), the
  `DOCKING - HALDER SOLAR REFINERY` card and refinery-flavoured captain lines, 15 t credited and 10 t surplus per
  intact cask into a save-game base fuel stock shown on the `VELL EXTRACTOR - DOCK B DEPARTURES` board, two
  code-spawned signs. Report: `Docs/reports/M3-7-refinery-dock-and-base-fuel.md`. **90 tests.**
- **Content**: `StationHub` (Interiors demo hall + docks/chambers/sky), `Hauler_Mk1` (Engineer demo hall + doors +
  the bridge). Packs: `THIRD_PARTY_ASSETS.md` (git-ignored). No map has changed since the bridge graft; every
  fixture, cask, pallet and sign is spawned from a code table.

## In flight (2026-09-12, late night)
- Nothing. Both editors are open and idle: port 8000 on `Airlock\` (`main`, StationHub, no PIE) and port 8001 on
  the worktree `..\Airlock-claude` (`claude/m3-7-refinery-dock`, merged; switch it to a fresh branch from `main`
  before the next Claude task).

## Next tasks
1. **M3-8 Legibility pass** (brief to write, then Astra or Claude): the follow-ups below are its list - label sizes
   and facing (the carried crate's label fills the view; crate/pad labels face -X only; the twelve-cask hold's
   labels overlap; engineering's module lights flood the life-support and Engines views), the task board's `+n
   more` on a Priority manifest (a `12 loose casks` summary row), steam that reads (puffs, vents, the feed leak),
   the puff/heat light on casks, the HUD's exposure and fuel readouts, the hub sky's refinery mirror field, and
   a first pass of sound placeholders (alarm, cough, valve). Content may now change maps for dressing (the hold's
   cryo rack, engineering's fixtures) with the Nanite resave rule.
2. **M4 groundwork**: gate every console helper behind a dev-cheats flag; move exposure to the PlayerState and the
   base fuel stock server-side; a two-client PIE proof of the carry, the seat and the scrubber control.
3. **Playtest** the refining run solo and with two people once M3-8 lands; the design note's section 7 questions
   (names, tone, the refinery as a place) are Tom's calls before any polish spends on them.

## Open follow-ups (not blocking)
- Every console helper is an unchecked Server RPC: gate before M4 (item 2).
- Solo schedules can draw three coolant leaks out of four incidents (seed 20708 did); type variety in M3-8.
- Slate accessibility snapshots omit the PIE HUD; proofs must use captures for widget checks.
- Exposure/scalability pass before any playtest (PIE was 2-6 FPS in one period).
- Steam on lines, vented/puffing crates and the feed valve is faint; the red lights carry them (M3-8).
- The carried crate's label fills the view; crate and pad labels face -X only; the twelve-cask hold's labels
  overlap where rows are 100 cm apart; engineering's lights flood close views (M3-8).
- The task board caps at 8 rows: a Priority manifest shows `Secure crate C1..C8` then `+4 more` (M3-8).
- Cargo and fuel fixture positions are code-table placeholders; the hold's and engineering's dressing waits for a
  map pass (M3-8).
- The save game is per machine and unversioned (`AirlockSaveReset` clears it); server-side in M4.
- `StarfieldFree` creator name still to fill in `THIRD_PARTY_ASSETS.md`; industrial packs get Nanite on demand.
- Cascade particles are deprecated; convert with the Cascade-to-Niagara plugin when convenient.
- Remote: `origin` = `https://github.com/tqzlin/airlock.git` (private); `main` and task branches are pushed after
  each merge.

## Agent loop facts
- Runner: `ai_game_studio\agent\runner.bat` polls `agent\queue\*.ps1` in name order, logs to `agent\results\`,
  moves scripts to `agent\done\`. Next free script number: **177**. Staged inputs under `agent\staged\handoffNN\`
  (next: `handoff21`; `handoff19` / `handoff20` hold the M3-5 / M3-6 prompts and launch scripts). Scripts must be
  CRLF + ASCII. The launch guard stops on *any* untracked or modified file in `Airlock\`: commit everything before
  queueing; docs that must stay out of Astra's tree land on `main` from the worktree.
- Astra: Codex CLI on stack, `gpt-6-astra`, launched by a queue script that pipes the prompt on stdin and waits on
  the codex *process* (pattern: script 176). M3-3: 35 min; M3-4: 39 min (cut short by a physical Escape on its
  Computer Use tool); M3-5: 38 min, clean; M3-6: 72 min, clean, and it stopped at a genuine brief contradiction
  with a clear report instead of bending the proof - the behaviour we want. When a brief's numbers are changed at
  red-team time (90 units, not 80), re-derive every timing that depended on them.
- Review from Claude Code: merge, `close-editor` -> `build-editor` -> `run-tests` -> `run-editor` -> the proofs in
  sequence (nine now: loop, ship-graph, incidents, voyage-clock, level-pass, cargo, atmosphere, fuel, refinery, about
  17 minutes), `git merge --no-ff`, one docs commit, push. Two branches can be merged and proven once on the merged
  tree, as M3-6 and M3-7 were tonight.
- Branch rule: one task per `<agent>/<task>` branch; reviewer (Claude) merges `--no-ff` after tests + proofs.
