# Third-party content inventory

Content packs that live in this project but are **not in git**. They are ignored by .gitignore; a fresh clone
restores them from Fab (Epic Games Launcher -> Library -> Fab Library -> Add to project, or Unreal Editor ->
Window -> Fab) into the exact folder listed here. Do not move or rename pack folders after import (Fab re-adds
create redirectors and duplicates).

| Folder under Content/ | Pack (Fab listing) | Creator | Licence | Claimed | Notes |
|---|---|---|---|---|---|
| ModSci_Engineer   | Modular SciFi: Engineer Interiors | Jonathon Frederick | Fab Standard Licence | 2026-09-11 | corridor kit (360 pitch), demo `Maps/Example_Dynamic` = the source of `Hauler_Mk1` |
| ModSci_EngiProps  | Modular SciFi Engineer Props | Jonathon Frederick | Fab Standard Licence | 2026-09-11 | 41 props: pipes, valves, tanks, wrench, extinguisher, alarm light, cases, pallets |
| ModSciInteriors   | Modular SciFi: Interiors (Season 1) | Jonathon Frederick | Fab Standard Licence | 2026-09-11 | room kit (670 pitch), demo `Maps/Example_Dynamic` = the source of `StationHub`; `PS_*` Cascade particles |
| StarterBundle     | Modular SciFi Season 1 Starter Bundle (free): `ModularSci_Comm` = Command Center (79 meshes, central terminal + observation deck), `ModularScifiHallways`, `ModularScifiProps` = Props I (desks, stool, laptop, keypad, hand scanner, temp control, cabinets, shelves), `ModularSciFiMats` = Materials & Decals, `ModularSci_Int` = a second copy of Interiors, `NatureMaterials_Ice`, `UE4_Assets`, `CollectionMaps` | Jonathon Frederick | Fab Standard Licence | 2026-09-12 | the control room / bridge kit; `SM_Terminal_A(_UI)`, `SM_Wall_Window_*`, railings, stairs. The `ModularSci_Int` copy is unused - reference `/Game/ModSciInteriors` |
| StarfieldFree     | Starfield Free (star / nebula spheres, 8K Milky Way + nebula textures, BP_Starfield_* sky blueprints) | (see Fab listing) | Fab Standard Licence | 2026-09-11 | space sky for every map; use BP_Starfield_Stars_Free or the Milky Way sphere, never edit the pack |
| SD_Art            | Industrial Infrastructure (`SD_Art/Industrial_Infrastructure`): catwalks, ladders, I-beams, support columns, wall panels, vents, large pipes and valves; 8K textures, two 100+ MB HDRIs | (see Fab listing) | Fab Standard Licence | 2026-09-12 | engineering-bay dressing (catwalks, cage ladders). Heavy textures - use sparingly on the laptop |
| Construction_VOL2 | Construction props vol. 2: electric drill, rebar cutter, sander, jackhammer, ladders, canisters, propane tanks, cord wraps, spools, hard hats | (see Fab listing) | Fab Standard Licence | 2026-09-12 | hand-tool candidates (`SM_ElectricDrill_01a` = repair tool, `SM_Spool_01a` / `SM_CordWrap_01a` = tape/wire) |
| Warehouse_Storage | Warehouse storage (shelving, pallets, assembled racks) | (see Fab listing) | Fab Standard Licence | 2026-09-12 | cold rack / cargo-hold shelving candidates |

## Rules

- Fab Standard Licence: OK to use in the shipped game; NOT OK to redistribute the source assets (hence not in git).
- Everything Airlock authors on top of a pack (level instances, material instances, blueprints) lives outside the
  pack folders (see Content/Airlock/...) so it IS versioned.
- When a pack is updated on Fab, bump the "Claimed" date here and note anything that changed in the module grid.
- Every new pack folder gets a line in `.gitignore` *before* the next `git add` (the packs are multi-GB).
- Reference: docs/airlock-asset-architecture-2026-09.md (one architectural family; grid + door standard).
- Fab bundles are collections: the launcher adds each pack of a bundle separately, so check that every pack you
  expect actually produced a folder (the Season 1 bundle's Command Center was missed for a day this way).

## Post-import step: StarfieldFree collision

The sky sphere meshes used by `BP_Starfield_Stars_Free` / `BP_Starfield_bgNebula_Free` ship with collision. Any pawn spawned inside them is depenetrated to the sphere surface (X = -6.9e8) - warps look fine, spawns and level arrivals do not. After importing the pack, run `StaticMeshTools.remove_collisions` on those two meshes (agent script 133) and save them. The same applies to any sky sphere or backdrop mesh (`SD_Art/.../SM_Skysphere`, `Construction_VOL2/Meshes/SM_ZSphere`, `SM_ZBackdrop_01`) if one is ever placed in a map.

## Post-import step: Nanite

After importing a Modular SciFi pack, enable Nanite on every opaque kit static mesh (skip glass/window/holo/decal/UI/sky meshes - Nanite cannot render translucency). Without it the demo-hall-sized levels overflow the Virtual Shadow Map non-Nanite marking queue. `Scripts/agent/135-enable-kit-nanite.ps1` does it through Unreal MCP: `find_assets` (StaticMesh, recursive) -> `is_nanite_enabled` -> `set_nanite_enabled` -> `save_assets`, 60 meshes per call, idempotent. 2026-09-11: 177 meshes switched (Engineer 47, EngiProps 40, Interiors 90). 2026-09-12: the StarterBundle packs (script 151). The industrial packs (SD_Art, Construction_VOL2, Warehouse_Storage) are done on demand when a mesh from them is first placed.

Exceptions: `SM_Door_Small_UI`, `SM_Terminal_A_UI` and `SM_AlarmLight` stay non-Nanite (translucent slots; Nanite logs an invalid-material warning).

After enabling Nanite, play each map once and resave every material instance the log reports as `missing usage flag Nanite` (they render as the default material in game until then); meshes the log calls out for translucent slots stay off Nanite.
