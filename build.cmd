@echo off
setlocal
set "PROJECT_ROOT=%~dp0"
set "PLATFORMIO_CORE_DIR=%PROJECT_ROOT%.platformio"
"%PLATFORMIO_CORE_DIR%\python3\Scripts\pio.exe" run -e jc2432w328c
exit /b %ERRORLEVEL%

