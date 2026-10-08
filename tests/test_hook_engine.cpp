#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <windows.h>
#include <fstream>
#include <sstream>
#include "HookEngine.h"

// Mock globals required by HookEngine.cpp
typedef void (*tConsolePrint)(int channel, const char* fmt, ...);
tConsolePrint g_ConsolePrint = nullptr;
uintptr_t g_BaseAddress = 0;
typedef void (__fastcall *tRegisterHook)(void* pScriptSys, void* pFunc);
tRegisterHook g_RegisterHook = nullptr;

void Log(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    // std::cout << "[LOG] " << buf << std::endl;
}

// Dynamic resolver for unpacked_assets_* folder
// Automatically detects unpacked_assets_<buildNumber> created by PACKAGER.exe,
// or gracefully falls back to None if the user hasn't unpacked base assets.
std::string FindUnpackedAssetsDir() {
    const char* searchPatterns[] = {
        "..\\..\\unpacked_assets_*",
        "..\\unpacked_assets_*",
        "unpacked_assets_*"
    };
    const char* basePrefixes[] = {
        "..\\..\\",
        "..\\",
        ""
    };

    for (int p = 0; p < 3; p++) {
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPatterns[p], &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            std::string latestFolder = "";
            do {
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    std::string name = findData.cFileName;
                    if (name != "." && name != "..") {
                        latestFolder = std::string(basePrefixes[p]) + name;
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
            if (!latestFolder.empty()) return latestFolder;
        }
    }

    return "";
}

std::string ReadFileToString(const std::string& path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// -----------------------------------------------------------------------------
// Hermetic Test Fixtures (Embedded AngelScript code matching actual game classes)
// -----------------------------------------------------------------------------
static const char* kMockPlayerScript =
"class Player : PlayerBase\n"
"{\n"
"    Player(UnitPtr unit, SValue@ params)\n"
"    {\n"
"        super(unit, params);\n"
"    }\n"
"\n"
"    void Damage(DamageInfo@ dmg, vec2 pos, vec2 dir) override\n"
"    {\n"
"        // Deal damage logic\n"
"        m_currentHp -= dmg.Damage;\n"
"    }\n"
"}\n";

static const char* kMockPlayerRecordScript =
"class PlayerRecord\n"
"{\n"
"    string name;\n"
"    int hp;\n"
"\n"
"    void RefreshModifiers()\n"
"    {\n"
"        // Calculate bonuses\n"
"        if (hp > 0) {\n"
"            string debugMsg = \"Braces inside { string } and comments /* } */ // }\";\n"
"        }\n"
"    }\n"
"\n"
"    void AssignUnit(UnitPtr unit)\n"
"    {\n"
"        // Unit assignment\n"
"    }\n"
"}\n";

static const char* kMockGameModeScript =
"class AGameplayGameMode : BaseGameMode\n"
"{\n"
"    vec2 GetCameraPos(int idt) override\n"
"    {\n"
"        return m_camPos;\n"
"    }\n"
"}\n";

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

void TestSnippetNeutralization() {
    std::cout << "[TEST] Snippet Neutralization (.patch and .inc)... ";
    assert(HookEngine::IsSnippetSection("patches/my_patch.patch") == true);
    assert(HookEngine::IsSnippetSection("scripts/my_include.inc") == true);
    assert(HookEngine::IsSnippetSection("scripts/Behaviors/Player.as") == false);
    assert(HookEngine::IsSnippetSection(nullptr) == false);
    std::cout << "PASSED\n";
}

void TestMetadataPreprocessing() {
    std::cout << "[TEST] In-Flight Metadata Preprocessing ([Hook] tag stripping)... ";
    std::string input =
        "[Hook \"OnDamage\"]\n"
        "void InterceptDamage(Player@ player, DamageInfo@ dmg) {\n"
        "    print(\"damage\");\n"
        "}\n";
    std::string output;
    bool res = HookEngine::PreprocessModScript(input, output);
    assert(res == true);
    assert(output.find("[Hook") == std::string::npos);
    assert(output.find("void OnDamage(Player@ player, DamageInfo@ dmg)") != std::string::npos);

    // Code without hooks should return false (no change)
    std::string clean = "void NormalFunction() {}";
    std::string cleanOut;
    assert(HookEngine::PreprocessModScript(clean, cleanOut) == false);
    std::cout << "PASSED\n";
}

void TestHermeticHookAndPatchProcessing() {
    std::cout << "[TEST] Hermetic Hook and Line-Patch Processing (Self-Contained Fixtures)... ";
    
    // Create a temporary mock info.xml
    std::string testXml =
        "<dict>\n"
        "    <string name=\"name\">TestMod</string>\n"
        "    <array name=\"hooks\">\n"
        "        <dict>\n"
        "            <string name=\"class\">Player</string>\n"
        "            <string name=\"function\">Damage</string>\n"
        "            <string name=\"custom_call_name\">OnPlayerDamage</string>\n"
        "        </dict>\n"
        "    </array>\n"
        "    <array name=\"patches\">\n"
        "        <dict>\n"
        "            <string name=\"class\">PlayerRecord</string>\n"
        "            <string name=\"function\">RefreshModifiers</string>\n"
        "            <string name=\"action\">end</string>\n"
        "            <string name=\"code\">print(\"Patched end\");</string>\n"
        "        </dict>\n"
        "        <dict>\n"
        "            <string name=\"class\">AGameplayGameMode</string>\n"
        "            <string name=\"function\">GetCameraPos</string>\n"
        "            <string name=\"action\">start</string>\n"
        "            <string name=\"code\">print(\"Patched start\");</string>\n"
        "        </dict>\n"
        "    </array>\n"
        "</dict>\n";

    std::string tempConfig = "test_temp_info.xml";
    std::ofstream out(tempConfig);
    out << testXml;
    out.close();

    HookEngine::LoadConfigFile(tempConfig, "", "TestMod");
    DeleteFileA(tempConfig.c_str());

    assert(HookEngine::GetHookCount() >= 1);
    assert(HookEngine::GetPatchCount() >= 2);

    void* mockModule = (void*)0x12345000;
    std::string transformed;
    bool injected = false;

    // 1. Test Dynamic Hook Injection into Player::Damage
    bool res1 = HookEngine::ProcessScriptSection(
        mockModule,
        "scripts/Behaviors/Actors/Player/Player.as",
        kMockPlayerScript,
        strlen(kMockPlayerScript),
        transformed,
        injected
    );
    assert(res1 == true);
    assert(injected == true);
    assert(transformed.find("Hooks::Call(\"OnPlayerDamage\", @this, @dmg, pos, dir);") != std::string::npos);

    // 2. Test Line Patch "end" into PlayerRecord::RefreshModifiers
    std::string transformed2;
    bool injected2 = false;
    bool res2 = HookEngine::ProcessScriptSection(
        mockModule,
        "scripts/Behaviors/Actors/Player/PlayerRecord.as",
        kMockPlayerRecordScript,
        strlen(kMockPlayerRecordScript),
        transformed2,
        injected2
    );
    assert(res2 == true);
    assert(injected2 == true);
    assert(transformed2.find("/* [Patch: TestMod] */") != std::string::npos);
    assert(transformed2.find("if (HWR_IsModActive(\"TestMod\"))") != std::string::npos);
    assert(transformed2.find("print(\"Patched end\");") != std::string::npos);

    // 3. Test Line Patch "start" into AGameplayGameMode::GetCameraPos
    std::string transformed3;
    bool injected3 = false;
    bool res3 = HookEngine::ProcessScriptSection(
        mockModule,
        "scripts/GameModes/AGameplayGameMode.as",
        kMockGameModeScript,
        strlen(kMockGameModeScript),
        transformed3,
        injected3
    );
    assert(res3 == true);
    assert(injected3 == true);
    assert(transformed3.find("print(\"Patched start\");") != std::string::npos);

    // Clean up
    HookEngine::OnModuleBuildComplete(mockModule);
    std::cout << "PASSED\n";
}

void TestLiveGameAssetsIfAvailable() {
    std::cout << "[TEST] Dynamic Discovery of Unpacked Game Assets... ";
    std::string assetsDir = FindUnpackedAssetsDir();
    if (assetsDir.empty()) {
        std::cout << "SKIPPED (No unpacked_assets_* directory found; base game assets not unpacked via PACKAGER.exe. Hermetic tests verified!)\n";
        return;
    }

    std::cout << "\n       -> Discovered dynamic assets directory: " << assetsDir << "\n";

    // Verify Player.as
    std::string playerPath = assetsDir + "\\scripts\\Behaviors\\Actors\\Player\\Player.as";
    std::string playerContent = ReadFileToString(playerPath);
    if (!playerContent.empty()) {
        std::cout << "       -> Validating against real Player.as (" << playerContent.length() << " bytes)... ";
        std::string out;
        bool inj = false;
        void* mockMod = (void*)0x9999000;
        bool res = HookEngine::ProcessScriptSection(mockMod, "scripts/Behaviors/Actors/Player/Player.as", playerContent.c_str(), playerContent.length(), out, inj);
        assert(res == true && inj == true);
        HookEngine::OnModuleBuildComplete(mockMod);
        std::cout << "OK\n";
    }

    // Verify PlayerRecord.as
    std::string recordPath = assetsDir + "\\scripts\\Behaviors\\Actors\\Player\\PlayerRecord.as";
    std::string recordContent = ReadFileToString(recordPath);
    if (!recordContent.empty()) {
        std::cout << "       -> Validating against real PlayerRecord.as (" << recordContent.length() << " bytes)... ";
        std::string out;
        bool inj = false;
        void* mockMod = (void*)0x9999000;
        bool res = HookEngine::ProcessScriptSection(mockMod, "scripts/Behaviors/Actors/Player/PlayerRecord.as", recordContent.c_str(), recordContent.length(), out, inj);
        assert(res == true && inj == true);
        HookEngine::OnModuleBuildComplete(mockMod);
        std::cout << "OK\n";
    }

    std::cout << "       -> Live game asset validation PASSED\n";
}

int main() {
    std::cout << "========================================================\n";
    std::cout << " Heroes of Hammerwatch 2 - HookEngine Sturdy Unit Tests\n";
    std::cout << "========================================================\n";

    TestSnippetNeutralization();
    TestMetadataPreprocessing();
    TestHermeticHookAndPatchProcessing();
    TestLiveGameAssetsIfAvailable();

    std::cout << "\n========================================================\n";
    std::cout << " [ALL TESTS PASSED SUCCESSFULLY]\n";
    std::cout << " Tests are fully hermetic and resilient to game updates!\n";
    std::cout << "========================================================\n";
    return 0;
}
