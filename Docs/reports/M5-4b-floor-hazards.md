# M5-4b: Floor hazards with bodies

Astra, 2026-09-13. Branch `astra/m5-4b-floor-hazards`, created from `main` at `096386b`. Part b of
`Docs/tasks/M5-4-floor-hazards.md`. Not merged to main.

## What the crew can walk into and clean up

A refinery flood now leaves a dark, steaming puddle on the deck. Its disc, trace marker and litre label shrink
as it evaporates. A pop produces its own single puddle through the refinery model's following Spill event; the
Pop event is never fed into the hazard model a second time. Evaporated litres feed the existing room atmosphere.

The server checks the crew's feet against the puddle every frame. Inside, walking is multiplied by 0.6, composed
with carrying and exposure. Carrying in the crude for four seconds drops the cask through the existing put-down
verb, counts a stumble and prompts Okafor. Leaving, losing the carried cask or dropping it resets dwell. Empty
hands can still turn handles and hold controls while standing in the spill.

Holding E on the puddle for six seconds squeegees up to 300 L through `HazardSqueegee`. A larger merged puddle
needs another hold. Clearing or evaporation removes the body through the game mode's publication function;
removal during an active hold is safe. Litres, radii and actor presentation come from `GameState.FloorHazards`.

A running still becoming Popped flashes a white point light for 0.5 seconds and emits steam on both sides from
the replicated status transition. A newly bound actor already seeing Popped does not replay it. The board places
the persistent medical count first, urgent still tasks next, then one white squeegee row per puddle, followed by
the existing fuel/air and other tasks. The bunk reads `empty` or `<n> critical`. These are the existing floor
blackout totals and medical fine; this brief does not introduce permanent incapacitation or carrying crew to beds.

`AirlockFloor` logs the hazard summary and `FloorCriticals`; `AirlockSpill <Still> [Litres]` defaults to 200 L.
Both use cheat-gated Server RPCs. Injected puddles bypass the refinery model and do not inflate its paid spill count.

## Files and judgement calls

- `Core/AirlockGameMode.*` owns the hazard model, resolves spill points, wires only Spill events, ticks between
  refinery and fuel, publishes whole litres/radii and manages every puddle spawn/destruction. It publishes the
  blackout total and sets the existing bunk fixture's replicated second line. `HazardCountStumble` is the small
  additional Hazard API that counts/publishes a successful character drop and speaks the prescribed line.
- `Core/AirlockGameState.*` replicates the hazard mirror and the medical total. `Ship/RefineryLayouts.*` adds only
  the unscaled spill offset/point. `Ship/Spill.*` supplies the mirror-bound cylinder, dynamic material, marker,
  label, steam restart and hold. `Core/AirlockCharacter.*` adds owner-only `InSpill`, speed composition and dwell.
- `Ship/Still.*` adds only the pop effect and cached transition baseline. Chosen presentation values where the
  brief leaves discretion: 300 cd, 500 cm attenuation, relative Z 230 at the tank top, steam scale 2.5; the tank's
  existing 0.8 actor scale is retained. Emission deactivates with the half-second light timer; existing particles
  finish naturally. No looping restart on the pop puff.
- `UI/TaskBoardWidget.*` preserves seven-argument calls with defaulted trailing hazard/critical parameters.
  Because FloorCriticals is plain replicated data and can arrive separately from a hazard notification, the
  local board also notices changes to that total in its existing per-frame update. Medical text reads the
  existing refinery medical rate. No existing fuel, air, still-priority or cargo text was rewritten.
- `Core/AirlockPlayerController.*` supplies the two helpers; `DevCheatsTests.cpp` changes only their two calls and
  the count/description from 32 to 34. New interaction tests and an added task-board test cover the change.
  They live in `Tests/FloorHazardInteractionTests.cpp` and `Tests/TaskBoardTests.cpp`.
- `Scripts/verify-floor.ps1` adds the exact third-cask, flood, stumble and squeegee sequence, the three spill
  probes, 3/3 tally assertions and captures under M5-4b. It restores the in-puddle calibration viewpoint after
  the spill capture. Additional active-squeegee and cleared captures make the cleanup reviewable. The existing
  wider tank threshold regex is retained, as accepted in the main merge; the minimum tank total stays 600 L.
- The mirror/API commit precedes the actor commit, so actor lifecycle wiring landed with the actor and both
  commits built. Character dwell resets in the existing detach path as well as on a dry/empty/blackout tick,
  covering drop-and-pickup before another tick. No new refusal is imposed merely for being in a puddle.

### Literal geometry retained, with tests

The brief's stated 62 cm dry ring assumes a horizontal 312 cm hold reach. The existing character uses a 312.5 cm
three-dimensional sphere from its origin, 96 cm above the spill: the actual dry ring beyond a fresh 250 cm
puddle is `sqrt(312.5^2 - 96^2) - 250 = 47.389 cm`. A pawn 290 cm away horizontally can hold from dry deck; at
300 cm it cannot. The requested 220 cm proof position works. No reach or hazard constant changed.

The required invisible Visibility-blocking box has its top 30 cm above the deck. The unchanged put-down trace
hits that box, so a cask dropped within it rests at spill Z +30 (Z -35 on the Z -65 deck), still contained in
the crude. The test proves this rather than silently changing the marker or cargo trace. Its square corners
also extend outside the circular slow area. M5-5 can decide how to separate interaction and landing surfaces.

In the live requested viewpoint, C3 drops toward the still's controls and appears suspended above the deck in
`floor-squeegee.png` and `floor-cleared.png`. A read-only MCP probe of its retained transform after reloading
found `(-5400, 1720, 31.154)`, 96 cm above the probed deck. This differs from the isolated puddle-box test: the
live drop has additional fixtures along its trace. The exact hit actor was not instrumented. Evidence:
`Saved/m5-4b-live-drop-position.txt`. The server still makes C3 Loose and the proof successfully loads it into
Still_1 afterward. The visual landing defect remains an M5-5 question; the existing put-down trace was not edited.

`LiteralHoldReachAndDropSurface` reports:

```text
Literal geometry: dry ring 47.389 cm; put-down Z -35.000 at deck Z -65.000
```

## Automation and builds

**158/158 passed**, including the unchanged 150-test main suite. All six implementation steps passed 150/150
before their commits. The tests and proof-script steps passed 158/158. The first new-test run was 156/158:
two new assertions assumed the first quarter-second world tick had already started a batch and that repeated
float ppm addition would publish exactly 2 at the integer boundary. The test now waits for Running before its
specified six-second flood interval and measures 25 seconds of evaporation (2.5 ppm floored to 2), away from that
boundary. No production code or model constant changed for either failure. No existing assertion failed.

All eight new tests (`Airlock.Floor.HazardInteraction.` prefix) pass:

| Test | Result |
| --- | --- |
| `FloodEventSpawnsMirrorBody` | PASS: real flood, one body, unscaled offset, floored mirror, actor follows mirror |
| `MergeEvaporationFumesAndSectionGates` | PASS: merge, separate still, evaporation/removal, room ppm, slot and section gates |
| `SlowCompositionStumbleAndDwellReset` | PASS: feet, composed speed, 4.25 s drop, loose cask, count/cue, reset |
| `SqueegeeInsidePartialAndEvaporatedTarget` | PASS: six-second inside hold, safe destruction, partial cleanup, evaporation |
| `LiteralHoldReachAndDropSurface` | PASS: both literal geometry gaps above |
| `CriticalsBunkAndConsoleGate` | PASS: both helpers gated, default 200 L, medical count once and persistent after recovery |
| `PopSpillsOnceBurstsAndSkipsLateBaseline` | PASS: real pop, exactly 300 L, one body, headless burst timer, late baseline |
| `TaskBoardMedicalStillSpillPriorityAndOverflow` | PASS: row text/colour/order, persistence and overflow |

Every build returned `Result: Succeeded`; **zero build failures**.

| Saved build log | Total execution time |
| --- | --- |
| `m5-4b-step1-build.txt` | 10.17 s |
| `m5-4b-step2-build.txt` | 10.16 s |
| `m5-4b-step3-build.txt` | 11.82 s |
| `m5-4b-step4-build.txt` | 12.55 s |
| `m5-4b-step5-build.txt` | 13.30 s |
| `m5-4b-step6-build.txt` | 12.48 s |
| `m5-4b-step7-build.txt` | 13.28 s |
| `m5-4b-step7-build-retry.txt` | 5.93 s (new test corrections) |
| `m5-4b-step8-build.txt` | 10.23 s |
| `m5-4b-proof-timing-build.txt` | 1.01 s (up to date; proof-only timing correction) |
| `m5-4b-proof-queue-build.txt` | 1.02 s (up to date; queued-cask proof assertion) |

Final build lines:

```text
Target is up to date
Using Unreal Build Accelerator local executor to run 0 action(s)
Result: Succeeded
Total execution time: 1.02 seconds
```

Test outputs are `Saved/m5-4b-step1-tests.txt` through `step8-tests.txt`, plus `step7-tests-retry.txt`.
JSON suite exports are `Saved/m5-4b-step4-automation.json` through `step8-automation.json`; the initial failing
new-test export is retained as `Saved/m5-4b-step7-first-automation.json`. The current suite is also at
`Saved/Automation/Report/index.json`. SDK availability messages for unused platforms appeared in headless
stdout; the exported suite and test process exits were checked, not inferred from the last stdout line.
The proof timing correction also passed 158/158: `Saved/m5-4b-proof-timing-tests.txt` and
`Saved/m5-4b-proof-timing-automation.json`.

### Initial proof failure and the timing correction

The literal first run failed before exercising the floor. Today's Milk-run incident schedule targets Clamp_3.
The final `AirlockVoyageClock 5` consumed that skipped incident after C3 was secured, releasing it immediately
before docking. The assertion correctly rejected the two-cask arrival:

```text
Cargo C3 Secured temp=0
Cargo C3 Loose temp=0
Incident CargoShift Clamp_3 (0.00) at 950.117s elapsed=1195.017s
Ship result: 2/4 crates intact, 0 vented, 2 loose, hull 92.80%, pay 536, air 0 ppm, 0 blackouts, 0 burned (2 casks to the floor)
Slot clock started: 600s endServerTime=635.316 casks=2
No fresh ': Ship result:' line containing '3 casks to the floor' within 30 seconds
```

Evidence retained: `Saved/m5-4b-floor-proof-initial.txt` and
`Saved/AgentCaptures/M5-4b/floor-log-initial.txt`. The allowed proof script now calls `AirlockVoyageClock 75`
immediately after the incident director starts, while all clamps are empty. Skipped incidents run normally;
the proof then performs the specified carry/place sequence and final five-second docking jump. This is a
declared deviation in proof setup needed to carry out the requested three-cask proof, not a change to incident,
cargo, refinery or hazard constants, and no assertion was weakened. Build and the full suite passed afterward.

The next run passed the flood, stumble and cleanup but reached an inherited two-cask assertion expecting
`Still Still_1 Idle`. With C3 queued the model keeps Running through the empty-charge transition, then starts
C3 on the following tick:

```text
Refinery BatchDone Still_1 C1 diesel=752 gas=50 tar=197
Still Still_1 Running
Still Still_1 Running cask=C3
```

The allowed proof now explicitly requires `Running cask=C3` in place of the obsolete Idle check. The C1, C2
and C3 BatchDone assertions all remain. Evidence: `Saved/m5-4b-floor-proof-queued-idle.txt` and
`Saved/AgentCaptures/M5-4b/floor-log-queued-idle.txt`. Another up-to-date build and 158/158 suite passed:
`Saved/m5-4b-proof-queue-build.txt`, `Saved/m5-4b-proof-queue-tests.txt`,
`Saved/m5-4b-proof-queue-automation.json`. Neither failed proof indicated a hazard-code or automation regression.

## Position probes and PIE proof

`verify-floor.ps1 -ProbePositions` exited **0**, reporting **58 positions**, including all three new spill
points. All three settled without an XY nudge, on the walkable deck:

| Spill point | Authored XYZ (cm) | Settled pawn Z | Feet Z | Moved |
| --- | --- | --- | --- | --- |
| Still_1 | (-5800, 1670, -65) | 31.154 | -64.846 | False |
| Still_2 | (-5400, 1670, -65) | 31.154 | -64.846 | False |
| Still_3 | (-5000, 1670, -65) | 31.154 | -64.846 | False |

The centres are 400 cm apart, beyond the model's merge distance. The original fixture/control probes remain.
Evidence: `Saved/m5-4b-probe.txt`, `Saved/AgentCaptures/M5-4b/floor-position-probe.csv` and
`floor-probe-log.txt`. The Still_2 face capture was inspected before trusting the positions; the final
`floor-spill.png` confirms the actual puddle on that deck.

The final floor proof exited **0** on the tested binary, single-player (`crew=1`, seed 20709). C3 finished
about 246 seconds into the 600-second slot. The prescribed in-puddle calibration hold completed, and Reset
correctly refused the running still. Selected fresh `LogAirlock` lines, with UTC timestamps:

```text
[18.49.01:439] Incident director started: crew=1 entries=4 seed=20709
[18.49.27:060] Slot clock started: 600s endServerTime=636.318 casks=3
[18.50.34:438] Refinery Spill Still_2 C2 litres=200
[18.50.34:438] Spill S1 by Still_2: 200 L (r 200 cm)
[18.51.05:388] Stumble AirlockCharacter_0 drops C3 in S1
[18.51.05:388] PutDown C3 by AirlockCharacter_0
[18.51.12:281] Spill S1 cleared by AirlockCharacter_0
[18.51.12:282] Spill S1 gone
[18.51.12:282] Hold S1 complete by AirlockCharacter_0
[18.51.12:980] No spills
[18.51.12:980] FloorCriticals 0
[18.51.41:189] Refinery BatchDone Still_2 C2 diesel=506 gas=40 tar=253
[18.51.52:939] Refinery BatchDone Still_1 C1 diesel=752 gas=50 tar=197
[18.51.53:190] Still Still_1 Running cask=C3
[18.53.32:946] Refinery BatchDone Still_1 C3 diesel=593 gas=50 tar=356
[18.53.33:455] Tank diesel=1853 gas=140 tar=806 spilled=200 refined=3 lost=0 waiting=0
[18.53.43:144] Okafor: Horn. Slot's over - clear the floor.
[18.53.43:144] Floor result: floor: 3/3 casks refined, 1853 L diesel, 806 L tar, 1 spill, 0 pops, 0 criticals, pay 847, fuel +15 t
[18.53.43:144] Voyage result: 3/4 crates intact, 0 vented, 1 loose, hull 87.10%, pay 1471, air 0 ppm, 0 blackouts, 0 burned | floor: 3/3 casks refined, 1853 L diesel, pay 847, fuel +15 t
[18.53.43:144] Transition card: UNDOCKING - HOME TO THE VELL EXTRACTOR / the Rig's board has your pay
[18.53.43:144] Base fuel 545 t (+15 this run), runs 56
[18.53.56:005] Save reset: base fuel 200 t, runs 0
```

Exact success tail:

```text
FLOOR OK: Milk run -> C1/C3 secured, C2 racked -> floor: three casks, six fixtures, 36 controls -> Still 2 by-hand batch floods 200 L into S1 -> C3 stumble and put-down -> six-second squeegee clears S1 -> C1/C3 in Still 1 and C2 in Still 2 done, 1853 L diesel, 1 spill, 0 pops -> horn -> combined result (3/3 refined, +15 t), undocking card -> hub board, save reset
```

Full proof stdout: [floor proof](../../Saved/m5-4b-floor-proof.txt). Fresh gameplay excerpt:
[floor-log.txt](../../Saved/AgentCaptures/M5-4b/floor-log.txt). Each capture follows an `AirlockWhere` line
confirming the floor map and actual viewpoint. The following final-run captures were visually inspected:

- [floor-spill.png](../../Saved/AgentCaptures/M5-4b/floor-spill.png): dark reflective disc on the deck, 193 L
  label after evaporation, white `Spill by Still_2 - squeegee` board row.
- [floor-squeegee.png](../../Saved/AgentCaptures/M5-4b/floor-squeegee.png): 165 L remaining, active hold bar,
  Okafor's stumble caption and the dropped C3. The cask's elevated landing is documented above.
- [floor-cleared.png](../../Saved/AgentCaptures/M5-4b/floor-cleared.png): disc, spill label, task row and hold
  bar gone; the ordinary deck is visible again.
- [hub-board-floor.png](../../Saved/AgentCaptures/M5-4b/hub-board-floor.png): `floor: 3/3 refined`, pay 1,471 cr,
  +15 t credited. This capture precedes the required save reset.

Other retained captures in that directory: `floor-still-1-face.png`, `floor-still-2-face.png`,
`floor-still-3-face.png`, `floor-arrival.png`, `floor-stills.png`, `floor-still-gauges.png`,
`floor-still-handles.png`, `floor-still-running.png` and `floor-tank.png`. Pop/light timing and the medical
total were verified by automation; this required PIE run intentionally has zero pops and zero criticals.

## Mandatory regression proofs

The unchanged `verify-cargo.ps1` exited **0** on this binary, with single-player PIE:

```text
CARGO OK: secure two -> shift and cool C2 -> vent C4 -> 2/4 intact, pay 400 -> hub last run
```

Evidence: [cargo stdout](../../Saved/m5-4b-proof-cargo.txt), `Saved/AgentCaptures/M3-4/cargo-log.txt`
and that directory's fresh captures.

The first two-client attempt could not connect to port 8000 before starting PIE (exit 1). The editor log
shows `QUIT_EDITOR` / `UUnrealEdEngine::CloseEditor()` at 18:56:21 UTC and orderly shutdown, not a fatal
error. The shutdown's source was not established; this session did not request that close. The script's
settings restoration ran. Evidence: `Saved/m5-4b-proof-two-clients-startup.txt` and
`Saved/m5-4b-editor-shutdown.txt`. Relaunched only this checkout through `run-editor.bat`, waited for port
8000 plus 30 seconds, confirmed StationHub/no PIE, and retried the unchanged script. No source or binary
changed between proofs.

The retry of unchanged `verify-two-clients.ps1` exited **0**:

```text
TWO CLIENTS OK: server + client 1 on the hub -> the client crosses Dock B, both aboard -> client carries and secures C1 -> client at the seat -> client scrubs -> cheat refused at DevCheats 0 -> dock, both home, the second arrival steps aside
Play settings: {"PlayNetMode":"PIE_Standalone","PlayNumberOfClients":1,"RunUnderOneProcess":true,"bLaunchSeparateServer":false,"NewWindowWidth":1280,"NewWindowHeight":720}
Play settings ini section restored
```

Evidence: [two-client stdout](../../Saved/m5-4b-proof-two-clients.txt),
`Saved/AgentCaptures/M4-1/two-clients-log.txt` and the fresh client/server/editor captures in M4-1.
The exact existing scripts were used. Their assertions cover cargo state, the client carry/place path,
seat/scrub control, cheat refusal and travel; they do not measure floor slip speed or assert a floor
stumble across two clients. Speed composition and actual ServerPutDown are covered by the new automation
and the standalone floor sequence above. The full twelve-proof suite, including `verify-loop.ps1`, is left
to the reviewer as requested; it was not rerun or claimed here.

## Open questions for M5-5

- Separate a cask's landing surface from the puddle and control interaction markers. The isolated box test
  and the live elevated C3 landing are measured above; the current carry/put-down implementation is retained.
- Decide whether the literal 47.389 cm dry hold ring is sufficient. The specified 220 cm cleanup viewpoint
  works; changing reach or puddle constants needs a separate brief.
- Add the deferred flash flicker and author audio cues: the boiling crude hiss and the still-pop bang.
  The current half-second pop effect has a headless transition/timer test, not a recorded PIE pop capture.
- Review puddle contrast, steam readability and the already crowded still/control labels in the actual
  room lighting. The disc and disappearance are clear in the captures; this is still primitive presentation.
- Exercise two crew on the floor together for puddle shrink/removal, owner-only speed changes, late join
  pop baselines and medical-row/bunk updates. The required existing two-client script is a regression proof,
  not a complete multiplayer floor-hazard scenario.
- Balance floor fumes and medical consequences in a longer floor playtest. The pure rates are unchanged,
  and this single cleaned-up 200 L spill stayed at 0 published room ppm under the existing scrubbers. A
  critical remains the existing cumulative blackout/medical-fine count; nobody is transported to the bunk.

## Scope and reproduction

Nothing outside the brief's allowed file list was changed. No maps or assets, models/constants,
`FloorFixture.*`, or forbidden test files were touched. No existing automation assertion changed except
the explicitly permitted dev-cheat RPC count. The two proof-script setup/assertion corrections are declared
above. `Docs/STATUS.md` and main remain for the reviewer; this branch is not merged or pushed.

From this checkout in PowerShell, reproduce the build and suite:

```powershell
$env:AIRLOCK_MCP_PORT = '8000'
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\close-editor.ps1
& Scripts\build-editor.bat
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
& Scripts\run-tests.bat Airlock
if ($LASTEXITCODE -ne 0) { throw 'Automation failed' }
& Scripts\run-editor.bat
$readyBy = (Get-Date).AddMinutes(3)
while (-not (Get-NetTCPConnection -LocalPort 8000 -State Listen -ErrorAction SilentlyContinue)) {
    if ((Get-Date) -gt $readyBy) { throw 'Editor MCP did not start' }
    Start-Sleep -Seconds 2
}
Start-Sleep -Seconds 30
```

Force and read back single-player PIE before the single-player proofs (do this again if the editor was
previously closed in two-client mode). If package restore blocks startup, use HANDOFF section 5.

```powershell
. Scripts\lib\mcp.ps1
Mcp-Init
$play = @{refPath='/Script/UnrealEd.Default__LevelEditorPlaySettings'}
Mcp-Show 'Play metadata' (Mcp-Call $TS_Object 'list_properties' @{instance=$play})
Mcp-Show 'Standalone' (Mcp-Call $TS_Object 'set_properties' @{
    instance=$play
    values='{"PlayNetMode":"PIE_Standalone","PlayNumberOfClients":1,"RunUnderOneProcess":true,"bLaunchSeparateServer":false}'
})
Mcp-Show 'Play mode' (Mcp-Call $TS_Object 'get_properties' @{
    instance=$play; properties=@('PlayNetMode','PlayNumberOfClients')
})
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-floor.ps1 -ProbePositions
if ($LASTEXITCODE -ne 0) { throw 'Floor probes failed' }
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-floor.ps1
if ($LASTEXITCODE -ne 0) { throw 'Floor proof failed' }
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-cargo.ps1
if ($LASTEXITCODE -ne 0) { throw 'Cargo proof failed' }
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-two-clients.ps1
if ($LASTEXITCODE -ne 0) { throw 'Two-client proof failed' }
```

These proof scripts stop PIE and check map cleanliness. Confirm the final state without changing a map:

```powershell
Mcp-Show 'PIE' (Mcp-Call $TS_App 'IsPIERunning' @{})
Mcp-Show 'Level' (Mcp-Call $TS_Scene 'get_current_level' @{})
foreach ($map in @('/Game/Maps/StationHub','/Game/Maps/Ships/Hauler_Mk1','/Game/Maps/RefineryFloor')) {
    Mcp-Show $map (Mcp-Call $TS_Asset 'is_dirty' @{asset_path=$map})
}
```

Final checks after both mandatory proofs: editor PID 22824 is listening on **127.0.0.1:8000**, open on
`/Game/Maps/StationHub`; `IsPIERunning=false`; StationHub, Hauler_Mk1 and RefineryFloor each report
`is_dirty=false`; `PlayNetMode=PIE_Standalone`, `PlayNumberOfClients=1`. Evidence:
[final editor state](../../Saved/m5-4b-final-editor.txt). The editor is left open. All task work and this report
are committed on `astra/m5-4b-floor-hazards`; main was not changed.

## Reviewer's verification (Claude, 2026-09-13 afternoon)

Merged into `main` as `2864ed2` (`git merge --no-ff` from `096386b`; the merged tree is the branch's `e05d2cb`
exactly, nothing outside the brief's allowed files). Rebuilt in `Airlock\` (`Result: Succeeded`) and the headless
suite on the merged tree: **158 passed, 0 failed, 0 not run** (`Saved\Automation\Report\index.json`, 12:06:03).

The twelve proofs in sequence on that binary, one editor (port 8000), single-player play settings forced through
the MCP first, 12:07 to 12:29:

| Proof | Exit | Tail |
| --- | --- | --- |
| loop | 0 | `LOOP OK: hub spawn -> booked -> Dock B -> Hauler_Mk1 PlayerStart -> exit -> hub PlayerStart, voyage complete` |
| ship-graph | 0 | `GRAPH OK: seven nominal modules -> coolant leak -> degraded coolant drifts core -> hold repairs line -> coolant restored -> hub PlayerStart, empty graph` |
| incidents | 0 | `INCIDENTS OK: solo schedule -> teaching leak and captain -> hold repair clears row -> core task -> climax -> no replay -> hub` |
| voyage-clock | 0 | `CLOCK OK: 75 seconds remaining -> one-minute captain line -> automatic return card -> StationHub PlayerStart, voyage complete` |
| level-pass | 0 | `LEVEL OK: hold -> engineering (2 lines repaired) -> corridor (2 lines repaired) -> galley -> bridge seat, board, stand -> hub PlayerStart` |
| cargo | 0 | `CARGO OK: secure two -> shift and cool C2 -> vent C4 -> 2/4 intact, pay 400 -> hub last run` |
| atmosphere | 0 | `ATMOSPHERE OK: warm cask puffs -> hazard scrub -> rack -> blackout and recovery -> clean air, pay 400 -> hub last run` |
| fuel | 0 | `FUEL OK: feed leak -> shut -> bypass -> burn C3/C1 -> clear injectors -> clean air, pay 270 -> hub last run` |
| refinery | 0 | `REFINERY OK: reset -> priority contract, 12 casks -> 2 intact -> refinery dock, +20 t -> hub board -> empty run adds 0 -> reset` |
| legibility | 0 | `LEGIBILITY OK: Priority hold -> north/south labels -> carried label hidden -> drop -> cargo summaries -> engineering panel -> feed steam -> refinery result -> hub, save reset` |
| two-clients | 0 | `TWO CLIENTS OK: server + client 1 on the hub -> the client crosses Dock B, both aboard -> client carries and secures C1 -> client at the seat -> client scrubs -> cheat refused at DevCheats 0 -> dock, both home, the second arrival steps aside`; play settings restored |
| floor | 0 | `FLOOR OK: Milk run -> C1/C3 secured, C2 racked -> floor: three casks, six fixtures, 36 controls -> Still 2 by-hand batch floods 200 L into S1 -> C3 stumble and put-down -> six-second squeegee clears S1 -> C1/C3 in Still 1 and C2 in Still 2 done, 1853 L diesel, 1 spill, 0 pops -> horn -> combined result (3/3 refined, +15 t), undocking card -> hub board, save reset` |

The floor proof's fresh `LogAirlock` lines on the merged tree (`Saved\AgentCaptures\M5-4b\floor-log.txt`, 12:29):

```text
Ship result: 3/4 crates intact, 0 vented, 1 loose, hull 87.28%, pay 625, air 0 ppm, 0 blackouts, 0 burned (3 casks to the floor)
Slot clock started: 600s endServerTime=635.644 casks=3
Spill S1 by Still_2: 200 L (r 200 cm)
Okafor: Flood on Still 2 - crude over the lip. Feed down, and mind your footing, it's slick.
Stumble AirlockCharacter_0 drops C3 in S1
PutDown C3 by AirlockCharacter_0
Okafor: Cask down - AirlockCharacter_0 went over in the crude. Get it up before it warms.
Spill S1 cleared by AirlockCharacter_0
Spill S1 gone
No spills
FloorCriticals 0
Floor result: floor: 3/3 casks refined, 1853 L diesel, 806 L tar, 1 spill, 0 pops, 0 criticals, pay 847, fuel +15 t
Voyage result: 3/4 crates intact, 0 vented, 1 loose, hull 87.28%, pay 1472, air 0 ppm, 0 blackouts, 0 burned | floor: 3/3 casks refined, 1853 L diesel, pay 847, fuel +15 t
```

Captures inspected from this batch: `floor-spill.png` (the dark disc under Still 2's handle row, `SPILL` / `193 L -
squeegee (hold E)`, the board's `Spill by Still_2 - squeegee`), `floor-squeegee.png` (the hold bar at a third,
Okafor's stumble caption, `164 L`, and C3 sitting up on the control row - the landing flaw below),
`floor-cleared.png`, `hub-board-floor.png`.

### Review notes (accepted as is; nothing changed on the branch)

- **The code follows the brief.** The hazard model is touched only through the game mode, the mirror is the only
  thing actors read, spawn and destruction happen in one place, only `Spill` events reach the model, the dwell
  clock resets on every detach path, and the still's burst keys off the replicated transition with a late-join
  baseline. The tests pin what the brief asked for, including the two measured facts.
- **The put-down surface** (Astra's first open question) is the one visible defect: the puddle's marker box and
  the still controls' marker boxes block Visibility, and `PutDownLocation`'s floor trace is a Visibility trace,
  so a cask dropped in a puddle rests 30 cm up in the isolated test and 96 cm up on a control marker in the live
  viewpoint. The fix belongs in M5-5 (Claude): the put-down trace ignores interaction markers, or the markers
  move to their own object type. The brief's marker prescription was mine, so this is not held against the branch.
- **`Spill S1 cleared by <Pawn>` is logged for a partial pass too** (a 400 L puddle keeps 100 L). Wording for
  M5-5 (`Spill S1 squeegeed by <Pawn>: 300 L off, 100 L left`), not a behaviour change.
- **The 47 cm dry ring** is the truth of the existing hold reach (a 312.5 cm sphere from an origin 96 cm up); the
  brief's 62 cm was a horizontal figure. Accepted as measured; a different reach or puddle cap needs a brief.
- **The proof's `AirlockVoyageClock 75` right after the director starts** is accepted as proof set-up: today's
  Milk-run schedule targets Clamp_3 with a cargo shift that the docking jump consumes, which released C3 at the
  dock in Astra's first run. The game did what it should; the three-cask delivery needed the clamps empty when the
  skipped incidents fire. The queued cask's `Running cask=C3` in place of the old `Idle` wait is the model's real
  behaviour (a queued hopper starts the next cask on the tick after `BatchDone`).
- **The spill label among the control labels** is crowded but readable from the walkway; it joins M5-5's
  legibility pass with the gauge stack.
- **Two crew on the floor with a puddle** (client-side shrink and removal, the owner-only speed, the pop baseline
  on a late joiner) is proven by automation and the single-player proof only; the two-client proof stays a
  regression proof. Listed under STATUS's open follow-ups.

Reproduce the reviewer's batch from `Airlock\` (the editor open, PIE stopped): `Scripts\close-editor.ps1`,
`Scripts\build-editor.bat`, `Scripts\run-tests.bat Airlock` (read `Saved\Automation\Report\index.json`),
`Scripts\run-editor.bat`, force single-player play settings through `Scripts\lib\mcp.ps1` (HANDOFF section 5),
then `Scripts\verify-<name>.ps1` for loop, ship-graph, incidents, voyage-clock, level-pass, cargo, atmosphere,
fuel, refinery, legibility, two-clients, floor - about 23 minutes.
