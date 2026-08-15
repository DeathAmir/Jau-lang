# Jau Native Extensions (`.jaux`)

Jau can package precompiled C/C++ COFF objects together with a Jau wrapper and install them with JauPM. The installed archive remains protected on disk; `jauc native` extracts only the target object to a temporary build directory, links it, then removes the temporary copy.

## Package layout

```text
CppMath/
├── jau.pkg
├── src/
│   └── main.jau
└── native/
    ├── windows-x86_64/
    │   └── cppmath.obj
    └── windows-x86/
        └── cppmath.obj
```

`jau.pkg`:

```ini
name="CppMath"
version="1.0.0"
main="src/main.jau"
type="native"
native_windows_x86_64="native/windows-x86_64/cppmath.obj"
native_windows_x86="native/windows-x86/cppmath.obj"
dependencies=""
```

Multiple object files can be comma-separated in a target field.

## C/C++ ABI

Keep exported extension entry points on the C ABI. In C++ use `extern "C"`.

```cpp
extern "C" long long cppmath_mul(long long a, long long b) {
    return a * b;
}
```

Windows x64 example with MinGW:

```bash
x86_64-w64-mingw32-g++ -c -O2 -fno-exceptions -fno-rtti cppmath.cpp -o native/windows-x86_64/cppmath.obj
```

Windows x86 example:

```bash
i686-w64-mingw32-g++ -c -O2 -fno-exceptions -fno-rtti cppmath.cpp -o native/windows-x86/cppmath.obj
```

The current Jau AOT FFI boundary is integer/bool machine values. Do not pass Jau strings, arrays or owned VM objects directly through this ABI yet.

## Jau wrapper

```jau
extern func cppmath_mul(a, b);

namespace CppMath {
    func mul(a, b) {
        return cppmath_mul(a, b);
    }
}
```

## Pack and install

`.jaux` uses the same protected JAUPKG2 container as normal Jau packages, with native target metadata in the manifest.

```cmd
jaupm pack dist\CppMath-1.0.0.jaux
jaupm verify dist\CppMath-1.0.0.jaux
jaupm install dist\CppMath-1.0.0.jaux
```

User code:

```jau
import "pkg:CppMath"

print(CppMath.mul[6, 7]);
```

Build a Windows x64 executable:

```cmd
jauc native main.jau -o app.exe --target windows-x86_64
```

No GCC/MSVC/LLD linker is used for that final Jau executable. The Windows path is:

```text
Jau source + protected package source
             │
             ▼
           jauc
             │
             ▼
        Intel assembly
             │
             ▼
           jauas
             │
             ▼
       COFF .obj files
             │
             ▼
           jauld
             │
             ▼
       PE32 / PE32+ EXE
```

`jauld` currently resolves Jau/C-ABI object symbols, common x86/x64 COFF relocations, and a small Windows import set used by the Jau AOT runtime (`msvcrt.dll` and `kernel32.dll`). Native extension authors should keep package objects self-contained and avoid C++ exceptions/RTTI or large external runtime dependencies.
