# Jau Internal Windows Linker (`jauld`)

Jau owns the final Windows link step for its supported native subset. `jauld` consumes COFF objects and writes PE32 or PE32+ executables directly.

## Pipeline

```text
app.jau
  ↓ jauc AOT
app.s
  ↓ jauas
app.obj
  ↓ jauld
app.exe
```

Normal users can ask `jauc` to run the full path:

```cmd
jauc native app.jau -o app.exe --target windows-x86_64
```

This final Jau → Windows executable path does not invoke GCC, MinGW, MSVC `link.exe`, or LLVM LLD.

## Direct object linking

Jau 0.7 fixes the entry mismatch between `jauc obj` and `jauld`.

```cmd
jauc obj add.jau -o add.obj --target windows-x86_64 -O3
jauld add.obj -o add.exe
```

`jauc obj` uses library/C-ABI mode, so a Jau function named `main` is exported as `jau_fn_main`. Earlier linker versions only searched for `main`, which produced:

```text
jauld: entry symbol not found (expected main)
```

Jau 0.7 emits linker metadata and the linker also has a compatibility fallback for `jau_fn_main`.

## JAUMETA1

For an object build, `jauc` writes a sidecar file:

```text
add.obj
add.obj.jmeta
```

Example:

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

The metadata gives the linker information that cannot reliably be inferred from raw COFF alone.

Current fields:

| Field | Meaning |
|---|---|
| `producer` | Tool that emitted the object/metadata |
| `version` | Jau toolchain version |
| `target` | `windows-x86_64` or `windows-x86` |
| `abi` | Native ABI contract, currently `jau-c-v1` |
| `kind` | Object purpose such as `jau-object` |
| `subsystem` | Intended PE subsystem, currently console metadata |
| `entry` | Logical symbol the generated startup thunk calls |
| `optimize` | Optimization level used by the compiler |

`jauld add.obj -o add.exe` automatically looks for `add.obj.jmeta`.

An explicit metadata path can also be supplied:

```cmd
jauld add.obj -o add.exe --meta custom.jmeta
```

## Entry selection

Priority:

1. `--entry symbol` supplied by the user;
2. `entry=` from `JAUMETA1`;
3. intelligent fallback.

Fallback candidates:

```text
x64: main, jau_fn_main
x86: _main, main, _jau_fn_main, jau_fn_main
```

Explicit override:

```cmd
jauld add.obj -o add.exe --entry jau_fn_main --target windows-x86_64
```

This lets raw Jau library-mode objects link even if their sidecar metadata is intentionally removed.

## Target selection

Priority:

1. `--target`;
2. metadata `target=`;
3. COFF machine field.

Supported targets:

```text
windows-x86_64 → PE32+
windows-x86    → PE32
```

## What `jauld` currently handles

The current implementation covers the native subset exercised by Jau and CI:

- COFF x86 and AMD64 inputs;
- multiple objects;
- global symbol resolution;
- Jau/C ABI symbol conventions;
- x86/x64 relative and absolute relocations used by the toolchain;
- `.text`, read-only data, writable data and import layout;
- PE DOS/NT/COFF/optional/section headers;
- import descriptor tables and IAT generation;
- startup thunk generation;
- PE32 and PE32+ output;
- imported Win32/CRT functions used by current AOT/native packages.

The internal import resolver knows the current Jau runtime surface, including console/CRT helpers and Windows time functions such as `GetTickCount`, `GetTickCount64`, `QueryPerformanceCounter`, and `QueryPerformanceFrequency`.

## Scope

`jauld` is a Jau linker, not a claim to implement every feature of MSVC LINK or LLD. The current release does not promise universal support for arbitrary third-party COFF features such as every COMDAT policy, PDB generation, arbitrary SEH/TLS/resource layouts, delay-load machinery, import libraries, signing, or every relocation emitted by every compiler configuration.

That distinction matters: Jau native executables and supported `.jaux` objects are tested against this linker; unrelated complex C++ application objects may require features outside the current Jau ABI subset.
