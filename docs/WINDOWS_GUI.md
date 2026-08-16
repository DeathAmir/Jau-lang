# Windows GUI with Jau

Jau 0.9 can build a real Windows GUI by combining Jau AOT with a tiny C ABI bridge and declaring the required Windows system DLL.

## Minimal MessageBox example

`examples/windows_gui/window.cpp`:

```cpp
#include <windows.h>
#include <cstdint>

extern "C" std::intptr_t jau_message_box(const char* text, const char* title) {
    return MessageBoxA(nullptr, text, title, MB_OK | MB_ICONINFORMATION);
}
```

Jau:

```jau
extern func jau_message_box(text, title):int;

func main() {
    jau_message_box("Hello from a native Jau GUI", "Jau 0.9");
    return 0;
}
```

AOT string literals are emitted in `.rodata` and can be passed as borrowed `const char*` to native functions.

## Build x64

```cmd
cl /nologo /c /EHsc examples\windows_gui\window.cpp /Fo:examples\windows_gui\window.obj

jauc native examples\windows_gui\main.jau -o jau-window.exe ^
  --target windows-x86_64 ^
  --link examples\windows_gui\window.obj ^
  --system-lib user32 ^
  --subsystem windows
```

`--subsystem windows` creates a GUI-subsystem PE instead of a console-subsystem PE.

## Going beyond MessageBox

The same pattern works for a normal Win32 window loop. A native bridge can expose `RegisterClass`, `CreateWindowEx`, `ShowWindow`, `GetMessage`, GDI calls and controls. Keep the C ABI narrow and stable; let Jau own application decisions while the extension owns OS-specific structs/callbacks until Jau has a native struct/function-pointer ABI.

For reusable GUI code, package the wrapper and target objects as a `.jaux` and declare:

```ini
system_libs_windows="user32,gdi32"
```

## Current boundary

This is real GUI interoperability, not a built-in cross-platform widget toolkit. Jau 0.9 does not ship a native `Window` class hierarchy or a platform-independent layout engine yet.
