#include <windows.h>
#include <string>
#include <vector>
#include <set>
#include <stdio.h>
#include "MinHook.h"
#include "HookEngine.h"

// RVA definitions for HWR2.exe (Release)
#define RVA_CONSOLE_EXECUTE_LINE    0x1F44C0
#define RVA_CONSOLE_PRINT           0x110D80
#define RVA_AS_CREATE_ENGINE        0x454AD0
#define RVA_GLOBAL_ENGINE           0x69F320
#define RVA_AS_MODULE_ADD_SECTION   0x48A410
#define RVA_AS_MODULE_BUILD         0x48A070
#define RVA_PROCESS_METADATA        0x12D540
#define RVA_REGISTER_HOOK           0x130360
#define ENGINE_SCRIPT_ENGINE_OFFSET 0x5D0
#define RVA_GET_MODDED              0x1F740
#define RVA_E_CHEATS                0x632330

// Forward declare proxy export initializer
extern "C" void InitAllWinmmExports();

// Diagnostic file logger forward declaration
void Log(const char* fmt, ...);

// Structures matching HWR2 MSVC x64 ABI
struct StdString {
    union {
        char buf[16];
        char* ptr;
    };
    size_t length;
    size_t capacity;

    const char* c_str() const {
        return (capacity >= 16) ? ptr : buf;
    }
};

// Function pointer types for vanilla game functions
typedef void (*tConsolePrint)(int category, const char* fmt, ...);
typedef void (*tConsoleExecuteLine)(void* pConsole, StdString* pLine);
typedef void* (*t_asCreateScriptEngine)(uint32_t version);
typedef int (__fastcall *tAddScriptSection)(void* pModule, const char* sectionName, const char* code, size_t codeLength, int lineOffset);

tConsolePrint g_ConsolePrint = nullptr;
static tConsoleExecuteLine g_OriginalExecuteLine = nullptr;
static t_asCreateScriptEngine g_Orig_asCreateScriptEngine = nullptr;
static tAddScriptSection g_OriginalAddScriptSection = nullptr;

typedef bool (*tGetModded)();
static tGetModded g_GetModded = nullptr;
static uint8_t* g_pCheatsEnabled = nullptr;

// Query whether current active profile is modded (SEH guarded against uninitialized profile state)
static bool IsProfileModded() {
    if (!g_GetModded) return false;
    __try {
        return g_GetModded();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[Patch SEH] Exception caught in IsProfileModded (no active profile loaded)");
        return false;
    }
}

// Query whether e_cheats is currently enabled (SEH guarded)
static bool IsCheatsEnabled() {
    if (!g_pCheatsEnabled) return false;
    __try {
        return (*g_pCheatsEnabled != 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[Patch SEH] Exception caught in IsCheatsEnabled");
        return false;
    }
}

#define RVA_RESOURCE_TASK_LAMBDA    0x7BAD0
#define RVA_SECONDARY_TASK_LAMBDA   0x7BB40

typedef void (__fastcall *tResourceTaskLambda)(void* thisPtr, void* pContext);
static tResourceTaskLambda g_OriginalResourceTaskLambda = nullptr;

typedef void (__fastcall *tSecondaryTaskLambda)(void* thisPtr, void* pContext);
static tSecondaryTaskLambda g_OriginalSecondaryTaskLambda = nullptr;

typedef void (__fastcall *tProcessMetadata)(void* pScriptSys, void* pBuilder);
static tProcessMetadata g_OriginalProcessMetadata = nullptr;

typedef void (__fastcall *tRegisterHook)(void* pScriptSys, void* pFunc);
tRegisterHook g_RegisterHook = nullptr;

typedef int (__fastcall *tModuleBuild)(void* pModule);
static tModuleBuild g_OriginalModuleBuild = nullptr;

// Hook for asCModule::AddScriptSection
// Intercepts script compilation in memory and injects dynamic hooks and line patches non-destructively
static int __fastcall Hooked_AddScriptSection(
    void* pModule,
    const char* sectionName,
    const char* code,
    size_t codeLength,
    int lineOffset
) {
    size_t actualLen = (codeLength > 0) ? codeLength : (code ? strlen(code) : 0);
    std::string modifiedCode;

    // 1. Safety net: If section name matches any snippet file (.patch, .inc, or registered patch file),
    // neutralize it with a dummy comment so the engine's file scanner doesn't fail on bare snippet statements!
    if (HookEngine::IsSnippetSection(sectionName)) {
        Log("[HookEngine] Neutralized snippet section: %s", sectionName ? sectionName : "<null>");
        static const char* kNeutralized = "// Neutralized snippet file\n";
        return g_OriginalAddScriptSection(pModule, sectionName, kNeutralized, strlen(kNeutralized), lineOffset);
    }

    // 2. If this section is loaded by the game engine and contains [Hook] metadata tags,
    // preprocess it so AngelScript core parser doesn't throw 'SE : Unexpected token '[''
    if (code && HookEngine::PreprocessModScript(std::string(code, actualLen), modifiedCode)) {
        Log("[HookEngine] Preprocessed mod script section '%s' (stripped [Hook] metadata)", sectionName ? sectionName : "");
        return g_OriginalAddScriptSection(
            pModule,
            sectionName,
            modifiedCode.c_str(),
            modifiedCode.length(),
            lineOffset
        );
    }

    // 3. Inject hook calls (Hooks::Call) and line patches into target methods
    bool injectedHooksOrPatches = false;
    if (HookEngine::ProcessScriptSection(pModule, sectionName, code, actualLen, modifiedCode, injectedHooksOrPatches)) {
        Log("[HookEngine] Section '%s' injected with custom hooks/patches! (size: %zu -> %zu)",
            sectionName ? sectionName : "<null>", actualLen, modifiedCode.length());
        if (g_ConsolePrint && injectedHooksOrPatches) {
            g_ConsolePrint(0, "\\c00ffaa[HookEngine]\\d Injected custom hooks/patches into %s\n",
                sectionName ? sectionName : "");
        }
        return g_OriginalAddScriptSection(
            pModule,
            sectionName,
            modifiedCode.c_str(),
            modifiedCode.length(),
            lineOffset
        );
    }

    return g_OriginalAddScriptSection(pModule, sectionName, code, codeLength, lineOffset);
}

uintptr_t g_BaseAddress = 0;
static void* g_pCapturedEngine = nullptr;

// Diagnostic file logger
void Log(const char* fmt, ...);

// Safety guard hook for resource task lambda (RVA 0x7BAD0)
// Prevents access violation 0xC0000005 when reloading assets / switching profiles with stale resource pointers
static void __fastcall Hooked_ResourceTaskLambda(void* thisPtr, void* pContext) {
    __try {
        if (!thisPtr) return;
        void* pObj = *(void**)((uintptr_t)thisPtr + 8);
        if (!pObj) return;
        void* pResource = *(void**)((uintptr_t)pObj + 0x88);
        if (!pResource) return; // Null resource is normal in vanilla engine (rcx == 0 exit)

        if ((uintptr_t)pResource < 0x10000) {
            Log("[Crash Guard] Prevented crash: invalid resource pointer %p at 0x7BAD0 during reload.", pResource);
            if (g_ConsolePrint) {
                g_ConsolePrint(0, "\\c00ffaa[Crash Guard]\\d Prevented crash at 0x7BAF1 (invalid resource pointer %p during reload)\n", pResource);
            }
            return;
        }

        g_OriginalResourceTaskLambda(thisPtr, pContext);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DWORD exc = GetExceptionCode();
        Log("[Crash Guard] Intercepted access violation (0x%08X) at 0x7BAF1 during reload! Crash prevented.", exc);
        if (g_ConsolePrint) {
            g_ConsolePrint(0, "\\c00ffaa[Crash Guard]\\d Successfully prevented game crash! (Access Violation 0x%08X intercepted at 0x7BAF1)\n", exc);
        }
    }
}

// Safety guard hook for secondary task lambda (RVA 0x7BB40)
static void __fastcall Hooked_SecondaryTaskLambda(void* thisPtr, void* pContext) {
    __try {
        if (!thisPtr) return;
        void* pObj = *(void**)((uintptr_t)thisPtr + 0x10);
        if (!pObj) return;

        if ((uintptr_t)pObj < 0x10000) {
            Log("[Crash Guard] Prevented crash: invalid secondary pointer %p at 0x7BB40 during reload.", pObj);
            if (g_ConsolePrint) {
                g_ConsolePrint(0, "\\c00ffaa[Crash Guard]\\d Prevented crash at 0x7BB40 (invalid pointer %p during reload)\n", pObj);
            }
            return;
        }

        g_OriginalSecondaryTaskLambda(thisPtr, pContext);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DWORD exc = GetExceptionCode();
        Log("[Crash Guard] Intercepted access violation (0x%08X) at 0x7BB40 during reload! Crash prevented.", exc);
        if (g_ConsolePrint) {
            g_ConsolePrint(0, "\\c00ffaa[Crash Guard]\\d Successfully prevented game crash! (Access Violation 0x%08X intercepted at 0x7BB40)\n", exc);
        }
    }
}

// Diagnostic file logger
void Log(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    FILE* f = fopen("patch_scr.log", "a");
    if (f) {
        fputs(buf, f);
        fputc('\n', f);
        fclose(f);
    }
    OutputDebugStringA(buf);
}

static std::vector<void*> g_CapturedEngines;
static int g_ForcedEngineIndex = -1;

// Hooked asCreateScriptEngine (RVA 0x454AD0)
// Transparently captures all asIScriptEngine* instances created by the game!
static void* Hooked_asCreateScriptEngine(uint32_t version) {
    void* engine = g_Orig_asCreateScriptEngine(version);
    if (engine) {
        bool exists = false;
        for (void* e : g_CapturedEngines) {
            if (e == engine) { exists = true; break; }
        }
        if (!exists) {
            g_CapturedEngines.push_back(engine);
            Log("[Patch] Captured engine #%zu from asCreateScriptEngine: %p (version=0x%X)", g_CapturedEngines.size() - 1, engine, version);
        }
    }
    g_pCapturedEngine = engine;
    return engine;
}

// Helper: Safely obtain active AngelScript engine
// Traverses GameEngine -> ActiveWorld (+0x98) -> ScriptSystem (+0x60) -> asIScriptEngine* (+0x158)
static void* GetScriptEngine(void* pConsole) {
    if (g_ForcedEngineIndex >= 0 && g_ForcedEngineIndex < (int)g_CapturedEngines.size()) {
        Log("[Patch] Using manually forced engine #%d: %p", g_ForcedEngineIndex, g_CapturedEngines[g_ForcedEngineIndex]);
        return g_CapturedEngines[g_ForcedEngineIndex];
    }

    auto SafeRead = [](void* ptr) -> void* {
        __try {
            if (ptr && !IsBadReadPtr(ptr, sizeof(void*))) {
                return *(void**)ptr;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        return nullptr;
    };

    void* candidates[4] = { nullptr };
    int numCandidates = 0;

    if (pConsole) {
        void* c1 = SafeRead((char*)pConsole + 8);
        if (c1) candidates[numCandidates++] = c1;

        void* c2 = SafeRead(pConsole);
        if (c2 && c2 != c1) candidates[numCandidates++] = c2;
    }

    if (g_BaseAddress) {
        void* pGlobalObj = SafeRead((void*)(g_BaseAddress + 0x648820));
        if (pGlobalObj) {
            void* c3 = SafeRead((char*)pGlobalObj + 0x38);
            if (c3) candidates[numCandidates++] = c3;
        }
    }

    for (int i = 0; i < numCandidates; i++) {
        void* pEng = candidates[i];
        if (!pEng) continue;

        // Active World (+0x98) -> Script System (+0x60) -> Script Engine (+0x158)
        void* pActiveWorld = SafeRead((char*)pEng + 0x98);
        if (pActiveWorld) {
            void* pScriptSys = SafeRead((char*)pActiveWorld + 0x60);
            if (pScriptSys) {
                void* se = SafeRead((char*)pScriptSys + 0x158);
                if (se) {
                    Log("[Patch] Resolved ACTIVE script engine from ActiveWorld (+0x98): %p", se);
                    return se;
                }
            }
        }

        // Fallback World (+0x100) -> Script System (+0x18) -> Script Engine (+0x158)
        void* pFallbackWorld = SafeRead((char*)pEng + 0x100);
        if (pFallbackWorld) {
            void* pScriptSys = SafeRead((char*)pFallbackWorld + 0x18);
            if (pScriptSys) {
                void* se = SafeRead((char*)pScriptSys + 0x158);
                if (se) {
                    Log("[Patch] Resolved fallback script engine from World (+0x100): %p", se);
                    return se;
                }
            }
        }
    }

    // Default to Engine 0 (primary gameplay bootstrap engine), avoiding scratch/preload engines!
    if (!g_CapturedEngines.empty()) {
        Log("[Patch] Defaulting to primary captured engine #0: %p", g_CapturedEngines[0]);
        return g_CapturedEngines[0];
    }

    if (g_pCapturedEngine) return g_pCapturedEngine;
    return nullptr;
}

// Exact AngelScript MSVC x64 Vtable Invokers for HWR2
// Engine vtable invokers
inline void* Engine_GetModule(void* engine, const char* name, int flag) {
    // Slot 47 (+0x178): asIScriptModule* GetModule(const char* module, asEGMFlags flag)
    typedef void* (*tFn)(void*, const char*, int);
    void** vtable = *(void***)engine;
    tFn fn = (tFn)(vtable[47]);
    return fn(engine, name, flag);
}

inline void* Engine_RequestContext(void* engine) {
    // Slot 69 (+0x228): asIScriptContext* RequestContext()
    typedef void* (*tFn)(void*);
    void** vtable = *(void***)engine;
    tFn fn = (tFn)(vtable[69]);
    return fn(engine);
}

inline void* Engine_CreateContext(void* engine) {
    // Slot 61 (+0x1E8): asIScriptContext* CreateContext()
    typedef void* (*tFn)(void*);
    void** vtable = *(void***)engine;
    tFn fn = (tFn)(vtable[61]);
    return fn(engine);
}

inline int Engine_ReturnContext(void* engine, void* ctx) {
    // Slot 70 (+0x230): int ReturnContext(asIScriptContext* ctx)
    typedef int (*tFn)(void*, void*);
    void** vtable = *(void***)engine;
    tFn fn = (tFn)(vtable[70]);
    return fn(engine, ctx);
}

// Hook for ProcessMetadata (RVA 0x12D540)
static void __fastcall Hooked_ProcessMetadata(void* pScriptSys, void* pBuilder) {
    if (g_OriginalProcessMetadata) {
        g_OriginalProcessMetadata(pScriptSys, pBuilder);
    }
}

// Hook for asCModule::Build (RVA 0x48A070)
// Fires every time an AngelScript module finishes compilation across all engine instances.
// Directly resolves ScriptSystem from engine->GetUserData(0) and registers mod hooks!
static int __fastcall Hooked_ModuleBuild(void* pModule) {
    int r = g_OriginalModuleBuild(pModule);
    if (r >= 0 && pModule && (uintptr_t)pModule >= 0x10000) {
        __try {
            void** vtable = *(void***)pModule;
            // Slot 0: asIScriptEngine* GetEngine() const
            typedef void* (*tGetEngine)(void*);
            tGetEngine fnGetEngine = (tGetEngine)vtable[0];
            if (fnGetEngine) {
                void* engine = fnGetEngine(pModule);
                if (engine && (uintptr_t)engine >= 0x10000) {
                    void** engVtable = *(void***)engine;
                    // Slot 82 (+0x290): GetUserData(0)
                    typedef void* (*tGetUserData)(void*, size_t);
                    tGetUserData fnGetUserData = (tGetUserData)engVtable[0x290 / 8];
                    void* pScriptSys = nullptr;
                    if (fnGetUserData) {
                        pScriptSys = fnGetUserData(engine, 0);
                    }
                    if (!pScriptSys) {
                        tGetUserData fnGetUserData84 = (tGetUserData)engVtable[0x2A0 / 8];
                        if (fnGetUserData84) pScriptSys = fnGetUserData84(engine, 0);
                    }

                    if (pScriptSys && (uintptr_t)pScriptSys >= 0x10000) {
                        HookEngine::RegisterModHooks(pScriptSys, pModule);
                    }
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("[HookEngine Exception] Intercepted exception during post-build mod hook registration!");
        }
    }
    HookEngine::OnModuleBuildComplete(pModule);
    return r;
}

// Module vtable invokers
inline int Module_CompileFunction(void* mod, const char* sectionName, const char* code, int lineOffset, uint32_t flags, void** outFunc) {
    // Slot 6 (+0x30): int CompileFunction(const char* sectionName, const char* code, int lineOffset, asDWORD compileFlags, asIScriptFunction** outFunc)
    typedef int (*tFn)(void*, const char*, const char*, int, uint32_t, void**);
    void** vtable = *(void***)mod;
    tFn fn = (tFn)(vtable[6]);
    return fn(mod, sectionName, code, lineOffset, flags, outFunc);
}

inline int Module_RemoveFunction(void* mod, void* func) {
    // Slot 15 (+0x78): int RemoveFunction(asIScriptFunction* func)
    typedef int (*tFn)(void*, void*);
    void** vtable = *(void***)mod;
    tFn fn = (tFn)(vtable[15]);
    return fn(mod, func);
}

// Context vtable invokers (Table 0x5ABA78)
inline int Context_Prepare(void* ctx, void* func) {
    // Slot 3 (+0x18): int Prepare(asIScriptFunction* func)
    typedef int (*tFn)(void*, void*);
    void** vtable = *(void***)ctx;
    tFn fn = (tFn)(vtable[3]);
    return fn(ctx, func);
}

inline int Context_Execute(void* ctx) {
    // Slot 5 (+0x28): int Execute()
    typedef int (*tFn)(void*);
    void** vtable = *(void***)ctx;
    tFn fn = (tFn)(vtable[5]);
    return fn(ctx);
}

inline int Context_Release(void* ctx) {
    // Slot 1 (+0x08): int Release()
    typedef int (*tFn)(void*);
    void** vtable = *(void***)ctx;
    tFn fn = (tFn)(vtable[1]);
    return fn(ctx);
}

inline const char* Context_GetExceptionString(void* ctx) {
    // Slot 35 (+0x118): const char* GetExceptionString()
    typedef const char* (*tFn)(void*);
    void** vtable = *(void***)ctx;
    tFn fn = (tFn)(vtable[35]);
    return fn(ctx);
}

inline int Context_GetExceptionLineNumber(void* ctx) {
    // Slot 33 (+0x108): int GetExceptionLineNumber(int* col, const char** section)
    typedef int (*tFn)(void*, int*, const char**);
    void** vtable = *(void***)ctx;
    tFn fn = (tFn)(vtable[33]);
    return fn(ctx, nullptr, nullptr);
}

// Helper: Trim trailing semicolons or spaces
static std::string CleanExpression(const std::string& str) {
    std::string s = str;
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n' || s.back() == ';')) {
        s.pop_back();
    }
    return s;
}

// Internal compilation and execution with C++ string management
// Matches the exact vanilla beta wrapping using _cgrab and GetSelectedUnit!
static void ExecuteScriptInternal(void* engine, const std::string& snippet, bool isExpression) {
    // Retrieve "Scripts" module (0 = asGM_ONLY_IF_EXISTS)
    void* mod = Engine_GetModule(engine, "Scripts", 0);
    if (!mod) {
        Log("[Patch Warning] Module 'Scripts' not found with flag 0, trying flag 1");
        mod = Engine_GetModule(engine, "Scripts", 1);
    }
    if (!mod) {
        Log("[Patch Warning] Still null, trying ephemeral module ''");
        mod = Engine_GetModule(engine, "", 2); // 2 = asGM_ALWAYS_CREATE
    }

    if (!mod) {
        Log("[Patch Error] Failed to obtain any script module.");
        if (g_ConsolePrint) {
            g_ConsolePrint(0, "\\cff4444[Patch Error] Failed to obtain script module.\\d\n");
        }
        return;
    }

    std::string clean = CleanExpression(snippet);
    std::string wrappedCode;
    void* func = nullptr;
    int r = -1;

    // Strategy matching vanilla beta:
    // 1. Try wrapping with _cgrab (works on objects, handles, primitives, and expressions)
    //    For 's': also injects 'auto sunit = GetSelectedUnit();' into local scope!
    if (isExpression) {
        // "scr <expr>" -> _cgrab(<expr>)
        wrappedCode = "void ExecuteString() {\n  _cgrab(\n" + clean + "\n);\n;}";
        r = Module_CompileFunction(mod, "ExecuteString", wrappedCode.c_str(), -1, 0, &func);
    } else {
        // "s <code/expr>" -> auto sunit = GetSelectedUnit(); _cgrab(<code>)
        wrappedCode = "void ExecuteString() {\n  auto sunit = GetSelectedUnit(); _cgrab(\n" + clean + "\n);\n;}";
        r = Module_CompileFunction(mod, "ExecuteString", wrappedCode.c_str(), -1, 0, &func);
    }

    // 2. If _cgrab compilation failed (e.g., snippet is a statement like "print(...)" or "for (...)"),
    //    fallback to statement execution with sunit available!
    if (r < 0) {
        Log("[Patch] _cgrab compilation failed (code %d), falling back to statement execution...", r);
        if (isExpression) {
            wrappedCode = "void ExecuteString() {\n  " + snippet + ";\n;\n;}";
        } else {
            wrappedCode = "void ExecuteString() {\n  auto sunit = GetSelectedUnit();\n  " + snippet + ";\n;\n;}";
        }
        r = Module_CompileFunction(mod, "ExecuteString", wrappedCode.c_str(), -1, 0, &func);
    }

    // 3. Fallback without sunit if GetSelectedUnit() is not in scope
    if (r < 0) {
        wrappedCode = "void ExecuteString() {\n  " + snippet + ";\n;\n;}";
        r = Module_CompileFunction(mod, "ExecuteString", wrappedCode.c_str(), -1, 0, &func);
    }

    if (r < 0 || !func) {
        Log("[Patch Error] Compile failed with error code %d", r);
        if (g_ConsolePrint) {
            g_ConsolePrint(0, "\\cff4444[AngelScript Compile Error] Failed to compile code (code %d)\\d\n", r);
        }
        return;
    }

    // Obtain execution context
    void* ctx = Engine_RequestContext(engine);
    if (!ctx) {
        ctx = Engine_CreateContext(engine);
    }

    if (!ctx) {
        Log("[Patch Error] Failed to obtain script context.");
        if (g_ConsolePrint) {
            g_ConsolePrint(0, "\\cff4444[Patch Error] Failed to create execution context.\\d\n");
        }
        Module_RemoveFunction(mod, func);
        return;
    }

    int prep_r = Context_Prepare(ctx, func);
    if (prep_r >= 0) {
        int exec_r = Context_Execute(ctx);
        Log("[Patch] Execution finished with code %d", exec_r);

        if (exec_r == 3) { // asEXECUTION_EXCEPTION
            const char* exStr = Context_GetExceptionString(ctx);
            int line = Context_GetExceptionLineNumber(ctx);
            Log("[Patch Exception] Line %d: %s", line, exStr ? exStr : "Unknown");
            if (g_ConsolePrint) {
                g_ConsolePrint(0, "\\cff4444[AngelScript Exception] Line %d: %s\\d\n", line, exStr ? exStr : "Unknown");
            }
        } else if (exec_r != 0) { // not asEXECUTION_FINISHED
            Log("[Patch Warning] Execution status: %d", exec_r);
            if (g_ConsolePrint) {
                g_ConsolePrint(0, "\\cffaa44[AngelScript Warning] Execution status: %d\\d\n", exec_r);
            }
        }
    } else {
        Log("[Patch Error] Context Prepare failed (code %d)", prep_r);
        if (g_ConsolePrint) {
            g_ConsolePrint(0, "\\cff4444[AngelScript Error] Prepare failed (code %d)\\d\n", prep_r);
        }
    }

    // Return context and clean up function
    Engine_ReturnContext(engine, ctx);
    Module_RemoveFunction(mod, func);
    Log("[Patch] Execution cleanup complete.");
}

// Wrapper with SEH to guarantee the game never crashes
static void SafeExecuteSnippet(void* engine, const std::string& snippet, bool isExpression) {
    __try {
        ExecuteScriptInternal(engine, snippet, isExpression);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DWORD code = GetExceptionCode();
        Log("[Patch SEH] Catastrophic exception caught in ExecuteScriptSnippet! Code=0x%08X", code);
        if (g_ConsolePrint) {
            g_ConsolePrint(0, "\\cff4444[Patch SEH Error] Access violation caught (0x%08X). Crash prevented.\\d\n", code);
        }
    }
}

// Core execution engine for "s" and "scr"
static void ExecuteScriptSnippet(void* pConsole, const std::string& snippet, bool isExpression) {
    Log("[Patch] ExecuteScriptSnippet called: expr=%d, code='%s'", isExpression, snippet.c_str());

    // Diagnostic meta-command: list all engines
    if (snippet == "__engines") {
        void* active = GetScriptEngine(pConsole);
        if (g_ConsolePrint) {
            g_ConsolePrint(0, "\\c00ffffCaptured AngelScript Engines (%zu):\\d\n", g_CapturedEngines.size());
            for (size_t i = 0; i < g_CapturedEngines.size(); i++) {
                g_ConsolePrint(0, "  [%zu] %p %s%s\n",
                    i,
                    g_CapturedEngines[i],
                    (g_CapturedEngines[i] == active ? "\\c00ff00[ACTIVE]\\d " : ""),
                    ((int)i == g_ForcedEngineIndex ? "\\cffff00[FORCED]\\d" : ""));
            }
        }
        return;
    }

    // Diagnostic meta-command: switch active engine manually
    if (snippet.rfind("__use ", 0) == 0) {
        int idx = atoi(snippet.c_str() + 6);
        if (idx >= 0 && idx < (int)g_CapturedEngines.size()) {
            g_ForcedEngineIndex = idx;
            if (g_ConsolePrint) {
                g_ConsolePrint(0, "\\c00ff00Forced script engine to [%d] %p\\d\n", idx, g_CapturedEngines[idx]);
            }
        } else if (idx == -1) {
            g_ForcedEngineIndex = -1;
            if (g_ConsolePrint) {
                g_ConsolePrint(0, "\\c00ff00Reset to automatic active engine detection\\d\n");
            }
        } else {
            if (g_ConsolePrint) {
                g_ConsolePrint(0, "\\cff4444Invalid engine index %d (available: 0..%zu)\\d\n", idx, g_CapturedEngines.size() - 1);
            }
        }
        return;
    }

    void* engine = GetScriptEngine(pConsole);
    if (!engine) {
        Log("[Patch Error] AngelScript engine is null.");
        if (g_ConsolePrint) {
            g_ConsolePrint(0, "\\cff4444[Patch Error] AngelScript engine is not available yet.\\d\n");
        }
        return;
    }

    int engIdx = -1;
    for (size_t i = 0; i < g_CapturedEngines.size(); i++) {
        if (g_CapturedEngines[i] == engine) { engIdx = (int)i; break; }
    }
    Log("[Patch] Executing on engine #%d (%p)", engIdx, engine);

    SafeExecuteSnippet(engine, snippet, isExpression);
}

// Detoured Console::ExecuteLine (RVA 0x1F44C0)
// This intercepts raw console inputs before cvar or command dispatch!
static void Hooked_ConsoleExecuteLine(void* pConsole, StdString* pLine) {
    if (pLine) {
        const char* str = pLine->c_str();
        if (str) {
            // Skip leading whitespace
            while (*str == ' ' || *str == '\t') str++;

            bool is_s = (strncmp(str, "s ", 2) == 0 || strcmp(str, "s") == 0);
            bool is_scr = (strncmp(str, "scr ", 4) == 0 || strcmp(str, "scr") == 0);

            if (is_s || is_scr) {
                const char* code = str + (is_scr ? 3 : 1);
                while (*code == ' ' || *code == '\t') code++;

                // Echo the command in the console: "> s <code>"
                if (g_ConsolePrint) {
                    g_ConsolePrint(0, "> %s\n", str);
                }

                if (!IsProfileModded()) {
                    if (g_ConsolePrint) {
                        g_ConsolePrint(0, "\\cff5555[Error]\\d Command '%s' can only be executed on a modded profile.\n", is_scr ? "scr" : "s");
                    }
                    return;
                }

                if (!IsCheatsEnabled()) {
                    if (g_ConsolePrint) {
                        g_ConsolePrint(0, "\\cff5555[Error]\\d Command '%s' can only be executed when e_cheats = 1.\n", is_scr ? "scr" : "s");
                    }
                    return;
                }

                if (*code) {
                    ExecuteScriptSnippet(pConsole, code, is_scr);
                } else {
                    if (g_ConsolePrint) {
                        g_ConsolePrint(0, "\\c8888ffUsage: %s <AngelScript code>\\d\n", is_scr ? "scr" : "s");
                    }
                }
                return; // Handled! Do not fall through to "No such cvar"
            }
        }
    }

    // Delegate all other commands/cvars to vanilla handler
    g_OriginalExecuteLine(pConsole, pLine);
}

// Fast binary pattern scanner across executable sections
static void* PatternScan(HMODULE hMod, const unsigned char* pattern, size_t patternLen) {
    if (!hMod || !pattern || patternLen == 0) return nullptr;

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hMod;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uintptr_t)hMod + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        // Only scan executable code sections (.text)
        if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            uintptr_t start = (uintptr_t)hMod + sec->VirtualAddress;
            size_t size = sec->Misc.VirtualSize;
            if (size < patternLen) continue;

            const unsigned char* pStart = (const unsigned char*)start;
            const unsigned char* pEnd = pStart + size - patternLen;
            for (const unsigned char* p = pStart; p <= pEnd; p++) {
                if (*p == pattern[0] && memcmp(p, pattern, patternLen) == 0) {
                    return (void*)p;
                }
            }
        }
    }
    return nullptr;
}

// Pattern scanner with wildcard '?' support for update resilience
static void* PatternScanMask(HMODULE hMod, const unsigned char* pattern, const char* mask) {
    if (!hMod || !pattern || !mask) return nullptr;
    size_t patternLen = strlen(mask);
    if (patternLen == 0) return nullptr;

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hMod;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uintptr_t)hMod + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            uintptr_t start = (uintptr_t)hMod + sec->VirtualAddress;
            size_t size = sec->Misc.VirtualSize;
            if (size < patternLen) continue;

            const unsigned char* pStart = (const unsigned char*)start;
            const unsigned char* pEnd = pStart + size - patternLen;
            for (const unsigned char* p = pStart; p <= pEnd; p++) {
                bool match = true;
                for (size_t k = 0; k < patternLen; k++) {
                    if (mask[k] != '?' && pattern[k] != p[k]) {
                        match = false;
                        break;
                    }
                }
                if (match) return (void*)p;
            }
        }
    }
    return nullptr;
}

// Background thread initialization to install hooks
static DWORD WINAPI InitThread(LPVOID lpParam) {
    Log("[Patch] InitThread started.");
    g_BaseAddress = (uintptr_t)GetModuleHandleA(NULL);
    if (!g_BaseAddress) {
        Log("[Patch Error] Failed to get main module handle.");
        return 0;
    }

    // 1. Resolve Console::Print: Pattern scan first (survives updates), fallback to hardcoded RVA
    static const unsigned char kSigConsolePrint[] = {
        0x48, 0x89, 0x54, 0x24, 0x10, 0x4C, 0x89, 0x44, 0x24, 0x18, 0x4C, 0x89, 0x4C, 0x24, 0x20, 0x55,
        0x53, 0x56, 0x57, 0x48, 0x83, 0xEC, 0x48, 0x48, 0x8D, 0x6C, 0x24, 0x30
    };
    void* pPrintTarget = PatternScan((HMODULE)g_BaseAddress, kSigConsolePrint, sizeof(kSigConsolePrint));
    if (pPrintTarget) {
        g_ConsolePrint = (tConsolePrint)pPrintTarget;
        Log("[Patch] Found Console::Print via pattern scan: %p (RVA +0x%llX)", pPrintTarget, (unsigned long long)((uintptr_t)pPrintTarget - g_BaseAddress));
    } else {
        g_ConsolePrint = (tConsolePrint)(g_BaseAddress + RVA_CONSOLE_PRINT);
        Log("[Patch] Fallback to RVA for Console::Print: %p", g_ConsolePrint);
    }

    // Initialize MinHook
    if (MH_Initialize() != MH_OK) {
        Log("[Patch Error] MH_Initialize failed.");
        return 0;
    }

    // 2. Resolve asCreateScriptEngine: PE export table first, fallback to RVA
    void* pAsCreateTarget = (void*)GetProcAddress((HMODULE)g_BaseAddress, "asCreateScriptEngine");
    if (pAsCreateTarget) {
        Log("[Patch] Resolved asCreateScriptEngine via PE export table: %p", pAsCreateTarget);
    } else {
        pAsCreateTarget = (void*)(g_BaseAddress + RVA_AS_CREATE_ENGINE);
        Log("[Patch] Fallback to RVA for asCreateScriptEngine: %p", pAsCreateTarget);
    }

    if (MH_CreateHook(pAsCreateTarget, (void*)&Hooked_asCreateScriptEngine, (void**)&g_Orig_asCreateScriptEngine) == MH_OK) {
        MH_EnableHook(pAsCreateTarget);
        Log("[Patch] Hooked asCreateScriptEngine at %p", pAsCreateTarget);
    } else {
        Log("[Patch Warning] Failed to hook asCreateScriptEngine");
    }

    // 3. Resolve Console::ExecuteLine: Pattern scan first (survives updates), fallback to RVA
    static const unsigned char kSigExecuteLine[] = {
        0x48, 0x89, 0x5C, 0x24, 0x18, 0x48, 0x89, 0x54, 0x24, 0x10, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41,
        0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC, 0x24, 0x70, 0xFE, 0xFF, 0xFF, 0x48, 0x81, 0xEC,
        0x90, 0x02, 0x00, 0x00
    };
    void* pTarget = PatternScan((HMODULE)g_BaseAddress, kSigExecuteLine, sizeof(kSigExecuteLine));
    if (pTarget) {
        Log("[Patch] Found Console::ExecuteLine via pattern scan: %p (RVA +0x%llX)", pTarget, (unsigned long long)((uintptr_t)pTarget - g_BaseAddress));
    } else {
        pTarget = (void*)(g_BaseAddress + RVA_CONSOLE_EXECUTE_LINE);
        Log("[Patch] Fallback to RVA for Console::ExecuteLine: %p", pTarget);
    }

    if (MH_CreateHook(pTarget, (void*)&Hooked_ConsoleExecuteLine, (void**)&g_OriginalExecuteLine) == MH_OK) {
        MH_EnableHook(pTarget);
        Log("[Patch] Hooked Console::ExecuteLine at %p", pTarget);
    } else {
        Log("[Patch Error] Failed to hook Console::ExecuteLine");
    }

    // Dynamically resolve e_cheats pointer from Console::ExecuteLine instructions
    if (pTarget) {
        uint8_t* pCode = (uint8_t*)pTarget;
        for (int i = 0; i < 0x400; i++) {
            if (pCode[i] == 0x40 && pCode[i+1] == 0x38 && pCode[i+2] == 0x3D && pCode[i+7] == 0x74 && pCode[i+8] == 0x1D) {
                int32_t disp = *(int32_t*)(pCode + i + 3);
                g_pCheatsEnabled = (uint8_t*)(pCode + i + 7 + disp);
                Log("[Patch] Dynamically resolved e_cheats from Console::ExecuteLine at %p (RVA +0x%llX)", g_pCheatsEnabled, (unsigned long long)((uintptr_t)g_pCheatsEnabled - g_BaseAddress));
                break;
            }
        }
    }
    if (!g_pCheatsEnabled) {
        g_pCheatsEnabled = (uint8_t*)(g_BaseAddress + RVA_E_CHEATS);
        Log("[Patch] Fallback to RVA for e_cheats: %p", g_pCheatsEnabled);
    }

    // Resolve PersistentSaves::GetModded (0x1F740): Pattern scan first, fallback to RVA
    static const unsigned char kSigGetModded[] = {
        0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x48, 0x38, 0x48, 0x8B, 0x49, 0x08, 0xE9, 0x5C, 0xAB, 0x00, 0x00
    };
    static const char kMaskGetModded[] = "xxx????xxxxxxxxxxxxx";

    void* pGetModdedTarget = PatternScanMask((HMODULE)g_BaseAddress, kSigGetModded, kMaskGetModded);
    if (pGetModdedTarget) {
        g_GetModded = (tGetModded)pGetModdedTarget;
        Log("[Patch] Found PersistentSaves::GetModded via pattern scan: %p (RVA +0x%llX)", pGetModdedTarget, (unsigned long long)((uintptr_t)pGetModdedTarget - g_BaseAddress));
    } else {
        g_GetModded = (tGetModded)(g_BaseAddress + RVA_GET_MODDED);
        Log("[Patch] Fallback to RVA for PersistentSaves::GetModded: %p", g_GetModded);
    }

    // 4. Resolve and Hook ResourceTaskLambda (0x7BAD0): Crash guard for profile switching & asset reload
    static const unsigned char kSigResourceTaskLambda[] = {
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x41, 0x08, 0x48, 0x8B, 0xD9, 0x48, 0x8B, 0x88,
        0x88, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC9, 0x74
    };
    void* pLambdaTarget = PatternScan((HMODULE)g_BaseAddress, kSigResourceTaskLambda, sizeof(kSigResourceTaskLambda));
    if (pLambdaTarget) {
        Log("[Patch] Found ResourceTaskLambda via pattern scan: %p (RVA +0x%llX)", pLambdaTarget, (unsigned long long)((uintptr_t)pLambdaTarget - g_BaseAddress));
    } else {
        pLambdaTarget = (void*)(g_BaseAddress + RVA_RESOURCE_TASK_LAMBDA);
        Log("[Patch] Fallback to RVA for ResourceTaskLambda: %p", pLambdaTarget);
    }

    if (MH_CreateHook(pLambdaTarget, (void*)&Hooked_ResourceTaskLambda, (void**)&g_OriginalResourceTaskLambda) == MH_OK) {
        MH_EnableHook(pLambdaTarget);
        Log("[Patch] Hooked ResourceTaskLambda at %p (Profile Switch Safety Guard Active)", pLambdaTarget);
    } else {
        Log("[Patch Error] Failed to hook ResourceTaskLambda");
    }

    // 5. Resolve and Hook SecondaryTaskLambda (0x7BB40)
    static const unsigned char kSigSecondaryTaskLambda[] = {
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x79, 0x10, 0x48, 0x8B, 0xD9
    };
    void* pSecondaryTarget = PatternScan((HMODULE)g_BaseAddress, kSigSecondaryTaskLambda, sizeof(kSigSecondaryTaskLambda));
    if (pSecondaryTarget) {
        Log("[Patch] Found SecondaryTaskLambda via pattern scan: %p (RVA +0x%llX)", pSecondaryTarget, (unsigned long long)((uintptr_t)pSecondaryTarget - g_BaseAddress));
    } else {
        pSecondaryTarget = (void*)(g_BaseAddress + RVA_SECONDARY_TASK_LAMBDA);
        Log("[Patch] Fallback to RVA for SecondaryTaskLambda: %p", pSecondaryTarget);
    }

    if (MH_CreateHook(pSecondaryTarget, (void*)&Hooked_SecondaryTaskLambda, (void**)&g_OriginalSecondaryTaskLambda) == MH_OK) {
        MH_EnableHook(pSecondaryTarget);
        Log("[Patch] Hooked SecondaryTaskLambda at %p (Profile Switch Secondary Guard Active)", pSecondaryTarget);
    } else {
        Log("[Patch Error] Failed to hook SecondaryTaskLambda");
    }

    // 6. Initialize Universal Hook Engine & Hook asCModule::AddScriptSection (0x48A410)
    HookEngine::Initialize();

    static const unsigned char kSigAddScriptSection[] = {
        0x4C, 0x89, 0x4C, 0x24, 0x20, 0x4C, 0x89, 0x44, 0x24, 0x18, 0x48, 0x89, 0x54, 0x24, 0x10, 0x53,
        0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x38
    };
    void* pAddSecTarget = PatternScan((HMODULE)g_BaseAddress, kSigAddScriptSection, sizeof(kSigAddScriptSection));
    if (pAddSecTarget) {
        Log("[Patch] Found asCModule::AddScriptSection via pattern scan: %p (RVA +0x%llX)", pAddSecTarget, (unsigned long long)((uintptr_t)pAddSecTarget - g_BaseAddress));
    } else {
        pAddSecTarget = (void*)(g_BaseAddress + RVA_AS_MODULE_ADD_SECTION);
        Log("[Patch] Fallback to RVA for asCModule::AddScriptSection: %p", pAddSecTarget);
    }

    if (MH_CreateHook(pAddSecTarget, (void*)&Hooked_AddScriptSection, (void**)&g_OriginalAddScriptSection) == MH_OK) {
        MH_EnableHook(pAddSecTarget);
        Log("[Patch] Hooked asCModule::AddScriptSection at %p (Universal Hook Engine Active)", pAddSecTarget);
    } else {
        Log("[Patch Error] Failed to hook asCModule::AddScriptSection");
    }

    // 7. Hook asCModule::Build (0x48A070) - Fires every time any script module is built
    static const unsigned char kSigModuleBuild[] = {
        0x40, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x33, 0xD2, 0x48, 0x8B, 0xF9
    };
    void* pBuildTarget = PatternScan((HMODULE)g_BaseAddress, kSigModuleBuild, sizeof(kSigModuleBuild));
    if (pBuildTarget) {
        Log("[Patch] Found asCModule::Build via pattern scan: %p (RVA +0x%llX)", pBuildTarget, (unsigned long long)((uintptr_t)pBuildTarget - g_BaseAddress));
    } else {
        pBuildTarget = (void*)(g_BaseAddress + RVA_AS_MODULE_BUILD);
        Log("[Patch] Fallback to RVA for asCModule::Build: %p", pBuildTarget);
    }

    if (MH_CreateHook(pBuildTarget, (void*)&Hooked_ModuleBuild, (void**)&g_OriginalModuleBuild) == MH_OK) {
        MH_EnableHook(pBuildTarget);
        Log("[Patch] Hooked asCModule::Build at %p (Post-Build Mod Hook Registration Active)", pBuildTarget);
    } else {
        Log("[Patch Error] Failed to hook asCModule::Build");
    }

    // 8. Resolve RegisterHook (0x130360) and Hook ProcessMetadata (0x12D540)
    static const unsigned char kSigRegisterHook[] = {
        0x48, 0x89, 0x5C, 0x24, 0x18, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
        0x48, 0x8D, 0x6C, 0x24, 0xD9, 0x48, 0x81, 0xEC, 0x90, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x05, 0x00,
        0x00, 0x00, 0x00, 0x48, 0x33, 0xC4, 0x48, 0x89, 0x45, 0x17, 0x4C, 0x8B, 0xEA, 0x4C, 0x8B, 0xE1,
        0x48, 0x8B, 0x02, 0x48, 0x8B, 0xCA, 0xFF, 0x50, 0x68
    };
    static const char kMaskRegisterHook[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxx";

    void* pRegTarget = PatternScanMask((HMODULE)g_BaseAddress, kSigRegisterHook, kMaskRegisterHook);
    if (pRegTarget) {
        g_RegisterHook = (tRegisterHook)pRegTarget;
        Log("[Patch] Found RegisterHook via pattern scan: %p (RVA +0x%llX)", pRegTarget, (unsigned long long)((uintptr_t)pRegTarget - g_BaseAddress));
    } else {
        g_RegisterHook = (tRegisterHook)(g_BaseAddress + RVA_REGISTER_HOOK);
        Log("[Patch] Fallback to RVA for RegisterHook: %p", g_RegisterHook);
    }

    static const unsigned char kSigProcessMetadata[] = {
        0x48, 0x89, 0x5C, 0x24, 0x18, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
        0x48, 0x8D, 0x6C, 0x24, 0xD9, 0x48, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x05
    };
    void* pMetaTarget = PatternScan((HMODULE)g_BaseAddress, kSigProcessMetadata, sizeof(kSigProcessMetadata));
    if (pMetaTarget) {
        Log("[Patch] Found ProcessMetadata via pattern scan: %p (RVA +0x%llX)", pMetaTarget, (unsigned long long)((uintptr_t)pMetaTarget - g_BaseAddress));
    } else {
        pMetaTarget = (void*)(g_BaseAddress + RVA_PROCESS_METADATA);
        Log("[Patch] Fallback to RVA for ProcessMetadata: %p", pMetaTarget);
    }

    if (MH_CreateHook(pMetaTarget, (void*)&Hooked_ProcessMetadata, (void**)&g_OriginalProcessMetadata) == MH_OK) {
        MH_EnableHook(pMetaTarget);
        Log("[Patch] Hooked ProcessMetadata at %p (Post-Build Hook Registration Active)", pMetaTarget);
    } else {
        Log("[Patch Error] Failed to hook ProcessMetadata");
    }

    Log("[Patch] Initialization completed successfully.");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        InitAllWinmmExports();
        Log("[Patch] DllMain DLL_PROCESS_ATTACH");
        CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        Log("[Patch] DllMain DLL_PROCESS_DETACH");
    }
    return TRUE;
}
