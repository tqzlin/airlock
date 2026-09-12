# Airlock - where we are (kept current by the reviewing agent at every merge)

Last update: 2026-09-12, afternoon (Claude, after the M3-3 merge). Read `AGENTS.md` (what the game is, code map) and
`Docs/HANDOFF.md` (how to build, drive the editor, prove a change) first; this file is the *what next*.

## Verified on `main`
- **M2 loop** (`verify-loop.ps1`): hub PlayerStart -> book a voyage at the board -> Dock B -> `Hauler_Mk1` PlayerStart ->
  exit airlock -> hub PlayerStart, voyage complete. Gated dock doors, transition card, airlock chambers, space sky.
- **M3-1 voyage clock** (`verify-voyage-clock.ps1`): replicated clock, captain announcements, automatic return.
- **M3-2 ship systems graph** (`verify-ship-graph.ps1`): 7 modules, 4 lines, leak -> coolant -> core -> brownout
  propagation, replicated module/line actors with lights, labels and steam, hold-E repair with progress bar,
  console helpers.
- **M3-3 Incident Director + task board** (`verify-incidents.ps1`, merged 2026-09-12): seeded, paced incident
  schedule (teaching leak at 150 s, min gap 90 s, two-incident climax at 78..85 %), server director that applies
  faults/damage through the graph API and speaks captain lines, top-left task board from replicated state,
  `AirlockIncident` / `AirlockIncidentSchedule` helpers. 34 automation tests. Report: `Docs/reports/M3-3-incident-director.md`.
  Decision recorded in the brief: crew clamps to 1..5 for the count (1 -> 4, 4 -> 7, 9 -> 8), per Astra's reading of the tests.
- **Content**: `StationHub` (Interiors demo hall + our docks/chambers/sky), `Hauler_Mk1` (Engineer demo hall + our
  doors + the **bridge**: Command Center room grafted onto the east corridor dead-end, 2026-09-12). Packs: see
  `THIRD_PARTY_ASSETS.md` (git-ignored; Season 1 bundle and three industrial packs added 2026-09-12).

## In flight
- Nothing on a branch. Tom's decision 2026-09-12: Claude moves from the Cowork file queue to **Claude Code on stack**
  (`Docs/HANDOFF.md` section 7) starting with the level pass below; the runner stays as the fallback.

## Next tasks (design source: project doc "Airlock voyage loop design", sections 3-4 and 8)
1. Level pass (Claude, editor work, first Claude Code task): move placeholder modules/lines from the west hall into
   real rooms with real kit dressing (engineering: the Interiors nook off the corridor at `(-4300, 1780)`, floor
   `Z -68`, and the corridor walls for lines; cargo hold: the west hall; galley: a nook off the east L-corridor);
   control station seat on the bridge terminal (`AControlStation`, design doc section 8.3); Nanite usage-flag resave
   for every new mesh placed. Positions live in `Ship/ShipLayouts.cpp`, never in a map.
2. M3-4 Carry verb + volatile cargo + scoring (Astra-sized): carry socket, `ACargoCrate` with volatility/vent, cold
   rack, delivered-intact tally, results on the hub board. Brief to write: `Docs/tasks/M3-4-carry-and-cargo.md`.
   Have a second model red-team the brief for contradictions before launch (M3-3's crew-count clash cost a question).
3. M3-5 Cannibalize + calibrate: part slots on modules, remove/install through Carry, Core calibration minigame at the
   bridge terminal, the galley as the sacrificial module.
4. M3-6 legibility pass, M3-7 hub payoff, then M4 multiplayer.

## Open follow-ups (not blocking)
- Every console helper (now including `AirlockIncident*`) is an unchecked Server RPC: gate behind a dev-cheats flag before M4.
- Solo schedules can draw three coolant leaks out of four incidents (seed 20708 did); consider type variety in the
  draw when M3-6 tunes the curve.
- Slate accessibility snapshots omit the PIE HUD; proofs must use captures for widget checks (M3-3 report).
- Exposure/scalability pass before any playtest (PIE was 2-6 FPS in one period; unclear if throttle or GPU).
- Steam on leaking lines is hard to see in `CaptureEditorImage` frames (M3-6).
- Module labels face -X; readable from the corridor side only (M3-6).
- `StarfieldFree` creator name still to fill in `THIRD_PARTY_ASSETS.md`; industrial packs get Nanite on demand.
- Cascade particles still run in 5.8 but are deprecated; convert with the Cascade-to-Niagara plugin when convenient.
- The repo has no remote. Push to a private GitHub repo with LFS before any second machine or agent.

## Agent loop facts
- Runner: `ai_game_studio\agent\runner.bat` polls `agent\queue\*.ps1` in name order, logs to `agent\results\`,
  moves scripts to `agent\done\`. Next free script number: **174**. Staged inputs live under
  `agent\staged\handoffNN\` (next: `handoff18`). Scripts must be CRLF + ASCII.
- Astra: Codex CLI on stack, `gpt-6-astra`, launched by a queue script that pipes the prompt file on stdin and waits
  on the codex *process* (pattern: script 171 v2, no usage-window wait since Tom upgraded the Codex plan on
  2026-09-12). M3-3 took 35 min and 231k tokens end to end. Prompts for done tasks: `agent\staged\handoff10..15\astra-*.txt`.
- Review pattern: `agent\done\172` (diff vs main, do-not-touch, rebuild if the binary is stale, tests, three proofs);
  merge pattern: `agent\done\173`.
- Branch rule: one task per `<agent>/<task>` branch; reviewer (Claude) merges `--no-ff` after tests + proofs.
