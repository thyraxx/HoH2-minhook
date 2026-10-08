$target = "..\winmm.dll"
$src = "winmm.dll"

if (!(Test-Path $src)) {
    Write-Host "[*] winmm.dll not found, building first..." -ForegroundColor Yellow
    & .\build.ps1
}

if (Test-Path $src) {
    Copy-Item -Force $src $target
    Write-Host "[SUCCESS] Installed winmm.dll to game root ($target)" -ForegroundColor Green
    Write-Host "[INFO] Launch the game normally. Console commands 's <statements>' and 'scr <expression>' are now active in the tilde (~) console!" -ForegroundColor Cyan
} else {
    Write-Error "Failed to install: winmm.dll missing."
}
