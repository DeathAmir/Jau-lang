<div align="center">

# Jau Programming Language

**A small systems-oriented language, bytecode VM, native AOT backend, protected package ecosystem, object-file interop, and staged self-hosting project.**

[![Build](https://github.com/DeathAmir/Jau-lang/actions/workflows/build.yml/badge.svg)](https://github.com/DeathAmir/Jau-lang/actions/workflows/build.yml)
![Version](https://img.shields.io/badge/version-0.6.0-blue)
![C++](https://img.shields.io/badge/stage--0-C%2B%2B17-00599C)
![Targets](https://img.shields.io/badge/targets-Windows%20%7C%20Linux-4c8bf5)

</div>

---

## What is Jau?

Jau is an experimental programming language and toolchain built around a compact syntax and a layered execution model:

```text
.jau source
    │
    ├── jauc build ───────► .jbc ───────► jur VM
    │
    ├── jauc standalone ──► single executable with embedded JBC runtime
    │
    ├── jauc asm ─────────► Intel/GNU assembly
    │
    └── jauc obj ─────────► ELF .o / COFF .obj ──► C/C++ linker
```

Version **0.6.0** focuses on package protection, namespaces, C/C++ interoperability, Windows object generation, a more capable Jau-written package manager, and another bootstrap/self-hosting step.

## Language at a glance

```jau
namespace MathBox {
    func add(a, b) {
        return a + b;
    }
}

let values = [10, 20, 30];
push(values, 40);

print(MathBox.add[20, 22]);
```

Implemented today:

- integers, floats, booleans, strings and null;
- shared arrays and indexing;
- mutable `let` and enforced `const`;
- functions, recursion and namespaces;
- `if / else`, `while`, `break`, `continue`, `return`;
- source imports and protected package imports;
- arithmetic, comparison and boolean operators;
- bytecode compilation and JUR execution;
- integer/bool AOT code generation for x86 and x86-64;
- C ABI object interoperability.

## Toolchain

| Tool | Purpose |
|---|---|
| `jauc` | Compiler, runner, AOT emitter, object builder and standalone bundler |
| `jur` | JBC virtual machine and embedded-payload runtime |
| `jauas` | Dependency-free assembler/object writer for Jau's supported x86 subset |
| `jaupm` | Package manager whose command logic is written in Jau |
| `jau-setup` | Installs the toolchain and configures `JAU_HOME` / `PATH` |

### Compile and run

```bash
jauc run app.jau
jauc build app.jau -o app.jbc
jur app.jbc
```

### Standalone executable

```bash
jauc standalone app.jau -o app.exe
```

The standalone format embeds JBC into the JUR executable image. It does not require GCC/MinGW at application build time.

### Assembly

```bash
jauc asm app.jau -o app.s --target windows-x86_64
```

Supported AOT targets:

```text
linux-x86_64
linux-x86
windows-x86_64
windows-x86
```

## Native object files

Jau 0.6 can emit relocatable object files that can be linked with normal C/C++ projects:

```bash
jauc obj math.jau -o math.o --target linux-x86_64
jauc obj math.jau -o math.obj --target windows-x86_64
```

`jauas` writes:

- ELF64 relocatable objects on Linux x86-64;
- ELF32 relocatable objects on Linux x86;
- COFF x86-64 objects on Windows x86-64;
- COFF x86 objects on Windows x86.

Jau functions are exported using the stable prefix:

```text
jau_fn_<function>
```

For example:

```jau
func add(a, b) {
    return a + b;
}
```

can be called from C++:

```cpp
extern "C" long long jau_fn_add(long long, long long);
```

## Jau calling C/C++

Declare a foreign C-ABI function:

```jau
extern func cpp_mul(a, b);

print(cpp_mul(6, 7));
```

Implement it in C++:

```cpp
extern "C" long long cpp_mul(long long a, long long b) {
    return a * b;
}
```

Then build both in one command:

```bash
jauc native app.jau -o app --target linux-x86_64 --link helper.cpp
```

`--link` can also accept C sources and prebuilt object files. Rich FFI types are not implemented yet; the current stable AOT boundary is integer/bool machine values.

See [`docs/INTEROP.md`](docs/INTEROP.md).

## Protected packages — JAUPKG2

JauPM builds protected binary `.jaup` archives:

```bash
jaupm init MathBox
jaupm pack
jaupm verify dist/MathBox-0.1.0.jaup
jaupm install dist/MathBox-0.1.0.jaup
```

Installed packages stay opaque on disk:

```text
$JAU_HOME/
└── packages/
    └── MathBox/
        ├── package.jaup
        └── .installed
```

The compiler does **not** need to extract package source to disk. During `run`, `build`, `asm`, `obj`, `native`, and `standalone`, imported package modules are decoded from the archive in memory and merged into the compilation unit.

```jau
import "pkg:MathBox"

print(MathBox.add[20, 22]);
```

JAUPKG2 is source-obfuscation/protection, not a promise of cryptographic secrecy against a determined reverse engineer.

## Internet package registry

Default registry:

```text
https://irautox.ir/package
```

Installing:

```bash
jaupm install JTTP
```

requests:

```text
https://irautox.ir/package/JTTP
```

Registry configuration:

```bash
jaupm registry
jaupm config set registry https://example.com/package
jaupm config get registry
```

`JAU_REGISTRY` overrides the configured registry for the current environment.

## JTTP

The repository includes **JTTP — Jau HTTP**, a Jau package that provides higher-level URL, GET, download, query, cache and response helpers on top of the runtime networking primitives.

```jau
import "pkg:JTTP"

let page = JTTP.fetch["https://example.com"];
print(page);
```

## `jaupm` is a real standalone executable

The JauPM command logic lives in:

```text
tools/jaupm.jau
```

During the toolchain build, Jau compiles it through `jauc standalone` into `jaupm` / `jaupm.exe`.

The runtime resolves the actual process image rather than trusting a bare `argv[0]`, so launching through `PATH` works correctly:

```cmd
jaupm version
```

without requiring `jur.exe` to be manually addressed by the user.

## Bootstrap and self-hosting

Jau uses staged bootstrapping:

```text
Stage-0 C++ compiler
        │
        ▼
Jau-written Stage-1 compiler
        │
        ▼
Intel assembly
        │
        ▼
jauas
        │
        ▼
Stage-2 executable
```

The Jau-written Stage-1 currently handles:

- `let` / `const` integer declarations;
- identifier and integer expression evaluation;
- `+`, `-`, `*` bootstrap expressions;
- integer and string `print(...)`;
- integer `return`;
- Linux x86 and x86-64 freestanding output.

This is genuine staged self-hosting work, but **Jau is not truthfully 80% self-hosted yet**. The production lexer, parser, optimizer, JBC compiler, VM, package loader and full AOT backend still live in Stage-0 C++. Jau will only claim a self-host percentage once component ownership is measured reproducibly and Stage-2/Stage-3 compiler equivalence exists.

See [`docs/BOOTSTRAP.md`](docs/BOOTSTRAP.md).

## Build from source

### Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

### Windows x64

```cmd
cmake -S . -B build -A x64 -DJAU_ENABLE_ASM_FASTOPS=OFF
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

GitHub Actions continuously builds:

- Linux x86-64;
- Linux x86;
- Windows x86-64;
- Windows x86.

## Current limitations

- AOT/FFI is currently integer/bool oriented; strings and arrays remain VM-first features.
- `jauas` supports the instruction subset emitted by Jau's current AOT/bootstrap backends; it is not a general replacement for GNU `as`, MASM or NASM.
- Windows `jauas` emits COFF `.obj`; final PE linking is intentionally delegated to a normal C/C++ linker/driver.
- Native HTTPS uses vetted platform/external transport rather than a home-grown TLS implementation.
- Full compiler self-host parity is not complete.

## Documentation

- [`docs/PACKAGES.md`](docs/PACKAGES.md) — JAUPKG2, registry and dependencies
- [`docs/INTEROP.md`](docs/INTEROP.md) — Jau ↔ C/C++ object and ABI integration
- [`docs/BOOTSTRAP.md`](docs/BOOTSTRAP.md) — staged self-hosting model
- [`bootstrap/README.md`](bootstrap/README.md) — bootstrap source layout

---

<div align="center">

**Jau 0.6 — small language, real toolchain, measurable bootstrap progress.**

</div>
