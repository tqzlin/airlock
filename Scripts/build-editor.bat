@echo off
rem Compile the AirlockEditor target (Development). Extra args are passed through to UnrealBuildTool.
pushd "%~dp0.."
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AirlockEditor Win64 Development -Project="%CD%\Airlock.uproject" -WaitMutex %*
set RC=%ERRORLEVEL%
popd
exit /b %RC%