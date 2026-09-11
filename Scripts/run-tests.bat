@echo off
rem Run the project's automation tests headless. Filter with an argument, e.g. run-tests.bat Airlock.
pushd "%~dp0.."
set FILTER=%1
if "%FILTER%"=="" set FILTER=Airlock
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Airlock.uproject" -ExecCmds="Automation RunTests %FILTER%; Quit" -unattended -nopause -nullrhi -log -ReportExportPath="%CD%\Saved\Automation\Report"
set RC=%ERRORLEVEL%
popd
exit /b %RC%