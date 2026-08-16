from pathlib import Path


def read(p): return Path(p).read_text(encoding='utf-8')
def write(p,s):
    Path(p).parent.mkdir(parents=True,exist_ok=True)
    Path(p).write_text(s,encoding='utf-8')
def repall(s,a,b,label):
    if a not in s: raise SystemExit('anchor not found: '+label)
    return s.replace(a,b)

# Installer: JauM is a first-class tool and must be copied by jau-setup.
p='installer/jau_setup.cpp'; s=read(p)
s=repall(s,
'copy_if(root/"jauld.exe",prefix/"bin"/"jauld.exe");copy_if(root/"jau-setup.exe"',
'copy_if(root/"jauld.exe",prefix/"bin"/"jauld.exe");copy_if(root/"jaum.exe",prefix/"bin"/"jaum.exe");copy_if(root/"jau-setup.exe"','installer Windows jaum')
s=repall(s,
'copy_if(root/"jauld",prefix/"bin"/"jauld");copy_if(root/"jau-setup"',
'copy_if(root/"jauld",prefix/"bin"/"jauld");copy_if(root/"jaum",prefix/"bin"/"jaum");copy_if(root/"jau-setup"','installer Linux jaum')
write(p,s)

# Main build artifacts must include JauM on all four supported targets.
p='.github/workflows/build.yml'; s=read(p)
s=s.replace('build/jauas build/jauld build/jau-setup build/jaupm dist/', 'build/jauas build/jauld build/jaum build/jau-setup build/jaupm dist/')
s=s.replace('build32/jauas build32/jauld build32/jau-setup build32/jaupm dist32/', 'build32/jauas build32/jauld build32/jaum build32/jau-setup build32/jaupm dist32/')
s=s.replace('build\\Release\\jauas.exe,build\\Release\\jauld.exe,build\\Release\\jau-setup.exe,build\\Release\\jaupm.exe dist\\', 'build\\Release\\jauas.exe,build\\Release\\jauld.exe,build\\Release\\jaum.exe,build\\Release\\jau-setup.exe,build\\Release\\jaupm.exe dist\\')
s=s.replace('build32\\Release\\jauas.exe,build32\\Release\\jauld.exe,build32\\Release\\jau-setup.exe,build32\\Release\\jaupm.exe dist32\\', 'build32\\Release\\jauas.exe,build32\\Release\\jauld.exe,build32\\Release\\jaum.exe,build32\\Release\\jau-setup.exe,build32\\Release\\jaupm.exe dist32\\')
s=s.replace('if (!(Test-Path .\\install-test\\bin\\jauld.exe)) { throw "jauld.exe was not installed" }', 'if (!(Test-Path .\\install-test\\bin\\jauld.exe)) { throw "jauld.exe was not installed" }\n          if (!(Test-Path .\\install-test\\bin\\jaum.exe)) { throw "jaum.exe was not installed" }')
write(p,s)

# Official Win32 GUI example. The C++ side is intentionally tiny: Jau owns the
# application logic; the extension exposes a stable C ABI and declares user32 as
# a linker dependency through the build command/package manifest.
write('examples/windows_gui/window.cpp', r'''#include <windows.h>
#include <cstdint>

extern "C" std::intptr_t jau_message_box(const char* text, const char* title) {
    return static_cast<std::intptr_t>(MessageBoxA(nullptr, text, title, MB_OK | MB_ICONINFORMATION));
}
''')
write('examples/windows_gui/main.jau', r'''extern func jau_message_box(text, title):int;

func main() {
    jau_message_box("Hello from a native Jau GUI", "Jau 0.9");
    return 0;
}
''')
write('examples/windows_gui/CMakeLists.txt', r'''cmake_minimum_required(VERSION 3.16)
project(JauWindowNative LANGUAGES CXX)
add_library(jau_window OBJECT window.cpp)
''')
write('examples/windows_gui/jaum.txt', '''name=jau-window
source=examples/windows_gui/main.jau
type=exe
target=windows-x86_64
output=build/{name}-{target}{ext}
optimize=0
subsystem=windows
links=examples/windows_gui/window.obj
system_libs=user32
''')

README=r'''<div align="center">

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
| `linux-x86` | ✅ | ELF `.o` | ELF | toolchain path* | archive path* |

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
cmake -S examples\windows_gui -B gui-native -A x64
cmake --build gui-native --config Release
copy gui-native\Release\jau_window.obj examples\windows_gui\window.obj

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
'''
write('README.md',README)

LANG=r'''# Jau Language Reference — 0.9

This document describes implemented source-language behavior. VM-only or AOT-only differences are called out instead of hidden.

## Program entry

```jau
func main() {
    print("hello");
    return 0;
}
```

A zero-argument `main` is invoked automatically when there is no explicit top-level call. Do not append `main();` to ordinary applications.

## Comments

```jau
// one line
/* block comment */
```

## Variables and constants

```jau
let count = 1;
var other = 2;
const limit = 10;
count += 1;
count++;
```

`let` and `var` create mutable bindings. `const` rejects reassignment.

## Values

The VM supports null, booleans, integers, floating-point values, strings and arrays. Current native AOT is intentionally narrower and primarily targets machine-word integer/bool values plus literal string pointers at the C ABI boundary.

```jau
let nothing = null;
let also_nothing = nil;
let ok = true;
let n = 42;
let pi = 3.14;
let text = "Jau";
let list = [1, 2, 3];
```

## Numeric literals

```jau
let decimal = 1_000_000;
let hex = 0xff;
let binary = 0b1010;
let octal = 0o755;
```

## Operators

Arithmetic: `+ - * / %`

Comparison: `== != < <= > >=`

Logical: `&& || !` and aliases `and or not`

Bitwise: `& | ^ ~ << >>`

Assignment: `= += -= *= /= %=`

Increment/decrement: `++ --`

## Functions

```jau
func add(a:int, b:int):int {
    return a + b;
}
```

`fn`, `def` and `function` are accepted aliases for `func`. Type annotations are currently syntax/ABI metadata, not a complete static type checker.

## Control flow

```jau
if (score >= 50) {
    print("pass");
} else {
    print("fail");
}

let i = 0;
while (i < 10) {
    i++;
}

for (let j = 0; j < 10; j++) {
    if (j == 3) { continue; }
    if (j == 8) { break; }
}
```

Recursion is supported by the VM and supported AOT function subset.

## Arrays

```jau
let values = [10, 20, 30];
print(values[0]);
values[1] = 42;
push(values, 99);
print(pop(values));
print(join(values, ","));
```

VM/JBC indexed assignment is bounds-checked. Compound indexed assignment is not yet syntax sugar; write:

```jau
values[i] = values[i] + 1;
```

instead of `values[i] += 1`.

Arrays and indexed mutation are VM/JBC features in 0.9. Native AOT reports an error rather than silently compiling invalid container code.

## Strings

Common VM helpers include:

```text
len str int contains starts_with substr char_at find
trim upper lower replace split join read_line
```

Literal strings can be emitted in native AOT `.rodata` and passed to C ABI functions as borrowed `const char*`. Dynamic ownership across the native ABI is not standardized yet.

## Namespaces

```jau
namespace Math {
    func add(a, b) { return a + b; }
}

func main() {
    print(Math.add(20, 22));
}
```

The bracket call form remains accepted for compatibility:

```jau
Math.add[20, 22]
```

## Imports

```jau
import "local.jau"
import "pkg:MathX"
```

Package imports read the package manifest to find Jau wrapper source. Native package object members never enter the lexer/parser.

## Native C ABI declarations

```jau
extern func native_add(a:int, b:int):int;
```

Inside a namespace, an `extern func` still keeps its raw C ABI linker symbol. The namespace applies to Jau wrapper functions, not to the external C name.

## JSON helpers

```jau
let doc = "{\"user\":{\"name\":\"Amir\"},\"items\":[3,7]}";
print(json_string(doc, "user.name"));
print(json_get(doc, "items[1]"));
```

See `HTTP_JSON.md` for the full helper list.

## Standard/builtin surface

Filesystem/system helpers include `read_file`, `write_file`, `file_exists`, `mkdir`, `remove_file`, `remove_tree`, `list_dir`, `file_size`, `path_join`, `cwd`, `temp_dir`, `getenv`, `platform`, `arch`, `random_int`.

Timing includes `clock_ms`, `clock_ns`, `time.now_ms`, `time.now_ns`, `sleep_ms`, `time.sleep_ms`.

Networking includes `http_get` and `download` with platform-specific transport behavior.

## VM vs native

The VM is the broad dynamic runtime. AOT is intentionally explicit: if a construct has no stable native ABI/codegen path, compilation should fail with a stage-specific diagnostic. This is a design rule, not something callers should work around by accepting wrong output.
'''
write('docs/LANGUAGE.md',LANG)

CLI=r'''# Jau CLI Reference

## `jauc`

`jauc` is the main compiler driver.

```text
jauc run file.jau
jauc debug file.jau
jauc check file.jau
jauc build file.jau -o app.jbc
jauc standalone file.jau -o app
jauc asm file.jau -o app.s --target TARGET
jauc obj file.jau -o app.obj --target TARGET
jauc native file.jau -o app --target TARGET
jauc shared file.jau -o library --target TARGET --export name
jauc static file.jau -o library --target TARGET
jauc targets
jauc --version
```

### Native linker options

```text
--link path.obj        add an object/native link input
--system-lib name      request a system library/DLL
--import symbol=dll    exact Windows PE import mapping
--subsystem console    Windows console subsystem
--subsystem windows    Windows GUI subsystem
--export name          public shared-library export
--export public=inner  export alias
-I path                import search path
--debug                print package/object/symbol/link diagnostics
-O0 .. -O3             accepted optimization level
```

Optimization transforms remain correctness-safe/no-op where disabled in the 0.9 line.

## `jur`

Runs JBC bytecode and is also the payload runtime used by `jauc standalone`.

```text
jur app.jbc arg1 arg2
```

A bundled standalone user program does not print Jau's tool copyright banner.

## `jauas`

Converts Jau-generated assembly into ELF/COFF objects and supported executable forms.

```text
jauas app.s -o app.obj --target windows-x86_64 --object
jauas app.s -o app.o --target linux-x86_64 --object
```

## `jauld`

Internal Windows PE linker:

```text
jauld main.obj native.obj -o app.exe --target windows-x86_64
jauld main.obj native.obj -o app.exe --system-lib user32
jauld lib.obj -o lib.dll --target windows-x86_64 --dll --export add
```

`--system-lib` reads the requested DLL's export table when available. `--import symbol=dll` is the deterministic override.

## `jaum`

Project builder. See `JAUM.md`.

## `jaupm`

Package manager for `.jaup` and `.jaux` packages.

## Exit behavior

Compiler/runtime failures use non-zero exit status and diagnostics. Unsupported native constructs should fail instead of producing a binary that silently omits behavior.
'''
write('docs/CLI.md',CLI)

TARGETS=r'''# Targets and Binary Formats

Jau 0.9 exposes these AOT target names:

```text
windows-x86_64
windows-x86
linux-x86_64
linux-x86
```

Use `jauc targets` rather than guessing target spellings.

## Windows

`windows-x86_64` emits Intel x86-64 assembly, COFF objects and PE32+ images. `windows-x86` emits x86 assembly, COFF objects and PE32 images.

Jau's internal `jauld` handles the Jau-supported COFF/PE subset, imports, Jau/native symbols, console/GUI subsystem selection and DLL exports. It is intentionally not advertised as a universal replacement for every MSVC LINK/LLD feature.

## Linux

`linux-x86_64` and `linux-x86` emit GNU-style assembly and ELF objects. Final native ELF/shared linking uses the host system linker/compiler driver because Jau does not yet ship a general internal ELF dynamic linker.

## Libraries

| Output | Windows | Linux |
|---|---|---|
| object | `.obj` | `.o` |
| executable | `.exe` PE | ELF executable |
| shared | `.dll` | `.so` |
| static | `.lib` | `.a` |

The `.lib` from `jauc static` is a static COFF archive, not an import library for a DLL.

## ABI width

Jau native integer ABI currently follows machine word width: 64-bit on x64, 32-bit on x86. Native C/C++ extensions should prefer `intptr_t`/`std::intptr_t` at this boundary.

## Not implemented targets

0.9 does not claim production macOS, ARM32, ARM64, RISC-V, WebAssembly or mobile-native AOT backends. Adding a target means adding codegen/object/link/runtime tests, not only accepting a target string.
'''
write('docs/TARGETS.md',TARGETS)

SYSTEM=r'''# Native System Libraries

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
'''
write('docs/SYSTEM_LIBRARIES.md',SYSTEM)

LIBS=r'''# Shared and Static Libraries

## Shared libraries

Source:

```jau
func add(a:int,b:int):int { return a + b; }
func answer():int { return 42; }
```

Windows:

```cmd
jauc shared math.jau -o jaucalc --target windows-x86_64 --export add --export answer
```

Produces `jaucalc.dll`. Public export `add` maps to Jau's internal `jau_fn_add` symbol. CI loads the DLL with `LoadLibrary` and calls exports with `GetProcAddress`.

Linux:

```bash
jauc shared math.jau -o libjaucalc --target linux-x86_64 --export add --export answer
```

Produces `libjaucalc.so`; exported aliases are visible through `dlsym("add")`.

Alias form:

```text
--export public_name=internal_jau_name
```

## Static libraries

Windows:

```cmd
jauc static math.jau -o jaucalc --target windows-x86_64
```

Produces `jaucalc.lib` with Jau's internal COFF archive writer.

Linux:

```bash
jauc static math.jau -o libjaucalc --target linux-x86_64
```

Produces `libjaucalc.a` with an archive symbol index usable by GCC/binutils.

C/C++ static callers normally reference Jau's ABI symbol names:

```cpp
extern "C" intptr_t jau_fn_add(intptr_t, intptr_t);
```

## Shared vs static `.lib`

`jauc static` creates a static `.lib`. Jau 0.9 does not claim that this same file is a generated DLL import library. If a build requires a conventional import library for a Jau DLL, use dynamic loading or the platform's import-library tools until Jau gains a dedicated `--implib` feature.

## JauM

```ini
type=shared
exports=add,answer
```

or:

```ini
type=static
```

See `JAUM.md`.
'''
write('docs/LIBRARIES.md',LIBS)

JAUM=r'''# JauM Project Builder

`jaum` is Jau's project-oriented front end. It reads `jaum.txt` and invokes the same `jauc` pipeline used manually, so project builds do not invent a second linker/package model.

## Create a project

```cmd
jaum init MyApp
```

Typical configuration:

```ini
name=MyApp
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

## Commands

```text
jaum build
jaum build --target windows-x86
jaum build-all
jaum clean
jaum show
```

## Output types

```text
type=exe
type=shared
type=static
type=obj
type=asm
```

Shared libraries can declare:

```ini
exports=add,answer
```

Native project dependencies:

```ini
links=native/math.obj,native/window.obj
system_libs=user32,gdi32
imports=SpecialCall=special.dll
```

## Multi-target builds

`build-all` uses the `targets=` list. A target is only valid if the compiler/toolchain supports it; JauM does not pretend to create macOS/ARM output from unsupported backends.

## Windows process execution

JauM launches `jauc` with `CreateProcess`, not fragile `cmd.exe` command concatenation. This keeps paths containing spaces working correctly.
'''
write('docs/JAUM.md',JAUM)

HTTP=r'''# HTTP and JSON

Jau's VM is the current high-level networking/data runtime.

## HTTP

```jau
func main() {
    let body = http_get("http://example.com/api");
    print(body);
}
```

`download(url, path)` writes a response to disk.

Transport behavior is platform-dependent. Plain HTTP has a POSIX socket path. HTTPS currently falls back to platform tools (for example curl on Linux or PowerShell on Windows) where Jau does not yet provide a native TLS implementation. Do not treat this as a promise of an embedded TLS stack.

## JSON validation

```jau
print(json_valid("{\"ok\":true}"));
```

## Query paths

```jau
let body = "{\"user\":{\"name\":\"Jau\"},\"items\":[3,7]}";
print(json_string(body, "user.name"));
print(json_get(body, "items[1]"));
```

Paths support object keys separated by `.` and array indexes in brackets.

## Functions

`json_valid(text)` returns a boolean and does not throw for malformed JSON.

`json_get(text, path)` returns a decoded string scalar or serialized JSON for numbers, booleans, null, arrays and objects.

`json_string`, `json_int` and `json_bool` require the value to have the requested JSON type and produce a diagnostic otherwise.

`json_type(text, path)` returns `null`, `bool`, `number`, `string`, `array`, `object` or `missing`.

`json_has(text, path)` checks path existence.

`json_escape(text)` escapes text for insertion into a JSON string value.

The parser handles objects, arrays, numbers, booleans, null, standard string escapes and Unicode `\uXXXX` escapes including surrogate pairs.

## Native status

These JSON helpers operate in the VM/JBC runtime in 0.9. Native AOT has no stable dynamic JSON/object ABI yet and should reject programs that require VM-owned dynamic containers rather than silently dropping operations.
'''
write('docs/HTTP_JSON.md',HTTP)

GUI=r'''# Windows GUI with Jau

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
cmake -S examples\windows_gui -B gui-native -A x64
cmake --build gui-native --config Release
copy gui-native\Release\jau_window.obj examples\windows_gui\window.obj

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
'''
write('docs/WINDOWS_GUI.md',GUI)

V090=r'''# Jau 0.9

Jau 0.9 expands the toolchain around native interoperability and project-scale builds while keeping correctness ahead of aggressive optimization.

## Linker and native dependencies

- `--system-lib` Windows DLL export resolution in `jauld`.
- exact `--import symbol=dll` mappings.
- platform/target system library metadata in `.jaux` manifests.
- console vs Windows GUI PE subsystem selection.
- Linux system library forwarding through the native linker path.

## Project builder

New `jaum` tool with `jaum.txt`, multi-target build lists and `exe/shared/static/obj/asm` project types.

## Libraries

- real Windows x64/x86 DLL exports, validated with `LoadLibrary/GetProcAddress`.
- Linux x64 `.so` exports, validated with `dlopen/dlsym`.
- internal Windows `.lib` static archive generation validated with MSVC x64/x86.
- internal Linux `.a` generation validated with GCC.

## Language/runtime

- indexed array assignment in VM/JBC: `items[i] = value`.
- JSON parser/query helpers with nested object/array paths and Unicode escapes.
- native literal string pointers for C ABI calls.
- clearer failure behavior for VM-only containers in native builds.

## Distribution policy

Jau's copyright notice belongs to Jau command-line tools on stderr. Generated user assembly, object files, executables, standalone programs and libraries are not branded by the compiler.

## Supported AOT family

Windows x86/x64 and Linux x86/x64. Other CPU/OS combinations are roadmap work, not claimed support.
'''
write('docs/V090.md',V090)

MAINT=r'''# Jau Maintainer Notes

Copyright notice for Jau command tools: **DeathAmir Jau @ DeathAmir 2026 (C)**

## Correctness invariants

1. `func main()` is the application entry in VM and native executable mode. Do not require users to append `main();`.
2. `.jaux` native object members are binary linker inputs and must never be sent to the Jau parser.
3. Namespace-local `extern func` declarations retain the raw external C ABI symbol name.
4. Generated user artifacts must not receive the CLI copyright banner. Keep branding on actual Jau command-line tools/stderr.
5. Optimizer transforms stay disabled until each transform has side-effect, entry-point, loop, namespace, native-call, string/container and error-path semantic regressions.
6. Unsupported native syntax must fail with a diagnostic. Never silently drop a statement or fabricate a successful binary.
7. VM/JBC bytecode opcode numbering is compatibility-sensitive. Append new opcodes when possible instead of shifting existing values.
8. Array indexing and indexed assignment are bounds-checked in VM/JBC.
9. Windows system DLL dependencies flow from CLI/package metadata to `jauld`; do not hard-code application dependencies into codegen.
10. `jauc shared` and `jauc static` are validated by external clients, not only by checking that output files exist.
11. JauM must invoke the same compiler/linker pipeline as direct `jauc` usage; avoid a second incompatible dependency model.

## Library rules

- Windows shared: real PE export directory and `IMAGE_FILE_DLL`.
- Linux shared: public exported symbol aliases must match requested `--export` names.
- Windows static `.lib`: COFF archive linker members must remain consumable by MSVC.
- Linux static `.a`: archive symbol index must remain consumable by the system linker.
- A static `.lib` is not automatically a DLL import library.

## Native ABI

Current machine integer ABI follows target word width. C/C++ fixtures should use `intptr_t`. Literal strings may cross as borrowed `const char*`; do not expose VM-owned dynamic strings/arrays as though ownership were defined.

## Release gates

Before a release, validate:

- VM and JBC language suite.
- Windows x64/x86 native PE execution.
- Linux x64/x86 build/execute path.
- real `.jaux` C++ package flow.
- system DLL import flow (`user32` regression).
- DLL/.so external load and calls.
- `.lib/.a` external static link and calls.
- JauM executable/shared/static projects.
- installer includes `jaum`.
- generated artifacts do not contain the CLI copyright banner.
- invalid source/JSON/index operations return diagnostics, not process crashes.
'''
write('docs/MAINTAINERS.md',MAINT)

print('Jau 0.9 docs/distribution patch applied')
