# Airlock - Claude Code project brief

Airlock is the internal codename for a spaceship MMO built in Unreal Engine 5.8 (C++). The marketed title may differ; never rename the module.

## What the game is
- Big ships are server shards. A ship's sections are joined by airlock loading screens; life-support caps population per section.
- MVP scope: a station hub plus NPC-captained, instanced ~20-minute voyages (mining, pirates, derelicts). Nothing else is in scope until the MVP loop is playable end to end.
- Gameplay is server-authoritative. Anything that affects state (position, cargo, damage, economy) is decided on the server and replicated; clients predict at most.

## Machine and toolchain (laptop "stack")
- Engine: installed (Launcher) build at `C:\Program Files\Epic Games\UE_5.8` (5.8.2). Do not modify engine files; engine-level changes need a source build and are a project decision.
- Compiler: Visual Studio Build Tools 2026 (MSVC 14.51, Windows 11 SDK 10.0.26100). There is no Visual Studio IDE installed; the editor of record is VS Code. Epic's recommended toolchain for 5.8 is MSVC 14.50 - if UnrealBuildTool complains about the compiler version, say so instead of working around it.
- Long paths are enabled (Windows and git). Git LFS is configured for binary assets via `.gitattributes`.
- Dedicated-server targets (`AirlockServer.Target.cs`) require a source-built engine; the Launcher build cannot compile them. Open item - do not add a Server target yet.

## Build / run / test (always from the project root)
- `Scripts\build-editor.bat`  - compile the AirlockEditor target (what the editor loads). Run this after any C++ change.
- `Scripts\build-game.bat`    - compile the standalone game target.
- `Scripts\run-editor.bat`    - open the project in Unreal Editor with the MCP server auto-started.
- `Scripts\run-tests.bat [Filter]` - headless automation tests (`UnrealEditor-Cmd -ExecCmds="Automation RunTests ..."`).
- `Scripts\gen-projectfiles.bat` - regenerate the .sln and VS Code workspace after adding/removing source files or modules.
- Engine and project logs: `Saved\Logs\Airlock.log`. Build logs print to the console; UnrealBuildTool logs are under `%LOCALAPPDATA%\UnrealBuildTool`.

## Driving the editor (Unreal MCP)
- The project enables the experimental `ModelContextProtocol` ("Unreal MCP") and `AllToolsets` plugins. `.mcp.json` in this folder points Claude Code at `http://127.0.0.1:8000/mcp`.
- The server starts with the editor when launched via `Scripts\run-editor.bat`; otherwise run `ModelContextProtocol.StartServer` in the editor console. Loopback-only, no auth - never expose the port.
- Experimental: if the editor misbehaves after a plugin update, disable both plugins in `Airlock.uproject` and report it.

## Code conventions
- Game code lives in `Source/Airlock`. Add new gameplay systems as folders under it (`Source/Airlock/Ship`, `Source/Airlock/Voyage`, `Source/Airlock/Station` ...); split into separate modules only when compile times demand it.
- Prefix: `A` actors, `U` objects/components, `F` structs, `E` enums, `I` interfaces (standard UE). Export with `AIRLOCK_API`.
- Prefer C++ for systems and Blueprints only for content glue (visuals, tuning, prototyping). Anything that must be deterministic or replicated is C++.
- Every new system gets an automation test (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`) or a headless smoke test that `Scripts\run-tests.bat` can run.
- Never commit `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` (already ignored). Commit `.uasset`/`.umap` through LFS only.
- Small, single-purpose commits on `main` for now; describe the gameplay effect, not the file list.

## Studio layout
- Claude (Fable): reasoning, architecture, tech-lead review, this repo's C++.
- GPT-6 Astra: GUI, vision and computer-use tasks.
- Local Qwen3.8-27B (LM Studio, `http://127.0.0.1:1234/v1`): bulk, spec'd, automatically verifiable work only.