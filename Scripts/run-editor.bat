@echo off
rem Open the project in Unreal Editor with the MCP server auto-started (http://127.0.0.1:8000/mcp).
rem Launched through PowerShell Start-Process (ShellExecute), so the editor does NOT inherit the caller's stdout/stderr:
rem an editor started with cmd "start" from inside an agent pipeline kept that pipeline open for hours (agent script 152).
pushd "%~dp0.."
powershell -NoProfile -Command "Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList @('\"%CD%\Airlock.uproject\"', '-ModelContextProtocolStartServer')"
popd
