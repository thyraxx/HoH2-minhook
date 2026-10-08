@echo off
setlocal

echo ========================================================
echo  Heroes of Hammerwatch 2 - Running HookEngine Tests
echo ========================================================

cd /d "%~dp0"

echo.
echo [*] Executing Python Test Suite...
python --version >nul 2>&1
if %errorlevel% equ 0 (
    python test_hook_engine.py
    if errorlevel 1 (
        echo [ERROR] Python tests failed!
        exit /b 1
    )
) else (
    echo [!] Python not found, skipping Python runner.
)

echo.
echo [*] Locating MSVC compiler...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe not found at "%VSWHERE%"
    exit /b 1
)

for /f "usebackq delims=" %%i in (`call "%%ProgramFiles(x86)%%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do (
    set "VSINSTALL=%%i"
)

if not exist "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo [ERROR] vcvarsall.bat not found in %VSINSTALL%
    exit /b 1
)

echo [*] Initializing MSVC and compiling test harness...
call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

cl.exe /nologo /O2 /EHsc /MD /W3 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /I..\include test_hook_engine.cpp ..\src\HookEngine.cpp /link /OUT:test_hook_engine.exe user32.lib kernel32.lib
if not exist test_hook_engine.exe (
    echo [ERROR] Failed to compile test_hook_engine.exe!
    exit /b 1
)

echo.
echo [*] Executing C++ Test Binary...
test_hook_engine.exe
set TEST_RESULT=%errorlevel%

del /f /q *.obj test_hook_engine.exe >nul 2>&1

if %TEST_RESULT% neq 0 (
    echo [ERROR] C++ tests failed with exit code %TEST_RESULT%!
    exit /b %TEST_RESULT%
)

echo.
echo [SUCCESS] All test suites passed cleanly!
exit /b 0
