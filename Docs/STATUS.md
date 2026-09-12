# Airlock - where we are (kept current by the reviewing agent at every merge)

Last update: 2026-09-12, evening (Claude, after the M3-4 merge). Read `AGENTS.md` (what the game is, code map) and
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
  `AirlockIncident` / `AirlockIncidentSchedule` helpers. Report: `Docs/reports/M3-3-incident-director.md`.
  Decision recorded in the brief: crew clamps to 1..5 for the count (1 -> 4, 4 -> 7, 9 -> 8), per Astra's reading of the tests.
- **Level pass + control seat** (`verify-level-pass.ps1`, merged 2026-09-12, Claude on Claude Code): modules and lines
  live in rooms - cargo hold (west hall), main-corridor lines, engineering in the "CENTRAL ENGINEERING" L-corridor
  (X -1531..-1106, Y 230..950, floor 22), galley in the junction's west stub, Nav on the bridge - with labels facing
  their reader; `AControlStation` seat inside the bridge terminal's U (E sits, control board with every module and
  line, E stands); `AirlockSit`/`AirlockStand`/`AirlockLook` helpers. Report: `Docs/reports/level-pass.md`.
  Finding: the Interiors room north of the hall is a vista (sill, glass, trench), not walkable; a future map pass can
  cut a door. Tooling for two agents (worktree + second editor on port 8001) is in `Docs/HANDOFF.md` section 7.
- **M3-4 Carry verb + volatile cargo + delivery tally** (`verify-cargo.ps1`, merged 2026-09-12; GPT-6 Astra, reviewed
  and finished by Claude): four crates, three clamps and a cold rack spawn in the hold from `Ship/CargoLayouts`; E
  picks a crate up (server-owned hands, 60 % walk speed, prompt bottom-centre), E again puts it down or places it in
  the pad you look at; loose or carried volatile crates heat once the voyage has run 150 s, the rack cools, 100 C vents
  (steam, red light, inert); a `CargoShift` is always one of the two climax incidents and shakes a clamped crate loose;
  the task board shows `Cool crate C2 - 60C` first and `Secure crate C3` last; docking logs `Voyage result: 2/4 crates
  intact, 1 vented, 1 loose, hull 100.00%, pay 400` and the hub board shows `Last run: ...`. **56 tests.** Report:
  `Docs/reports/M3-4-carry-and-cargo.md` (Astra's checkpoint plus the reviewer's verification). Astra's run was cut
  short by a physical Escape on its Computer Use tool; the reviewer merged `main` in (seat/carry guards), fixed the
  racy log-capture test and the proof's incident count, and reproduced all five proofs plus the position probe.
- **Content**: `StationHub` (Interiors demo hall + our docks/chambers/sky), `Hauler_Mk1` (Engineer demo hall + our
  doors + the **bridge**: Command Center room grafted onto the east corridor dead-end, 2026-09-12). Packs: see
  `THIRD_PARTY_ASSETS.md` (git-ignored; Season 1 bundle and three industrial packs added 2026-09-12).

## In flight (2026-09-12, evening)
- Nothing. Two editors are open and idle: port 8000 on `Airlock\` (`main`, the M3-4 binary, StationHub, no PIE) and
  port 8001 on the worktree `..\Airlock-claude` (still on `claude/level-pass`; merge `main` in before using it).
- Tom's decision 2026-09-12: Claude works from **Claude Code on stack**; the file-queue runner stays as the fallback
  and is what launches Astra (`Docs/HANDOFF.md` section 7).

## Next tasks (design source: project doc "Airlock voyage loop design", sections 3-4 and 8)
1. **M3-5 Cannibalize + calibrate** - brief to write (Claude), red-team by a second model, then Astra: part slots on
   modules, remove/install through Carry (a part is a carried item with a slot; `ACargoCrate`/`ACargoSlot` and the
   character's server-owned hands already exist), the Core calibration minigame at the bridge terminal (the seat and
   `ControlBoardWidget` exist), the galley as the sacrificial module. Settle in the brief: what a part actor is (a
   crate kind or its own class), how a module missing a part reads on the task board and in the graph, and what
   calibration changes. Brief file: `Docs/tasks/M3-5-cannibalize-and-calibrate.md`.
2. Map pass for engineering and the hold (Claude, editor work, optional): the L-corridor already reads as engineering;
   if the Interiors room north of the hall is wanted as a proper bay, bridge the trench at Y 2100 and replace one
   670 cm sill + divider piece with a doorway; give the hold real clamp fixtures and a cold locker for the rack.
   Nanite usage-flag resave for every new mesh placed.
3. M3-6 legibility pass (the follow-ups below are its list), M3-7 hub payoff, then M4 multiplayer.

## Open follow-ups (not blocking)
- Every console helper (now the six cargo ones too) is an unchecked Server RPC: gate behind a dev-cheats flag before M4.
- Solo schedules can draw three coolant leaks out of four incidents (seed 20708 did); consider type variety in the
  draw when M3-6 tunes the curve.
- Slate accessibility snapshots omit the PIE HUD; proofs must use captures for widget checks (M3-3 report).
- Exposure/scalability pass before any playtest (PIE was 2-6 FPS in one period; unclear if throttle or GPU).
- Steam on leaking lines and on vented crates is hard to see in `CaptureEditorImage` frames; the red heat light is
  what reads (M3-6).
- The label of a carried crate hangs 80 cm from the camera and fills the view (`C1  Carried`); hide or shrink it while
  carried. Crate and pad labels face -X only (M3-6).
- Engineering's east-wall labels (life support, `Power_1`, engines) crowd within 4 m; the coolant tank is the kit
  tank scaled 0.6 to fit the corridor; the seat has no chair mesh (the kit has none) (M3-6).
- Cargo positions are placeholders on the hall floor (X -6200..-5000, Y 1300..1700); the hold's dressing waits for the
  map pass (item 2).
- `StarfieldFree` creator name still to fill in `THIRD_PARTY_ASSETS.md`; industrial packs get Nanite on demand.
- Cascade particles still run in 5.8 but are deprecated; convert with the Cascade-to-Niagara plugin when convenient.
- Remote: `origin` = `https://github.com/tqzlin/airlock.git` (private). `main` and every task branch are pushed after
  each merge; pull before starting on a second machine.

## Agent loop facts
- Runner: `ai_game_studio\agent\runner.bat` polls `agent\queue\*.ps1` in name order, logs to `agent\results\`,
  moves scripts to `agent\done\`. Next free script number: **175**. Staged inputs live under
  `agent\staged\handoffNN\` (next: `handoff19`; `handoff18` holds the M3-4 prompt and launch script). Scripts must be
  CRLF + ASCII. From Claude Code most of this is unnecessary: the M3-4 review ran entirely from Claude Code (merge,
  `close-editor` -> `build-editor` -> `run-tests` -> `run-editor` -> the proofs in sequence, then the `--no-ff` merge
  and the docs commit, all on `Airlock\` once Astra's process had exited); the runner only launches Astra now.
- Astra: Codex CLI on stack, `gpt-6-astra`, launched by a queue script that pipes the prompt file on stdin and waits
  on the codex *process* (pattern: script 174). M3-3 took 35 min and 231k tokens; M3-4 took 39 min and 259k tokens and
  ended at a checkpoint (its Computer Use tool reported a physical Escape and stopped), so it committed a checkpoint
  report and left the PIE proofs to the reviewer. Next prompt: tell Astra not to reach for Computer Use when the file
  console and `Scripts\close-editor.ps1` cover the job, and how to clear a Restore Packages dialog (HANDOFF section 5).
  Prompts for done tasks: `agent\staged\handoff10..15\astra-*.txt`, `handoff18\astra-m3-4-prompt.txt`.
- Review pattern: `agent\done\172` for the runner; from Claude Code the same steps by hand (above). Merge pattern:
  `agent\done\173` (merge `--no-ff`, then one docs commit: `AGENTS.md` code map, `HANDOFF.md`, this file, the brief's
  decisions).
- Branch rule: one task per `<agent>/<task>` branch; reviewer (Claude) merges `--no-ff` after tests + proofs.
