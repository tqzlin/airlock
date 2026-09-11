# Airlock - Claude Code project brief

Airlock is the internal codename for a spaceship MMO built in Unreal Engine 5.8 (C++). The marketed title may differ; never rename the module.

## What the game is
- Big ships are server shards. A ship's sections are joined by airlock loading screens; life-support caps population per section.
- MVP scope: a station hub plus NPC-captained, instanced ~20-minute voyages (mining, pirates, derelicts). Nothing else is in scope until the MVP loop is playable end to end.
- Gameplay is server-authoritative. Anything that affects state (position, cargo, damage, economy) is decided on the server and replicated; clients predict at most.

## Code map (Source/Airlock)
- `Airlock.h/.cpp` - module + `LogAirlock` log category.
- `Core/AirlockGameMode` - the one game mode for every level; sets pawn/controller/state classes, seamless travel on.
- `Core/AirlockGameState` - replicated section state: `CurrentSectionId`, `SectionPopulationCap`, `SectionPopulation`.
- `Core/AirlockPlayerController` - Enhanced Input built in C++ (IMC + IA_Move/IA_Look/IA_Interact as default subobjects; WASD, mouse, gamepad). No input assets exist or are needed.
- `Core/AirlockCharacter` - first-person capsule + camera (no mesh yet). Binds the controller's actions. `Interact` line-traces `InteractReach` cm.
- `Voyage/VoyageTypes.h` - `EVoyageType`, `FVoyageSpec` (type, seed, 1200 s, ship class, captain, crew cap).
- `Voyage/VoyageSubsystem` - GameInstance subsystem: `MakeVoyageSpec` (pure, deterministic), `GetOfferedVoyages`, `RequestVoyage` (server-only stub).
- `Ship/AirlockDoor` - trigger box + placeholder frame; on server overlap logs and broadcasts `OnTransitionRequested(Pawn, TargetSectionId)`. Level travel not wired yet.
- `Tests/AirlockSmokeTests.cpp` - `Airlock.Voyage.SpecIsDeterministic`, `Airlock.Core.GameModeDefaults`.
- Maps: `Content/Maps/StationHub` (editor startup + game default map; duplicated from the engine Basic template).

## Machine and toolchain (laptop "stack")
- Engine: Launcher build at ``C:\Program Files\Epic Games\UE_5.8`` (5.8.2). Never modify engine files.
- Compiler: Visual Studio Build Tools 2026 with MSVC 14.50 (Epic's recommended; 14.51 also present) + Windows 11 SDK 10.0.26100 + .NET Framework 4.8 SDK. No Visual Studio IDE; VS Code is the editor of record. If UnrealBuildTool complains about the compiler version, report it rather than working around it.
- Long paths are enabled (Windows and git). Git LFS handles binary assets via ``.gitattributes``.
- Dedicated-server targets need a source-built engine; the Launcher build cannot compile them. Do not add a Server target yet.

## Build / run / test (always from the project root)
- ``Scripts\build-editor.bat`` - compile AirlockEditor. **Close the editor first (or press Ctrl+Alt+F11 inside it for Live Coding)**: an open editor holds the Live Coding lock and UBT refuses external builds.
- ``Scripts\build-game.bat`` - standalone game target.
- ``Scripts\run-editor.bat`` - open the project with the MCP server auto-started.
- ``Scripts\run-tests.bat [Filter]`` - headless automation tests (default filter ``Airlock``). Inside the editor, the MCP ``AutomationTestToolset`` runs them too.
- ``Scripts\gen-projectfiles.bat`` - regenerate ``Airlock.slnx`` / VS Code workspace after adding source files (runs UBT through ``Build.bat`` so the bundled .NET is used).
- Logs: ``Saved\Logs\Airlock.log``; UBT: ``%LOCALAPPDATA%\UnrealBuildTool\Log.txt``.

## Driving the editor (Unreal MCP)
- Plugins ``ModelContextProtocol`` + ``AllToolsets`` are enabled (editor-only). ``.mcp.json`` points Claude Code at ``http://127.0.0.1:8000/mcp``.
- The server is a meta-tool surface: ``list_toolsets`` -> ``describe_toolset`` -> ``call_tool(toolset_name, tool_name, arguments)``. Useful toolsets: ``editor_toolset.toolsets.scene.SceneTools`` (load level, add/find actors, save), ``...asset.AssetTools`` (exists/duplicate/save_assets), ``...object.ObjectTools`` (list/get/set properties - always ``list_properties`` first), ``EditorToolset.EditorAppToolset`` (PIE, camera, CaptureViewport), ``EditorToolset.LogsToolset``, ``AutomationTestToolset.AutomationTestToolset`` (DiscoverTests -> RunTests -> GetTestResults), ``ConfigSettingsToolset.ConfigSettingsToolset`` (Project/Project/Maps etc.).
- Actor and asset type arguments are object paths (``/Script/Engine.PlayerStart``, ``/Script/Airlock.AirlockDoor``), not short names. ``load_level`` refuses to leave a dirty level - save first.
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
