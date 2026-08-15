<div align="center">

# Jau Programming Language

### Simple source. Native code. A toolchain Jau can increasingly own itself.

**VM · Native AOT · Internal Windows PE32/PE32+ linker · Protected packages · C/C++ extensions · Staged self-hosting**

[![Build](https://github.com/DeathAmir/Jau-lang/actions/workflows/build.yml/badge.svg)](https://github.com/DeathAmir/Jau-lang/actions/workflows/build.yml)
[![Native 0.7](https://github.com/DeathAmir/Jau-lang/actions/workflows/v07-native.yml/badge.svg)](https://github.com/DeathAmir/Jau-lang/actions/workflows/v07-native.yml)
![Version](https://img.shields.io/badge/Jau-0.7.0-4c8bf5)
![Windows](https://img.shields.io/badge/Windows-PE32%20%7C%20PE32%2B-0078D4)
![Linux](https://img.shields.io/badge/Linux-x86%20%7C%20x64-FCC624)
![Stage0](https://img.shields.io/badge/Stage--0-C%2B%2B17-00599C)

`Jau → VM` &nbsp; `Jau → ASM` &nbsp; `Jau → OBJ` &nbsp; `Jau → PE EXE` &nbsp; `C/C++ ↔ Jau`

</div>

---

## What is Jau?

Jau is an experimental systems-oriented programming language built around a compact syntax and a deliberately visible toolchain. The project contains its own bytecode VM, x86/x86-64 AOT backend, assembler/object writer, Windows PE linker, package manager, protected package format and staged self-hosting path.

For supported Windows native programs the final executable path is owned by Jau:

```text
main.jau
   │
   ▼
  jauc          parser + optimizer + AOT
   │
   ▼
Intel assembly
   │
   ▼
 jauas          x86/x64 object writer
   │
   ▼
COFF .obj
   │
   ▼
 jauld          Jau's PE linker
   │
   ├────────────► PE32   Windows x86
   └────────────► PE32+  Windows x86-64
```

For that supported path, **OBJ → EXE does not require GCC, MinGW, MSVC `link.exe`, or LLVM LLD**.

---

## 30-second example

```jau
func add(a, b) {
    return a + b;
}

func main() {
    let answer = add(20, 22);
    print(answer);
    return 0;
}

main();
```

Run it:

```cmd
jauc run main.jau
```

Build a native Windows x64 executable:

```cmd
jauc native main.jau -o main.exe --target windows-x86_64
main.exe
```

Expected output:

```text
42
```

`jauc native` defaults to the current maximum optimization level (`-O3`) unless you explicitly select another level.

---

## Jau 0.7 highlights

### Smart object metadata (`JAUMETA1`)

`jauc obj` now emits a sidecar metadata file together with the object:

```cmd
jauc obj main.jau -o main.obj --target windows-x86_64 -O3
```

Produces:

```text
main.obj
main.obj.jmeta
```

Example metadata:

```ini
JAUMETA1
producer="jauc"
version="0.7.0"
target="windows-x86_64"
abi="jau-c-v1"
kind="jau-object"
subsystem="console"
entry="jau_fn_main"
optimize=3
```

The linker reads this automatically:

```cmd
jauld main.obj -o main.exe
```

This fixes the old mismatch where library-mode Jau objects exported `jau_fn_main` while `jauld` only searched for `main`.

Even if the metadata is removed, `jauld` has compatibility entry discovery for `main` / `jau_fn_main` and their x86 decorated forms.

### Monotonic time API

```jau
let start = time.now_ms();

let i = 0;
while (i < 1000000) {
    i = i + 1;
}

let finish = time.now_ms();
print(finish - start);
```

Available names:

```text
time.now_ms()
time.now_ns()
time.sleep_ms(ms)
clock_ms()
clock_ns()
sleep_ms(ms)
clock()
```

`time.now_ms()` is the recommended benchmark clock. The VM uses `steady_clock`; Windows native AOT maps to the platform monotonic tick API.

### Faster integer AOT path

Jau 0.7 improves common integer arithmetic/comparison code generation by avoiding part of the temporary push/pop traffic for simple variable/literal expressions and adds a final peephole cleanup pass.

Typical hot-loop source:

```jau
i = i + 1;
sum = sum + i;
```

is now a better target for direct register operations than in previous releases.

Jau does **not** claim that every program is universally faster than optimized C++. Performance claims should be measured per workload and checked against generated assembly. See [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md).

---

## Language at a glance

### Variables and constants

```jau
let score = 10;
score = score + 1;

const max_score = 100;
```

### Functions

```jau
func mul(a, b) {
    return a * b;
}
```

### Control flow

```jau
if (score >= 10) {
    print(1);
} else {
    print(0);
}

let i = 0;
while (i < 10) {
    i = i + 1;
}
```

`break`, `continue`, recursion and `return` are supported.

### Namespaces

```jau
namespace MathBox {
    func add(a, b) {
        return a + b;
    }
}

print(MathBox.add(20, 22));
print(MathBox.add[20, 22]);
```

Both normal calls and Jau bracket-call syntax are supported.

### Arrays (VM)

```jau
let values = [10, 20, 30];
print(values[0]);
push(values, 40);
print(pop(values));
```

Current limitation: indexed assignment such as `values[1] = 99;` is not yet part of the assignment grammar.

### Imports

```jau
import "local_module.jau"
import "pkg:MathBox"
```

See the full implemented syntax in [`docs/LANGUAGE.md`](docs/LANGUAGE.md).

---

## Execution models

```text
.jau source
    │
    ├── jauc run ─────────► execute in VM
    │
    ├── jauc build ───────► .jbc ───────► jur
    │
    ├── jauc standalone ──► runtime + embedded JBC executable
    │
    ├── jauc asm ─────────► assembly
    │
    ├── jauc obj ─────────► ELF .o / COFF .obj + Jau metadata
    │
    └── jauc native ──────► native executable
```

Supported AOT targets:

```text
linux-x86_64
linux-x86
windows-x86_64
windows-x86
```

---

## Toolchain

| Tool | Role |
|---|---|
| `jauc` | compiler driver, VM runner, JBC builder, AOT emitter, object/native driver |
| `jur` | JBC virtual machine and embedded-payload runtime |
| `jauas` | Jau x86/x86-64 assembler/object writer |
| `jauld` | internal COFF → PE32/PE32+ Windows linker |
| `jaupm` | package manager whose high-level command layer is written in Jau |
| `jau-setup` | toolchain installer / `JAU_HOME` setup |

### Common commands

```cmd
jauc run main.jau
jauc build main.jau -o main.jbc
jur main.jbc
jauc asm main.jau -o main.s --target windows-x86_64 -O3
jauc obj main.jau -o main.obj --target windows-x86_64 -O3
jauld main.obj -o main.exe
jauc native main.jau -o main.exe --target windows-x86_64 -O3
jauc native main.jau -o main32.exe --target windows-x86 -O3
```

---

## Internal PE linker

`jauld` reads supported x86/x64 COFF objects, resolves Jau/C ABI symbols and relocations, builds imports/IAT, lays out PE sections, creates a startup thunk and writes a Windows executable.

Direct usage:

```cmd
jauld app.obj -o app.exe
```

Optional overrides:

```cmd
jauld app.obj -o app.exe --entry jau_fn_main
jauld app.obj -o app.exe --meta custom.jmeta
jauld app.obj -o app.exe --target windows-x86_64
```

Current native CI exercises PE32 and PE32+ executables on real Windows runners.

`jauld` is scoped to the object/ABI subset supported by Jau. It is **not advertised as a complete drop-in replacement for every feature of MSVC LINK or LLD**.

Detailed linker/metadata reference: [`docs/LINKER.md`](docs/LINKER.md).

---

## C/C++ native extensions (`.jaux`)

A native package combines a Jau wrapper with target-specific precompiled C/C++ objects.

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

C++:

```cpp
extern "C" long long cppmath_mul(long long a, long long b) {
    return a * b;
}
```

Jau wrapper:

```jau
extern func cppmath_mul(a, b);

namespace CppMath {
    func mul(a, b) {
        return cppmath_mul(a, b);
    }
}
```

Manifest:

```ini
name="CppMath"
version="1.0.0"
main="src/main.jau"
type="native"
native_windows_x86_64="native/windows-x86_64/cppmath.obj"
native_windows_x86="native/windows-x86/cppmath.obj"
```

Package it:

```cmd
jaupm pack dist\CppMath-1.0.0.jaux
jaupm verify dist\CppMath-1.0.0.jaux
jaupm install dist\CppMath-1.0.0.jaux
```

Consume it normally:

```jau
import "pkg:CppMath"
print(CppMath.mul[6, 7]);
```

```cmd
jauc native main.jau -o app.exe --target windows-x86_64
```

Producing the C/C++ object initially still requires a C/C++ compiler. **The Jau user consuming an already-built `.jaux` package does not need that compiler for Jau's final PE link step.**

See [`docs/NATIVE_EXTENSIONS.md`](docs/NATIVE_EXTENSIONS.md).

---

## Jau ↔ C/C++ object ABI

Object/library mode exports Jau functions as symbols such as:

```text
jau_fn_add
jau_fn_MathBox_mul
```

C/C++ can call them through the current machine-value C ABI:

```cpp
extern "C" long long jau_fn_add(long long, long long);
```

The reverse direction:

```jau
extern func cpp_mul(a, b);
```

The current stable native FFI boundary is integer/bool-sized machine values. VM-owned strings and arrays do not yet have a stable native ownership ABI.

See [`docs/INTEROP.md`](docs/INTEROP.md).

---

## Protected packages — JAUPKG2

```cmd
jaupm init MathBox
jaupm pack
jaupm verify dist\MathBox-0.1.0.jaup
jaupm install dist\MathBox-0.1.0.jaup
```

Installed package source can remain inside the protected archive instead of being extracted as plaintext:

```text
$JAU_HOME/
└── packages/
    └── MathBox/
        ├── package.jaup
        └── .installed
```

```jau
import "pkg:MathBox"
print(MathBox.add[20, 22]);
```

JAUPKG2 is practical source protection/obfuscation, not a claim of unbreakable cryptographic secrecy.

Default registry:

```text
https://irautox.ir/package
```

```cmd
jaupm install JTTP
jaupm config set registry https://example.com/package
```

See [`docs/PACKAGES.md`](docs/PACKAGES.md).

---

## Optimization and benchmarking

Optimization levels:

```text
-O0  minimal/no optimization
-O1  AST optimization
-O2  AST + native register/peephole improvements
-O3  current maximum
```

Inspect optimized assembly:

```cmd
jauc asm bench.jau -o bench.s --target windows-x86_64 -O3
```

Benchmark with Jau's monotonic timer:

```jau
func main() {
    let start = time.now_ms();
    let i = 0;
    let value = 0;

    while (i < 1000000000) {
        value = value + i;
        i = i + 1;
    }

    let finish = time.now_ms();
    print(value);
    print(finish - start);
    return 0;
}

main();
```

```cmd
jauc native bench.jau -o bench.exe --target windows-x86_64 -O3
bench.exe
```

Use identical algorithms, integer widths and release settings when comparing against C/C++/Rust/Zig. Full performance notes and optimizer roadmap: [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md).

---

## Bootstrap / self-hosting

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

The Stage-1 compiler processes a growing subset of Jau, but production lexer/parser/optimizer/VM/package/native-backend ownership is not yet fully self-hosted. The project intentionally avoids inventing a fake self-hosting percentage.

See [`docs/BOOTSTRAP.md`](docs/BOOTSTRAP.md).

---

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

GitHub Actions covers Linux/Windows x86/x64 plus dedicated native PE and extension tests.

---

## Current boundaries

Jau 0.7 is a real working experimental toolchain, not yet a mature replacement for industrial C++/Rust/Go ecosystems.

- VM supports a broader value/builtin surface than native AOT.
- AOT is currently integer/bool focused.
- Native string/array ownership ABI is not stable yet.
- Indexed l-value assignment (`arr[i] = x`) is not implemented yet.
- Windows AOT `time.now_ns()` currently has millisecond effective resolution; prefer `time.now_ms()` for benchmarks.
- `jauas` covers the instruction subset generated by Jau rather than every general-purpose assembler syntax.
- `jauld` covers Jau's supported COFF/PE subset rather than the complete Windows linker universe.
- Building C/C++ extension objects from `.c/.cpp` source requires a C/C++ compiler; consuming prebuilt `.jaux` packages does not require an external linker for the Jau → PE stage.
- Full compiler self-host parity remains in progress.

---

## Documentation

- [`docs/LANGUAGE.md`](docs/LANGUAGE.md) — implemented syntax and builtins
- [`docs/LINKER.md`](docs/LINKER.md) — `JAUMETA1`, entry resolution and PE linking
- [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md) — optimizer, timing and benchmark methodology
- [`docs/PACKAGES.md`](docs/PACKAGES.md) — packages, registry and dependencies
- [`docs/NATIVE_EXTENSIONS.md`](docs/NATIVE_EXTENSIONS.md) — C/C++ `.jaux` packages
- [`docs/INTEROP.md`](docs/INTEROP.md) — Jau ↔ C/C++ ABI/object integration
- [`docs/BOOTSTRAP.md`](docs/BOOTSTRAP.md) — staged self-hosting
- [`bootstrap/README.md`](bootstrap/README.md) — bootstrap source layout

---

<div align="center">

## Jau 0.7

**Small syntax · visible compiler · native Windows binaries · protected packages · C/C++ extensions**

</div>
