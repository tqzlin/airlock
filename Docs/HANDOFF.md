# Airlock - operating guide for any agent on stack

Everything here runs on the laptop "stack" (Windows 11). Paths are absolute on purpose so a fresh session can copy them.
Read `AGENTS.md` first (what the game is, code map, conventions); this file is the *how*.

## 1. Places
| What | Where |
|---|---|
| Repo (git, `main`) | `C:\Users\tclin\Desktop\ai_game_studio\Airlock` |
| Engine | `C:\Program Files\Epic Games\UE_5.8` (Launcher build 5.8.2 - never edit engine files) |
| Editor log | `Airlock\Saved\Logs\Airlock.log` |
| Build log | `%LOCALAPPDATA%\UnrealBuildTool\Log.txt` |
| Claude's file-queue runner (optional, Claude's own tooling) | `C:\Users\tclin\Desktop\ai_game_studio\agent\` - scripts in `done\`, logs in `results\`, captures in `results\images\`. Read them for history; you do not need the runner to work. |
| Packs (git-ignored, do not edit) | `Airlock\Content\ModSci_Engineer`, `ModSci_EngiProps`, `ModSciInteriors`, `StarfieldFree` |

## 2. Build, run, test - the cycle
1. Close the editor if it is open (it holds the Live Coding lock; UBT refuses external builds while it runs). Politely: `Get-Process UnrealEditor | % { $_.CloseMainWindow() }`, wait for exit, kill only if it is still there after 90 s (a modal dialog).
2. Build: `Scripts\build-editor.bat` (= `Build.bat AirlockEditor Win64 Development`, 5-15 s incremental). A failing build leaves the editor closed - fix and rebuild.
3. Relaunch: `Scripts\run-editor.bat`. The Unreal MCP server starts with the editor on `http://127.0.0.1:8000/mcp` (~60-90 s until the port listens). Wait ~30 s more before driving it.
4. Tests: `Scripts\run-tests.bat Airlock` (headless), or in-editor through MCP `AutomationTestToolset` (`DiscoverTests` -> `RunTestsByFilter 'Airlock'` -> `GetTestStatus` until not running -> `GetTestResults`). Four tests exist; all must stay green.
5. PIE proof: `Scripts\verify-loop.ps1` (section 4). Attach its output.

Live Coding (`Ctrl+Alt+F11` inside the editor) is untested in this project; the close/build/reopen cycle is the known-good path.

## 3. Driving the editor (Unreal MCP)
- Client: `Scripts\lib\mcp.ps1` - dot-source it in PowerShell 5.1: `Mcp-Init`, then `Mcp-Call <toolset> <tool> @{...}` returns `{ok, text, json, images}`. Toolset name constants are at the bottom of the file (`$TS_Scene`, `$TS_Asset`, `$TS_Object`, `$TS_App`, `$TS_Logs`, `$TS_Tests`, `$TS_Slate`, ...). Any MCP client works too (Streamable HTTP, no auth, loopback only).
- Batching: `editor_toolset.toolsets.programmatic.ProgrammaticToolset.execute_tool_script` runs a sandboxed Python `run()` that calls `execute_tool(name, json)`; hundreds of calls per round-trip. An inner tool error raises *and* fails the whole call, so isolate risky calls in their own script. `import unreal` is not available.
- Console commands in PIE go through Slate: `SlateInspectorToolset.Snapshot` lists widgets; the editor's Cmd box is the `textbox` right after `text "Cmd"`; `Type(ref, cmd, submit=true)` runs a console command (retry once if it returns false). Never `Click` a widget matched by loose text ("Close" once matched "Close Unreal Editor").
- Exec helpers on `AAirlockPlayerController` (usable from that console box): `AirlockBoard` (open the departure UI), `AirlockBook <0-2>` (book today's voyage of that type), `AirlockWarp x y z` (server teleport), `AirlockWhere` (log map + pawn/view location - run it before believing a dark or empty frame).
- Captures: `EditorToolset.EditorAppToolset.CaptureViewport` (editor camera; takes a transform) and `CaptureEditorImage` (whole editor window, i.e. the PIE view). Give exposure 6-10 s to settle after a spawn or warp before capturing.
- `StartPIE` spawns wherever the editor camera is unless you pass `startTransform`; for a real player view pass the PlayerStart (hub: `-5640 1560 118`).
- The full lesson list (property quirks, level load rules, what closes the editor) is in `AGENTS.md` "Driving the editor".

## 4. The loop proof (`Scripts\verify-loop.ps1`)
Requires the editor open on StationHub with the MCP port up. It: starts PIE at the hub PlayerStart, `AirlockWhere`, warps to Dock B, `AirlockBook 0`, crosses the threshold (`-4265 1560 116`), waits for `Section Hauler_Mk1`, `AirlockWhere` (expects `X=-6660`), warps to the ship exit threshold (`-7015 1560 116`), waits for the return travel, `AirlockWhere` (expects `X=-5640`), stops PIE. Exit code 0 only if both arrivals landed at their PlayerStarts and `Voyage ... complete` was logged. Captures land in `Saved\AgentCaptures\`.

Useful coordinates (StationHub unless noted): hub PlayerStart `(-5640, 1560)` lower deck; Dock B door `(-4300, 1560, 20)` yaw 180, threshold `(-4265, 1560, 116)`, approach `(-4700, 1560, 116)`; departure board `(-4450, 1330)`; Dock A (sealed) `(-6980, 1560, 20)`. Hauler_Mk1: PlayerStart `(-6660, 1560)`, exit door `Airlock_ToHub` `(-6980, 1560, 20)` yaw 0, threshold `(-7015, 1560, 116)`.

## 5. Rules that bite
- Never save over pack assets (duplicate into `/Game/Maps` or `/Game/Airlock`). The two documented exceptions (Nanite, StarfieldFree collision) are already applied; see `THIRD_PARTY_ASSETS.md`.
- Server-authoritative gameplay: state changes on the server, replicate, clients predict at most. UI is local.
- C++ for systems; no UMG/Blueprint assets so far (widgets are built in C++ in `RebuildWidget`). Keep it that way unless a brief says otherwise.
- `load_level` refuses when the current or target level is dirty - `save_assets` first. Don't leave maps dirty at the end of a task.
- If the editor misbehaves (stuck dialog, MCP not answering), close it and relaunch rather than clicking around. Report what happened.

## 6. Reporting
End every task with: behaviour changed; files; test names added/changed and their result; the `verify-loop.ps1` tail and any extra `LogAirlock` lines; captures; open questions; anything touched outside the brief. Put it in the PR / final message, and keep the branch pushed or at least committed locally.
