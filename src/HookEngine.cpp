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
static std::vector<ModScriptRecord> g_DiscoveredModScripts;

// Helpers: String manipulation
static std::string EscapeAngelScriptString(const std::string& str) {
    std::string out;
    out.reserve(str.length() + 8);
    for (char c : str) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

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

    // 3. Scan .bin package buffer for AngelScript (.as) files to detect mod conflicts
    std::set<std::string> seenScripts;
    const char* pBuf = (const char*)buffer.data();
    size_t pos = 0;
    while (pos < fileSize) {
        if (pos + 8 >= fileSize) break;
        if ((pBuf[pos] == 's' || pBuf[pos] == 'S') &&
            (_strnicmp(pBuf + pos, "scripts/", 8) == 0 || _strnicmp(pBuf + pos, "scripts\\", 8) == 0))
        {
            size_t start = pos;
            size_t p = start;
            while (p < fileSize && p - start < 260) {
                char c = pBuf[p];
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '_' || c == '/' || c == '\\' || c == '.' || c == '-') {
                    p++;
                } else {
                    break;
                }
            }
            std::string candidate(pBuf + start, p - start);
            std::string candLower = ToLower(candidate);
            if (candLower.length() >= 3 && candLower.substr(candLower.length() - 3) == ".as") {
                std::string normPath = NormalizePath(candidate);
                if (seenScripts.insert(normPath).second) {
                    HookEngine::RegisterModScript(modName, modName, normPath);
                    HookEngine::RegisterModScript(GetBasename(binPath), modName, normPath);
                }
            }
            pos = (p > start ? p : pos + 8);
        } else {
            pos++;
        }
    }
}

static void ScanDirectoryForScripts(
    const std::string& baseDir,
    const std::string& currentSubDir,
    const std::string& modId,
    const std::string& modName
) {
    std::string searchDir = currentSubDir.empty() ? baseDir : (baseDir + "\\" + currentSubDir);
    std::string pattern = searchDir + "\\*";

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }

        std::string fileName = findData.cFileName;
        std::string relPath = currentSubDir.empty() ? fileName : (currentSubDir + "/" + fileName);

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ScanDirectoryForScripts(baseDir, relPath, modId, modName);
        } else {
            std::string lowerName = ToLower(fileName);
            if (lowerName.length() >= 3 && lowerName.substr(lowerName.length() - 3) == ".as") {
                if (lowerName.length() >= 6 && lowerName.substr(lowerName.length() - 6) == ".patch") continue;
                if (lowerName.length() >= 4 && lowerName.substr(lowerName.length() - 4) == ".inc") continue;
                if (g_RegisteredSnippetFiles.count(NormalizePath(relPath)) > 0) continue;
                if (g_RegisteredSnippetFiles.count(GetBasename(fileName)) > 0) continue;

                std::string fullRelScript = "scripts/" + relPath;
                if (ToLower(relPath).substr(0, 8) == "scripts/") {
                    fullRelScript = relPath;
                }
                HookEngine::RegisterModScript(modId, modName, fullRelScript);
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
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
                std::string parsedModName = folderName;
                if (GetFileAttributesA(infoPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    LoadConfigFile(infoPath, fullPath, folderName);
                    std::string xmlStr = ReadFileToString(infoPath);
                    std::regex nameRegex("<string\\s+name=[\"']name[\"']>([^<]+)</string>", std::regex::icase);
                    std::smatch nameMatch;
                    if (std::regex_search(xmlStr, nameMatch, nameRegex)) {
                        parsedModName = Trim(nameMatch[1].str());
                    }
                }

                std::string hooksXml = fullPath + "\\hooks.xml";
                if (GetFileAttributesA(hooksXml.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    LoadConfigFile(hooksXml, fullPath, folderName);
                }

                std::string scriptsDir = fullPath + "\\scripts";
                if (GetFileAttributesA(scriptsDir.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    ScanDirectoryForScripts(scriptsDir, "", folderName, parsedModName);
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
    g_DiscoveredModScripts.clear();

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

    Log("[HookEngine] Configurations loaded: %zu dynamic hooks, %zu line patches, %zu snippet files, %zu mod scripts registered",
        g_HookDefinitions.size(), g_PatchDefinitions.size(), g_RegisteredSnippetFiles.size(), g_DiscoveredModScripts.size());
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

    // Mod Conflict Detector: In-memory patch of ModlistWindow.as
    if (secStr.find("modlistwindow.as") != std::string::npos) {
        if (PatchModlistWindow(source)) {
            modified = true;
            outInjectedHooksOrPatches = true;
            Log("[HookEngine] Injected Mod Conflict Detector into %s", sectionName ? sectionName : "<unknown>");
            if (g_ConsolePrint) {
                g_ConsolePrint(CONSOLE_CHANNEL_INFO, "[HookEngine] Injected Mod Conflict Detector into ModlistWindow\n");
            }
        }
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
                g_ConsolePrint(CONSOLE_CHANNEL_INFO, "[HookEngine] Registered custom hook: %s\n", def.hookName.c_str());
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

size_t HookEngine::GetDiscoveredModScriptCount() {
    return g_DiscoveredModScripts.size();
}

void HookEngine::RegisterModScript(const std::string& modId, const std::string& modName, const std::string& scriptPath) {
    if (modId.empty() || scriptPath.empty()) return;
    std::string normPath = NormalizePath(scriptPath);
    for (const auto& rec : g_DiscoveredModScripts) {
        if (_stricmp(rec.modId.c_str(), modId.c_str()) == 0 &&
            _stricmp(rec.scriptPath.c_str(), normPath.c_str()) == 0) {
            return;
        }
    }
    ModScriptRecord rec;
    rec.modId = modId;
    rec.modName = modName.empty() ? modId : modName;
    rec.scriptPath = normPath;
    g_DiscoveredModScripts.push_back(rec);
}

void HookEngine::ClearDiscoveredModScripts() {
    g_DiscoveredModScripts.clear();
}

static std::string GenerateModConflictDetectorCode() {
    std::stringstream ss;
    ss << "\n// [HookEngine] Mod Conflict Detector\n";
    ss << "namespace ModConflictDetector\n{\n";
    ss << "\tclass ModScriptEntry\n\t{\n";
    ss << "\t\tstring modId;\n";
    ss << "\t\tstring modName;\n";
    ss << "\t\tstring scriptPath;\n\n";
    ss << "\t\tModScriptEntry(const string &in id, const string &in name, const string &in path)\n";
    ss << "\t\t{\n";
    ss << "\t\t\tmodId = id;\n";
    ss << "\t\t\tmodName = name;\n";
    ss << "\t\t\tscriptPath = path;\n";
    ss << "\t\t}\n";
    ss << "\t}\n\n";
    ss << "\tarray<ModScriptEntry@> g_modScripts;\n";
    ss << "\tbool g_initialized = false;\n\n";
    ss << "\tvoid Initialize()\n\t{\n";
    ss << "\t\tif (g_initialized)\n\t\t\treturn;\n";
    ss << "\t\tg_initialized = true;\n";

    for (const auto& rec : g_DiscoveredModScripts) {
        ss << "\t\tg_modScripts.insertLast(ModScriptEntry(\""
           << EscapeAngelScriptString(rec.modId) << "\", \""
           << EscapeAngelScriptString(rec.modName) << "\", \""
           << EscapeAngelScriptString(rec.scriptPath) << "\"));\n";
    }

    ss << "\t}\n\n";

    ss << "\tbool ModMatches(ResourceMod@ mod, const string &in targetId, const string &in targetName)\n\t{\n";
    ss << "\t\tif (mod is null)\n\t\t\treturn false;\n";
    ss << "\t\tstring mId = mod.ID.toLower();\n";
    ss << "\t\tstring mName = mod.Name.toLower();\n";
    ss << "\t\tstring tId = targetId.toLower();\n";
    ss << "\t\tstring tName = targetName.toLower();\n";
    ss << "\t\tif (mId == tId || mName == tName)\n\t\t\treturn true;\n";
    ss << "\t\tif (tId.length() >= 4 && tId.substr(tId.length() - 4) == \".bin\" && mId == tId.substr(0, tId.length() - 4))\n\t\t\treturn true;\n";
    ss << "\t\tif (mId.length() >= 4 && mId.substr(mId.length() - 4) == \".bin\" && mId.substr(0, mId.length() - 4) == tId)\n\t\t\treturn true;\n";
    ss << "\t\treturn false;\n";
    ss << "\t}\n\n";

    ss << "\tbool IsModActiveOnProfile(const string &in targetId, const string &in targetName)\n\t{\n";
    ss << "\t\tauto enabledMods = PersistentSaves::GetEnabledResourceMods();\n";
    ss << "\t\tif (enabledMods is null)\n\t\t\treturn false;\n";
    ss << "\t\tfor (uint k = 0; k < enabledMods.length(); k++)\n\t\t{\n";
    ss << "\t\t\tif (ModMatches(enabledMods[k], targetId, targetName))\n\t\t\t\treturn true;\n";
    ss << "\t\t}\n";
    ss << "\t\treturn false;\n";
    ss << "\t}\n\n";

    ss << "\tbool HasConflict(ResourceMod@ targetMod)\n\t{\n";
    ss << "\t\tInitialize();\n";
    ss << "\t\tif (targetMod is null)\n\t\t\treturn false;\n";
    ss << "\t\tauto enabledMods = PersistentSaves::GetEnabledResourceMods();\n";
    ss << "\t\tif (enabledMods is null || enabledMods.length() == 0)\n\t\t\treturn false;\n";
    ss << "\t\tfor (uint i = 0; i < g_modScripts.length(); i++)\n\t\t{\n";
    ss << "\t\t\tauto ms = g_modScripts[i];\n";
    ss << "\t\t\tif (!ModMatches(targetMod, ms.modId, ms.modName))\n\t\t\t\tcontinue;\n";
    ss << "\t\t\tfor (uint j = 0; j < g_modScripts.length(); j++)\n\t\t\t{\n";
    ss << "\t\t\t\tif (i == j)\n\t\t\t\t\tcontinue;\n";
    ss << "\t\t\t\tauto other = g_modScripts[j];\n";
    ss << "\t\t\t\tif (other.scriptPath.toLower() != ms.scriptPath.toLower())\n\t\t\t\t\tcontinue;\n";
    ss << "\t\t\t\tif (ModMatches(targetMod, other.modId, other.modName))\n\t\t\t\t\tcontinue;\n";
    ss << "\t\t\t\tif (IsModActiveOnProfile(other.modId, other.modName))\n\t\t\t\t\treturn true;\n";
    ss << "\t\t\t}\n";
    ss << "\t\t}\n";
    ss << "\t\treturn false;\n";
    ss << "\t}\n\n";

    ss << "\tstring GetConflictWarning(ResourceMod@ targetMod)\n\t{\n";
    ss << "\t\tInitialize();\n";
    ss << "\t\tif (targetMod is null)\n\t\t\treturn \"\";\n";
    ss << "\t\tauto enabledMods = PersistentSaves::GetEnabledResourceMods();\n";
    ss << "\t\tif (enabledMods is null || enabledMods.length() == 0)\n\t\t\treturn \"\";\n";
    ss << "\t\tbool isTargetEnabled = PersistentSaves::IsModEnabled(targetMod);\n";
    ss << "\t\tarray<string> clashList;\n";
    ss << "\t\tfor (uint i = 0; i < g_modScripts.length(); i++)\n\t\t{\n";
    ss << "\t\t\tauto ms = g_modScripts[i];\n";
    ss << "\t\t\tif (!ModMatches(targetMod, ms.modId, ms.modName))\n\t\t\t\tcontinue;\n";
    ss << "\t\t\tfor (uint j = 0; j < g_modScripts.length(); j++)\n\t\t\t{\n";
    ss << "\t\t\t\tif (i == j)\n\t\t\t\t\tcontinue;\n";
    ss << "\t\t\t\tauto other = g_modScripts[j];\n";
    ss << "\t\t\t\tif (other.scriptPath.toLower() != ms.scriptPath.toLower())\n\t\t\t\t\tcontinue;\n";
    ss << "\t\t\t\tif (ModMatches(targetMod, other.modId, other.modName))\n\t\t\t\t\tcontinue;\n";
    ss << "\t\t\t\tif (IsModActiveOnProfile(other.modId, other.modName))\n\t\t\t\t{\n";
    ss << "\t\t\t\t\tstring entry = \" - \" + ms.scriptPath + \" (clashes with: \" + other.modName + \")\";\n";
    ss << "\t\t\t\t\tbool exists = false;\n";
    ss << "\t\t\t\t\tfor (uint c = 0; c < clashList.length(); c++)\n\t\t\t\t\t{\n";
    ss << "\t\t\t\t\t\tif (clashList[c] == entry) { exists = true; break; }\n";
    ss << "\t\t\t\t\t}\n";
    ss << "\t\t\t\t\tif (!exists)\n\t\t\t\t\t\tclashList.insertLast(entry);\n";
    ss << "\t\t\t\t}\n";
    ss << "\t\t\t}\n";
    ss << "\t\t}\n";
    ss << "\t\tif (clashList.length() == 0)\n\t\t\treturn \"\";\n\n";
    ss << "\t\tstring warning = \"\";\n";
    ss << "\t\tif (isTargetEnabled)\n";
    ss << "\t\t{\n";
    ss << "\t\t\twarning = \"\\\\cff2222[!] SCRIPT FILE CONFLICT DETECTED\\\\d\\n\" +\n";
    ss << "\t\t\t          \"This enabled mod overwrites AngelScript files also provided by other enabled mods:\\n\";\n";
    ss << "\t\t}\n";
    ss << "\t\telse\n";
    ss << "\t\t{\n";
    ss << "\t\t\twarning = \"\\\\cffaa22[!] SCRIPT FILE CONFLICT (IF ENABLED)\\\\d\\n\" +\n";
    ss << "\t\t\t          \"Enabling this mod will overwrite AngelScript files with currently enabled mods:\\n\";\n";
    ss << "\t\t}\n";
    ss << "\t\tfor (uint c = 0; c < clashList.length(); c++)\n\t\t{\n";
    ss << "\t\t\twarning += clashList[c] + \"\\n\";\n";
    ss << "\t\t}\n";
    ss << "\t\treturn warning;\n";
    ss << "\t}\n\n";

    ss << "\tstring GetSummaryOfActiveConflicts()\n\t{\n";
    ss << "\t\tInitialize();\n";
    ss << "\t\tauto enabledMods = PersistentSaves::GetEnabledResourceMods();\n";
    ss << "\t\tif (enabledMods is null || enabledMods.length() < 2)\n\t\t\treturn \"\";\n";
    ss << "\t\tarray<string> conflictLines;\n";
    ss << "\t\tfor (uint i = 0; i < g_modScripts.length(); i++)\n\t\t{\n";
    ss << "\t\t\tauto ms1 = g_modScripts[i];\n";
    ss << "\t\t\tif (!IsModActiveOnProfile(ms1.modId, ms1.modName))\n\t\t\t\tcontinue;\n";
    ss << "\t\t\tfor (uint j = i + 1; j < g_modScripts.length(); j++)\n\t\t\t{\n";
    ss << "\t\t\t\tauto ms2 = g_modScripts[j];\n";
    ss << "\t\t\t\tif (ms1.scriptPath.toLower() != ms2.scriptPath.toLower())\n\t\t\t\t\tcontinue;\n";
    ss << "\t\t\t\tif (ms1.modId.toLower() == ms2.modId.toLower())\n\t\t\t\t\tcontinue;\n";
    ss << "\t\t\t\tif (!IsModActiveOnProfile(ms2.modId, ms2.modName))\n\t\t\t\t\tcontinue;\n";
    ss << "\t\t\t\tstring line = \"* \" + ms1.scriptPath + \" (\" + ms1.modName + \" vs \" + ms2.modName + \")\";\n";
    ss << "\t\t\t\tbool exists = false;\n";
    ss << "\t\t\t\tfor (uint c = 0; c < conflictLines.length(); c++)\n\t\t\t\t{\n";
    ss << "\t\t\t\t\tif (conflictLines[c] == line) { exists = true; break; }\n";
    ss << "\t\t\t\t}\n";
    ss << "\t\t\t\tif (!exists)\n\t\t\t\t\tconflictLines.insertLast(line);\n";
    ss << "\t\t\t}\n";
    ss << "\t\t}\n";
    ss << "\t\tif (conflictLines.length() == 0)\n\t\t\treturn \"\";\n\n";
    ss << "\t\tstring summary = \"\";\n";
    ss << "\t\tuint maxShow = 6;\n";
    ss << "\t\tuint countToShow = conflictLines.length() < maxShow ? conflictLines.length() : maxShow;\n";
    ss << "\t\tfor (uint c = 0; c < countToShow; c++)\n\t\t{\n";
    ss << "\t\t\tsummary += conflictLines[c] + \"\\n\";\n";
    ss << "\t\t}\n";
    ss << "\t\tif (conflictLines.length() > maxShow)\n\t\t{\n";
    ss << "\t\t\tsummary += \"... and \" + (conflictLines.length() - maxShow) + \" more file(s)\\n\";\n";
    ss << "\t\t}\n";
    ss << "\t\treturn summary;\n";
    ss << "\t}\n";
    ss << "}\n\n";
    return ss.str();
}

bool HookEngine::PatchModlistWindow(std::string& source) {
    if (source.find("class ModlistWindow") == std::string::npos) {
        return false;
    }

    bool anyPatched = false;

    // Anchor 1: Badge in RefreshList()
    const std::string kBadgeTarget = "cast<TextWidget>(wNewMod.GetWidgetById(\"text\")).SetText(mod.Name);";
    size_t posBadge = source.find(kBadgeTarget);
    if (posBadge != std::string::npos) {
        std::string badgeReplacement =
            "string displayName = mod.Name;\n"
            "\t\t\tif (PersistentSaves::IsModEnabled(mod) && ModConflictDetector::HasConflict(mod))\n"
            "\t\t\t\tdisplayName += \" \\cff2222[! Conflict]\\d\";\n"
            "\t\t\tcast<TextWidget>(wNewMod.GetWidgetById(\"text\")).SetText(displayName);";
        source.replace(posBadge, kBadgeTarget.length(), badgeReplacement);
        anyPatched = true;
    }

    // Anchor 2: Null-guard and Dialog callback handling in OnFunc
    const std::string kOnFuncHeader = "void OnFunc(Widget@ sender, const string &in name) override";
    size_t posOnFunc = source.find(kOnFuncHeader);
    if (posOnFunc != std::string::npos) {
        size_t openBrace = source.find('{', posOnFunc);
        if (openBrace != std::string::npos) {
            std::string dialogGuard =
                "\n\t\tif (name == \"mod-conflict-warning yes\")\n"
                "\t\t{\n"
                "\t\t\tm_closing = true;\n"
                "\t\t\treturn;\n"
                "\t\t}\n"
                "\t\telse if (name == \"mod-conflict-warning\" || name == \"mod-conflict-warning no\")\n"
                "\t\t{\n"
                "\t\t\treturn;\n"
                "\t\t}\n"
                "\t\tif (sender is null)\n"
                "\t\t\treturn;\n";
            source.insert(openBrace + 1, dialogGuard);
            anyPatched = true;
        }
    }

    // Anchor 3: Hover description warning in OnFunc("hover")
    const std::string kDescTarget = "cast<TextWidget>(m_widget.GetWidgetById(\"mod-desc\")).SetText(descText);";
    size_t posDesc = source.find(kDescTarget);
    if (posDesc != std::string::npos) {
        std::string descReplacement =
            "string conflictWarn = ModConflictDetector::GetConflictWarning(senderMod);\n"
            "\t\t\tif (!conflictWarn.isEmpty())\n"
            "\t\t\t\tdescText += \"\\n\\n\" + conflictWarn;\n"
            "\t\t\tcast<TextWidget>(m_widget.GetWidgetById(\"mod-desc\")).SetText(descText);";
        source.replace(posDesc, kDescTarget.length(), descReplacement);
        anyPatched = true;
    }

    // Anchor 4: Intercept "close" to show confirmation dialog if conflicts exist
    size_t posClose = source.find("name == \"close\"");
    if (posClose != std::string::npos) {
        size_t openBrace = source.find('{', posClose);
        if (openBrace != std::string::npos) {
            size_t closeBrace = FindMatchingClosingBrace(source, openBrace);
            if (closeBrace != std::string::npos) {
                std::string closeBody =
                    "{\n"
                    "\t\t\tstring conflictsSummary = ModConflictDetector::GetSummaryOfActiveConflicts();\n"
                    "\t\t\tif (!conflictsSummary.isEmpty() && g_gameMode !is null)\n"
                    "\t\t\t{\n"
                    "\t\t\t\tg_gameMode.ShowDialog(\n"
                    "\t\t\t\t\t\"mod-conflict-warning\",\n"
                    "\t\t\t\t\t\"WARNING: Mod Conflicts Detected!\\n\\nMultiple enabled mods overwrite the same AngelScript (.as) files:\\n\\n\" + conflictsSummary + \"\\nAre you sure you want to continue?\",\n"
                    "\t\t\t\t\t\"Continue\",\n"
                    "\t\t\t\t\t\"Go Back\",\n"
                    "\t\t\t\t\tthis\n"
                    "\t\t\t\t);\n"
                    "\t\t\t}\n"
                    "\t\t\telse\n"
                    "\t\t\t{\n"
                    "\t\t\t\tm_closing = true;\n"
                    "\t\t\t}\n"
                    "\t\t}";
                source.replace(openBrace, closeBrace - openBrace + 1, closeBody);
                anyPatched = true;
            }
        }
    }

    if (anyPatched) {
        source = GenerateModConflictDetectorCode() + "\n" + source;
        return true;
    }

    return false;
}
