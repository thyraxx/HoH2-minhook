$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host " Heroes of Hammerwatch 2 - Running HookEngine Tests" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan

# 1. Run Python Tests
Write-Host "`n[*] Executing Python Test Suite..." -ForegroundColor Yellow
$py = Get-Command python -ErrorAction SilentlyContinue
if ($py) {
    & python "test_hook_engine.py"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Python tests failed!"
    }
} else {
    Write-Host "[!] Python not found in PATH, skipping Python runner." -ForegroundColor DarkYellow
}

# 2. Locate MSVC via vswhere
Write-Host "`n[*] Locating MSVC compiler..." -ForegroundColor Yellow
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $vsWhere)) {
    Write-Error "vswhere.exe not found at $vsWhere"
}

$vsPath = & $vsWhere -latest -products * -property installationPath
if (!$vsPath -or !(Test-Path $vsPath)) {
    Write-Error "Visual Studio installation not found."
}

$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
if (!(Test-Path $vcvars)) {
    Write-Error "vcvarsall.bat not found at $vcvars"
}

# 3. Compile C++ Test Harness
Write-Host "[*] Compiling test_hook_engine.cpp with MSVC..." -ForegroundColor Cyan
$compileFlags = "/nologo /O2 /EHsc /MD /W3 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG"
$includes = "/I..\include /I..\minhook\include"
$srcFiles = "test_hook_engine.cpp ..\src\HookEngine.cpp"
$linkFlags = "/link /OUT:test_hook_engine.exe user32.lib kernel32.lib"

$cmd = "call `"$vcvars`" x64 && cl.exe $compileFlags $includes $srcFiles $linkFlags"
cmd.exe /c $cmd

if (!(Test-Path "test_hook_engine.exe")) {
    Write-Error "Compilation failed: test_hook_engine.exe was not created."
}

# 4. Execute C++ Test Harness
Write-Host "`n[*] Executing C++ Test Binary..." -ForegroundColor Yellow
& ".\test_hook_engine.exe"
$cppExit = $LASTEXITCODE

# 5. Clean up build artifacts
Remove-Item -Force -ErrorAction SilentlyContinue *.obj, test_hook_engine.exe

if ($cppExit -ne 0) {
    Write-Error "C++ tests failed with exit code $cppExit"
}

Write-Host "`n[SUCCESS] All test suites passed cleanly!" -ForegroundColor Green
