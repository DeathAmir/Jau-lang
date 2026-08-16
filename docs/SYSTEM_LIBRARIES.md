# Native System Libraries

A native object can reference operating-system APIs. Jau 0.9 carries those dependencies explicitly through the compiler, package manifest and linker.

## Windows example

C++:

```cpp
#include <windows.h>
extern "C" long win_width() { return GetSystemMetrics(0); }
```

Jau:

```jau
extern func win_width():int;
func main() { print(win_width()); }
```

Link:

```cmd
jauc native main.jau -o app.exe --target windows-x86_64 ^
  --link native.obj ^
  --system-lib user32
```

`jauld` resolves imports from requested DLL exports. It searches explicit paths, `JAU_SYSTEM_LIB_PATH`, Windows system directories and the current directory. Common system DLLs also have fallback export maps for cross-link scenarios.

## Exact import mapping

```cmd
--import MessageBoxA=user32.dll
```

Use this when the same export exists in multiple requested DLLs or when you need deterministic mapping.

## Package manifest

```ini
system_libs_windows="user32,gdi32"
system_libs_windows_x86_64="ws2_32"
imports_windows="CustomCall=custom.dll"
```

Supported scopes are generic, platform and exact target forms. Package dependencies flow into `jauc native` automatically.

## Linux

Logical names become normal system linker arguments:

```cmd
--system-lib m
--system-lib pthread
```

Explicit `.a`/`.so` paths can also be passed.

## Why objects alone are not enough

An OBJ defining `open_window` may itself reference `__imp_MessageBoxA`. Resolving `open_window` therefore does not finish the link: the system DLL import must also be declared. This is normal native linking behavior, not a Jau parser issue.
