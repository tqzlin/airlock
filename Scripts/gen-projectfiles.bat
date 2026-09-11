@echo off
rem Regenerate the Visual Studio solution and the VS Code workspace. Uses the engine's Build.bat so UnrealBuildTool
rem runs on the .NET runtime bundled with UE (no system .NET SDK required).
pushd "%~dp0.."
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="%CD%\Airlock.uproject" -game -progress
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="%CD%\Airlock.uproject" -game -progress -VSCode
popd