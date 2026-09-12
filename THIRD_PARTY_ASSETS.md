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