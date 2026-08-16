# Jau Analyzer

Jau Analyzer is a cross-platform binary inspection example built on JGui. It is intentionally a foundation for a Jau-oriented reverse-engineering tool rather than a claim to replace IDA or Ghidra.

## Current capabilities

- Detect PE32 / PE32+ files and show architecture plus section layout.
- Detect little-endian ELF32 / ELF64 files and show architecture plus section layout.
- Detect `JBC1` Jau bytecode.
- Decode JBC function metadata, arity, local count and bytecode instructions using Jau's current opcode table.
- Display analysis in a Dear ImGui UI through JGui.
- Perform bounds checks before reading binary structures so malformed/truncated files fail cleanly.

## Build

Windows x64:

```cmd
cmake -S examples/jau_analyzer -B analyzer-build -A x64
cmake --build analyzer-build --config Release --parallel 2
```

Windows x86:

```cmd
cmake -S examples/jau_analyzer -B analyzer-build32 -A Win32
cmake --build analyzer-build32 --config Release --parallel 2
```

Linux x64:

```sh
cmake -S examples/jau_analyzer -B analyzer-build -DCMAKE_BUILD_TYPE=Release
cmake --build analyzer-build -j2
```

Run it with a binary or bytecode file:

```sh
./jau-analyzer program.jbc
./jau-analyzer app.exe
./jau-analyzer libplugin.so
```

## Scope

The current analyzer does not yet perform native machine-code disassembly, decompilation to high-level Jau, control-flow recovery, debugger attachment or symbolic execution. Those are natural follow-up layers once the binary parsers and JBC disassembler are stable.
