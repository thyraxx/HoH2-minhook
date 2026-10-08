# Heroes of Hammerwatch 2 - Console Script Execution ("s" & "scr")
## Technical Architecture, Reverse Engineering Findings & Implementation Guide

This document is mirrored from the primary reversing artifacts. For complete, in-depth reports with assembly walkthroughs, minidump analysis, mermaid diagrams, and technical breakdowns, please refer to:
- [Console Scripting ('s' & 'scr') Architecture](file:///e:/SteamLibrary/steamapps/common/Heroes%20of%20Hammerwatch%202%20AGY/patch_scr/reversing_console_s_and_scr.md)
- [Profile Switch Crash (0x7BAF1) Reversing & Crash Guard Fix](file:///e:/SteamLibrary/steamapps/common/Heroes%20of%20Hammerwatch%202%20AGY/patch_scr/reversing_profile_switch_crash_and_fix.md)

### Quick Summary of Key Architecture:
1. **180-Export Proxy DLL (`winmm.dll`)**: Intercepts `Console::ExecuteLine` (`RVA 0x1F44C0`) and forwards all original 180 Windows Multimedia APIs via MASM assembly trampolines to `C:\Windows\System32\winmm.dll`.
2. **Update Resilience (Pattern Scanning & Export Resolution)**:
   - `asCreateScriptEngine`: Dynamically resolved via Windows `GetProcAddress` on the game module (exported by name in PE header).
   - `Console::ExecuteLine`: Scans `.text` at runtime for the unique 36-byte machine code signature `48 89 5C 24 18 48 89 54 24 10 55 56 57 41 54 41 55...` (verified identical between Oct 2025 beta and Aug 2026 release).
   - `Console::Print`: Scans `.text` for the unique 28-byte signature `48 89 54 24 10 4C 89 44 24 18 4C 89 4C 24 20...`.
3. **Vtable ABI Compatibility**:
   - `asIScriptEngine::GetModule`: **Slot 47** (`+0x178`) in HWR2.exe (not Slot 49 as in generic SDK headers).
   - `asIScriptContext::GetExceptionLineNumber`: **Slot 33** (`+0x108`) with `(nullptr, nullptr)`.
   - `asIScriptContext::GetExceptionString`: **Slot 35** (`+0x118`).
4. **Multi-Engine Active World Resolution**:
   `HWR2.exe` spawns 5 AngelScript engines (including an asset preload engine). The live gameplay engine is dynamically resolved via:
   ```text
   Console* -> GameEngine (+0x8) -> ActiveWorld (+0x98) -> ScriptSystem (+0x60) -> asIScriptEngine* (+0x158)
   ```
5. **Developer Helper Functions**:
   - `_cgrab(...)`: Polymorphic formatting function for primitives, strings, and object references.
   - `sunit`: Automatically bound to `GetSelectedUnit()` for instant crosshair target inspection.
6. **Universal Dynamic Script Hooking & Line Patching Loader**:
   - Zero Disk Modification: All modifications occur in RAM during script compilation (`AddScriptSection` and `asCModule::Build`).
   - Package & Config Discovery:
     - Unpacked mods: `mods/*/info.xml`, `mods/*/hooks.xml`, `mods/*/hooks.json`.
     - Packed `.bin` local packages: `mods/*.bin` and `mods/*/*.bin`.
     - Steam Workshop packages: `workshop/content/<AppID>/*/*.bin` (decodes `HW2R`/`HWRR` binary SValue dictionaries).
   - In `info.xml`, `<array name="hooks">` and `<array name="patches">` reside inside the root `<dict>`:
     ```xml
     <dict>
         <string name="name">MyMod</string>
         <array name="hooks">
             <dict>
                 <string name="class">Player</string>
                 <string name="function">Damage</string>
                 <string name="custom_call_name">OnPlayerDamage</string>
             </dict>
         </array>
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
   - **Feature A (Dynamic Hooking)**: Injects `if (HwrSaves::IsModded()) Hooks::Call("EventName", @this, ...);` into the target function entry point. Modders write `[Hook]` subscribers in their mod scripts; the loader strips metadata tags in-flight to prevent compiler token errors and connects subscribers to the engine's hook table on post-build.
   - **Feature B (Non-Overwriting Line Patching)**: Supports actions `start`, `end`, `before`, and `after`. Code can be provided via external snippet files (`.patch`, `.inc`), inline `<string name="code">`, or `<string name="code"><![CDATA[ ... ]]></string>`.
   - **Profile Awareness & Isolation**: Injects `bool HWR_IsModActive(const string &in modId)` into the primary game script module. Every injected line patch is wrapped in `if (HWR_IsModActive("ModName")) { ... }` so modifications instantly deactivate when the player switches to an unmodded profile or disables the mod.
   - **MetaScript Boundary**: Strictly ignores `.mas` sections, ensuring `HWR_IsModActive` and game helpers are only injected into main game `.as` sections.
   - **Snippet Neutralization**: Automatically detects any registered patch files (or files ending in `.patch`/`.inc`) entering `AddScriptSection` and neutralizes them with a dummy comment buffer to prevent premature standalone compilation.
7. **Profile Switch Crash Guard (`0x7BAD0` & `0x7BB40`)**:
   - Intercepts access violation crashes (`0xC0000005` at `0x7BAF1`) during profile switching when AngelScript modules are partially unlinked while background resource worker threads are executing.
   - Dual-layer protection: Low-memory pointer checks (`< 0x10000`) and hardware SEH (`__try / __except`) matching the engine's native null branch exit (`0x7BB2D`).
   - Detailed writeup in [reversing_profile_switch_crash_and_fix.md](file:///e:/SteamLibrary/steamapps/common/Heroes%20of%20Hammerwatch%202%20AGY/patch_scr/reversing_profile_switch_crash_and_fix.md).