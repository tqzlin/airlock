# Airlock - Claude Code project brief

Airlock is the internal codename for a spaceship MMO built in Unreal Engine 5.8 (C++). The marketed title may differ; never rename the module.

## What the game is
- Big ships are server shards. A ship's sections are joined by airlock loading screens; life-support caps population per section.
- MVP scope: a station hub plus NPC-captained, instanced ~20-minute voyages (mining, pirates, derelicts). Nothing else is in scope until the MVP loop is playable end to end.
- Gameplay is server-authoritative. Anything that affects state (position, cargo, damage, economy) is decided on the server and replicated; clients predict at most.

## Code map (Source/Airlock)
- `Airlock.h/.cpp` - module + `LogAirlock` log category.
- `Core/AirlockGameMode` - the one game mode for every level; sets pawn/controller/state classes, seamless travel on. `InitGameState` fills the section id (map name), the day seed and `bVoyageBooked`; `BeginPlay` binds every `AAirlockDoor::OnTransitionRequested` to `HandleTransitionRequested`, which `ServerTravel`s to the door's `TargetLevel` - only if a voyage is booked, or always when `TargetSectionId == "StationHub"` (which also ends the voyage).
- `Core/AirlockGameState` - replicated section state: `CurrentSectionId`, `SectionPopulationCap`, `SectionPopulation`, `DaySeed` (days since 1970, drives the three offers), `bVoyageBooked`.
- `Core/AirlockPlayerController` - Enhanced Input built in C++ (IMC + IA_Move/IA_Look/IA_Interact as default subobjects; WASD, mouse, gamepad). No input assets exist or are needed. Voyage board: `OpenVoyageBoard`/`CloseVoyageBoard` (local UI), `ServerChooseVoyage(Type)` -> `ClientVoyageBooked`. Console helpers for agent smoke tests: `AirlockBoard`, `AirlockBook <0-2>`, `AirlockWarp x y z`.
- `Core/AirlockCharacter` - first-person capsule + camera (no mesh yet). Binds the controller's actions. `Interact` line-traces `InteractReach` cm.
- `Voyage/VoyageTypes.h` - `EVoyageType`, `FVoyageSpec` (type, seed, 1200 s, ship class, captain, crew cap).
- `Voyage/VoyageSubsystem` - GameInstance subsystem (survives level travel): `MakeVoyageSpec` (pure, deterministic), `GetOfferedVoyages`, `RequestVoyage` (server-only; also sets the pending voyage), `HasPendingVoyage`/`GetPendingVoyage`/`ClearPendingVoyage`, `TodaySeed`.
- `Core/Interactable` - `IInteractable` (BlueprintNativeEvent `Interact(Pawn)` / `GetInteractPrompt`). The character's E-key line trace calls it on the first hit actor.
- `Station/VoyageBoard` - the departure board (kit wall panel on a stand; `Interact` opens the controller's board UI). Placed in StationHub on the Dock B platform as `VoyageBoard_DockB`.
- `UI/VoyageBoardWidget` - the departure list, built entirely in C++ (`RebuildWidget` constructs Border/VerticalBox/Buttons via `WidgetTree`; no UMG asset). `OnVoyageChosen(EVoyageType)` / `OnClosed`.
- `Ship/AirlockDoor` - the section boundary. Wears the kit door (soft refs to `SM_Doorway_Small_A` / `_A3` / `SM_Door_Small_A`; primitive fallback when the packs are absent). Server-owned `bIsOpen` replicated, leaf slides up `OpenDistance` (268 cm) over `OpenTime`. Two volumes: `ProximityVolume` (wide, opens the door) and `TransitionVolume` (thin, in the wall plane; server broadcasts `OnTransitionRequested(Pawn, TargetSectionId)`). Level travel not wired yet. Origin = centre of the opening on the walking surface; yaw 0 means the walking axis is X.
- `Tests/AirlockSmokeTests.cpp` - `Airlock.Voyage.SpecIsDeterministic`, `Airlock.Core.GameModeDefaults`.
- Maps: `Content/Maps/Ships/Hauler_Mk1` is a copy of the Engineer pack's `Example_Dynamic` cargo hall with all 10 doorway slots replaced by `AAirlockDoor`s (`Airlock_ToHub` at x -6980 targets StationHub; `Door_01..09` are plain doors with no target) and `PlayerStart_Ship` just inside the exit. `Content/Maps/StationHub` (editor startup + game default map) is a copy of the Interiors pack's dressed demo hall (`ModSciInteriors/Maps/Example_Dynamic`) with the pack's blueprint doors and doorway slots at both ends replaced by `AAirlockDoor`s `Airlock_Dock_A` (x -6980) and `Airlock_Dock_B` (x -4300), plus `PlayerStart_Hub` mid-hall. Rebuild recipe: `agent\queue\107-109`.

## The MVP loop as it stands (verified in PIE through the console box)
`AirlockBoard` (or E on the board) -> pick an offer -> `ServerChooseVoyage` -> `RequestVoyage` sets the pending voyage, `bVoyageBooked` replicates -> walk through `Airlock_Dock_B` (TargetLevel Hauler_Mk1) -> `ServerTravel` -> `Section Hauler_Mk1: voyage underway` -> walk through `Airlock_ToHub` -> voyage cleared, back in StationHub. Without a booking the dock door logs "no voyage booked" and stays put. `net.AllowPIESeamlessTravel=1` is set in DefaultEngine.ini `[SystemSettings]`; TransitionMap is `/Engine/Maps/Entry`.

## Content packs and the kit standard
- Purchased/claimed Fab packs live in `Content/ModSci_Engineer`, `Content/ModSci_EngiProps`, `Content/ModSciInteriors` and are **git-ignored** (licence + size); `THIRD_PARTY_ASSETS.md` lists what to re-claim from Fab. Never move, rename or save over pack assets - copy (`duplicate`) into `/Game/Airlock/...` or `/Game/Maps/...` first.
- `Docs/KitStandard.md` is the measured grid: Engineer corridor kit 360 cm pitch / 480 tall, Interiors room kit 670 cm pitch / 390 wall band under a cove (~6.5 m clear), shared door standard (360-wide slot, 166 x 275 leaf). `Docs/kit-mesh-bounds.csv` has every mesh's bounds and LOD0 tris.
- Anything Airlock authors on top of a pack (levels, level instances, material instances, blueprints) lives outside the pack folders so it is versioned.

## Machine and toolchain (laptop "stack")
- Engine: Launcher build at ``C:\Program Files\Epic Games\UE_5.8`` (5.8.2). Never modify engine files.
- Compiler: Visual Studio Build Tools 2026 with MSVC 14.50 (Epic's recommended; 14.51 also present) + Windows 11 SDK 10.0.26100 + .NET Framework 4.8 SDK. No Visual Studio IDE; VS Code is the editor of record. If UnrealBuildTool complains about the compiler version, report it rather than working around it.
- Long paths are enabled (Windows and git). Git LFS handles binary assets via ``.gitattributes``.
- Dedicated-server targets need a source-built engine; the Launcher build cannot compile them. Do not add a Server target yet.

## Build / run / test (always from the project root)
- ``Scripts\build-editor.bat`` - compile AirlockEditor. **Close the editor first (or press Ctrl+Alt+F11 inside it for Live Coding)**: an open editor holds the Live Coding lock and UBT refuses external builds.
- ``Scripts\build-game.bat`` - standalone game target.
- ``Scripts\run-editor.bat`` - open the project (the MCP server auto-starts on any launch via ``Config/DefaultEditorPerProjectUserSettings.ini``; the script also passes ``-ModelContextProtocolStartServer``).
- ``Scripts\run-tests.bat [Filter]`` - headless automation tests (default filter ``Airlock``). Inside the editor, the MCP ``AutomationTestToolset`` runs them too.
- ``Scripts\gen-projectfiles.bat`` - regenerate ``Airlock.slnx`` / VS Code workspace after adding source files (runs UBT through ``Build.bat`` so the bundled .NET is used).
- Logs: ``Saved\Logs\Airlock.log``; UBT: ``%LOCALAPPDATA%\UnrealBuildTool\Log.txt``.

## Driving the editor (Unreal MCP)
- Plugins ``ModelContextProtocol`` + ``AllToolsets`` are enabled (editor-only). ``.mcp.json`` points Claude Code at ``http://127.0.0.1:8000/mcp``.
- The server is a meta-tool surface: ``list_toolsets`` -> ``describe_toolset`` -> ``call_tool(toolset_name, tool_name, arguments)``. Useful toolsets: ``editor_toolset.toolsets.scene.SceneTools`` (load level, add/find actors, save), ``...asset.AssetTools`` (exists/duplicate/save_assets), ``...object.ObjectTools`` (list/get/set properties - always ``list_properties`` first), ``EditorToolset.EditorAppToolset`` (PIE, camera, CaptureViewport), ``EditorToolset.LogsToolset``, ``AutomationTestToolset.AutomationTestToolset`` (DiscoverTests -> RunTests -> GetTestResults), ``ConfigSettingsToolset.ConfigSettingsToolset`` (Project/Project/Maps etc.).
- Actor and asset type arguments are object paths (``/Script/Engine.PlayerStart``, ``/Script/Airlock.AirlockDoor``), not short names. Asset object refs need the ``Package.Name`` form (``/Game/X/SM_Y.SM_Y``). ``load_level`` refuses when the current level *or the target* is dirty - ``save_assets`` first (a freshly ``duplicate``d level is unsaved until you do). ``delete`` of a loaded level only works after another level is current. ``editor_toolset.toolsets.programmatic.ProgrammaticToolset.execute_tool_script`` batches hundreds of calls in one round-trip (Python ``run()`` returning a dict; any inner tool error fails the whole call, so keep property reads per-class).
- Slate automation (`SlateInspectorToolset`): `Snapshot` gives lines like `textbox [pos=..] [ref=tb2]`; the editor's console box is the `textbox` right after `text "Cmd"` - `Type(ref, cmd, submit=true)` runs console commands against the PIE world, including `UFUNCTION(Exec)` helpers on the player controller. NEVER `Click` a widget found by loose text: "Close" matched "Close Unreal Editor" and shut the editor. `Windows(list/close)` by title is the safe way to dismiss a stray window (the Message Log pops on PIE).
- A programmatic script over ~2000 actors can exceed 3 minutes; `agent\lib\mcp.ps1` now waits 900 s. If the client times out the editor still finishes the script - re-inspect before assuming.
- `Type` sometimes returns false the first time (focus); retry once.
- Renaming a C++ actor's root component orphans already-placed instances (their serialized ``RootComponent`` dangles, they snap to the origin and cannot be moved) - respawn them.
- Loopback-only, no auth - never expose the port. Experimental: if the editor misbehaves after a plugin update, disable both plugins in ``Airlock.uproject`` and report it.

## Code conventions
- Game code lives in ``Source/Airlock`` under feature folders (``Core``, ``Voyage``, ``Ship``, ``Station``, ``Tests``); split into separate modules only when compile times demand it. Include headers by module-relative path (``"Core/AirlockGameMode.h"``).
- Prefix: ``A`` actors, ``U`` objects/components, ``F`` structs, ``E`` enums, ``I`` interfaces (standard UE). Export with ``AIRLOCK_API``. Log with ``LogAirlock``.
- Prefer C++ for systems and Blueprints only for content glue (visuals, tuning, prototyping). Anything that must be deterministic or replicated is C++.
- Every new system gets an automation test (``IMPLEMENT_SIMPLE_AUTOMATION_TEST``) or a headless smoke test that ``Scripts\run-tests.bat`` can run.
- Never commit ``Binaries/``, ``Intermediate/``, ``Saved/``, ``DerivedDataCache/``, ``*.slnx`` (already ignored). Commit ``.uasset``/``.umap`` through LFS only.
- Small, single-purpose commits on ``main`` for now; describe the gameplay effect, not the file list.

## Studio layout
- Claude (Fable): reasoning, architecture, tech-lead review, this repo's C++.
- GPT-6 Astra: GUI, vision and computer-use tasks.
- Local Qwen3.8-27B (LM Studio, ``http://127.0.0.1:1234/v1``): bulk, spec'd, automatically verifiable work only.
