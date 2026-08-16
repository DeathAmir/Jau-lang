<div align="center">

# Jau Programming Language

### Small syntax, visible toolchain, native interoperability.

**VM · Bytecode · x86/x64 AOT · Internal Windows PE linker · DLL/.so · LIB/.a · Native packages · JauM**

[![Build](https://github.com/DeathAmir/Jau-lang/actions/workflows/build.yml/badge.svg)](https://github.com/DeathAmir/Jau-lang/actions/workflows/build.yml)
![Version](https://img.shields.io/badge/Jau-0.9.0-4c8bf5)
![Windows](https://img.shields.io/badge/Windows-x86%20%7C%20x64-0078D4)
![Linux](https://img.shields.io/badge/Linux-x86%20%7C%20x64-FCC624)
![License](https://img.shields.io/badge/license-MIT-green)

`Jau → VM` · `Jau → JBC` · `Jau → ASM` · `Jau → OBJ` · `Jau → EXE/ELF` · `Jau → DLL/.so` · `Jau → LIB/.a`

</div>

---

## What is Jau?

Jau is an experimental programming language and toolchain focused on a compact language, native code generation, explicit interoperability and understandable build stages. It ships a bytecode VM, x86/x86-64 AOT backend, assembler/object writer, an internal Windows PE linker, protected packages, native C/C++ extension packages and the `jaum` project builder.

Jau 0.9 is a **correctness-first** release line. Optimization flags are accepted, but unsafe optimizer transformations remain disabled until their semantic regression coverage is strong enough. Working code is more important than a smaller benchmark number.

## Hello Jau

```jau
func main() {
    let a = 20;
    let b = 22;
    print(a + b);
    return 0;
}
```

Run in the VM:

```bash
jauc run main.jau
```

Build a Windows x64 executable:

```cmd
jauc native main.jau -o main.exe --target windows-x86_64
main.exe
```

Build a Linux x64 executable:

```bash
jauc native main.jau -o main --target linux-x86_64
./main
```

`func main()` is the program entry. You do **not** add a second `main();` call at the bottom of the file.

## Supported targets

| Target | ASM | OBJ | Native executable | Shared library | Static library |
|---|---:|---:|---:|---:|---:|
| `windows-x86_64` | ✅ | COFF `.obj` | PE32+ `.exe` | `.dll` | `.lib` |
| `windows-x86` | ✅ | COFF `.obj` | PE32 `.exe` | `.dll` | `.lib` |
| `linux-x86_64` | ✅ | ELF `.o` | ELF | `.so` | `.a` |
| `linux-x86` | ✅ | ELF `.o` | ELF | not release-gated | not release-gated |

`jauc targets` prints the AOT target names implemented by the compiler. Windows x86/x64 and Linux x86/x64 are the supported CPU/OS family in 0.9. macOS and ARM are not advertised as implemented targets yet.

## Language snapshot

```jau
func main() {
    let values = [10, 20, 30];
    values[1] = 42;

    for (let i = 0; i < 3; i++) {
        print(values[i]);
    }
}
```

Jau includes functions, namespaces, imports, variables/constants, arithmetic/comparisons, `if/else`, `while`, `for`, `break`, `continue`, recursion, arrays, indexing, indexed assignment, compound variable assignment, increment/decrement, bitwise operators and numeric literals such as `0xff`, `0b1010`, `0o755` and `1_000_000`.

Aliases such as `fn`, `def`, `function`, `var`, `nil`, `and`, `or` and `not` are accepted.

See [docs/LANGUAGE.md](docs/LANGUAGE.md).

## JSON + HTTP

The VM has practical JSON query functions for API responses:

```jau
func main() {
    let body = "{\"user\":{\"name\":\"Jau\"},\"ok\":true}";

    print(json_valid(body));
    print(json_string(body, "user.name"));
    print(json_bool(body, "ok"));
}
```

Useful functions:

```text
json_valid(text)
json_get(text, path)
json_string(text, path)
json_int(text, path)
json_bool(text, path)
json_type(text, path)
json_has(text, path)
json_escape(text)
http_get(url)
download(url, path)
```

JSON supports nested dot paths and array indexes such as `users[0].name`. JSON/container helpers are currently VM/JBC features; native AOT rejects unsupported dynamic container code instead of silently generating a broken binary.

HTTP is documented in [docs/HTTP_JSON.md](docs/HTTP_JSON.md). Plain HTTP has a POSIX socket path; HTTPS currently uses platform tools where native TLS is not yet implemented.

## Native system libraries

A Jau application can link a C/C++ object that depends on Windows DLL APIs:

```cmd
jauc native main.jau -o app.exe --target windows-x86_64 ^
  --link window.obj ^
  --system-lib user32
```

`jauld` can inspect real PE DLL export tables and resolve requested imports. Explicit mappings are available when you need deterministic control:

```cmd
--import MessageBoxA=user32.dll
```

Native `.jaux` packages can carry these dependencies in `jau.pkg`:

```ini
system_libs_windows="user32,gdi32"
imports_windows="SomeSymbol=some.dll"
```

See [docs/SYSTEM_LIBRARIES.md](docs/SYSTEM_LIBRARIES.md).

## Build a real Windows GUI

`examples/windows_gui` contains a tiny Win32 example. Build the C++ bridge object first, then link it through Jau:

```cmd
cl /nologo /c /EHsc examples\windows_gui\window.cpp /Fo:examples\windows_gui\window.obj

jauc native examples\windows_gui\main.jau -o jau-window.exe ^
  --target windows-x86_64 ^
  --link examples\windows_gui\window.obj ^
  --system-lib user32 ^
  --subsystem windows
```

The Jau program passes literal strings across the C ABI to `MessageBoxA`. See [docs/WINDOWS_GUI.md](docs/WINDOWS_GUI.md).

## Shared libraries

Jau can build actual loadable libraries with public exports:

```bash
jauc shared math.jau -o jaucalc --target windows-x86_64 \
  --export add --export answer
```

Windows output: `jaucalc.dll`.

```bash
jauc shared math.jau -o libjaucalc --target linux-x86_64 \
  --export add --export answer
```

Linux output: `libjaucalc.so`.

Windows CI validates the DLL with `LoadLibrary/GetProcAddress`; Linux CI validates the `.so` with `dlopen/dlsym`.

See [docs/LIBRARIES.md](docs/LIBRARIES.md).

## Static libraries

```cmd
jauc static math.jau -o jaucalc --target windows-x86_64
```

Produces `jaucalc.lib` using Jau's internal COFF archive writer.

```bash
jauc static math.jau -o libjaucalc --target linux-x86_64
```

Produces `libjaucalc.a` using Jau's internal archive writer.

These are **static archives**. A Windows `.lib` produced by `jauc static` is not the same thing as a DLL import library.

## JauM project builds

For larger projects, keep build intent in `jaum.txt`:

```ini
name=myapp
source=src/main.jau
type=exe
target=windows-x86_64
targets=windows-x86_64,windows-x86,linux-x86_64,linux-x86
output=build/{name}-{target}{ext}
optimize=0
subsystem=console
links=
system_libs=
imports=
include_paths=stdlib
```

Then:

```cmd
jaum build
jaum build-all
jaum clean
jaum show
```

Library projects use `type=shared` or `type=static`; shared projects can set `exports=add,answer`.

See [docs/JAUM.md](docs/JAUM.md).

## Native `.jaux` packages

A native package keeps Jau wrapper source separate from target object code:

```text
MathX/
├── jau.pkg
├── src/main.jau
└── native/
    ├── windows-x86_64/mathx.obj
    └── windows-x86/mathx.obj
```

C/C++ ABI functions should use `extern "C"`. For integer values portable across x86/x64, `std::intptr_t` matches Jau's current machine-word integer ABI better than hard-coding `long long`.

Package manifests can declare system libraries, and package object bytes are linker inputs—not parser input.

See [docs/NATIVE_EXTENSIONS.md](docs/NATIVE_EXTENSIONS.md) and [docs/PACKAGES.md](docs/PACKAGES.md).

## Toolchain

| Tool | Purpose |
|---|---|
| `jauc` | compiler driver, VM runner, JBC/AOT/object/native/library builder |
| `jur` | JBC VM and embedded standalone runtime |
| `jauas` | x86/x64 assembler and COFF/ELF object writer |
| `jauld` | internal Windows PE EXE/DLL linker and DLL import resolver |
| `jaum` | project builder driven by `jaum.txt` |
| `jaupm` | Jau package manager |
| `jau-setup` | installer and `JAU_HOME`/PATH setup |

Only the command-line tools print `DeathAmir Jau @ DeathAmir 2026 (C)` on stderr. Generated source, assembly, objects, executables and libraries are user artifacts and do not receive that tool banner.

## Command map

```text
jauc run        source -> VM
jauc build      source -> JBC
jauc standalone source -> bundled runtime + JBC
jauc asm        source -> assembly
jauc obj        source -> .obj/.o
jauc native     source -> .exe/ELF
jauc shared     source -> .dll/.so
jauc static     source -> .lib/.a
jauc check      parse/validate without running
jauc debug      traced VM execution
jauc targets    supported AOT target names
```

Full CLI reference: [docs/CLI.md](docs/CLI.md).

## Current boundaries

Jau is a working experimental toolchain, not a claim of feature parity with mature C++/Rust/Go ecosystems.

- AOT and VM do not yet expose the same dynamic value surface.
- Dynamic arrays and JSON helpers are VM/JBC features; unsupported native constructs fail explicitly.
- Native literal strings can cross the C ABI, but a general owned native string/array ABI is not stable yet.
- Optimizer transformations are currently disabled/safe-mode while semantic regressions are expanded.
- `jauas` implements the instruction subset needed by Jau rather than every general assembler dialect.
- `jauld` is a Jau-focused PE linker, not a complete replacement for every LINK.EXE/LLD feature.
- Windows DLL creation and loading is regression-tested, but advanced PE features should be added with dedicated tests before being advertised.
- Native TLS is not yet a built-in Jau runtime feature.

## Documentation

- [Language reference](docs/LANGUAGE.md)
- [CLI reference](docs/CLI.md)
- [Targets and binary formats](docs/TARGETS.md)
- [System libraries](docs/SYSTEM_LIBRARIES.md)
- [Shared/static libraries](docs/LIBRARIES.md)
- [JauM](docs/JAUM.md)
- [HTTP + JSON](docs/HTTP_JSON.md)
- [Windows GUI](docs/WINDOWS_GUI.md)
- [Native extensions](docs/NATIVE_EXTENSIONS.md)
- [Packages](docs/PACKAGES.md)
- [Linker internals](docs/LINKER.md)
- [Interop](docs/INTEROP.md)
- [Bootstrap](docs/BOOTSTRAP.md)
- [Maintainer invariants](docs/MAINTAINERS.md)
- [Jau 0.9 notes](docs/V090.md)

## Build from source

Linux:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Windows x64:

```cmd
cmake -S . -B build -A x64 -DJAU_ENABLE_ASM_FASTOPS=OFF
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Windows x86:

```cmd
cmake -S . -B build32 -A Win32 -DJAU_ENABLE_ASM_FASTOPS=OFF
cmake --build build32 --config Release --parallel 2
```

---

**DeathAmir Jau @ DeathAmir 2026 (C)** · MIT License
