# Report M5-2b: Work lights on the refinery floor

Claude, 2026-09-13 (midday, while Astra runs M5-3b). Branch `claude/m5-2b-floor-lighting` from `main` at f92eb5c.
Content only (`Content/Maps/RefineryFloor.umap`, LFS) plus one proof fix. Worktree `..\Airlock-claude`, editor on
port 8001.

## What changed

The floor was lit only by the Interiors pack's emissive strips (the M5-2 captures: a black deck, labels legible
only against the cyan glow). Nine movable lights now sit in the map's `Floor/Lights` folder, warm (linear
1.0 / 0.84 / 0.63) against the pack's cyan, every one a work light over something the crew uses:

| Actor | Kind | Location | Intensity |
| --- | --- | --- | --- |
| `FloorLight_Still1..3` | spot, straight down, 25 / 50 deg | (-5800 / -5400 / -5000, 1900, 340) | 600 cd |
| `FloorLight_Tank` | spot | (-4650, 1900, 400) | 500 cd |
| `FloorLight_Pallets_W` / `_E` | spot | (-6100 / -5600, 1325, 340) | 500 cd |
| `FloorLight_Panel` | spot | (-5400, 1150, 340) | 400 cd |
| `FloorLight_Walk_W` / `_E` | point, fill | (-6200 / -4800, 1560, 320) | 80 cd |

Attenuation 1000 cm (900 for the fills), mobility Movable (no lighting build; the maps run virtual shadow maps),
candela units set explicitly. Spawned by an idempotent MCP script (`add_to_scene_from_class`, then
`set_properties` on the light component; a colour only takes as lowercase `r/g/b/a` floats - the first pass read
back white), saved with `save_assets`, the editor returned to StationHub with both maps clean.

`Scripts\verify-floor.ps1`: the wait for the tank's first 300 L now accepts the floored litre value the log
prints at the crossing (`Tank 3[0-9][0-9] L diesel`; one run printed `301`).

## How it was verified

`Scripts\verify-floor.ps1` on port 8001 with the lit map: **FLOOR OK** (199 s; the batch's tank count, the result
and the base fuel unchanged in kind: 752 L, pay 406, +6 t). Captures under `Saved\AgentCaptures\M5-2\` on the
worktree, the M5-3a set kept in `before-lights\` for the comparison: `floor-arrival` (the deck reads as a floor,
the stills and the tank stand out at the far end), `floor-stills` (Still 2 and its label under its own light),
`floor-pallets` (the staging row lit, the cask labels white on a lit deck), `floor-still-running`, `floor-tank`.
PIE ran at about 50 FPS with the nine shadow-casting lights (the log's frame counters).

## Unsure about
- The still labels sit between the light and the tank top; readable, but a second pass may angle the still
  spots toward the walkway (pitch -60 from a point behind the still) so the gauge stack M5-3b adds is front-lit.
- Nothing in engineering or the hold changed; the ship's lighting pass is still on the follow-up list.

## Outside the brief
No brief for this pass (a content-only follow-up from STATUS, "the floor is dark in its pack dressing"); no code.

## Reproduce
```
AIRLOCK_MCP_PORT=8001 powershell -ExecutionPolicy Bypass -File Scripts\verify-floor.ps1
```
The light table lives in the map; the spawning script is in the reviewer's session scratchpad and the table
above is its content.
