$ErrorActionPreference = "Stop"

Set-Location (Split-Path -Parent $MyInvocation.MyCommand.Path)

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

Write-Host "[*] Initializing MSVC environment via $vcvars..." -ForegroundColor Cyan

$srcFiles = @(
    "src\dllmain.cpp",
    "src\HookEngine.cpp",
    "src\winmm_proxy.cpp",
    "minhook\src\buffer.c",
    "minhook\src\hook.c",
    "minhook\src\trampoline.c",
    "minhook\src\hde\hde64.c",
    "winmm_exports.obj"
) -join " "

$includes = "/Iinclude /Iminhook\include /Iminhook\src /Iminhook\src\hde"
$compileFlags = "/nologo /O2 /EHsc /MD /W3 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG"
$linkFlags = "/link /DLL /DEF:winmm.def /OUT:winmm.dll user32.lib kernel32.lib"

$cmd = "call `"$vcvars`" x64 && ml64.exe /c src\winmm_exports.asm && cl.exe $compileFlags $includes $srcFiles $linkFlags"

Write-Host "[*] Assembling exports and compiling winmm.dll..." -ForegroundColor Cyan
cmd.exe /c $cmd

if (Test-Path "winmm.dll") {
    Write-Host "[SUCCESS] winmm.dll successfully built with ALL 180 exports!" -ForegroundColor Green
    Remove-Item -Force -ErrorAction SilentlyContinue *.obj, *.exp, *.lib
} else {
    Write-Error "Build failed: winmm.dll was not created."
}
