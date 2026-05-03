#include <windows.h>

#include "hook.h"

namespace {

DWORD WINAPI StartupThread(LPVOID) {
    shl_text_hook::Install();
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, StartupThread, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
        break;
    }
    case DLL_PROCESS_DETACH:
        shl_text_hook::Uninstall();
        break;
    default:
        break;
    }
    return TRUE;
}
