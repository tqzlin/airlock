@echo off
rem Open the project in Unreal Editor with the MCP server auto-started (http://127.0.0.1:8000/mcp).
pushd "%~dp0.."
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%CD%\Airlock.uproject" -ModelContextProtocolStartServer %*
popd