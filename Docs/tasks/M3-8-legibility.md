# Task M3-8: Legibility pass - labels, rows, lights, steam and readouts

Owner: GPT-6 Astra (Codex on stack). Reviewer: Claude. Branch: `astra/m3-8-legibility` (from `main` after M3-7).
Design source: `Docs/design/refining-run.md` (the run as it now plays), the first design note's property 2
("legible: you can see and hear the problem before the UI tells you") and `Docs/STATUS.md` "Open follow-ups"
(the list this task closes, except "type variety in solo schedules", which lives in `Voyage/` and is out of
scope). This brief is the contract; where they disagree, this brief wins. Everything here is code and
code-spawned visuals: **no map, no pack asset, no new content** (the map dressing is a separate Claude task).
Numbers below are starting values; the reviewer tunes from the captures. Red-teamed by a second reader on
2026-09-12 before launch; 22 findings folded in.

**Parallel work (read this).** Claude works in the worktree `..\Airlock-claude` (editor on port 8001) while you
run and may touch `Core/AirlockPlayerController.*`, `Core/AirlockCharacter.*`, `Core/AirlockGameMode.*`,
`Core/AirlockGameState.*` and `Voyage/**`. So this task must not edit any of those - everything below lives in
actors, widgets, layouts and `Ship/ShipVisuals.*`. Your editor is the one on port 8000 on this checkout. Never close
editors by process name (`Scripts\close-editor.ps1` only). No Computer Use / GUI tool (HANDOFF section 5 covers the
one dialog that can appear).

## Prompt (what the reviewer sends to Codex)

```
You are the developer on Airlock, a UE 5.8 C++ spaceship MMO MVP. Work in this repo on a new branch
astra/m3-8-legibility (create it from main). Read AGENTS.md, Docs/HANDOFF.md (sections 2, 3, 5 and 7),
Docs/design/refining-run.md, Docs/STATUS.md, Docs/reports/M3-6-fuel-and-bypass.md and
Docs/reports/M3-7-refinery-dock-and-base-fuel.md (their "open questions" are your list), then
Docs/tasks/M3-8-legibility.md, then `git log --oneline -30`. State the currently verified run in your own words
before you start editing. Implement Docs/tasks/M3-8-legibility.md exactly as specified there.
```

## Why this task

The run works and every proof is green, but the captures say the same three things every time: labels are
unreadable where the player actually stands (they face -X while the player stands on the Y 1560 walkway; rows
100 cm apart overlap), a Priority manifest turns the task board into eight identical lines, and the lights that
are meant to say "hazard" wash out the thing they light. This pass makes what the systems already know visible
from where the player stands, without adding a mechanic. Glyph size is not the problem; facing and stacking are.

## Definition of done

1. **One label policy** in `Ship/ShipVisuals.h` + new `Ship/ShipVisuals.cpp`:
   `void ShipVisuals::ConfigureLabel(UTextRenderComponent* Label, float WorldSize, float FacingYaw, FLinearColor
   Colour, FVector RelativeOffset)` - `SetWorldSize`, `SetHorizontalAlignment(EHTA_Center)`, `SetVerticalAlignment
   (EVRTA_TextCenter)`, `SetRelativeLocation(RelativeOffset)`, `SetRelativeRotation(FRotator(0, FacingYaw - OwnerYaw,
   0))` where `OwnerYaw = Label->GetOwner() ? Label->GetOwner()->GetActorRotation().Yaw : 0`, `SetTextRenderColor
   (Colour.ToFColor(true))`, no collision. Used by every actor with a label: `AShipModule`, `AShipLine`,
   `ACargoCrate`, `ACargoSlot`, `AFuelValve`, `ABypassValve`, `AFuelIntake`, `AControlStation`, `AStationSign` (signs
   pass their actor yaw and their spec's size and colour). Each actor keeps its current label height and offsets
   (module and line 210, crate 95, pad 35, rack 180, feed valve (-100, 0, 155) and bypass (-35, 0, 155) in their
   own frame as today, intake 110, control station 200; the reviewer tunes from the captures). Sizes stay: modules,
   lines and fixtures 22, crates and pads 22, signs as they are.
   **Every label is two lines** - the id on line 1, the state on line 2 - by this table (existing strings on each
   line are unchanged, only the separator becomes a newline): module `PowerCore` / `Nominal  100%`
   (`LifeSupport` keeps its third line `Scrubbers <Mode>`, a test asserts it); line `Coolant_1` / `None|Leak|Short`
   as today's second word; crate `C2` / `Loose` or `Loose  45C` when volatile; pad `Clamp_3` / `empty` or `C3`;
   rack `ColdRack` / `empty` or `C2`; feed valve `FEED VALVE` / `shut|leak`; bypass `BYPASS` / `closed|OPEN`; intake
   `INTAKE` / `ready|bypass closed|C3 12 s`; the control station keeps `CONTROL STATION` / `<who has the conn>`.
   The crate's second line is drawn amber `(1, .6, .1)` at or above `FCargoRates().WarnTemperature` and red
   `(1, .15, .1)` at or above 80, white otherwise - a hazard cue that needs no light.
2. **Labels face the walkway.** `FCargoCrateSpec` and `FCargoSlotSpec` gain `UPROPERTY() float LabelYaw = 180.f`
   (180 keeps today's facing for hand-built specs) and `CargoLayouts::Build` sets it from `static float
   CargoLayouts::LabelYawFor(double Y)`: spots north of the Y 1560 walkway (`Y > 1560`) face `-90` (towards -Y, the
   reader), spots south face `90`; the rack faces `90`. `FFuelFixtureSpec::LabelYaw` (180, towards the X -1300
   walkway) and the module / line `LabelYaw` from `ShipLayouts` stay. `ACargoCrate::Refresh` re-applies
   `ConfigureLabel` against the crate's *current* actor yaw every time (a dropped crate takes the pawn's yaw, a
   racked crate the rack's; today's fixed relative yaw would face sideways).
3. **The carried crate has no label.** `ACargoCrate::Refresh` hides the label while `Carried` (the prompt widget
   already names it) and while `Burned`; a crate that is put down or placed shows it again.
4. **Task board summaries and readouts.** `UTaskBoardWidget::BuildRows` gains a trailing `bool bSummaries = false`;
   `RefreshRows` passes `true`, the new tests pass `true`, and every existing test keeps calling it without the
   flag and stays green byte for byte. With summaries: when more than 4 crates would get `Secure crate` rows, one row
   `Secure 10 loose casks` replaces them, where the first would be; when more than 2 would get `Cool crate` rows,
   one row `Cool 3 casks - hottest C4 71C` replaces them (red if the hottest is at or above 80); at 4 and 2 or fewer
   nothing changes. Summaries count what the rows counted (Vented and Burned crates are neither loose nor heating).
   The header becomes two lines: line 1 `SHIP TASKS`, line 2 the readouts for field players from a pure `static
   FText UTaskBoardWidget::BuildReadout(const FAtmosphereState* Air, const FFuelState* Fuel, int32 SecondsToDock)`
   -> `AIR 22 ppm  |  FUEL 80%  |  19:29 to dock` (null parts omitted, `SecondsToDock < 0` omits the clock, all absent
   -> empty; `AIR = FloorToInt(Ppm)`, `FUEL = FloorToInt(Units / FFuelRates().Capacity * 100)`), shown in a second
   `UTextBlock` refreshed from both `UpdateState` (the clock) and `RefreshRows` (air and fuel). The widget already
   receives the whole game state through `UpdateState`; nothing outside the widget changes.
5. **Lights that say hazard without washing out the thing.** `AShipModule`: `LightRadius` 250 -> 180 and
   `LightIntensity` 300 -> 150 cd (the green flood in engineering); `ACargoCrate::HeatLight` attenuation 180 -> 120;
   the valve lights unchanged. Best-effort emissive on the case: on the first `Refresh` that finds the body's
   material, probe its parameters once per process (`GetAllScalarParameterInfo` / `GetAllVectorParameterInfo`, one
   log line listing them) and, if `Emissive Light` or `Emissive Boost` exists, drive it with a
   `UMaterialInstanceDynamic` by the heat fraction (0 below `WarnTemperature`, 1 at `VentTemperature`); if neither
   exists, or the body is the cube fallback, log the skip once per process and do nothing. The label colour in item 1
   is the cue that cannot fail; the emissive is a bonus.
6. **Steam that reads.** Every use of `PS_SteamDoor` gets `SetRelativeScale3D(FVector(2.5))` on the particle
   component and a higher origin: crate `VentFX` 50 -> 80, `AShipLine::LeakFX` 120 -> 150, `AFuelValve::LeakFX`
   100 -> 130. Cascade sprite emitters scale with the component's world scale, but nobody has proven it on
   `PS_SteamDoor`: verify in the `feed-leak-steam` capture and, if the puff does not grow, report rather than tune.
   Nothing else about the FX changes (the Cascade-to-Niagara conversion stays deferred).
7. **Tests** (extend, never rewrite): `Tests/LegibilityTests.cpp` - `ConfigureLabel` sets size, yaw relative to the
   owner, centre and colour (a test actor with a text component); `LabelYawFor` returns -90 north of 1560 and 90 at
   or south of it, and `Build` assigns it for all 12 crate and 10 slot spots; the summary rows appear at 5 loose / 3
   heating and not at 4 / 2 with `bSummaries = true`, and the same inputs without the flag give the old rows; the
   carried crate's label is hidden and restored (spawned world, the `FCargoTestWorld` harness copied as
   `FLegibilityTestWorld`); `BuildReadout` for air + fuel + clock, for air only, and for nothing.
8. **PIE proof** `Scripts\verify-legibility.ps1` (captures under `Saved\AgentCaptures\M3-8`, `LEGIBILITY OK`;
   conventions of `verify-refinery.ps1`: log-anchored waits on the category boundary (`: Carry C1 by`, not
   `Carry C1 by` - the console echoes the words), HUD text capture-only). Steps: book the Priority run (`AirlockBook
   2`, 12 casks); on `Incident director started` send `AirlockVoyageClock 1200`; from the walkway `AirlockWarp -5450
   1560 116`, look north (`AirlockLook 90`, +Y) - capture `hold-north` (crate labels face the camera, two lines
   each); look south (`AirlockLook -90`) - capture `hold-south` (pads and the rack face the camera); warp beside C1
   (`AirlockWarp -6330 1700 116`, `AirlockLook 0`), `AirlockCarry C1` -> `: Carry C1 by` - capture
   `carrying-no-label` (the case in front of the camera carries no text); `AirlockDrop` -> `: PutDown C1 by`;
   `AirlockVoyageClock 1200`; `AirlockCrateHeat C2 70`, `AirlockCrateHeat C4 85`, `AirlockCrateHeat C6 60` -> `: Cargo
   C4 Loose temp=85`; within 5 s capture `board-summaries` (the first *cargo* row is `Cool 3 casks - hottest C4 85C`
   in red, then `Secure 9 loose casks`; an air row may sit above them once the warm casks puff - expected); warp to
   engineering (`AirlockWarp -1300 760 118`, `AirlockLook 0`) - capture `engineering-panel` (the life-support label
   readable, no green flood); `AirlockVoyageClock 1200`; `AirlockFuelLeak` -> `: SetPiece FuelLeak` (it fires the
   scheduled set piece early, captain lines included); warp to `-1300 280 118`, `AirlockLook 0` - capture
   `feed-leak-steam` (the steam visible above the valve); leave through the exit -> `: Voyage result:`. The reviewer
   judges the captures; the script asserts only those log lines.

## Do not touch
- `Core/AirlockPlayerController.*`, `Core/AirlockCharacter.*`, `Core/AirlockGameMode.*`, `Core/AirlockGameState.*`,
  `Voyage/**`, `Ship/ShipGraph.*`, `Ship/ShipLayouts.*` (positions), `Ship/FuelSystem.*`, `Ship/CargoModel.*`,
  `Ship/ShipAtmosphere.*`, `Core/Interactable.h`, `Core/AgentConsoleSubsystem.*`.
- Every existing test expectation (`BuildRows` without the flag is byte-for-byte today's; the LifeSupport label
  keeps `Scrubbers <Mode>`; `RefineryTests` compares ids and spots only) and every existing proof (the nine must exit
  0 unedited; their captures will look different, which is the point; no proof regex-matches a label or a row).
- `Content/**`, every map, the pack folders.

## Edge cases (decided)
- Summaries are a display choice of the live widget; the pure function's default keeps every older caller exact.
- A Burned crate's label never shows (the actor is hidden); a Vented crate keeps its label (red second line).
- `Cargo <Id> <State> temp=` is logged only on a state change or a 40 C crossing, so heating an already-warm crate
  logs nothing: the proof anchors on `temp=85` for C4, which crosses 40.
- Once per process means a static, not once per crate.

## Acceptance (reviewer runs)
`Scripts\build-editor.bat` clean; `Scripts\run-tests.bat Airlock` all green (90 + the new ones); all nine existing
proofs and `verify-legibility.ps1` exit 0; the captures show two-line labels facing the walkway, no label on the
carried crate, the summary rows and the readout line, a readable engineering panel and visible steam.
