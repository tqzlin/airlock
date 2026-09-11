# Airlock kit standard (Modular SciFi family)

Measured 2026-09-11 from the imported packs via Unreal MCP (`StaticMeshTools.get_bounds`, local-space AABB in cm).
Full table: `Docs/kit-mesh-bounds.csv` (153 meshes, LOD0 triangle counts included).

## The one architectural family

Everything the player walks through is built from **Modular SciFi (Jonathon Frederick)**:

| Set | Folder | Role in Airlock | Module pitch | Wall height |
|---|---|---|---|---|
| Season 1 – Engineer | `Content/ModSci_Engineer` | corridors, junctions, service spaces, ship interiors | **360 cm** along the run, corridor 480 cm wide (2 × 240 floor tiles) | **480 cm** |
| Season 2 – Interiors | `Content/ModSciInteriors` | rooms: bridge, cargo, quarters, hub halls | **670 cm** (610 main wall + 2 × 30 end caps), half-module 335 | **390 cm** (WVA/WVD), 490 (WVB), 430–440 (tall variants) |
| Season 1 – Engineer props | `Content/ModSci_EngiProps` | set dressing only | – | – |

Both sets ship the **same door pieces**, so a single door standard covers every transition in the game.

## Door standard (shared S1 / S2)

| Piece | Size (X depth × Y width × Z height) | Notes |
|---|---|---|
| `SM_Doorway_Small_A` | 80 × **360** × **390** | wall-slot piece: fills one 360 module, pivot at one end on the floor, body extends +Y |
| `SM_Doorway_Small_A3` | 16 × 215 × 314 | visible frame / trim around the opening |
| `SM_Door_Small_A` | 23 × **166 × 275** | the sliding leaf (z 19–294); glass insert `_A_Glass`, keypad `_UI` |
| `Door_A_BP` | – | pack blueprint: Trigger box → Timeline lerps the leaf between `DoorClosed` and `DoorOpen`, plays `SteamDoor` FX. **Not replicated** – visual reference only |

Clear opening ≈ 166 wide × 275 tall. Player capsule is 84 wide × 192 tall, so two players can pass shoulder to shoulder with margin.

**Airlock rule:** every section boundary is an `AAirlockDoor` (C++, replicated) wearing these meshes. The pack's `Door_A_BP`
is never placed in a shipping map; its open/closed offsets are copied into `AAirlockDoor` so the look is identical.
`AAirlockDoor` trigger volume: 440 × 160 × 300 cm centred on the opening (extent 220 × 80 × 150, z-centre 150) — matches the pack door's feel.

## Pivot and axis conventions (as authored in the packs)

- Engineer walls (`SM_Wall_HB_*`, `SM_Intersection_Wall*`): X = along the wall, run **−360 → 0**; thickness on −Y (41.5); Z 0 → 480. Place at the module's +X corner, wall face on the Y = 0 line.
- Engineer floor `SM_Floor_A1`: −360..0 × −240..0, top at Z = 0 (3.2 thick below). Ceiling `SM_Ceiling_HB_A`: −360..0 × −480..0, underside at Z = 0 → place at Z = 480.
- Interiors main walls (`SM_WVA_MainWall`, `SM_WVD_Wall_C`, …): run **−610 → 0** in X, Z 0 → 390; end caps `SM_WVD_Wall_A` / `SM_Wall_Divider_A_End` are 30 wide → 670 per module.
- Interiors floors: `SM_Floor_C` 670 × 900 × 60 (0..670, −450..450, top Z = 60); `SM_Floor_Main` half-module 335 × 436 × 33.
- Doorway pieces run along **+Y** (0 → 360) instead of −X; rotate yaw 90° to drop one into an Engineer wall run.
- Grid snapping while greyboxing: **10 cm** translate, 90° rotate. Level-instance origins sit on the 360 / 670 lattice.

## Perf budget (LOD0, from the CSV)

Structural pieces are 1–5 k tris; the heaviest are `SM_Rail_Pad` 23 k, `SM_Ceiling_Main_B` 19 k, `SM_Floor_B` 18 k,
`SM_Wall_Puffy_A` 18 k. Nothing needs Nanite for the MVP; revisit if a hub section passes ~2 M tris in view.

## Section library (M2 targets)

| Level instance | Built from | Footprint |
|---|---|---|
| `LI_Corridor_S1_x1` | Floor_A1 ×2, Wall_HB_A ×2, Ceiling_HB_A | 360 × 480 × 480 |
| `LI_Corridor_S1_x3` | 3 × the above, wire/pipe dressing | 1080 × 480 × 480 |
| `LI_Junction_S1` | Intersection_Floor/Ceiling/Wall ×4, Beam | 360 × 360 core + 4 stubs |
| `LI_Airlock_Std` | Doorway_Small_A + Door_Small_A (+Glass/UI) + 2 × Wall_HB_A, one `AAirlockDoor` | one 360 module |
| `LI_Room_S2_M` | Floor_C, WVA_MainWall ×2, WVD_Wall_C ×2 + caps, Ceiling_Main ×2 | 670 × 900 × 390 |
| `LI_Room_S2_L` | 2 × 2 modules, Wall_Divider_A partitions | 1340 × 1800 × 390 |

## Verified against the packs' own demo maps (`Example_Stationary`, 748 + 507 placed meshes)

Survey: `agent\results\102-demo-survey.json` (every StaticMeshActor's mesh, location, rotation). Findings, with
`z0` = the walking surface:

- **Engineer pitch = 360 confirmed.** `SM_Floor_A1`, `SM_Ceiling_HB_A`, `SM_GlassFloor_A/_Divider`, `SM_Intersection_Beam`,
  `SM_Floor_A2`, `SM_Floor_A_Pipes` all repeat at exactly 360 along the run. Junction cores are spaced 1580 apart in the
  demo (author's layout choice, not a constraint).
- Engineer vertical stack: walls and `Floor_A1` at `z0`; wall top `z0+480`; `Ceiling_HB_A` actor at **`z0+510`**
  (its 31.8 cm body hangs below the origin, so the underside meets the wall top). The demo's main walkway is the
  glass-floor system instead: `GlassFloor_A` at `z0-123` (top at `z0`) with walls dropped to `z0-90` and the gap
  hidden by `Floor_A2` skirting. For Airlock corridors use the simpler `Floor_A1` stack; glass floors only in showpiece rooms.
- **Interiors pitch = 670 confirmed** (335 half-module, 1340 double): `Floor_Main_Side*`, `Ceiling_Main_C/D/E/F`,
  `Wall_Divider_A`, `PowerCells_*`, `Floor_Below`, `WVD_Wall_A/B`, `WVA_MainWall` all step by 670 / 335 / 1340.
- Interiors vertical stack (tiered cove ceiling): main walls at `z0`, 390 tall; `Ceiling_Main_F` at `z0+390`,
  `Ceiling_Main_E` at `z0+430` (rises to `z0+710`), `Ceiling_Main` / `_C` / `_D` at **`z0+710`** with underside ≈ `z0+650`.
  So an S2 room is ~6.5 m clear, not 3.9 — the 390 is the wall band, the rest is cove. `Floor_Main_Side` at `z0-60`
  (60 tall), `Floor_Main` at `z0-33`.
- `SM_Doorway_Small_A` and its `_A3` frame sit directly on the walking surface (`z0`) in both sets.
- `Door_A_BP` internals: leaf `SM_Door_Small_A` slides **up 268 cm** (`DoorClosed` z 0 → `DoorOpen` z 268); trigger box
  is 447 × 146 × 540 cm centred 151 cm up (extent 32 × scale 6.98 / 2.29 / 8.43), i.e. it fires ~2.2 m either side of
  the door along the walking axis; two `SteamDoor` emitters at y ±75, z 35; keypad = 18 tiny decals on the frame.
  → `AAirlockDoor`: same meshes, leaf lift 268 cm over ~0.6 s, server-authoritative open state replicated to clients,
  trigger extent (220, 80, 150) at z 150.
