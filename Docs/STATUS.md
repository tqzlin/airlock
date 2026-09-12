# Airlock - where we are (kept current by the reviewing agent at every merge)

Last update: 2026-09-12, late afternoon (Claude, after the level-pass merge; Astra's M3-4 in flight). Read `AGENTS.md` (what the game is, code map) and
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
- **Level pass + control seat** (`verify-level-pass.ps1`, merged 2026-09-12, Claude on Claude Code): modules and lines
  live in rooms - cargo hold (west hall), main-corridor lines, engineering in the "CENTRAL ENGINEERING" L-corridor
  (X -1531..-1106, Y 230..950, floor 22), galley in the junction's west stub, Nav on the bridge - with labels facing
  their reader; `AControlStation` seat inside the bridge terminal's U (E sits, control board with every module and
  line, E stands); `AirlockSit`/`AirlockStand`/`AirlockLook` helpers. 38 tests. Report: `Docs/reports/level-pass.md`.
  Finding: the Interiors room north of the hall is a vista (sill, glass, trench), not walkable; a future map pass can
  cut a door. Tooling for two agents (worktree + second editor on port 8001) is in `Docs/HANDOFF.md` section 7.
- **Content**: `StationHub` (Interiors demo hall + our docks/chambers/sky), `Hauler_Mk1` (Engineer demo hall + our
  doors + the **bridge**: Command Center room grafted onto the east corridor dead-end, 2026-09-12). Packs: see
  `THIRD_PARTY_ASSETS.md` (git-ignored; Season 1 bundle and three industrial packs added 2026-09-12).

## In flight (2026-09-12, afternoon)
- **Astra: M3-4 Carry verb + volatile cargo + tally** on `astra/m3-4-carry-and-cargo`, from the brief
  `Docs/tasks/M3-4-carry-and-cargo.md` (red-teamed by a second model before launch; 16 findings folded in). Launched
  by `agent\queue\174-launch-astra-m3-4.ps1`; log in `agent\results\174-*`. Owns the checkout `Airlock\` and the
  editor on port 8000.
- Claude's level pass is merged (above). When M3-4 lands, the reviewer resolves the expected conflicts with it in
  `AirlockCharacter.*`, `AirlockPlayerController.*` and `AirlockGameMode.cpp` (both branches add to the same files),
  reruns all five proofs, and carries Astra's probed cargo positions into the hall layout if they clash with the
  clamp controller at (-6300, 1900).
- Tom's decision 2026-09-12: Claude moves from the Cowork file queue to **Claude Code on stack**; the runner stays
  as the fallback (it launched Astra this time). Claude works in the worktree `..\Airlock-claude` with its editor on
  port 8001 (`Docs/HANDOFF.md` section 7).

## Next tasks (design source: project doc "Airlock voyage loop design", sections 3-4 and 8)
1. Review and merge M3-4 (Astra, in flight): diff vs main, do-not-touch list, rebuild, 38+ tests, the five proofs
   (`verify-loop`, `verify-ship-graph`, `verify-incidents`, `verify-level-pass`, `verify-cargo`), captures.
2. Map pass for engineering dressing (Claude, editor work, optional): the L-corridor already reads as engineering;
   if the Interiors room north of the hall is wanted as a proper bay, bridge the trench at Y 2100 and replace one
   670 cm sill + divider piece with a doorway. Nanite usage-flag resave for every new mesh placed.
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
- Engineering's east-wall labels (life support, `Power_1`, engines) crowd within 4 m; the coolant tank is the kit
  tank scaled 0.6 to fit the corridor; the seat has no chair mesh (the kit has none) (M3-6).
- `StarfieldFree` creator name still to fill in `THIRD_PARTY_ASSETS.md`; industrial packs get Nanite on demand.
- Cascade particles still run in 5.8 but are deprecated; convert with the Cascade-to-Niagara plugin when convenient.
- The repo has no remote. Push to a private GitHub repo with LFS before any second machine or agent.

## Agent loop facts
- Runner: `ai_game_studio\agent\runner.bat` polls `agent\queue\*.ps1` in name order, logs to `agent\results\`,
  moves scripts to `agent\done\`. Next free script number: **175**. Staged inputs live under
  `agent\staged\handoffNN\` (next: `handoff19`; `handoff18` holds the M3-4 prompt and launch script). Scripts must be
  CRLF + ASCII. From Claude Code, most of this is unnecessary: Claude runs builds, tests, proofs and MCP directly
  from its worktree (HANDOFF section 7); the runner only launches Astra now.
- Astra: Codex CLI on stack, `gpt-6-astra`, launched by a queue script that pipes the prompt file on stdin and waits
  on the codex *process* (pattern: script 171 v2, no usage-window wait since Tom upgraded the Codex plan on
  2026-09-12). M3-3 took 35 min and 231k tokens end to end. Prompts for done tasks: `agent\staged\handoff10..15\astra-*.txt`;
  M3-4's is `handoff18\astra-m3-4-prompt.txt` (launch script 174 checks only this checkout's editor, never by process name).
- Review pattern: `agent\done\172` (diff vs main, do-not-touch, rebuild if the binary is stale, tests, three proofs);
  merge pattern: `agent\done\173`.
- Branch rule: one task per `<agent>/<task>` branch; reviewer (Claude) merges `--no-ff` after tests + proofs.
