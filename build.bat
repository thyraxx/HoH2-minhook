@echo off
setlocal

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe not found at "%VSWHERE%"
    exit /b 1
)

for /f "usebackq delims=" %%i in (`call "%%ProgramFiles(x86)%%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not exist "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo [ERROR] vcvarsall.bat not found in %VS_PATH%
    exit /b 1
)

echo [*] Initializing Visual Studio build tools...
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64

echo [*] Assembling exports...
ml64.exe /c /Fo winmm_exports.obj src\winmm_exports.asm
if errorlevel 1 (
    echo [ERROR] ml64 assembly failed.
    exit /b 1
)

echo [*] Compiling winmm.dll...
cl.exe /nologo /O2 /EHsc /MD /W3 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /Iinclude /Iminhook\include /Iminhook\src /Iminhook\src\hde src\dllmain.cpp src\winmm_proxy.cpp src\HookEngine.cpp minhook\src\buffer.c minhook\src\hook.c minhook\src\trampoline.c minhook\src\hde\hde64.c /link /DLL /DEF:winmm.def winmm_exports.obj /OUT:winmm.dll user32.lib kernel32.lib

if exist winmm.dll (
    echo [SUCCESS] winmm.dll successfully built with ALL 180 exports!
    del /f /q *.obj *.exp *.lib 2>nul
) else (
    echo [ERROR] Build failed.
    exit /b 1
)
