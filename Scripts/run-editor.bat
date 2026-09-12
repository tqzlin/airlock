@echo off
rem Open THIS checkout's project in Unreal Editor with the MCP server auto-started (http://127.0.0.1:8000/mcp).
rem Set AIRLOCK_MCP_PORT (e.g. 8001) to run a second editor from another checkout/worktree side by side;
rem Scripts\lib\mcp.ps1 reads the same variable (HANDOFF section 7).
rem Launched through PowerShell Start-Process (ShellExecute), so the editor does NOT inherit the caller's stdout/stderr:
rem an editor started with cmd "start" from inside an agent pipeline kept that pipeline open for hours (agent script 152).
pushd "%~dp0.."
set MCPPORT=%AIRLOCK_MCP_PORT%
if "%MCPPORT%"=="" set MCPPORT=8000
echo run-editor: %CD%\Airlock.uproject on MCP port %MCPPORT%
powershell -NoProfile -Command "Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList @('\"%CD%\Airlock.uproject\"', '-ModelContextProtocolStartServer', '-ModelContextProtocolPort=%MCPPORT%')"
popd
