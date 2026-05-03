# Stronghold Legends Text Hook

Small open-source C++/CMake DLL project for hooking text rendering in **Stronghold Legends**.

The DLL is intended to be injected into the 32-bit game process. It hooks
`Dragonfly::RenderFont::drawText`, captures the incoming STLPort wide string, logs it, and passes it unchanged to the original renderer. This is a minimal base for Persian translation work.

## Target

- Game module of interest: `dxrenderer.dll`
- Actual text renderer symbol:

```text
?drawText@RenderFont@Dragonfly@@QAEXAAVRenderer@2@ABV?$basic_string@GV?$char_traits@G@_STL@@V?$allocator@G@2@@_STL@@HHVColor@2@V?$RegionT@H@2@I_N@Z
```

In common retail builds, `dxrenderer.dll` imports this function from `dragonfly.dll`, so the hook loader first loads/checks `dxrenderer.dll`, then resolves the symbol from `dxrenderer.dll` if exported, otherwise from `dragonfly.dll`.

## Requirements

- Windows
- Visual Studio with MSVC C++ tools
- CMake 3.20+
- 32-bit/x86 build
- MinHook in `third_party/minhook`

## Getting MinHook

Use a submodule:

```bat
git submodule add https://github.com/TsudaKageyu/minhook third_party/minhook
git submodule update --init --recursive
```

Or clone it directly:

```bat
git clone https://github.com/TsudaKageyu/minhook third_party/minhook
```

## Build

From an x86 Visual Studio Developer Command Prompt:

```bat
cmake -B build
cmake --build build --config Release
```

If CMake selects a Visual Studio generator and defaults to x64, configure explicitly:

```bat
cmake -B build -A Win32
cmake --build build --config Release
```

The output DLL will be:

```text
build\Release\StrongholdLegendsTextHook.dll
```

## Injection

1. Start `StrongholdLegends.exe`.
2. Inject `StrongholdLegendsTextHook.dll` into the game process with a 32-bit DLL injector.
3. Run the game normally.

The hook writes a UTF-8 log file next to the game executable:

```text
StrongholdLegendsTextHook.log
```

It also writes messages to the debugger with `OutputDebugStringW`, so Sysinternals DebugView can be used to watch hook installation and captured text.

## Notes

- Stronghold Legends uses STLPort/VC7.1 strings, not modern MSVC `std::wstring` ABI strings. The project reads the incoming text through `stlport_vc7150.dll` exports (`data()` and `size()`) instead of casting it to a modern `std::wstring`.
- The hook currently logs and forwards the text unchanged. Text replacement or Persian shaping can be added inside `DrawTextDetour` before calling `g_originalDrawText`.
