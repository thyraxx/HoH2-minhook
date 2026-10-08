$target = "..\winmm.dll"

if (Test-Path $target) {
    Remove-Item -Force $target
    Write-Host "[SUCCESS] Uninstalled winmm.dll from game root." -ForegroundColor Green
} else {
    Write-Host "[INFO] winmm.dll was not present in game root." -ForegroundColor Yellow
}
