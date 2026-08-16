# Targets and Binary Formats

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
