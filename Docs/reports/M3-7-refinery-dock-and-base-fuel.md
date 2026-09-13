# M3-7 report: contract tiers, the refinery dock, base fuel and the Rig's identity

Claude (Fable, Claude Code on stack), 2026-09-12. Branch `claude/m3-7-refinery-dock` in the worktree
`..\Airlock-claude` (editor on port 8001), created from `main` after the M3-5 merge. Brief:
`Docs/tasks/M3-7-refinery-dock-and-base-fuel.md`. Design: `Docs/design/refining-run.md` sections 2, 4 and 6.

## Behaviour changed

- **The board sells one run in three sizes.** The three offers are the refining run's contract tiers: `Milk run -
  4 casks`, `Standard run - 8 casks`, `Priority run - 12 casks`, each with its captain and crew slots (2 / 4 / 6),
  all on `Hauler_Mk1`. A booked tier sizes the hold: 4 / 8 / 12 casks (every second one volatile, 2 / 4 / 6),
  3 / 6 / 9 transit clamps and the cold rack. Tier 0 is exactly the manifest every existing proof books, so nothing
  that passed before sees a difference; the eight extra crate spots and six extra clamp spots were found with a
  110-point grid probe of the hall floor and each was dropped on by a pawn. Pallets are drawn at 0.75 scale (a
  per-slot mesh scale on the pad only) so nine of them fit south of the walkway.
- **The ship docks at the refinery.** The hub-bound card reads `DOCKING - HALDER SOLAR REFINERY` / `cargo off,
  fuel on - then home to the Vell Extractor` for the door and for the automatic return. The captain's lines carry
  the fiction: departure names the sludge casks, the mid-voyage line differs per tier, docking names the refinery.
- **The Rig has a fuel stock.** Each docking credits 15 t of refined methane per intact cask and 10 t of it comes
  home as surplus; `UVoyageSubsystem` keeps `BaseFuelTonnes` and `RunsCompleted` in a save-game slot (`Airlock`,
  default 200 t), adds the surplus on every `SetLastVoyageResult` (`Base fuel 220 t (+20 this run), runs 1`), and the
  hub's game state carries both to the board: `Base fuel 220 t - runs 1 (+20 this run)` under the new title `VELL
  EXTRACTOR - DOCK B DEPARTURES`. The `Voyage result:` line gains `, fuel +<n> t` at its end (the tally's `Summary`
  text is unchanged, so M3-5's exact-string test stays green). The stock never falls in the MVP.
- **Signs on the walls, from code.** Two `AStationSign` text actors spawn on the hub from `Station/StationSign.cpp`:
  `VELL EXTRACTOR / DOCK B - HALDER SOLAR REFINERY RUNS` over Dock B and `VELL EXTRACTOR / DOCK A - NO VESSEL DOCKED`
  over Dock A. No map changed.
- Console: `AirlockBaseFuel`, `AirlockBaseFuelSet <t>`, `AirlockSaveReset` (deletes the slot; every proof that
  touches the stock ends with it).

## Files

- `Voyage/VoyageTypes.h` (tiers, `CaskCount`, `VolatileCount`), `Voyage/VoyageSubsystem.*` (tier specs, the save),
  `Voyage/VoyageSchedule.cpp` (lines), `Core/AirlockSaveGame.h` (new), `Ship/CargoLayouts.*` (`Build`, the spot
  tables, `MeshScale`), `Ship/CargoSlot.cpp` (pad scale), `Ship/CargoModel.*` (`FuelCredit`, `FuelSurplus`),
  `Core/AirlockGameMode.*` (`ActiveCargoLayout`, the cards, the signs, the result log), `Core/AirlockGameState.*`
  (`BaseFuelTonnes`, `RunsCompleted`), `UI/VoyageBoardWidget.*` (title, hint, offer line, base line),
  `Core/AirlockPlayerController.*` (helpers), `Station/StationSign.*` (new), `Tests/RefineryTests.cpp` (new),
  `Scripts/verify-refinery.ps1` (new). Nothing under `Content/`; no map touched.
- Decided test changes (the type-naming ones): `AirlockSmokeTests.cpp` - the old types become the tiers, "crew
  within 4..8" becomes "the Standard run seats four", "different types pick different ships" becomes "every tier
  flies the hauler" and "tiers size the manifest", and the offer description is checked for the tier and the cask
  count instead of the ship; `VoyageScheduleTests.cpp` - the tier names only.

## Verification

- `Scripts\build-editor.bat` in the worktree: Succeeded.
- `Scripts\run-tests.bat Airlock`: **75 / 75** (72 + `Airlock.Refinery.TiersAndManifests`,
  `Airlock.Refinery.TallyFuelAndBoardText`, `Airlock.Refinery.SaveRoundTrip`).
- `Scripts\verify-refinery.ps1 -ProbePositions` (port 8001): **REFINERY POSITIONS OK** - all 22 spots settle at feet
  Z -64.85 (Clamp_7 at -63.6, 1.2 cm proud; the probe allows 2 cm) and the two sign spots stand over the hall floor.
- `Scripts\verify-refinery.ps1` (port 8001): **REFINERY OK in 68 s**. Tail:

```text
> AirlockCargoStatus                      -> 12 x Cargo C<n> Loose temp=0, 9 x Cargo slot Clamp_<n> clamp empty, Cargo slot ColdRack rack empty
> AirlockCarry C1 / AirlockPlace Clamp_1   -> Cargo C1 Secured ; AirlockCarry C2 / AirlockPlace ColdRack -> Cargo C2 Racked
> AirlockWarp -7015 1560 116               -> Transition: DOCKING - HALDER SOLAR REFINERY -> /Game/Maps/StationHub
Voyage result: 2/12 crates intact, 0 vented, 10 loose, hull 100.00%, pay 550, air 0 ppm, 0 blackouts, fuel +20 t
Base fuel 220 t (+20 this run), runs 1
> AirlockBaseFuel                          -> Base fuel 220 t, runs 1
> AirlockBook 0 ... AirlockWarp -7015 1560 116
Voyage result: 0/4 crates intact, 0 vented, 4 loose, hull 100.00%, pay 350, air 0 ppm, 0 blackouts, fuel +0 t
Base fuel 220 t (+0 this run), runs 2
> AirlockSaveReset                         -> Save reset: base fuel 200 t, runs 0
REFINERY OK: reset -> priority contract, 12 casks -> 2 intact -> refinery dock, +20 t -> hub board -> empty run adds 0 -> reset
```

- Captures (`Saved\AgentCaptures\M3-7\`): `priority-hold` (twelve cases on the floor with labels, nine pallets and
  the rack, the board `Secure crate C1..C8` then `+4 more`), `hub-board-fresh` and `hub-board-after-run` (the title,
  `Base fuel 220 t - runs 1 (+20 this run)`, `Last run: 2/12 crates intact - hull 100% - pay 550 cr - air clean`, the
  three tier offers), `sign-dock-b` (the Rig's sign over the Dock B door).
- The seven regression proofs are run on the merged tree at review time (this branch merges after M3-6).

## Decisions and open questions

- The pay for a Priority run with two casks secured is 550 (200 + 200 hull + 150 clean air); no manifest bonus with
  ten loose. The fuel credit is independent of pay on purpose - the base's stock is the campaign meter.
- The save is per machine and unversioned; `AirlockSaveReset` is the escape hatch. M4 replaces it server-side.
- Sign placement is on the hall's centre line at Z 330, hanging over each dock door; it reads well from the hall
  (capture). The refinery's mirror field in the hub's sky is left to the map pass.
- Legibility (M3-8): in the twelve-cask hold the crate labels overlap where the rows are 100 cm apart, and the
  engineering module's green light floods the north rows; the task board's `+4 more` hides most of a Priority
  manifest - a "12 loose casks" summary row would read better than eight identical rows.
- Balance: a Priority run solo means twelve carries in twenty minutes with six volatile casks warming after the
  calm; that is the tier that "only really pays crewed", by design, but it has not been playtested.
