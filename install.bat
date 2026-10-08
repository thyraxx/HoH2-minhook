@echo off
setlocal

if not exist winmm.dll (
    echo [*] winmm.dll not found, building first...
    call build.bat
)

if exist winmm.dll (
    copy /y winmm.dll ..\winmm.dll >nul
    echo [SUCCESS] Installed winmm.dll to game root.
    echo [INFO] Launch the game normally. Console commands 's' and 'scr' are now active!
) else (
    echo [ERROR] Installation failed: winmm.dll was not found.
    exit /b 1
)
