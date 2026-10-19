@echo off
setlocal
if "%~2"=="" (
  echo Usage: prepare-release.cmd OWNER/REPOSITORY VERSION
  exit /b 2
)
set "PROJECT_ROOT=%~dp0"
"%PROJECT_ROOT%.platformio\python3\python.exe" "%PROJECT_ROOT%tools\prepare_release.py" "%~1" "%~2"
exit /b %ERRORLEVEL%

