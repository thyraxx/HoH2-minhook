#include "HookEngine.h"
#include "angelscript.h"
#include <windows.h>
#include <regex>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_set>
#include <algorithm>
#include <cstdint>

typedef void (*tConsolePrint)(int channel, const char* fmt, ...);
extern tConsolePrint g_ConsolePrint;
extern void Log(const char* fmt, ...);
extern uintptr_t g_BaseAddress;

typedef void (__fastcall *tRegisterHook)(void* pScriptSys, void* pFunc);
extern tRegisterHook g_RegisterHook;

static std::vector<HookDefinition> g_HookDefinitions;
static std::vector<PatchDefinition> g_PatchDefinitions;
static std::unordered_set<std::string> g_RegisteredSnippetFiles;
static std::set<void*> g_HelperInjectedModules;

// Helpers: String manipulation
static std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::string ToLower(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)::tolower(c); });
    return s;
}

static std::string NormalizePath(const std::string& path) {
    std::string s = path;
    for (char& c : s) {
        if (c == '\\') c = '/';
    }
    return ToLower(s);
}

static std::string GetBasename(const std::string& path) {
    std::string norm = NormalizePath(path);
    size_t lastSlash = norm.find_last_of('/');
    if (lastSlash != std::string::npos) {
        return norm.substr(lastSlash + 1);
    }
    return norm;
}

static std::string ReadFileToString(const std::string& path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool IsValueType(const std::string& typeName) {
    static const std::unordered_set<std::string> valueTypes = {
        "int", "int8", "int16", "int32", "int64",
        "uint", "uint8", "uint16", "uint32", "uint64",
        "float", "double", "bool", "char",
        "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "quaternion",
        "string", "UnitPtr"
    };
    std::string clean = Trim(typeName);
    size_t amp = clean.find('&');
    if (amp != std::string::npos) clean = clean.substr(0, amp);
    size_t c = clean.find("const ");
    if (c != std::string::npos) clean = clean.substr(c + 6);
    clean = Trim(clean);
    return valueTypes.count(clean) > 0;
}

// ------------------------------------------------------------------------------------------------
// Binary SValue Parser for .bin Mod Packages (HW2R / HWRR)
// ------------------------------------------------------------------------------------------------
struct SValue {
    enum Type {
        Null = 0,
        Bool = 1,
        Dictionary = 2,
        Array = 3,
        Integer = 4,
        Float = 5,
        String = 6,
        Vector2 = 7,
        Vector3 = 8,
        Vector4 = 9
    };
    Type type = Null;
    bool boolVal = false;
    int32_t intVal = 0;
    float floatVal = 0.0f;
    std::string strVal;
    std::vector<std::pair<std::string, SValue>> dictVal;
    std::vector<SValue> arrayVal;

    std::string GetString(const std::string& key, const std::string& def = "") const {
        if (type == Dictionary) {
            for (const auto& kv : dictVal) {
                if (_stricmp(kv.first.c_str(), key.c_str()) == 0) {
                    if (kv.second.type == String) return kv.second.strVal;
                }
            }
        }
        return def;
    }

    const SValue* Get(const std::string& key) const {
        if (type == Dictionary) {
            for (const auto& kv : dictVal) {
                if (_stricmp(kv.first.c_str(), key.c_str()) == 0) {
                    return &kv.second;
                }
            }
        }
        return nullptr;
    }
};

static bool ParseSValue(const uint8_t*& ptr, const uint8_t* end, SValue& out) {
    if (ptr >= end) return false;
    uint8_t t = *ptr++;
    out.type = (SValue::Type)t;
    switch (t) {
        case SValue::Null: return true;
        case SValue::Bool: {
            if (ptr >= end) return false;
            out.boolVal = (*ptr++ != 0);
            return true;
        }
        case SValue::Integer: {
            if (ptr + 4 > end) return false;
            out.intVal = *(const int32_t*)ptr;
            ptr += 4;
            return true;
        }
        case SValue::Float: {
            if (ptr + 4 > end) return false;
            out.floatVal = *(const float*)ptr;
            ptr += 4;
            return true;
        }
        case SValue::String: {
            const char* s = (const char*)ptr;
            while (ptr < end && *ptr != 0) ptr++;
            if (ptr >= end) return false;
            out.strVal = std::string(s, (const char*)ptr - s);
            ptr++; // skip '\0'
            return true;
        }
        case SValue::Vector2: {
            if (ptr + 8 > end) return false;
            ptr += 8;
            return true;
        }
        case SValue::Vector3: {
            if (ptr + 12 > end) return false;
            ptr += 12;
            return true;
        }
        case SValue::Vector4: {
            if (ptr + 16 > end) return false;
            ptr += 16;
            return true;
        }
        case SValue::Array: {
            if (ptr + 4 > end) return false;
            uint32_t count = *(const uint32_t*)ptr;
            ptr += 4;
            out.arrayVal.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                if (!ParseSValue(ptr, end, out.arrayVal[i])) return false;
            }
            return true;
        }
        case SValue::Dictionary: {
            if (ptr + 4 > end) return false;
            uint32_t count = *(const uint32_t*)ptr;
            ptr += 4;
            out.dictVal.reserve(count);
            for (uint32_t i = 0; i < count; i++) {
                const char* k = (const char*)ptr;
                while (ptr < end && *ptr != 0) ptr++;
                if (ptr >= end) return false;
                std::string key(k, (const char*)ptr - k);
                ptr++; // skip '\0'
                SValue val;
                if (!ParseSValue(ptr, end, val)) return false;
                out.dictVal.push_back({key, val});
            }
            return true;
        }
        default:
            return false;
    }
}

// ------------------------------------------------------------------------------------------------
// Registration helpers with deduplication
// ------------------------------------------------------------------------------------------------
static void RegisterHookDef(const HookDefinition& def) {
    if (def.targetClass.empty() || def.targetMethod.empty()) return;

    for (const auto& existing : g_HookDefinitions) {
        if (existing.targetClass == def.targetClass &&
            existing.targetMethod == def.targetMethod &&
            existing.hookName == def.hookName &&
            existing.isPrefix == def.isPrefix) {
            return; // Already registered
        }
    }
    g_HookDefinitions.push_back(def);
    Log("[HookEngine] Registered hook: %s::%s -> '%s' (mod: %s, %s)",
        def.targetClass.c_str(), def.targetMethod.c_str(), def.hookName.c_str(),
        def.modName.empty() ? "<global>" : def.modName.c_str(),
        def.isPrefix ? "Prefix" : "Postfix");
}

static void RegisterPatchDef(const PatchDefinition& def) {
    if (def.targetFunction.empty() || (def.code.empty() && def.file.empty())) return;

    for (const auto& existing : g_PatchDefinitions) {
        if (existing.modName == def.modName &&
            existing.targetClass == def.targetClass &&
            existing.targetFunction == def.targetFunction &&
            existing.action == def.action &&
            existing.anchor == def.anchor &&
            existing.code == def.code) {
            return; // Already registered
        }
    }
    g_PatchDefinitions.push_back(def);
    Log("[HookEngine] Registered patch: %s%s%s (action: %s, mod: %s)",
        def.targetClass.empty() ? "" : (def.targetClass + "::").c_str(),
        def.targetFunction.c_str(),
        def.file.empty() ? "" : (" [" + def.file + "]").c_str(),
        def.action.c_str(),
        def.modName.empty() ? "<global>" : def.modName.c_str());
}

// ------------------------------------------------------------------------------------------------
// XML & SValue Parsing for info.xml, hooks.xml, and .bin packages
// ------------------------------------------------------------------------------------------------
static void ParseInfoXmlContent(const std::string& xmlContent, const std::string& modFolder, const std::string& defaultModName) {
    std::string modName = defaultModName;

    // Extract mod name from <string name="name">...</string>
    std::regex nameRegex("<string\\s+name=[\"']name[\"']>([^<]+)</string>", std::regex::icase);
    std::smatch nameMatch;
    if (std::regex_search(xmlContent, nameMatch, nameRegex)) {
        modName = Trim(nameMatch[1].str());
    }

    // 1. Parse <array name="hooks">...</array>
    std::regex hooksArrayRegex("<array\\s+name=[\"']hooks[\"']>([\\s\\S]*?)</array>", std::regex::icase);
    std::smatch hooksArrayMatch;
    if (std::regex_search(xmlContent, hooksArrayMatch, hooksArrayRegex)) {
        std::string hooksBlock = hooksArrayMatch[1].str();
        std::regex dictRegex("<dict>([\\s\\S]*?)</dict>", std::regex::icase);
        auto it = std::sregex_iterator(hooksBlock.begin(), hooksBlock.end(), dictRegex);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            std::string d = it->str();
            HookDefinition def;
            def.modName = modName;
            def.isPrefix = true;

            std::smatch m;
            std::regex cls("<string\\s+name=[\"']class[\"']>([^<]+)</string>", std::regex::icase);
            if (std::regex_search(d, m, cls)) def.targetClass = Trim(m[1].str());

            std::regex fn("<string\\s+name=[\"'](?:function|method)[\"']>([^<]+)</string>", std::regex::icase);
            if (std::regex_search(d, m, fn)) def.targetMethod = Trim(m[1].str());

            std::regex hn("<string\\s+name=[\"'](?:custom_call_name|hook_name|name)[\"']>([^<]+)</string>", std::regex::icase);
            if (std::regex_search(d, m, hn)) def.hookName = Trim(m[1].str());

            std::regex tp("<string\\s+name=[\"'](?:type|action)[\"']>([^<]+)</string>", std::regex::icase);
            if (std::regex_search(d, m, tp)) {
                std::string t = ToLower(Trim(m[1].str()));
                if (t == "post" || t == "postfix") def.isPrefix = false;
            }

            if (def.hookName.empty() && !def.targetClass.empty() && !def.targetMethod.empty()) {
                def.hookName = def.targetClass + "_" + def.targetMethod + (def.isPrefix ? "_Pre" : "_Post");
            }

            RegisterHookDef(def);
        }
    }

    // 2. Parse <array name="patches">...</array>
    std::regex patchesArrayRegex("<array\\s+name=[\"']patches[\"']>([\\s\\S]*?)</array>", std::regex::icase);
    std::smatch patchesArrayMatch;
    if (std::regex_search(xmlContent, patchesArrayMatch, patchesArrayRegex)) {
        std::string patchesBlock = patchesArrayMatch[1].str();
        std::regex dictRegex("<dict>([\\s\\S]*?)</dict>", std::regex::icase);
        auto it = std::sregex_iterator(patchesBlock.begin(), patchesBlock.end(), dictRegex);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            std::string d = it->str();
            PatchDefinition def;
            def.modName = modName;
            def.action = "start";

            std::smatch m;
            std::regex cls("<string\\s+name=[\"']class[\"']>([^<]*)</string>", std::regex::icase);
            if (std::regex_search(d, m, cls)) def.targetClass = Trim(m[1].str());

            std::regex fn("<string\\s+name=[\"'](?:function|method)[\"']>([^<]+)</string>", std::regex::icase);
            if (std::regex_search(d, m, fn)) def.targetFunction = Trim(m[1].str());

            std::regex act("<string\\s+name=[\"']action[\"']>([^<]+)</string>", std::regex::icase);
            if (std::regex_search(d, m, act)) def.action = ToLower(Trim(m[1].str()));

            // Support CDATA or plain text for anchor
            std::regex anc("<string\\s+name=[\"']anchor[\"']>(?:<!\\[CDATA\\[([\\s\\S]*?)\\]\\]>|([^<]*))</string>", std::regex::icase);
            if (std::regex_search(d, m, anc)) {
                def.anchor = m[1].matched ? m[1].str() : m[2].str();
            }

            // Support CDATA or plain text for code
            std::regex cd("<string\\s+name=[\"']code[\"']>(?:<!\\[CDATA\\[([\\s\\S]*?)\\]\\]>|([^<]*))</string>", std::regex::icase);
            if (std::regex_search(d, m, cd)) {
                def.code = m[1].matched ? m[1].str() : m[2].str();
            }

            // Support external snippet file
            std::regex fl("<string\\s+name=[\"']file[\"']>([^<]+)</string>", std::regex::icase);
            if (std::regex_search(d, m, fl)) {
                def.file = Trim(m[1].str());
                g_RegisteredSnippetFiles.insert(NormalizePath(def.file));
                g_RegisteredSnippetFiles.insert(GetBasename(def.file));

                if (def.code.empty() && !modFolder.empty()) {
                    std::string fullPath = modFolder + "/" + def.file;
                    def.code = ReadFileToString(fullPath);
                    if (def.code.empty()) {
                        def.code = ReadFileToString(def.file);
                    }
                }
            }

            RegisterPatchDef(def);
        }
    }
}

void HookEngine::LoadConfigFile(const std::string& configPath, const std::string& modFolder, const std::string& defaultModName) {
    std::string content = ReadFileToString(configPath);
    if (content.empty()) return;

    Log("[HookEngine] Loading config: %s", configPath.c_str());
    ParseInfoXmlContent(content, modFolder, defaultModName);
}

void HookEngine::LoadBinPackage(const std::string& binPath) {
    std::ifstream file(binPath, std::ios::in | std::ios::binary);
    if (!file.is_open()) return;

    file.seekg(0, std::ios::end);
    size_t fileSize = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);
    if (fileSize < 14) return;

    std::vector<uint8_t> buffer(fileSize);
    file.read((char*)buffer.data(), fileSize);

    // Validate magic: HW2R (HoH2) or HWRR (HoH1)
    if (memcmp(buffer.data(), "HW2R", 4) != 0 && memcmp(buffer.data(), "HWRR", 4) != 0) {
        return;
    }

    uint32_t svalLen = *(const uint32_t*)(buffer.data() + 9);
    if (13 + svalLen > fileSize) return;

    const uint8_t* ptr = buffer.data() + 13;
    const uint8_t* end = ptr + svalLen;

    SValue rootSVal;
    if (!ParseSValue(ptr, end, rootSVal) || rootSVal.type != SValue::Dictionary) {
        return;
    }

    std::string modName = rootSVal.GetString("name");
    if (modName.empty()) {
        modName = GetBasename(binPath);
        size_t dot = modName.find_last_of('.');
        if (dot != std::string::npos) modName = modName.substr(0, dot);
    }

    Log("[HookEngine] Loaded .bin package '%s' (mod: '%s')", binPath.c_str(), modName.c_str());

    // 1. Extract hooks from binary SValue dictionary
    const SValue* hooksVal = rootSVal.Get("hooks");
    if (hooksVal && hooksVal->type == SValue::Array) {
        for (const auto& item : hooksVal->arrayVal) {
            if (item.type != SValue::Dictionary) continue;
            HookDefinition def;
            def.modName = modName;
            def.targetClass = item.GetString("class");
            def.targetMethod = item.GetString("function");
            if (def.targetMethod.empty()) def.targetMethod = item.GetString("method");
            def.hookName = item.GetString("custom_call_name");
            if (def.hookName.empty()) def.hookName = item.GetString("hook_name");
            if (def.hookName.empty()) def.hookName = item.GetString("name");
            std::string action = ToLower(item.GetString("action", item.GetString("type", "prefix")));
            def.isPrefix = (action != "post" && action != "postfix");

            if (def.hookName.empty() && !def.targetClass.empty() && !def.targetMethod.empty()) {
                def.hookName = def.targetClass + "_" + def.targetMethod + (def.isPrefix ? "_Pre" : "_Post");
            }
            RegisterHookDef(def);
        }
    }

    // 2. Extract patches from binary SValue dictionary
    const SValue* patchesVal = rootSVal.Get("patches");
    if (patchesVal && patchesVal->type == SValue::Array) {
        for (const auto& item : patchesVal->arrayVal) {
            if (item.type != SValue::Dictionary) continue;
            PatchDefinition def;
            def.modName = modName;
            def.targetClass = item.GetString("class");
            def.targetFunction = item.GetString("function");
            if (def.targetFunction.empty()) def.targetFunction = item.GetString("method");
            def.action = ToLower(item.GetString("action", "start"));
            def.anchor = item.GetString("anchor");
            def.code = item.GetString("code");
            def.file = item.GetString("file");

            if (!def.file.empty()) {
                g_RegisteredSnippetFiles.insert(NormalizePath(def.file));
                g_RegisteredSnippetFiles.insert(GetBasename(def.file));
            }
            RegisterPatchDef(def);
        }
    }
}

void HookEngine::ScanDirectory(const std::string& dirPath, bool isWorkshop) {
    std::string searchPattern = dirPath + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }

        std::string fullPath = dirPath + "\\" + findData.cFileName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::string folderName = findData.cFileName;

            if (isWorkshop) {
                // In Steam Workshop, each subfolder is a workshop item ID: scan for .bin packages
                WIN32_FIND_DATAA binFind;
                HANDLE hBin = FindFirstFileA((fullPath + "\\*.bin").c_str(), &binFind);
                if (hBin != INVALID_HANDLE_VALUE) {
                    do {
                        LoadBinPackage(fullPath + "\\" + binFind.cFileName);
                    } while (FindNextFileA(hBin, &binFind));
                    FindClose(hBin);
                }
            } else {
                // Local unpacked mod directory
                std::string infoPath = fullPath + "\\info.xml";
                if (GetFileAttributesA(infoPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    LoadConfigFile(infoPath, fullPath, folderName);
                }

                std::string hooksXml = fullPath + "\\hooks.xml";
                if (GetFileAttributesA(hooksXml.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    LoadConfigFile(hooksXml, fullPath, folderName);
                }

                // Check for packed .bin packages inside mod folder
                WIN32_FIND_DATAA subBin;
                HANDLE hSubBin = FindFirstFileA((fullPath + "\\*.bin").c_str(), &subBin);
                if (hSubBin != INVALID_HANDLE_VALUE) {
                    do {
                        LoadBinPackage(fullPath + "\\" + subBin.cFileName);
                    } while (FindNextFileA(hSubBin, &subBin));
                    FindClose(hSubBin);
                }
            }
        } else {
            // Check top-level .bin files in mods/*.bin
            std::string fileLower = ToLower(findData.cFileName);
            if (fileLower.length() > 4 && fileLower.substr(fileLower.length() - 4) == ".bin") {
                LoadBinPackage(fullPath);
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}

void HookEngine::LoadAllConfigurations() {
    g_HookDefinitions.clear();
    g_PatchDefinitions.clear();
    g_RegisteredSnippetFiles.clear();

    // 1. Root configuration files
    if (GetFileAttributesA("info.xml") != INVALID_FILE_ATTRIBUTES) LoadConfigFile("info.xml", "", "RootMod");
    if (GetFileAttributesA("hooks.xml") != INVALID_FILE_ATTRIBUTES) LoadConfigFile("hooks.xml", "", "RootMod");

    // 2. Local mods directory (unpacked & packed .bin)
    if (GetFileAttributesA("mods") != INVALID_FILE_ATTRIBUTES) {
        ScanDirectory("mods", false);
    }

    // 3. Steam Workshop packages (HoH2: 619820, HoH1 fallback: 677120)
    static const char* kWorkshopPaths[] = {
        "..\\..\\workshop\\content\\619820",
        "..\\workshop\\content\\619820",
        "workshop\\content\\619820",
        "..\\..\\workshop\\content\\677120",
        "workshop\\content\\677120"
    };

    for (const char* p : kWorkshopPaths) {
        if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) {
            Log("[HookEngine] Scanning Steam Workshop directory: %s", p);
            ScanDirectory(p, true);
        }
    }

    Log("[HookEngine] Configurations loaded: %zu dynamic hooks, %zu line patches, %zu snippet files registered",
        g_HookDefinitions.size(), g_PatchDefinitions.size(), g_RegisteredSnippetFiles.size());
}

void HookEngine::Initialize() {
    LoadAllConfigurations();
}

bool HookEngine::IsSnippetSection(const char* sectionName) {
    if (!sectionName) return false;
    std::string norm = NormalizePath(sectionName);

    // Any file with .patch or .inc extension
    if (norm.length() >= 6 && norm.substr(norm.length() - 6) == ".patch") return true;
    if (norm.length() >= 4 && norm.substr(norm.length() - 4) == ".inc") return true;

    // Any registered snippet file path or basename
    if (g_RegisteredSnippetFiles.count(norm) > 0) return true;
    if (g_RegisteredSnippetFiles.count(GetBasename(sectionName)) > 0) return true;

    return false;
}

bool HookEngine::PreprocessModScript(const std::string& inputCode, std::string& outputCode) {
    if (inputCode.find("[Hook") == std::string::npos && inputCode.find("[ Hook") == std::string::npos) {
        return false;
    }

    std::string processed = inputCode;

    // 1. [Hook "TargetHook"] void OnMyHook(...) -> void TargetHook(...)
    std::regex hookAttrRegex("\\[\\s*Hook\\s+\"([A-Za-z0-9_]+)\"\\s*\\]\\s*([A-Za-z0-9_<>@]+\\s+)([A-Za-z0-9_]+)");
    processed = std::regex_replace(processed, hookAttrRegex, "$2$1");

    // 2. Strip any remaining [Hook] or [Hook "..."] metadata tags!
    std::regex stripHookRegex("\\[\\s*Hook(?:\\s+\"[^\"]+\")?\\s*\\]\\s*");
    processed = std::regex_replace(processed, stripHookRegex, "");

    outputCode = processed;
    return true;
}

// ------------------------------------------------------------------------------------------------
// Profile Awareness Global Helper Function
// ------------------------------------------------------------------------------------------------
static const char* kProfileHelperSource =
"\n"
"// [HookEngine] Profile Awareness & Compatibility Helper\n"
"namespace HwrSaves\n"
"{\n"
"    bool IsModded() { return PersistentSaves::GetModded(); }\n"
"    array<ResourceMod@>@ GetEnabledMods() { return PersistentSaves::GetEnabledResourceMods(); }\n"
"}\n\n"
"bool HWR_IsModActive(const string &in modId)\n"
"{\n"
"    if (!PersistentSaves::GetModded())\n"
"        return false;\n"
"    if (modId.isEmpty())\n"
"        return true;\n"
"    auto@ mods = PersistentSaves::GetEnabledResourceMods();\n"
"    if (mods is null)\n"
"        return false;\n"
"    string target = modId.toLower();\n"
"    for (uint i = 0; i < mods.length(); i++)\n"
"    {\n"
"        if (mods[i] !is null && (mods[i].ID.toLower() == target || mods[i].Name.toLower() == target))\n"
"            return true;\n"
"    }\n"
"    return false;\n"
"}\n\n";


// Helper: Find matching closing brace using depth counter with string/comment skipping
static size_t FindMatchingClosingBrace(const std::string& str, size_t openBracePos) {
    int depth = 1;
    size_t i = openBracePos + 1;
    size_t len = str.length();

    while (i < len && depth > 0) {
        char c = str[i];
        if (c == '/' && i + 1 < len) {
            if (str[i + 1] == '/') {
                // Line comment: skip to newline
                i += 2;
                while (i < len && str[i] != '\n') i++;
                continue;
            } else if (str[i + 1] == '*') {
                // Block comment: skip to */
                i += 2;
                while (i + 1 < len && !(str[i] == '*' && str[i + 1] == '/')) i++;
                i += 2;
                continue;
            }
        }
        if (c == '"') {
            // String literal: skip to closing quote
            i++;
            while (i < len && str[i] != '"') {
                if (str[i] == '\\') i++;
                i++;
            }
            i++;
            continue;
        }
        if (c == '\'') {
            // Char literal
            i++;
            while (i < len && str[i] != '\'') {
                if (str[i] == '\\') i++;
                i++;
            }
            i++;
            continue;
        }

        if (c == '{') depth++;
        else if (c == '}') depth--;

        if (depth == 0) return i;
        i++;
    }
    return std::string::npos;
}

// ------------------------------------------------------------------------------------------------
// In-Memory Script Section Transformation (Hooks + Line Patches + Profile Guards)
// ------------------------------------------------------------------------------------------------
bool HookEngine::ProcessScriptSection(
    void* pModule,
    const char* sectionName,
    const char* inputCode,
    size_t inputLen,
    std::string& outputCode,
    bool& outInjectedHooksOrPatches
) {
    outInjectedHooksOrPatches = false;
    if (!inputCode || inputLen == 0) return false;

    std::string secStr = sectionName ? NormalizePath(sectionName) : "";

    // Pitfall 4: MetaScript Isolation
    // NEVER inject game helpers, hooks, or line patches into .mas sections!
    if (secStr.find(".mas") != std::string::npos) {
        return false;
    }

    // Only process main game script sections ending with .as
    bool isAsFile = (secStr.length() >= 3 && secStr.substr(secStr.length() - 3) == ".as");
    if (!isAsFile) return false;

    std::string source(inputCode, inputLen);
    bool modified = false;

    // Pitfall 3: Profile Isolation
    // Prepend HWR_IsModActive to the first .as section compiled for this module instance
    if (pModule && g_HelperInjectedModules.find(pModule) == g_HelperInjectedModules.end()) {
        source = kProfileHelperSource + source;
        g_HelperInjectedModules.insert(pModule);
        modified = true;
        Log("[HookEngine] Injected HWR_IsModActive helper into module %p (section: %s)",
            pModule, sectionName ? sectionName : "<unknown>");
    }

    // 1. Inject Dynamic Hooks (Hooks::Call)
    for (const auto& def : g_HookDefinitions) {
        size_t searchPos = 0;
        if (!def.targetClass.empty()) {
            std::regex classRegex(std::string("class\\s+") + def.targetClass + "\\b[^{]*\\{");
            std::smatch classMatch;
            if (!std::regex_search(source, classMatch, classRegex)) {
                continue; // Class not in this file
            }
            searchPos = classMatch.position() + classMatch.length();
        }

        std::regex methodRegex(
            std::string("(?:[A-Za-z0-9_<>@]+\\s+)+") + def.targetMethod + "\\s*\\(([^)]*)\\)\\s*(?:override|final)?\\s*\\{"
        );

        std::string searchSub = source.substr(searchPos);
        std::smatch methodMatch;

        if (std::regex_search(searchSub, methodMatch, methodRegex)) {
            std::string paramsStr = methodMatch[1].str();

            if (!def.params.empty()) {
                bool matchAll = true;
                for (const auto& reqType : def.params) {
                    if (paramsStr.find(reqType) == std::string::npos) {
                        matchAll = false;
                        break;
                    }
                }
                if (!matchAll) continue;
            }

            std::vector<std::string> argList;
            if (!def.targetClass.empty()) {
                argList.push_back("@this");
            }

            std::stringstream ss(paramsStr);
            std::string paramToken;
            while (std::getline(ss, paramToken, ',')) {
                paramToken = Trim(paramToken);
                size_t eqPos = paramToken.find('=');
                if (eqPos != std::string::npos) paramToken = Trim(paramToken.substr(0, eqPos));

                std::stringstream pss(paramToken);
                std::string word, ptype, pname;
                while (pss >> word) {
                    ptype = pname;
                    pname = word;
                }

                if (!pname.empty()) {
                    if (pname[0] == '@') argList.push_back(pname);
                    else if (ptype.find('@') != std::string::npos) argList.push_back("@" + pname);
                    else if (!IsValueType(ptype)) argList.push_back("@" + pname);
                    else argList.push_back(pname);
                }
            }

            // Blueprint Requirement: Guard dynamic hook call with profile modded status
            std::string hookCall = "\n\t\tif (PersistentSaves::GetModded()) Hooks::Call(\"" + def.hookName + "\"";
            for (const auto& arg : argList) {
                hookCall += ", " + arg;
            }
            hookCall += ");\n";

            size_t insertOffset = searchPos + methodMatch.position() + methodMatch.length();

            if (def.isPrefix) {
                source.insert(insertOffset, hookCall);
                modified = true;
                outInjectedHooksOrPatches = true;
                Log("[HookEngine] Injected PREFIX hook '%s' into %s%s%s (section: %s)",
                    def.hookName.c_str(),
                    def.targetClass.empty() ? "" : (def.targetClass + "::").c_str(),
                    def.targetMethod.c_str(),
                    def.modName.empty() ? "" : (" [Mod: " + def.modName + "]").c_str(),
                    sectionName ? sectionName : "<unknown>");
            }
        }
    }

    // 2. Inject Non-Overwriting Line Patches
    for (const auto& patch : g_PatchDefinitions) {
        if (patch.code.empty()) continue;

        size_t searchPos = 0;
        size_t scopeEnd = source.length();

        if (!patch.targetClass.empty()) {
            std::regex classRegex(std::string("class\\s+") + patch.targetClass + "\\b[^{]*\\{");
            std::smatch classMatch;
            if (!std::regex_search(source, classMatch, classRegex)) {
                continue; // Class not in this section
            }
            searchPos = classMatch.position() + classMatch.length();
            size_t classClose = FindMatchingClosingBrace(source, searchPos - 1);
            if (classClose != std::string::npos) scopeEnd = classClose;
        }

        std::regex funcRegex(
            std::string("(?:[A-Za-z0-9_<>@]+\\s+)+") + patch.targetFunction + "\\s*\\(([^)]*)\\)\\s*(?:override|final)?\\s*\\{"
        );

        std::string searchSub = source.substr(searchPos, scopeEnd - searchPos);
        std::smatch funcMatch;

        if (std::regex_search(searchSub, funcMatch, funcRegex)) {
            size_t openBrace = searchPos + funcMatch.position() + funcMatch.length() - 1;
            size_t closeBrace = FindMatchingClosingBrace(source, openBrace);
            if (closeBrace == std::string::npos) continue;

            // Blueprint Requirement: Wrap patch in profile awareness check
            std::string wrappedPatch =
                "\n\t\t/* [Patch: " + patch.modName + "] */\n"
                "\t\tif (HWR_IsModActive(\"" + patch.modName + "\")) {\n"
                "\t\t\t" + patch.code + "\n"
                "\t\t}\n";

            if (patch.action == "start") {
                source.insert(openBrace + 1, wrappedPatch);
                modified = true;
                outInjectedHooksOrPatches = true;
                Log("[HookEngine] Injected patch START into %s%s%s (mod: %s)",
                    patch.targetClass.empty() ? "" : (patch.targetClass + "::").c_str(),
                    patch.targetFunction.c_str(),
                    sectionName ? (" in " + std::string(sectionName)).c_str() : "",
                    patch.modName.c_str());
            } else if (patch.action == "end") {
                source.insert(closeBrace, wrappedPatch);
                modified = true;
                outInjectedHooksOrPatches = true;
                Log("[HookEngine] Injected patch END into %s%s%s (mod: %s)",
                    patch.targetClass.empty() ? "" : (patch.targetClass + "::").c_str(),
                    patch.targetFunction.c_str(),
                    sectionName ? (" in " + std::string(sectionName)).c_str() : "",
                    patch.modName.c_str());
            } else if (patch.action == "before" && !patch.anchor.empty()) {
                size_t anchorPos = source.find(patch.anchor, openBrace);
                if (anchorPos != std::string::npos && anchorPos < closeBrace) {
                    source.insert(anchorPos, wrappedPatch);
                    modified = true;
                    outInjectedHooksOrPatches = true;
                    Log("[HookEngine] Injected patch BEFORE anchor into %s%s%s (mod: %s)",
                        patch.targetClass.empty() ? "" : (patch.targetClass + "::").c_str(),
                        patch.targetFunction.c_str(),
                        sectionName ? (" in " + std::string(sectionName)).c_str() : "",
                        patch.modName.c_str());
                }
            } else if (patch.action == "after" && !patch.anchor.empty()) {
                size_t anchorPos = source.find(patch.anchor, openBrace);
                if (anchorPos != std::string::npos && anchorPos < closeBrace) {
                    size_t insertPos = anchorPos + patch.anchor.length();
                    source.insert(insertPos, wrappedPatch);
                    modified = true;
                    outInjectedHooksOrPatches = true;
                    Log("[HookEngine] Injected patch AFTER anchor into %s%s%s (mod: %s)",
                        patch.targetClass.empty() ? "" : (patch.targetClass + "::").c_str(),
                        patch.targetFunction.c_str(),
                        sectionName ? (" in " + std::string(sectionName)).c_str() : "",
                        patch.modName.c_str());
                }
            }
        }
    }

    if (modified) {
        outputCode = source;
        return true;
    }
    return false;
}

void HookEngine::RegisterModHooks(void* pScriptSys, void* pModule) {
    if (!pScriptSys || !pModule || g_HookDefinitions.empty() || !g_RegisterHook) return;

    asIScriptModule* mod = (asIScriptModule*)pModule;
    const char* modName = mod->GetName();
    // Only register hooks into the primary gameplay "Scripts" module
    if (modName && strcmp(modName, "Scripts") != 0) {
        return;
    }

    for (const auto& def : g_HookDefinitions) {
        asIScriptFunction* func = mod->GetFunctionByName(def.hookName.c_str());
        if (!func) {
            uint32_t cnt = mod->GetFunctionCount();
            for (uint32_t i = 0; i < cnt; i++) {
                asIScriptFunction* f = mod->GetFunctionByIndex(i);
                if (f && f->GetName() && strcmp(f->GetName(), def.hookName.c_str()) == 0) {
                    func = f;
                    break;
                }
            }
        }

        if (func) {
            g_RegisterHook(pScriptSys, func);
            Log("[HookEngine] Registered mod hook '%s' (%p) into ScriptSystem (%p)",
                def.hookName.c_str(), func, pScriptSys);
            if (g_ConsolePrint) {
                g_ConsolePrint(0, "\\c00ffaa[HookEngine]\\d Registered custom hook: %s\n", def.hookName.c_str());
            }
        } else {
            Log("[HookEngine] Hook subscriber '%s' not present in module '%s' (mod '%s' may not be active on current profile)",
                def.hookName.c_str(), modName ? modName : "<unknown>", def.modName.c_str());
        }
    }
}

void HookEngine::OnModuleBuildComplete(void* pModule) {
    if (pModule) {
        g_HelperInjectedModules.erase(pModule);
    }
}

size_t HookEngine::GetHookCount() {
    return g_HookDefinitions.size();
}

size_t HookEngine::GetPatchCount() {
    return g_PatchDefinitions.size();
}
