#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <set>

enum ConsoleChannel {
    CONSOLE_CHANNEL_INFO    = 0, // White
    CONSOLE_CHANNEL_WARNING = 1, // Yellow
    CONSOLE_CHANNEL_ERROR   = 2  // Red
};

struct HookDefinition {
    std::string modName;            // e.g. "MyMod"
    std::string targetClass;        // e.g. "Player"
    std::string targetMethod;       // e.g. "Damage"
    std::string hookName;           // e.g. "OnPlayerDamage"
    std::vector<std::string> params;// Optional parameter type filter for overloads
    bool isPrefix;                  // true = prefix, false = postfix
};

struct PatchDefinition {
    std::string modName;            // e.g. "MyMod"
    std::string targetClass;        // e.g. "PlayerRecord" (optional)
    std::string targetFunction;     // e.g. "RefreshModifiers"
    std::string action;             // "start", "end", "before", "after"
    std::string anchor;             // for "before" and "after"
    std::string code;               // actual code snippet to inject
    std::string file;               // external snippet file path (e.g. "patches/my_patch.patch")
};

struct ModScriptRecord {
    std::string modId;              // e.g. "CustomHooksMod" or "drop_me.bin"
    std::string modName;            // e.g. "CustomHooksMod" or "Drop Me"
    std::string scriptPath;         // e.g. "scripts/gui/playermenu/character.as"
};

class HookEngine {
public:
    static void Initialize();
    static void LoadAllConfigurations();
    static void ScanDirectory(const std::string& dirPath, bool isWorkshop);
    static void LoadConfigFile(const std::string& configPath, const std::string& modFolder, const std::string& defaultModName);
    static void LoadBinPackage(const std::string& binPath);

    static bool IsSnippetSection(const char* sectionName);
    static bool PreprocessModScript(const std::string& inputCode, std::string& outputCode);
    static bool ProcessScriptSection(
        void* pModule,
        const char* sectionName,
        const char* inputCode,
        size_t inputLen,
        std::string& outputCode,
        bool& outInjectedHooksOrPatches
    );
    static void RegisterModHooks(void* pScriptSys, void* pModule);
    static void OnModuleBuildComplete(void* pModule);

    static size_t GetHookCount();
    static size_t GetPatchCount();
    static size_t GetDiscoveredModScriptCount();
    static void RegisterModScript(const std::string& modId, const std::string& modName, const std::string& scriptPath);
    static void ClearDiscoveredModScripts();
    static bool PatchModlistWindow(std::string& source);
};

