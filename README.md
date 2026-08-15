<div align="center">

# Jau Programming Language

**Bytecode VM · Native AOT · Internal Windows PE linker · Protected packages · C/C++ native extensions · Staged self-hosting**

[![Build](https://github.com/DeathAmir/Jau-lang/actions/workflows/build.yml/badge.svg)](https://github.com/DeathAmir/Jau-lang/actions/workflows/build.yml)
[![PE Import Tests](https://github.com/DeathAmir/Jau-lang/actions/workflows/pe-import.yml/badge.svg)](https://github.com/DeathAmir/Jau-lang/actions/workflows/pe-import.yml)
![Version](https://img.shields.io/badge/Jau-0.6.0-4c8bf5)
![Targets](https://img.shields.io/badge/Windows-x86%20%7C%20x64-0078D4)
![Targets](https://img.shields.io/badge/Linux-x86%20%7C%20x64-FCC624)
![Stage0](https://img.shields.io/badge/Stage--0-C%2B%2B17-00599C)

`Jau → VM` &nbsp; `Jau → ASM` &nbsp; `Jau → OBJ` &nbsp; `Jau → PE EXE` &nbsp; `C/C++ ↔ Jau`

</div>

---

## Why Jau?

Jau is an experimental programming language and toolchain built around a compact syntax, a bytecode VM, an x86/x86-64 AOT backend, protected package distribution, native C/C++ interoperability, and measurable staged self-hosting.

The Windows native path is now owned by the Jau toolchain itself:

```text
main.jau
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
COFF .obj
   │
   ▼
 jauld
   │
   ├────────────► PE32   Windows x86
   └────────────► PE32+  Windows x86-64
```

For supported Jau AOT programs, the final Windows `.exe` no longer needs GCC, MinGW, MSVC `link.exe`, or LLVM LLD for the **OBJ → EXE** step.

## Execution models

```text
.jau source
    │
    ├── jauc run ─────────► execute immediately
    │
    ├── jauc build ───────► .jbc ───────► jur VM
    │
    ├── jauc standalone ──► JUR + embedded JBC executable
    │
    ├── jauc asm ─────────► Intel/GNU-style assembly
    │
    ├── jauc obj ─────────► ELF .o / COFF .obj
    │
    └── jauc native ──────► native executable
```

## Language at a glance

```jau
namespace MathBox {
    func add(a, b) {
        return a + b;
    }

    func mul(a, b) {
        return a * b;
    }
}

let answer = MathBox.add[20, 22];
print(answer);
```

Current language features include:

- integers, floats, booleans, strings and `null` in the VM;
- arrays, indexing, `push`, `pop`, `join`, `split` and string helpers;
- mutable `let` and enforced `const`;
- functions, recursion and namespaces;
- `if`, `else`, `while`, `break`, `continue`, `return`;
- normal calls `MathBox.add(1, 2)` and Jau bracket calls `MathBox.add[1, 2]`;
- local source imports and protected package imports;
- bytecode, standalone, assembly, object and native build paths;
- C ABI interoperability for the current integer/bool AOT boundary.

## Toolchain

| Tool | Purpose |
|---|---|
| `jauc` | Compiler, runner, bytecode builder, AOT emitter, object builder and native driver |
| `jur` | JBC virtual machine and embedded-payload runtime |
| `jauas` | Jau assembler/object writer for supported x86/x86-64 output |
| `jauld` | Internal Windows COFF → PE32/PE32+ linker |
| `jaupm` | Package manager whose command layer is written in Jau |
| `jau-setup` | Installs the toolchain and configures `JAU_HOME` / `PATH` |

## Quick start

Run source directly:

```bash
jauc run main.jau
```

Build bytecode:

```bash
jauc build main.jau -o main.jbc
jur main.jbc
```

Build a standalone VM executable:

```bash
jauc standalone main.jau -o main.exe
```

Emit assembly:

```bash
jauc asm main.jau -o main.s --target windows-x86_64
```

Emit a C/C++-linkable object:

```bash
jauc obj main.jau -o main.obj --target windows-x86_64
```

Build a native Windows executable with the internal PE linker:

```bash
jauc native main.jau -o main.exe --target windows-x86_64
```

32-bit Windows:

```bash
jauc native main.jau -o main32.exe --target windows-x86
```

Supported AOT targets:

```text
linux-x86_64
linux-x86
windows-x86_64
windows-x86
```

---

## Internal Windows PE linker

`jauld` consumes COFF objects produced by `jauas` and supported C-ABI objects, resolves symbols and relocations, creates imports when required, lays out PE sections, creates an entry point and emits Windows executables.

Direct use:

```cmd
jauas app.s -o app.obj --target windows-x86_64 --object
jauld app.obj -o app.exe --target windows-x86_64
```

Normal users usually only need:

```cmd
jauc native app.jau -o app.exe --target windows-x86_64
```

Currently exercised by CI on both Windows architectures:

- PE32+ / AMD64 executable creation;
- PE32 / i386 executable creation;
- COFF symbol resolution across multiple objects;
- x86/x64 relative and absolute relocation forms used by Jau;
- executable entry-point generation;
- native Jau package object linking;
- import-table/IAT paths used by the Jau AOT runtime.

`jauld` is deliberately scoped to Jau's supported native ABI and object subset. It is **not claimed to be a drop-in replacement for every feature of MSVC LINK or LLD** such as arbitrary COMDAT policy, PDB production, every SEH/TLS/resource scenario, delay-load libraries or the entire Windows linker surface.

---

## C/C++ → Jau native extensions (`.jaux`)

A C or C++ developer can compile native code into target-specific COFF objects, package those objects together with a Jau wrapper, install the package through JauPM, then import it like a normal Jau package.

Example package:

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

### C++ implementation

```cpp
extern "C" long long cppmath_mul(long long a, long long b) {
    return a * b;
}
```

Compile the extension object once for each architecture using your C/C++ compiler of choice:

```bash
x86_64-w64-mingw32-g++ -c -O2 -fno-exceptions -fno-rtti cppmath.cpp -o native/windows-x86_64/cppmath.obj
i686-w64-mingw32-g++   -c -O2 -fno-exceptions -fno-rtti cppmath.cpp -o native/windows-x86/cppmath.obj
```

C++ source compilation still requires a C/C++ compiler. After the object is packaged, **Jau users do not need that compiler to consume the extension**.

### `jau.pkg`

```ini
name="CppMath"
version="1.0.0"
main="src/main.jau"
type="native"
native_windows_x86_64="native/windows-x86_64/cppmath.obj"
native_windows_x86="native/windows-x86/cppmath.obj"
dependencies=""
```

Multiple objects may be comma-separated in a target field.

### Jau wrapper

```jau
extern func cppmath_mul(a, b);

namespace CppMath {
    func mul(a, b) {
        return cppmath_mul(a, b);
    }
}
```

### Pack and install

```cmd
jaupm pack dist\CppMath-1.0.0.jaux
jaupm verify dist\CppMath-1.0.0.jaux
jaupm install dist\CppMath-1.0.0.jaux
```

### Use from Jau

```jau
import "pkg:CppMath"

let answer = CppMath.mul[6, 7];
print(answer);
```

Build it:

```cmd
jauc native main.jau -o app.exe --target windows-x86_64
```

During native compilation Jau reads the wrapper and matching native object from the protected installed package, materializes the object only inside a temporary build directory, links it using `jauld`, then removes temporary build files.

See [`docs/NATIVE_EXTENSIONS.md`](docs/NATIVE_EXTENSIONS.md).

---

## Jau ↔ C/C++ object interoperability

Jau functions emitted in library/object mode use symbols such as:

```text
jau_fn_add
jau_fn_MathBox_mul
```

Jau:

```jau
func add(a, b) {
    return a + b;
}
```

Object:

```cmd
jauc obj math.jau -o math.obj --target windows-x86_64
```

C++:

```cpp
extern "C" long long jau_fn_add(long long, long long);
```

The reverse direction uses `extern func`:

```jau
extern func cpp_mul(a, b);

func calculate() {
    return cpp_mul(6, 7);
}
```

The current stable native FFI boundary is integer/bool-sized machine values. Rich VM-owned values such as Jau strings and arrays are not yet a stable C ABI.

See [`docs/INTEROP.md`](docs/INTEROP.md).

---

## Protected packages — JAUPKG2

JauPM packages source and package assets inside a protected binary archive:

```bash
jaupm init MathBox
jaupm pack
jaupm verify dist/MathBox-0.1.0.jaup
jaupm install dist/MathBox-0.1.0.jaup
```

Installed layout:

```text
$JAU_HOME/
└── packages/
    └── MathBox/
        ├── package.jaup
        └── .installed
```

The installed source does not need to exist as plaintext on disk. Package modules are read from `package.jaup` and expanded into the compilation unit in memory.

```jau
import "pkg:MathBox"
print(MathBox.add[20, 22]);
```

This works across the compiler paths used by `run`, `build`, `asm`, `obj`, `native` and `standalone`.

JAUPKG2 is practical source protection/obfuscation, **not a claim of unbreakable cryptographic secrecy** against determined reverse engineering.

## Internet registry

Default registry:

```text
https://irautox.ir/package
```

Install by package name:

```bash
jaupm install JTTP
```

JauPM requests:

```text
https://irautox.ir/package/JTTP
```

Configure another registry:

```bash
jaupm registry
jaupm config set registry https://example.com/package
jaupm config get registry
```

`JAU_REGISTRY` overrides the configured registry for the current environment.

---

## JauPM standalone

The high-level JauPM command implementation lives in:

```text
tools/jaupm.jau
```

The build turns it into a standalone `jaupm` / `jaupm.exe` executable.

```cmd
jaupm version
jaupm doctor
jaupm list
```

Launching `jaupm` by a bare PATH name is tested so it does not depend on the current directory to locate its embedded executable payload.

---

## Bootstrap and self-hosting

Jau has a staged bootstrap path:

```text
Stage-0 C++ toolchain
        │
        ▼
Jau-written Stage-1
        │
        ▼
assembly
        │
        ▼
jauas
        │
        ▼
Stage-2 executable
```

The Stage-1 implementation already processes a growing subset of Jau, but the production lexer, parser, optimizer, JBC compiler, VM, package/archive layer and full native backend still have substantial Stage-0 C++ ownership.

For that reason this repository does **not** advertise an invented “80% self-hosted” number. A real percentage should come from reproducible component ownership plus Stage-2/Stage-3 compiler equivalence.

See [`docs/BOOTSTRAP.md`](docs/BOOTSTRAP.md).

---

## Build Jau from source

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

GitHub Actions builds and tests:

```text
Linux x86-64
Linux x86
Windows x86-64
Windows x86
```

## Current native limitations

- AOT currently focuses on integer/bool values; strings and arrays remain VM-first.
- `jauas` supports the instruction subset generated by Jau's AOT/bootstrap pipelines rather than every MASM/NASM/GAS instruction form.
- `jauld` targets Jau-generated COFF and supported C-ABI objects; it does not promise every feature of industrial general-purpose Windows linkers.
- A `.jaux` package can contain precompiled C/C++ objects, but producing those C/C++ objects from source still requires a C/C++ compiler.
- HTTPS transport uses platform/external transport where appropriate rather than a home-grown TLS implementation.
- Full compiler self-host parity is still in progress.

## Documentation

- [`docs/PACKAGES.md`](docs/PACKAGES.md) — packages, registry and dependencies
- [`docs/NATIVE_EXTENSIONS.md`](docs/NATIVE_EXTENSIONS.md) — C/C++ `.jaux` packages
- [`docs/INTEROP.md`](docs/INTEROP.md) — Jau ↔ C/C++ ABI/object integration
- [`docs/BOOTSTRAP.md`](docs/BOOTSTRAP.md) — staged self-hosting model
- [`bootstrap/README.md`](bootstrap/README.md) — bootstrap source layout

---

<div align="center">

### Jau 0.6

**One language · VM + AOT · protected packages · native extensions · internal Windows PE linking**

</div>
