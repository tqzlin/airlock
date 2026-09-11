@echo off
rem Regenerate Visual Studio solution + VS Code workspace for the project.
pushd "%~dp0.."
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -projectfiles -project="%CD%\Airlock.uproject" -game -progress
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -projectfiles -project="%CD%\Airlock.uproject" -game -progress -VSCode
popd