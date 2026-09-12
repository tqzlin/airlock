@echo off
rem Compile the AirlockEditor target (Development). Extra args are passed through to UnrealBuildTool.
rem -NoHotReloadFromIDE: skip UBT's hot-reload / Live Coding detection. That check is keyed on the shared engine
rem UnrealEditor.exe, so without it a build here is refused while ANY UE_5.8 editor is open - including another
rem checkout's editor (worktrees, HANDOFF section 7). This checkout's OWN editor must still be closed first
rem (Scripts\close-editor.ps1): its loaded UnrealEditor-Airlock.dll cannot be relinked (LNK1104).
pushd "%~dp0.."
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AirlockEditor Win64 Development -Project="%CD%\Airlock.uproject" -WaitMutex -NoHotReloadFromIDE %*
set RC=%ERRORLEVEL%
popd
exit /b %RC%
