#include "hook.h"

#include <MinHook.h>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace shl_text_hook {
namespace {

constexpr const wchar_t* kLogFileName = L"StrongholdLegendsTextHook.log";
constexpr const char* kDxRendererDll = "dxrenderer.dll";
constexpr const char* kDragonflyDll = "dragonfly.dll";
constexpr const char* kStlportDll = "stlport_vc7150.dll";

constexpr const char* kDrawTextSymbol =
    "?drawText@RenderFont@Dragonfly@@QAEXAAVRenderer@2@ABV?$basic_string@GV?$char_traits@G@_STL@@V?$allocator@G@2@@_STL@@HHVColor@2@V?$RegionT@H@2@I_N@Z";

constexpr const char* kStringDataSymbol =
    "?data@?$basic_string@GV?$char_traits@G@_STL@@V?$allocator@G@2@@_STL@@QBEPBGXZ";
constexpr const char* kStringSizeSymbol =
    "?size@?$basic_string@GV?$char_traits@G@_STL@@V?$allocator@G@2@@_STL@@QBEIXZ";

struct Color {
    unsigned char b;
    unsigned char g;
    unsigned char r;
    unsigned char a;
};

struct RegionI {
    int left;
    int top;
    int right;
    int bottom;
};

// The game uses STLPort's basic_string<unsigned short>, not modern MSVC std::wstring.
using DrawTextFn = void(__thiscall*)(void* self,
                                    void* renderer,
                                    const void* stlportWideString,
                                    int x,
                                    int y,
                                    Color color,
                                    RegionI region,
                                    unsigned flags,
                                    bool wrap);
using StringDataFn = const wchar_t*(__thiscall*)(const void* stlportWideString);
using StringSizeFn = unsigned(__thiscall*)(const void* stlportWideString);

std::mutex g_logMutex;
DrawTextFn g_originalDrawText = nullptr;
LPVOID g_drawTextTarget = nullptr;
StringDataFn g_stringData = nullptr;
StringSizeFn g_stringSize = nullptr;
bool g_minHookInitialized = false;

std::wstring GetLogPath() {
    wchar_t path[MAX_PATH]{};
    DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return kLogFileName;
    }

    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) {
        return kLogFileName;
    }
    *(slash + 1) = L'\0';
    return std::wstring(path) + kLogFileName;
}

HMODULE WaitForModule(const char* name, DWORD timeoutMs) {
    constexpr DWORD kStepMs = 50;
    DWORD waited = 0;
    while (waited <= timeoutMs) {
        if (HMODULE module = GetModuleHandleA(name)) {
            return module;
        }
        Sleep(kStepMs);
        waited += kStepMs;
    }
    return nullptr;
}

FARPROC ResolveProc(HMODULE module, const char* symbol, const wchar_t* moduleNameForLog) {
    FARPROC proc = module ? GetProcAddress(module, symbol) : nullptr;
    if (!proc) {
        LogFormat(L"GetProcAddress failed in %ls for symbol: %S", moduleNameForLog, symbol);
    }
    return proc;
}

void LogIncomingText(const void* stlportWideString) {
    if (!stlportWideString || !g_stringData || !g_stringSize) {
        return;
    }

    const wchar_t* text = g_stringData(stlportWideString);
    const unsigned length = g_stringSize(stlportWideString);
    if (!text) {
        return;
    }

    std::wstring captured(text, text + length);
    LogFormat(L"drawText: \"%ls\"", captured.c_str());
}

void __fastcall DrawTextDetour(void* self,
                               void* /*edx*/,
                               void* renderer,
                               const void* stlportWideString,
                               int x,
                               int y,
                               Color color,
                               RegionI region,
                               unsigned flags,
                               bool wrap) {
    LogIncomingText(stlportWideString);
    g_originalDrawText(self, renderer, stlportWideString, x, y, color, region, flags, wrap);
}

} // namespace

void Log(const wchar_t* message) {
    std::lock_guard<std::mutex> lock(g_logMutex);

    FILE* file = nullptr;
    const std::wstring path = GetLogPath();
    _wfopen_s(&file, path.c_str(), L"a, ccs=UTF-8");
    if (file) {
        fwprintf(file, L"%ls\n", message);
        fclose(file);
    }

    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}

void LogFormat(const wchar_t* format, ...) {
    wchar_t buffer[4096]{};
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, format, args);
    va_end(args);
    Log(buffer);
}

bool Install() {
    Log(L"StrongholdLegendsTextHook: installing...");

    // Keep dxrenderer.dll loaded because it is the renderer target for this hook.
    HMODULE dxrenderer = GetModuleHandleA(kDxRendererDll);
    if (!dxrenderer) {
        dxrenderer = LoadLibraryA(kDxRendererDll);
    }
    if (!dxrenderer) {
        Log(L"StrongholdLegendsTextHook: dxrenderer.dll was not loaded and LoadLibraryA failed.");
        return false;
    }

    HMODULE stlport = WaitForModule(kStlportDll, 10000);
    if (!stlport) {
        stlport = LoadLibraryA(kStlportDll);
    }
    if (!stlport) {
        Log(L"StrongholdLegendsTextHook: stlport_vc7150.dll was not loaded and LoadLibraryA failed.");
        return false;
    }

    // In shipped Stronghold Legends builds dxrenderer imports this symbol from dragonfly.dll.
    FARPROC drawText = GetProcAddress(dxrenderer, kDrawTextSymbol);
    if (drawText) {
        Log(L"StrongholdLegendsTextHook: resolved drawText directly from dxrenderer.dll.");
    } else {
        Log(L"StrongholdLegendsTextHook: dxrenderer.dll does not export drawText; trying dragonfly.dll.");
        HMODULE dragonfly = WaitForModule(kDragonflyDll, 10000);
        if (!dragonfly) {
            dragonfly = LoadLibraryA(kDragonflyDll);
        }
        drawText = ResolveProc(dragonfly, kDrawTextSymbol, L"dragonfly.dll");
    }

    g_stringData = reinterpret_cast<StringDataFn>(ResolveProc(stlport, kStringDataSymbol, L"stlport_vc7150.dll"));
    g_stringSize = reinterpret_cast<StringSizeFn>(ResolveProc(stlport, kStringSizeSymbol, L"stlport_vc7150.dll"));

    if (!drawText || !g_stringData || !g_stringSize) {
        Log(L"StrongholdLegendsTextHook: failed to resolve required functions.");
        return false;
    }

    g_drawTextTarget = reinterpret_cast<LPVOID>(drawText);

    if (MH_Initialize() != MH_OK) {
        Log(L"StrongholdLegendsTextHook: MH_Initialize failed.");
        return false;
    }
    g_minHookInitialized = true;

    MH_STATUS createStatus = MH_CreateHook(g_drawTextTarget,
                                           reinterpret_cast<LPVOID>(&DrawTextDetour),
                                           reinterpret_cast<LPVOID*>(&g_originalDrawText));
    if (createStatus != MH_OK) {
        LogFormat(L"StrongholdLegendsTextHook: MH_CreateHook failed: %S", MH_StatusToString(createStatus));
        return false;
    }

    MH_STATUS enableStatus = MH_EnableHook(g_drawTextTarget);
    if (enableStatus != MH_OK) {
        LogFormat(L"StrongholdLegendsTextHook: MH_EnableHook failed: %S", MH_StatusToString(enableStatus));
        return false;
    }

    LogFormat(L"StrongholdLegendsTextHook: hook installed at 0x%p", g_drawTextTarget);
    return true;
}

void Uninstall() {
    if (g_drawTextTarget) {
        MH_DisableHook(g_drawTextTarget);
        MH_RemoveHook(g_drawTextTarget);
        g_drawTextTarget = nullptr;
        g_originalDrawText = nullptr;
    }

    if (g_minHookInitialized) {
        MH_Uninitialize();
        g_minHookInitialized = false;
    }
}

} // namespace shl_text_hook
