# Heroes of Hammerwatch 2 — Proxy DLL Mod Loader & Script Console (`winmm.dll`)

A lightweight, non-destructive proxy DLL and modding framework for **Heroes of Hammerwatch 2** (`HWR2.exe`). 

It transparently proxies Windows Multimedia (`winmm.dll`) to load directly with the game without modifying or patching any original game files on disk.

---

## What It Does

1. **In-Game Developer Script Console (`s` and `scr`)**:
   - Re-enables live AngelScript execution directly from the in-game developer console (`~`):
     - **`scr <expression>`**: Evaluates any AngelScript expression and prints the result.
     - **`s <statements>`**: Runs arbitrary AngelScript statements against the live game world.
   - Crosshair inspection: Automatically provides `sunit` bound to `GetSelectedUnit()`.
   - Mod & cheat protection: Gated behind a modded profile (`PersistentSaves::GetModded() == true`) and `e_cheats 1` to preserve legitimate unmodded saves.

2. **Universal Dynamic Hook & Line-Patching Framework**:
   - Modifies AngelScript in RAM as sections are compiled (`asCModule::AddScriptSection` and `asCModule::Build`) — **zero game files are modified on disk**.
   - Discovers mod rules from unpacked mods (`mods/*/info.xml`, `mods/*/hooks.xml`) and Steam Workshop packages (`.bin` SValue dictionaries).
   - **Dynamic Function Hooks**: Automatically injects `Hooks::Call` at function entry points and binds mod subscribers declared with `[Hook]` attributes.
   - **Non-Overwriting Line Patching**: Injects code snippets (`start`, `end`, `before`, `after`) into game methods without replacing whole `.as` files.
   - **Profile Isolation**: Automatically wraps patches in `HWR_IsModActive("ModName")` so changes deactivate when switching to an unmodded save profile or disabling the mod.

3. **Crash Guards (Profile Switch & Level Reload)**:
   - Hooks multithreaded worker lambdas (`0x7BAD0` and `0x7BB40`) with hardware Structured Exception Handling (SEH).
   - Intercepts dangling resource pointer access violations (`0xC0000005`) that vanilla HWR2 experiences during level reloads or profile switches.

4. **100% Transparent DLL Forwarding**:
   - Implements all **180 exports** of Windows Multimedia (`winmm.dll`).
   - Forwarding is implemented via naked 64-bit assembly trampolines (`ml64.exe`), preserving CPU registers and stack frames before delegating to `C:\Windows\System32\winmm.dll`.

---

## Prerequisites (What You Need)

To build this project from source, you need:

- **Windows 10 / 11 (64-bit)**
- **Visual Studio 2022** (Community, Professional, or Build Tools)
  - Workload: **Desktop development with C++** (includes MSVC `cl.exe`, `link.exe`, and MASM `ml64.exe`)
- **PowerShell 5.1+** (standard on Windows) or **Command Prompt**

> **Note:** [MinHook](https://github.com/TsudaKageyu/minhook) is already bundled in `minhook/`. No external package managers (vcpkg, conan) or internet access are required during compilation.

---

## How to Build Yourself

### 1. Build via PowerShell (Recommended)
Open PowerShell in the `root` directory and run:

```powershell
.\build.ps1
```

The script will:
1. Automatically locate your Visual Studio 2022 installation via `vswhere.exe`.
2. Initialize the x64 developer environment (`vcvarsall.bat x64`).
3. Assemble the 180 export trampolines using Microsoft Macro Assembler (`ml64.exe src\winmm_exports.asm`).
4. Compile the C/C++ source files (`dllmain.cpp`, `HookEngine.cpp`, `winmm_proxy.cpp`, and MinHook).
5. Link using `winmm.def` into `winmm.dll`.

### 2. Build via Command Prompt (CMD)
```cmd
build.bat
```

Upon a successful build, `winmm.dll` will be generated in the `root` folder.

---

## Installation & Uninstallation

### Install
Copy `winmm.dll` into the game directory next to `HWR2.exe`:

### Uninstall
Delete `winmm.dll` from the game root:

Because `HWR2.exe` was never touched, deleting `winmm.dll` completely restores the vanilla game.

---

## Console Examples

Open the in-game developer console using the tilde key (`~`):

### Expression Evaluation (`scr`)
```angelscript
scr 1 + 1
scr GetLocalPlayerRecord().name
scr GetLocalPlayerRecord().hp
scr sunit.GetDebugName()
scr g_players.length()
```

### Statement Execution (`s`)
```angelscript
s print("Hello from AngelScript!");
s GetLocalPlayerRecord().materials[0] += 50000;  // Add 50,000 gold
s sunit.Destroy();                              // Destroy unit under crosshair
```

*(Remember: `s` and `scr` require `e_cheats 1` and a modded save profile).*

---

## Mod XML Configuration Examples

Place your mod folder in `mods/<ModName>/`:

### `info.xml`
```xml
<dict>
    <string name="name">MyMod</string>
    <string name="author">AuthorName</string>
    <string name="description">Example Mod</string>

    <!-- Dynamic Hooks: Injects Hooks::Call into target methods -->
    <array name="hooks">
        <dict>
            <string name="class">Player</string>
            <string name="function">Damage</string>
            <string name="custom_call_name">OnPlayerDamage</string>
        </dict>
    </array>

    <!-- Line Patches: Injects code without overwriting files -->
    <array name="patches">
        <dict>
            <string name="class">PlayerRecord</string>
            <string name="function">RefreshModifiers</string>
            <string name="action">end</string>
            <string name="file">patches/my_patch.patch</string>
        </dict>
    </array>
</dict>
```

ex. patches/my_patch.patch:
```
// Literally
print("Hello world!");
```
---

## Project Structure

```text
root/
├── include/             # Header files (HookEngine.h, angelscript.h)
├── minhook/             # Bundled MinHook library (buffer, hook, trampoline, hde64)
├── src/
│   ├── dllmain.cpp      # DLL lifecycle, engine pattern scanners, console detours
│   ├── HookEngine.cpp   # In-memory script injector, XML/SValue parser, hook manager
│   ├── winmm_proxy.cpp  # Dynamic resolution of real System32 winmm.dll
│   └── winmm_exports.asm# MASM x64 naked export trampolines (180 functions)
├── build.ps1 / .bat     # Build scripts (VS2022 + MASM + MinHook)
├── install.ps1 / .bat   # Installs winmm.dll to game root
├── uninstall.ps1 / .bat # Removes winmm.dll from game root
├── winmm.def            # PE export definitions for all 180 winmm functions
└── README.md            # This documentation
```
