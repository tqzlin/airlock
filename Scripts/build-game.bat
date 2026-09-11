@echo off
rem Compile the standalone Airlock game target (Development). Use this before packaging.
pushd "%~dp0.."
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" Airlock Win64 Development -Project="%CD%\Airlock.uproject" -WaitMutex %*
set RC=%ERRORLEVEL%
popd
exit /b %RC%