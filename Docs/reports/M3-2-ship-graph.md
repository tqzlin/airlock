# Report: M3-2 ship systems graph, modules and lines, hold-E repair

Branch `astra/m3-2-ship-graph`, three commits by GPT-6 Astra (Codex on stack) across two sessions on 2026-09-12
(the first was cut off by a Codex usage limit after the model commit, the second after the proof script was
written but before it was committed). This report was written by the reviewer (Claude) from the branch, the
review logs (`agent/results/153-*`, `157-*`) and Astra's captures, because Astra's second session ended before it
could write one. Brief: `Docs/tasks/M3-2-ship-graph.md`.

## Behaviour changed
- The ship has a model. On `Hauler_Mk1` the game mode builds an `FShipGraph` from `ShipLayouts::Find("Hauler_Mk1")`
  (7 modules, 4 lines), spawns one `AShipModule` and one `AShipLine` actor per entry (outliner folder `Ship`), and
  ticks the graph at 4 Hz while a voyage is active. A leaking coolant line drains the CoolantLoop; a Degraded or
  Failed CoolantLoop drifts the PowerCore; a Degraded or Failed PowerCore browns out LifeSupport and CargoClamps; a
  shorted power line drains the module it feeds. Nothing recovers by itself.
- Everyone sees it. `AAirlockGameState` replicates `ShipModules` / `ShipLines`; `OnShipGraphChanged` drives the
  module status lights (green/amber/red) and labels, the leak steam (`PS_SteamDoor`) and the flickering short light.
- The first verb. `IInteractable` gained `GetInteractHoldSeconds` / `OnHoldComplete`; the character starts a
  server-side hold on E-press when the target is a hold interactable, pauses it out of range, resets it on release,
  and completes it after 3 s (leak) / 4 s (short), which clears the fault through `ShipRepairLine`. The local player
  sees `UHoldProgressWidget` (prompt + progress bar) while holding. The voyage board is still a press.
- Console helpers: `AirlockFault`, `AirlockDamage`, `AirlockRepairModule`, `AirlockShipStatus`, `AirlockHold`,
  `AirlockRelease`.

## Files
`Ship/ShipGraph.h/.cpp`, `Ship/ShipLayouts.h/.cpp`, `Ship/ShipModule.h/.cpp`, `Ship/ShipLine.h/.cpp`,
`Ship/ShipVisuals.h`, `UI/HoldProgressWidget.h/.cpp`, `Core/Interactable.h`, `Core/AirlockCharacter.h/.cpp`,
`Core/AirlockGameMode.h/.cpp`, `Core/AirlockGameState.h/.cpp`, `Core/AirlockPlayerController.h/.cpp`,
`Tests/ShipGraphTests.cpp`, `Tests/ShipInteractionTests.cpp`, `Scripts/verify-ship-graph.ps1` (committed by the
reviewer on Astra's behalf). 22 source files, +1553 lines. Nothing under `Content/`, nothing in the do-not-touch list.

## Tests
`Scripts\run-tests.bat Airlock` / in-editor `RunTestsByFilter Airlock`: **24 complete, 24 passed, 0 failed**
(9 existing + 11 `Airlock.ShipGraph.*` pure-model tests + 4 interaction tests in a spawned test world:
`Airlock.ShipGraph.CompetingHoldsAndDestroyedTarget` and siblings covering range, pause/reset, completion and
the board staying a press). Verified independently by the reviewer on the editor Astra left open (binary of
11:02:26).

## PIE proof (reviewer reproduction, `Scripts\verify-ship-graph.ps1`, exit 0, 72 s)
```
> AirlockShipStatus                     Ship graph empty                         (hub)
> AirlockWarp -4700 1560 116 / AirlockBook 0 / AirlockWarp -4265 1560 116
  Ship graph built: 7 modules, 4 lines (section Hauler_Mk1)
> AirlockShipStatus                     Ship PowerCore Nominal cond=100.00 ... Ship Galley Nominal cond=100.00
> AirlockFault Coolant_1 Leak           Line Coolant_1 fault=Leak
> AirlockDamage CoolantLoop 50          Ship CoolantLoop Degraded cond=49.50
  (12 s)  AirlockShipStatus             Ship PowerCore Nominal cond=93.75        (drifting)
> AirlockHold Coolant_1                 Hold Coolant_1 rejected: out of range (662 cm, reach 312 cm)
> AirlockWarp -6100 1400 116 / AirlockHold Coolant_1
  Hold Coolant_1 started by AirlockCharacter_0 duration=3.0s
  Hold Coolant_1 complete by AirlockCharacter_0
  Line Coolant_1 fault=None
> AirlockRepairModule CoolantLoop 100   Ship CoolantLoop Nominal cond=100.00
> AirlockWarp -7015 1560 116            Voyage ... complete; Arrival spawn ... PlayerStart_0 at X=-5640
> AirlockShipStatus                     Ship graph empty
GRAPH OK
```
`Scripts\verify-loop.ps1`: LOOP OK before and after. Captures: `Saved\AgentCaptures\M3-2\modules-nominal.png`,
`coolant-leak-and-degraded-labels.png`, `hold-progress.png`, `repaired-labels-no-steam.png`, `returned-to-hub.png`
(copies under `agent/results/images/astra-m3-2b/`).

## Reviewer notes
- Code read in full: server authority is respected everywhere (`HasAuthority()` on every mutation, owner-only
  replication of the hold, `COND_InitialOnly` spec on the actors); the graph is published only on change; the
  `PS_SteamDoor` puff is restarted on `OnSystemFinished` so a leak keeps venting.
- The pawn stands at Z 63.9 at the placeholder line position (-6100, 1400): the west hall's glass floor is lower
  than the corridor's 20 there, so the placeholder actors float ~50 cm. Expected; the level pass moves them.
- Astra's captures suggest the steam is hard to see in `CaptureEditorImage` frames (it made "render flags" and
  "unlit diagnostic" captures). Worth a look in the legibility pass (M3-6), not a blocker.
- Open from M3-1, still open: every console helper is an unchecked Server RPC - gate behind a dev-cheats flag
  before M4.
- Follow-ups for M3-3: `ShipSetLineFault` / `ShipDamageModule` are the Incident Director's API; `OnShipGraphChanged`
  is the task board's feed.
