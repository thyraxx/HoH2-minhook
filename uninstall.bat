@echo off
setlocal

if exist ..\winmm.dll (
    del /f /q ..\winmm.dll
    echo [SUCCESS] Uninstalled winmm.dll from game root.
) else (
    echo [INFO] winmm.dll is not installed in game root.
)
