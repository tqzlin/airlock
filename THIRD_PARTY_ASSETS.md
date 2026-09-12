# Third-party content inventory

Content packs that live in this project but are **not in git**. They are ignored by .gitignore; a fresh clone
restores them from Fab (Unreal Editor -> Window -> Fab -> My Library -> Add to project) into the exact folder listed
here. Do not move or rename pack folders after import (Fab re-adds create redirectors and duplicates).

| Folder under Content/ | Pack | Creator | Licence | Claimed | Notes |
|---|---|---|---|---|---|
| ModSci_Engineer   | Modular SciFi Season 1 Starter Bundle (Engineer set) | Jonathon Frederick | Fab Standard Licence | 2026-09-11 | modular walls/floors/ceilings/doors, demo map, blueprints |
| ModSci_EngiProps  | Modular SciFi Season 1 Starter Bundle (Engineer props) | Jonathon Frederick | Fab Standard Licence | 2026-09-11 | props/set dressing for the Engineer set |
| ModSciInteriors   | Modular SciFi Season 2 Starter Bundle (Interiors) | Jonathon Frederick | Fab Standard Licence | 2026-09-11 | interior modules, second architectural set |
| StarfieldFree      | Starfield Free (star / nebula spheres, 8K Milky Way + nebula textures, BP_Starfield_* sky blueprints) | (see Fab listing) | Fab Standard Licence | 2026-09-11 | space sky for every map; use BP_Starfield_Stars_Free or the Milky Way sphere, never edit the pack |

## Rules

- Fab Standard Licence: OK to use in the shipped game; NOT OK to redistribute the source assets (hence not in git).
- Everything Airlock authors on top of a pack (level instances, materials instances, blueprints) lives outside the
  pack folders (see Content/Airlock/...) so it IS versioned.
- When a pack is updated on Fab, bump the "Claimed" date here and note anything that changed in the module grid.
- Reference: docs/airlock-asset-architecture-2026-09.md (one architectural family; grid + door standard).

## Post-import step: StarfieldFree collision

The sky sphere meshes used by `BP_Starfield_Stars_Free` / `BP_Starfield_bgNebula_Free` ship with collision. Any pawn spawned inside them is depenetrated to the sphere surface (X = -6.9e8) - warps look fine, spawns and level arrivals do not. After importing the pack, run `StaticMeshTools.remove_collisions` on those two meshes (agent script 133) and save them.

## Post-import step: Nanite

After importing the Modular SciFi packs, enable Nanite on every opaque kit static mesh (skip glass/window/holo/decal meshes - Nanite cannot render translucency). Without it the demo-hall-sized levels overflow the Virtual Shadow Map non-Nanite marking queue. `Scripts/agent/135-enable-kit-nanite.ps1` does it through Unreal MCP: `find_assets` (StaticMesh, recursive) -> `is_nanite_enabled` -> `set_nanite_enabled` -> `save_assets`, 60 meshes per call, idempotent. 2026-09-11: 177 meshes switched (Engineer 47, EngiProps 40, Interiors 90).

Exception: `SM_Door_Small_UI` stays non-Nanite (its `MI_Door_Locked` material is translucent; Nanite logs an invalid-material warning).
