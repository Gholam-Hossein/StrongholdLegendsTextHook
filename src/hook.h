#pragma once

#include <windows.h>

namespace shl_text_hook {

bool Install();
void Uninstall();
void Log(const wchar_t* message);
void LogFormat(const wchar_t* format, ...);

} // namespace shl_text_hook
