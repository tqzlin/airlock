# Airlock - where we are (kept current by the reviewing agent at every merge)

Last update: 2026-09-13, early morning (Claude, after the M3-8 and M4-1 merges). Read `AGENTS.md` (what the game is,
code map) and `Docs/HANDOFF.md` (how to build, drive the editor, prove a change) first; this file is the *what next*.

## The mission (decided 2026-09-12)
The MVP voyage is **the refining run** (`Docs/design/refining-run.md`): a hauler lifts cryogenic hydrocarbon
crude ("sludge") from the Vell Extractor, the crude puffs nitriles and spills until the scrubbers are on and the
casks are racked, a feed valve fails mid-run and the crew limps to the Halder Solar Refinery by burning casks
through the crude bypass, the refinery refuels the ship and pays partly in fuel the base cannot make. The three
offers are three contract tiers of that run. Names are placeholders until Tom picks. **The refining run is playable
end to end on `main`, legible from where the player stands, and has been played by two clients in one PIE session.**

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
- **M3-4 Carry verb + volatile cargo + delivery tally** (`verify-cargo.ps1`; Astra). Report: `Docs/reports/M3-4-carry-and-cargo.md`.
- **M3-5 Hazard atmosphere, scrubbers, crew exposure** (`verify-atmosphere.ps1`; Astra). Report:
  `Docs/reports/M3-5-hazard-atmosphere.md`.
- **M3-6 Fuel, the feed leak and the crude bypass** (`verify-fuel.ps1`; Astra, reviewed by Claude). Report:
  `Docs/reports/M3-6-fuel-and-bypass.md`.
- **M3-7 Contract tiers, the refinery dock, base fuel, the Rig's signs** (`verify-refinery.ps1`; Claude). Report:
  `Docs/reports/M3-7-refinery-dock-and-base-fuel.md`.
- **M3-8 Legibility** (`verify-legibility.ps1`; Astra, reviewer-tuned, merged 2026-09-13): one label policy,
  two-line labels that face the walkway, no label on the carried cask, a heat-coloured state line, task-board
  summaries (`Secure 9 loose casks`, `Cool 3 casks - hottest C4 85C`) and the `AIR | FUEL | clock` readout, local
  status lights (100 cm / 12 cd), fixture labels at 14 cm, steam scaled 2.5 (it did not visibly grow - Cascade).
  Astra stopped at visual acceptance under the no-map rule and said exactly why; the reviewer moved two proof
  viewpoints and tuned three numbers. Report: `Docs/reports/M3-8-legibility.md` (Astra's, plus the reviewer's
  section). Astra's run: 38 min, clean.
- **M4-1 Dev-cheats gate, per-instance file console, the first two-client run** (`verify-two-clients.ps1`; Claude,
  merged 2026-09-13): every `Airlock*` helper's Server RPC is refused while `airlock.DevCheats` is 0 (on in editor
  builds, off in packaged ones), the file console has `commands.txt` for the server and `commands.client<N>.txt`
  per PIE client with `watching` / `ready` lines, `AirlockCapture <Label>` writes an instance's own viewport, and
  crew arriving at one PlayerStart step 120 cm aside instead of the second one spawning as a spectator (found by
  the proof). The proof runs a listen server plus one client through Dock B, a carry, the seat, the scrubbers,
  a refused cheat and the docking return. Report: `Docs/reports/M4-1-dev-cheats-and-two-clients.md`.
  **104 tests.**
- **Content**: `StationHub` (Interiors demo hall + docks/chambers/sky), `Hauler_Mk1` (Engineer demo hall + doors +
  the bridge). Packs: `THIRD_PARTY_ASSETS.md` (git-ignored). No map has changed since the bridge graft; every
  fixture, cask, pallet and sign is spawned from a code table.

## In flight (2026-09-13, early morning)
- Nothing. Both editors are open and idle on `main`'s binary: port 8000 on `Airlock\` (StationHub, no PIE) and
  port 8001 on the worktree `..\Airlock-claude` (switch it to a fresh branch from `main` before the next Claude task).

## Next tasks
1. **Map dressing pass** (Claude, needs the Nanite resave rule): the hold's pack props (barrels, gas bottles, the
   tarp table between X -5900 and -5300, Y 1650-1800) sit on the crate rows and block the walkway's view of the
   casks - move or remove them, dress the hold with the cryo rack, engineering with its fixtures, and put the
   refinery's mirror field in the hub sky. The first map change since the bridge graft.
2. **M4-2 Multiplayer proper**: exposure on the PlayerState (survives a pawn swap), the base fuel stock server-side
   with the save on the listen server only, PlayerState names in the server's gameplay log lines (`Carry C1 by
   <player>`), a client joining mid-voyage and leaving during a travel, the air / fuel beats and a blackout seen
   from the client, three players. Then the first packaged listen-server playtest (cheats default off there).
3. **Steam and sound**: convert `PS_SteamDoor` to Niagara so the 2.5 scale actually grows the puff; first sound
   placeholders (alarm, cough, valve, the airlock cycle).
4. **Playtest** the refining run solo and with two people; the design note's section 7 questions (names, tone,
   the refinery as a place) are Tom's calls before any polish spends on them.

## Open follow-ups (not blocking)
- Solo schedules can draw three coolant leaks out of four incidents (seed 20708 did); type variety.
- Slate accessibility snapshots omit the PIE HUD; proofs use captures (`AirlockCapture` for a PIE instance's own view).
- Exposure/scalability pass before any playtest (PIE was 2-6 FPS in one period; two PIE instances are heavier).
- The save game is per machine and unversioned (`AirlockSaveReset` clears it); server-side in M4-2.
- `StarfieldFree` creator name still to fill in `THIRD_PARTY_ASSETS.md`; industrial packs get Nanite on demand.
- Cascade particles are deprecated; convert with the Cascade-to-Niagara plugin (next task 3).
- Remote: `origin` = `https://github.com/tqzlin/airlock.git` (private); `main` and task branches are pushed after
  each merge.

## Agent loop facts
- Runner: `ai_game_studio\agent\runner.bat` polls `agent\queue\*.ps1` in name order, logs to `agent\results\`,
  moves scripts to `agent\done\`. Next free script number: **178**. Staged inputs under `agent\staged\handoffNN\`
  (next: `handoff22`; `handoff21` holds the M3-8 prompt and launch script). Scripts must be CRLF + ASCII. The launch
  guard stops on *any* untracked or modified file in `Airlock\`: commit everything before queueing; docs that must
  stay out of Astra's tree land on `main` from the worktree.
- Astra: Codex CLI on stack, `gpt-6-astra`, launched by a queue script that pipes the prompt on stdin and waits on
  the codex *process* (pattern: script 177). M3-5: 38 min; M3-6: 72 min; M3-8: 38 min, clean, and it stopped at a
  visual acceptance it could not meet without a map change and reported the exact reasons with captures - the
  behaviour we want. Give Astra briefs whose visual targets can be met inside its do-not-touch list, or say the
  reviewer tunes.
- Two agents share the laptop: never stop processes by script name (`verify-`) or `Stop-Process -Name`; filter by
  checkout path or pid. A cleanup by script name killed one of Astra's proof runs on 2026-09-12 (M4-1 report).
- Two-player PIE from a script: `verify-two-clients.ps1` is the recipe (play settings through `ObjectTools`, one
  console file per instance, `AirlockCapture` per instance, the play-settings ini section restored in `finally`).
- Review from Claude Code: merge, `close-editor` -> `build-editor` -> `run-tests` -> `run-editor` -> the proofs in
  sequence (eleven now: loop, ship-graph, incidents, voyage-clock, level-pass, cargo, atmosphere, fuel, refinery,
  legibility, two-clients; about 22 minutes), `git merge --no-ff`, one docs commit, push.
- Branch rule: one task per `<agent>/<task>` branch; reviewer (Claude) merges `--no-ff` after tests + proofs.
