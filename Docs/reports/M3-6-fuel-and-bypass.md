# M3-6: Fuel, the feed leak and the crude bypass

GPT-6 Astra, 2026-09-12. Branch `astra/m3-6-fuel-and-bypass`, created from `main` at `90cbea7`.
Implementation is committed locally through `302e054`; no merge to `main` was performed.

**Acceptance remains unresolved:** the final build, all 87 tests and all seven existing PIE proofs pass. The complete fuel PIE sequence passes
with `-ShortFollowup`, which explicitly sets 80 units before the leak. The literal default proof fails its
captain follow-up assertion because items 1 and 5 leave fuel reserve at ten seconds, while item 10 requires the
short-reserve line. Gameplay follows the specified rates and `IsShort` predicate. The default proof retains the
literal assertion. Details and the two reproducible runs are below.

## Behaviour changed

The starting verified loop booked a voyage on the hub, crossed Dock B into the hauler, ran the voyage clock and
incidents, and returned through the airlock or timed docking to a persisted tally. Cargo already supported carry,
clamps, cold-rack cooling, volatile heating and terminal venting. Atmosphere already combined cask puffs and
ruptures with scrubber modes, graph-dependent efficiency, exposure/blackouts and clean-air pay. M3-6 extends
those server-owned models with the fuel reversal:

- A pure fuel model starts at 90 units and burns four units per minute. The feed leak adds 40 units per minute
  until a three-second hold shuts the valve. Fuel publication floors units and rounds the burn countdown up;
  the model retains fractional units and integrates across a burn completion. Empty fuel latches exhaustion.
- Engineering has a feed valve, crude bypass valve and intake, spawned from the fuel layout after cargo. All
  three replicate their initial fixture specs and read replicated fuel on the existing ship-state delegate.
  The leaking feed valve has red light and repeating steam; the open bypass has amber light; the intake label
  shows its cask and seconds remaining. Mesh availability and position probes are attached below.
- Opening the bypass is a server-validated press. E while carrying routes an intake hit to feeding. The server
  checks blackout, reach, hands, bypass and intake availability. It stages the fuel operation, consumes cargo,
  then damages Engines and publishes; any refusal preserves both models and the carried actor. Detachment
  occurs only after successful consumption. Both volatile and nonvolatile casks burn.
- A fed cask becomes terminal `Burned` immediately: hidden, collision and label disabled, inert temperature,
  no pickup/place/drop/consume, no puff cycle, and no cargo task. The actor remains alive. It still counts in the
  manifest total, counts separately as burned, and always voids the clean-manifest bonus.
- A burn lasts 20 seconds, adds 15 fuel at completion and distributes 15 ppm of soot over its duration after the
  atmosphere tick. Each feed costs Engines ten condition, or twenty while fouled. Every second completed burn
  fouls the injectors; a four-second hold on Engines clears fouling without repairing condition.
- The independent seeded set piece fails the feed at 40-55% of the voyage. The existing voyage timer wakes for
  captain entries or this event, consumes each once, and preserves rewind behaviour. Normal catch-up speaks
  captain entries before the leak; a jump to docking consumes the leak before the final docking caption.
  Its single follow-up runs ten seconds later or immediately upon closure and chooses its line from the live
  nominal fuel reserve. An inactive/travelling voyage suppresses the follow-up.
- The first empty-fuel tick extends the clock by 120 seconds and announces coasting. Refilling does not undo
  lateness or permit another extension. Exhaustion itself never damages Engines. Existing travel functions,
  `SetVoyageSecondsRemaining`, captain schedule and incident director remain unchanged.
- Fuel leak/low-fuel tasks precede the air row; injector clearing follows air and precedes heating crates.
  The bridge shows fuel, bypass, burning cask/countdown and fouling. Docking deducts 40 credits per burned cask
  and 100 for lateness when fuel data is supplied. The specified examples pay 590, 630 without fuel data,
  and 410 when late. The hub appends burned and late text after air. `bHasFuelData` records whether Tally received
  fuel, so results without it retain their previous summary strings even when a burned count is present.
- New development helpers: `AirlockFuel`, `AirlockFuelSet`, `AirlockFuelLeak`, `AirlockBypass`, `AirlockFeed`.
  The feed helper requires the named carried cask and intake reach; bypass skips reach but checks blackout.

## Files

All source paths are under `Source/Airlock`:

| Area | Files |
|---|---|
| Pure fuel | `Ship/FuelSystem.h/.cpp` |
| Fixtures | `Ship/FuelLayouts.h/.cpp`, `FuelValve.h/.cpp`, `BypassValve.h/.cpp`, `FuelIntake.h/.cpp` |
| Scripted leak | `Voyage/VoyageSetPieces.h/.cpp` |
| Server state and verbs | `Core/AirlockGameMode.h/.cpp`, `AirlockGameState.h/.cpp`, `AirlockCharacter.h/.cpp`, `AirlockPlayerController.h/.cpp` |
| Cargo, soot qualification, injectors | `Ship/CargoModel.h/.cpp`, `CargoCrate.cpp`, `ShipAtmosphere.cpp`, `ShipModule.h/.cpp` |
| Local readouts | `UI/CarryPromptWidget.cpp`, `TaskBoardWidget.h/.cpp`, `ControlBoardWidget.h/.cpp`, `VoyageBoardWidget.cpp` |
| Tests | `Tests/FuelSystemTests.cpp`, `FuelInteractionTests.cpp`, `VoyageSetPieceTests.cpp`; extensions in `CargoModelTests.cpp`, `ShipAtmosphereTests.cpp`, `TaskBoardTests.cpp` |
| Proof/report | `Scripts/verify-fuel.ps1`, `Docs/reports/M3-6-fuel-and-bypass.md` |

## Build and automation

- Pure fuel model: first build succeeded, **4/4** tests passed before the model commit `4973975`.
- One later build failed on an ambiguous float/double comparison in a new set-piece test. Explicitly casting
  the expected time fixed the next build. No build failed twice on the same error.
- The first broad run passed **80/80**, but source-discovery inspection showed Unreal's cached makefile had
  omitted the new integration-test file. `Scripts\gen-projectfiles.bat` refreshed discovery; its generated
  `.ignore` additions were reverted, and the unity source was checked for `FuelInteractionTests.cpp`.
- Full discovery: **84 passed / 3 failed**, all failures in new test harnesses. BeginPlay timers needed an initial
  tick before measured advances, and follow-up log captures needed to start with each case instead of at queue
  construction. The next run was **86/87**: the exhaustion harness landed exactly on a timer expiry boundary.
  Advancing 0.26 seconds exercises one 4 Hz callback while staying within the specified one-second tolerance.
  The gameplay assertions and all original assertions were retained.
- Final build: **Succeeded**, UE 5.8.2 / MSVC 14.50.35738. Final automation: **87/87 passed**, including all original
  72 and fifteen additions. The original inert-cargo test also gained Burned expectations.

Evidence: [model build](../../Saved/AgentCaptures/M3-6/build-model.txt),
[model results](../../Saved/AgentCaptures/M3-6/model-tests.json),
[final build](../../Saved/AgentCaptures/M3-6/build-final.txt),
[all results](../../Saved/AgentCaptures/M3-6/all-tests.json),
[fuel automation logs](../../Saved/AgentCaptures/M3-6/fuel-automation-log.txt).

| Test | Result |
|---|---|
| `Airlock.Atmosphere.BurnedCaskDropsPuffClock` | Success |
| `Airlock.Fuel.BlackoutGuardsPublicationAndSectionGating` | Success |
| `Airlock.Fuel.BypassBurnSootAndFouling` | Success |
| `Airlock.Fuel.DeterminismAndSplitTicks` | Success |
| `Airlock.Fuel.ExhaustionClampInvalidAndPublication` | Success |
| `Airlock.Fuel.ExhaustionExtendsClockExactlyOnce` | Success |
| `Airlock.Fuel.FeedAtomicCarryBurnSootDamageAndInjectorHold` | Success |
| `Airlock.Fuel.FixturesValveHoldAndBypassPress` | Success |
| `Airlock.Fuel.FollowupReserveShortShutAndInactive` | Success |
| `Airlock.Fuel.ResetNominalLeakAndReserve` | Success |
| `Airlock.Fuel.SetPieceDockJumpBeforeDockingCaption` | Success |
| `Airlock.Fuel.SetPieceForwardJumpAndRewindOnce` | Success |
| `Airlock.Fuel.SetPieceWindowSeedOrderAndCauses` | Success |
| `Airlock.Fuel.SootLatePayManifestAndResultText` | Success |
| `Airlock.Fuel.TaskRowsPriorityWordsThresholdsAndCap` | Success |

Headless exhaustion evidence:

```text
Voyage clock moved: requested=720.000s remaining=720.000s endServerTime=720.000 nextAnnouncement=2
Fuel exhausted: docking 120 s late
```

The test starts at 600 seconds, verifies the 720-second result and unchanged Engines, refills and empties fuel
again, then verifies one extension/log and the late result. Separate tests cover burns, all-or-nothing refusal,
blackout guards, publication, an unbooked section, all follow-up branches, rewind and docking order.

## PIE verification

All seven existing proof scripts exited 0 on implementation commit `302e054`, without edits. The complete
fuel sequence also exited 0 with the explicit 80-unit setup. The default proof remains an acceptance failure;
its failure and the actual captain line are retained below.

| Proof | Exit | Seconds | Full output |
|---|---:|---:|---|
| `verify-loop.ps1` | 0 | 53.6 | [output](../../Saved/AgentCaptures/M3-6/verify-loop-output.txt) |
| `verify-ship-graph.ps1` | 0 | 72.6 | [output](../../Saved/AgentCaptures/M3-6/verify-ship-graph-output.txt) |
| `verify-incidents.ps1` | 0 | 60.2 | [output](../../Saved/AgentCaptures/M3-6/verify-incidents-output.txt) |
| `verify-level-pass.ps1` | 0 | 154.8 | [output](../../Saved/AgentCaptures/M3-6/verify-level-pass-output.txt) |
| `verify-cargo.ps1` | 0 | 91.3 | [output](../../Saved/AgentCaptures/M3-6/verify-cargo-output.txt) |
| `verify-voyage-clock.ps1` | 0 | 106.6 | [output](../../Saved/AgentCaptures/M3-6/verify-voyage-clock-output.txt) |
| `verify-atmosphere.ps1` | 0 | 124 | [output](../../Saved/AgentCaptures/M3-6/verify-atmosphere-output.txt) |
| `verify-fuel.ps1 -ShortFollowup` | 0 | 146.6 | [output](../../Saved/AgentCaptures/M3-6/verify-fuel-short-output.txt) |
| `verify-fuel.ps1` (literal default) | **1** | 26.1 | [output](../../Saved/AgentCaptures/M3-6/verify-fuel-default-output.txt) |

Proof tails:

```text
[20:50:35] LOOP OK: hub spawn -> booked -> Dock B -> Hauler_Mk1 PlayerStart -> exit -> hub PlayerStart, voyage complete
GRAPH OK: seven nominal modules -> coolant leak -> degraded coolant drifts core -> hold repairs line -> coolant restored -> hub PlayerStart, empty graph
INCIDENTS OK: solo schedule -> teaching leak and captain -> hold repair clears row -> core task -> climax -> no replay -> hub
LEVEL OK: hold -> engineering (2 lines repaired) -> corridor (2 lines repaired) -> galley -> bridge seat, board, stand -> hub PlayerStart
CARGO OK: secure two -> shift and cool C2 -> vent C4 -> 2/4 intact, pay 400 -> hub last run
CLOCK OK: 75 seconds remaining -> one-minute captain line -> automatic return card -> StationHub PlayerStart, voyage complete
ATMOSPHERE OK: warm cask puffs -> hazard scrub -> rack -> blackout and recovery -> clean air, pay 400 -> hub last run
FUEL OK: feed leak -> shut -> bypass -> burn C3/C1 -> clear injectors -> clean air, pay 270 -> hub last run
```

The loop's logged return was `map=UEDPIE_0_StationHub`, pawn X=-5640.001 Y=1559.997 Z=18.534. Its diagnostic
error-list dump included EOS SDK DNS-resolution warnings; the gameplay proof still exited 0. Full output above
preserves that diagnostic tail.

Selected fuel sequence logs (timestamps retained; [full LogAirlock capture](../../Saved/AgentCaptures/M3-6/fuel-log.txt)):

```text
[2026.09.13-03.47.28:318][633]LogAirlock: Fuel 89% leak=0 bypass=closed burning=none burned=0 fouled=0 exhausted=0
[2026.09.13-03.47.28:851][655]LogAirlock: Fuel 80% leak=0 bypass=closed burning=none burned=0 fouled=0 exhausted=0
[2026.09.13-03.47.29:353][672]LogAirlock: SetPiece FuelLeak at 633.98s elapsed=2.07s
[2026.09.13-03.47.39:336][ 51]LogAirlock: Captain Okonkwo: Captain Okonkwo here. At this burn we dock short. Sludge is methane - open the crude bypass and feed me casks.
[2026.09.13-03.47.53:881][652]LogAirlock: Hold FeedValve complete by AirlockCharacter_0
[2026.09.13-03.48.06:254][221]LogAirlock: Bypass opened by AirlockCharacter_0
[2026.09.13-03.48.21:007][906]LogAirlock: Fuel feed C3 started
[2026.09.13-03.48.40:857][802]LogAirlock: Fuel feed C3 done: +15
[2026.09.13-03.48.41:478][830]LogAirlock: Ship Engines Nominal cond=90.00
[2026.09.13-03.48.45:560][  2]LogAirlock: Fuel feed C1 started
[2026.09.13-03.49.05:361][896]LogAirlock: Injectors fouled
[2026.09.13-03.49.05:361][896]LogAirlock: Fuel feed C1 done: +15
[2026.09.13-03.49.05:824][918]LogAirlock: Ship Engines Nominal cond=80.00
[2026.09.13-03.49.21:632][647]LogAirlock: Injectors cleared
[2026.09.13-03.49.21:632][647]LogAirlock: Hold Engines complete by AirlockCharacter_0
[2026.09.13-03.49.26:492][874]LogAirlock: Tally now: 0/4 crates intact, 0 vented, 2 loose, hull 97.14%, pay 264, air 0 ppm, 0 blackouts, 2 burned
[2026.09.13-03.49.28:532][969]LogAirlock: Voyage result: 0/4 crates intact, 0 vented, 2 loose, hull 100.00%, pay 270, air 0 ppm, 0 blackouts, 2 burned
[2026.09.13-03.49.30:629][ 40]LogAirlock: No fuel system on this section
```

The literal default failure ([log](../../Saved/AgentCaptures/M3-6/fuel-default-log.txt)):

```text
[2026.09.13-04.01.19:292][811]LogAirlock: Captain Okonkwo: Captain Okonkwo here. We're underway. Welcome aboard, crew.
[2026.09.13-04.01.20:433][849]LogAirlock: Fuel 89% leak=0 bypass=closed burning=none burned=0 fouled=0 exhausted=0
[2026.09.13-04.01.20:974][871]LogAirlock: Fuel 89% leak=1 bypass=closed
[2026.09.13-04.01.20:974][871]LogAirlock: Captain Okonkwo: Captain Okonkwo here. Feed valve's failed open - we're bleeding methane. Engineering, shut it.
[2026.09.13-04.01.20:974][871]LogAirlock: SetPiece FuelLeak at 633.98s elapsed=1.53s
[2026.09.13-04.01.30:959][239]LogAirlock: Captain Okonkwo: Captain Okonkwo here. Shut that feed. We're still good for the refinery - for now.
No fresh ': Captain .*open the crude bypass and feed me casks' line within 12 seconds
```

The 80-unit mode changes only proof setup immediately before opening the leak. It does not change gameplay
rates, the one-shot follow-up timer, or the `IsShort` predicate. It proves the remaining required sequence,
including both 20-second burns, engine costs, injector clearing, soot scrubbing and the 264/270-credit results.
The default run stops before the fixture interaction steps; it is not counted as a passing fuel proof.

## Captures reviewed

All images below were inspected. The feed label was moved 100 cm west in front of the wall fittings, reduced
to world size 12, and recaptured; its full text is now visible. Both standpipes face the walkway (yaw -90), and
the intake countdown was lowered to 110 cm above its origin. Lights and steam remain attached to the fixtures.
The engineering scene has strong green lighting and large existing module labels; those existing assets/layouts
were not changed.

| Capture | Visible evidence |
|---|---|
| [feed-leak](../../Saved/AgentCaptures/M3-6/feed-leak.png) | Full `FEED VALVE leak` label, steam, red light, red leak task |
| [low-fuel](../../Saved/AgentCaptures/M3-6/low-fuel.png) | Closed bypass; `Fuel 24% - open the bypass and feed casks` |
| [bypass-open](../../Saved/AgentCaptures/M3-6/bypass-open.png) | Amber light, `BYPASS OPEN`, intake-feed task |
| [intake-burning](../../Saved/AgentCaptures/M3-6/intake-burning.png) | Pallet and `INTAKE C3 12 s`; low-fuel row suppressed during the burn |
| [injectors-fouled](../../Saved/AgentCaptures/M3-6/injectors-fouled.png) | `Clear the injectors` task and Engines at 80% |
| [hub-last-run-fuel](../../Saved/AgentCaptures/M3-6/hub-last-run-fuel.png) | Full last-run line, 270 cr, clean air and two burned |
| [engineering-overview](../../Saved/AgentCaptures/M3-6/engineering-overview.png) | Fuel fixtures in the existing engineering room |
| [bridge-control-board](../../Saved/AgentCaptures/M3-6/bridge-control-board.png) | FUEL/BYPASS header below AIR; all module/line rows and stand-up footer fit |

Captures and raw logs live under git-ignored `Saved/`; they are retained in this checkout, and the commands below
regenerate them. The report is committed; image binaries and packs are not committed.

## Probed fixture positions

Both mesh paths returned `true` from `AssetTools.exists`; no fallback was required:
`/Game/ModSci_Engineer/Meshes/SM_PipeValve_A.SM_PipeValve_A` and
`/Game/ModSci_EngiProps/Meshes/SM_PlasticPallet.SM_PlasticPallet`.
[Availability evidence](../../Saved/AgentCaptures/M3-6/mesh-exists.txt).

Final origins stayed at the proposed coordinates in `FuelLayouts.cpp`. All probes used `AirlockWarp` followed
by `AirlockWhere`; the character capsule half-height is 96 cm, so feet Z = logged pawn Z - 96.

| Probe | Requested X, Y | Layout floor Z | Settled pawn X, Y, Z | Feet Z |
|---|---|---|---|---|
| FeedValve | -1130, 280 | 22 | -1137.001, 290.360, 118.000 | 22.000 |
| BypassValve | -1130, 700 | 22 | -1146.941, 700.000, 118.150 | 22.150 |
| FuelIntake | -1200, 600 | 22 | -1200.000, 600.000, 118.000 | 22.000 |
| Feed walkway | -1300, 280 | 22 | -1300.000, 280.000, 118.000 | 22.000 |
| Bypass walkway | -1300, 700 | 22 | -1300.000, 700.000, 119.807 | 23.807 |
| Intake walkway | -1300, 600 | 22 | -1300.000, 600.000, 118.000 | 22.000 |
| Engines walkway | -1300, 880 | 22 | -1300.000, 880.000, 118.000 | 22.000 |

Fixture-centre probes beside the wall depenetrate the capsule slightly. All four interaction spots retain their
requested X/Y, settle within two centimetres of the engineering floor, and are within 312 cm of their targets.
The new fixtures block Visibility only and do not block pawns. Valve meshes use scale 0.3, yaw -90; the pallet
uses scale 0.6, yaw 0; all three labels face world yaw 180.

[Probe CSV](../../Saved/AgentCaptures/M3-6/fuel-position-probes.csv),
[probe log](../../Saved/AgentCaptures/M3-6/position-probe-log.txt),
[probe output](../../Saved/AgentCaptures/M3-6/probe-output.txt). The final label-only adjustment did not change
fixture transforms or collision; the proof below reproduces the same seven probes.

## Scope and open questions

- No protected files, pack assets, maps, existing proof scripts or existing test assertions changed. The five
  protected game-mode functions are byte-for-byte unchanged against `90cbea7` after newline normalization.
  Existing test bodies retain all original statements; only additions for Burned and new tests were made.
  [Scope audit](../../Saved/AgentCaptures/M3-6/scope-audit.json).
- `Scripts\gen-projectfiles.bat` temporarily regenerated `.ignore`; those additions were reverted. No final
  changes remain outside the brief. `bHasFuelData` is an added result-presence marker needed for the brief's
  optional-fuel summary behaviour. `-ShortFollowup` and `-ProbePositions` are explicit modes of the new proof.
- **Open contract question (asked during implementation; no answer received):** should the new proof set fuel to
  80 before its leak, or should its default 90-unit case expect the still-good line and separately test short fuel?
  The implementation preserves the exact defaults and follow-up selection. At leak start 90 units buy 1350 s
  against a 1200 s voyage. Ten seconds later, `90 - (4/60 + 40/60) * 10 = 82.6667` units buy 1240 s against
  about 1190 s remaining: roughly 50 s of reserve, so `IsShort` is false. The leak must stay open about 15 s to
  consume the initial reserve. Waiting twelve seconds cannot yield the requested short line because the one-shot
  follow-up has already spoken at ten seconds. This is the conflict between [brief items 1/5 and item 10](../tasks/M3-6-fuel-and-bypass.md).
- Exhaustion and its once-only extension are proven headlessly as required. PIE uses one local player; a
  multi-client network session was not part of this proof.
- Final editor state: port 8000 open on `/Game/Maps/StationHub`, PIE stopped; `AssetTools.is_dirty` returned false
  for StationHub and Hauler_Mk1. [Final state](../../Saved/AgentCaptures/M3-6/final-editor-state.txt).
  No Computer Use/GUI tools were used, and no editor on port 8001 was touched.

## Reproduce

From `C:\Users\tclin\Desktop\ai_game_studio\Airlock`:

```powershell
git switch astra/m3-6-fuel-and-bypass
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\close-editor.ps1
Scripts\build-editor.bat
$env:AIRLOCK_MCP_PORT = '8000'
Scripts\run-editor.bat
while (-not (Test-NetConnection 127.0.0.1 -Port 8000 -InformationLevel Quiet -WarningAction SilentlyContinue)) {
    Start-Sleep -Seconds 1
}
Start-Sleep -Seconds 30
```

Then run:

```powershell
Scripts\run-tests.bat Airlock
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-loop.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-ship-graph.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-incidents.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-level-pass.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-cargo.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-voyage-clock.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-atmosphere.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-fuel.ps1
```

The last command intentionally reproduces the unresolved literal assertion (exit 1). To run the complete fuel
proof with the short-reserve setup, then reproduce all seven engineering probes:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-fuel.ps1 -ShortFollowup
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\verify-fuel.ps1 -ProbePositions
```

`-ShortFollowup` first verifies the fresh 89/90-unit state, then sends `AirlockFuelSet 80` immediately before
`AirlockFuelLeak`; the remaining actions and assertions follow item 10. It is an explicit diagnostic workaround,
not a claim that the default acceptance command passes. Each proof stops PIE and checks both maps are clean.
`-ProbePositions` returns the editor to StationHub. All MCP traffic uses port 8000; no editor on 8001 is touched.

## Reviewer verification (Claude, 2026-09-12)

Reviewed from Claude Code on the same checkout after Astra's process had exited (72 min, no GUI tool, the editor
on port 8000 left on StationHub with no PIE and both maps clean).

- **The open contract question above is answered in code, not in the proof.** Astra's arithmetic is right: with
  90 units at leak start the ship is not short ten seconds later, so the one-shot follow-up correctly speaks the
  still-good line and the brief's literal expectation could never pass. The fix (commit on the branch before the
  merge) is a third captain call tied to the fuel rather than to a timer: `TickFuel` speaks
  `Captain <Name> here. Reserve's gone - open the crude bypass and feed me casks, or we dock short.` once per
  voyage on the first tick `FFuelSystem::IsShort(secondsRemaining)` turns true while no travel is pending
  (`bFuelShortSpoken`, reset when the clock starts). In the proof that lands about 15 s into the open leak, which is
  the "starts asking for crude" beat the design note wanted. `verify-fuel.ps1` waits up to 30 s for that line
  instead of asserting the follow-up's text, and the `Airlock.Fuel.FollowupBranches` test counts it (1 for the two
  leaking cases, 0 for the shut and bypass cases). Two things learnt on the way: a transient world's
  `FTimerManager::Tick(10)` catches the looping fuel timer up but does not move the world clock, and a log
  capture that counts by `StartsWith` never sees a captain line (they all begin `Captain <Name>:`), so the
  assertion counts by `Contains`.
- **Diff read** against the brief: the fuel model is pure and deterministic (leak and burn integrated per tick,
  the cask burn as a 20 s timer that latches the +15 at the end, fouling every second cask, exhaustion latched
  once with the 120 s late-dock extension); the set piece rides the captain timer and consumes like an incident
  (forward jump fires, rewind never replays, a docking jump fires it before the docking caption); the three
  fixtures are spawned from a code table with the same soft-mesh fallback as the modules; `ECargoState::Burned`
  is additive and the M3-4/M3-5 strings are untouched. Nothing on the do-not-touch list changed and the five
  protected travel functions are byte-identical.
- **Merged tree**: M3-7 merged on top of this branch the same night (union conflicts in the game mode and the
  tally struct only). Two pinned strings had to follow M3-7's renamed docking caption: the dock-jump test now
  looks for `Docking at the refinery` and `verify-voyage-clock.ps1` for `Transition card: DOCKING - HALDER SOLAR
  REFINERY` (commits 7c9f3f4, 881d390). No fuel assertion was weakened.
- **Tests** on the merged tree: `Scripts\run-tests.bat Airlock` headless, **90 / 90** in 21 s.
- **Proofs** on the merged tree (`main` at 881d390), one at a time, unedited except for the two strings above:

| Proof | Result | Time |
|---|---|---|
| `verify-loop.ps1` | `LOOP OK` | 53 s |
| `verify-ship-graph.ps1` | `GRAPH OK` | 73 s |
| `verify-incidents.ps1` | `INCIDENTS OK` | 59 s |
| `verify-voyage-clock.ps1` | `CLOCK OK` | 107 s |
| `verify-level-pass.ps1` | `LEVEL OK` | 156 s |
| `verify-cargo.ps1` | `CARGO OK` | 91 s |
| `verify-atmosphere.ps1` | `ATMOSPHERE OK` | 126 s |
| `verify-fuel.ps1` | `FUEL OK` (feed leak -> shut -> bypass -> burn C3/C1 -> clear injectors -> clean air, pay 270) | 150 s |
| `verify-refinery.ps1` | `REFINERY OK` | 68 s |

- **Captures reviewed** (`Saved\AgentCaptures\M3-6\`): `feed-leak` (`FEED VALVE  leak` in the valve's red light,
  `Fuel leak - shut the feed valve` red at the top of the board; the steam itself is faint - M3-8), `bypass-open`
  and `intake-burning` (`BYPASS` and `INTAKE  C3 12 s` counting down over the intake, the board down to the
  unsecured casks), `injectors-fouled` (the Engines row and the hold prompt), `low-fuel` (the control board's
  fuel line), `bridge-control-board`, `engineering-overview` (the three fixtures on the engineering wall; the
  module's green flood washes the frame - M3-8), `hub-last-run-fuel` (`Last run:` with the soot fine).
- **Reproduce** on `main`: the recipe in "Reproduce" above minus the branch switch; `verify-fuel.ps1` now exits 0
  with its defaults (`-ShortFollowup` still exercises the follow-up's own short branch by setting fuel to 20).
