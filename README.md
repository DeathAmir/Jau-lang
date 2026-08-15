# Jau Programming Language

Jau is an experimental systems-oriented language implementation written from scratch in C++17. The repository contains a real lexer/parser/compiler, portable JBC bytecode format, the JUR virtual machine, an optimizer, module imports, a standard-library seed, architecture-specific assembly fast paths, and an AOT assembly backend.

## Current language

```jau
import "math.jau"

func fib(n) {
    if (n <= 1) { return n; }
    return fib(n - 1) + fib(n - 2);
}

let answer = square(fib(10));
print(answer);
```

Supported today: dynamic `int`, `float`, `bool`, `string`, `null`; `let`/`const` declarations; assignment; arithmetic/comparison/boolean operators; functions and recursion; local/global variables; `if/else`; `while`; `return`; comments; local/import-path modules; builtins including `print`, `clock`, `len`, `str`, `int`, `read_file`, `write_file`, `contains`, and `starts_with`.

## Toolchain

- `jauc build main.jau -o main.jbc` — compile source to portable JBC bytecode.
- `jur main.jbc` — execute JBC in the JUR stack VM.
- `jauc run main.jau` — compile and run directly.
- `jauc asm main.jau -o main.s --target linux-x86_64` — emit real GNU-style assembly.
- `jauc native main.jau -o main --target linux-x86_64` — assemble/link using the matching system GCC/MinGW toolchain.

AOT targets: `linux-x86_64`, `linux-x86`, `windows-x86_64`, `windows-x86`. The portable VM supports the full language implemented above. The current AOT backend deliberately supports the integer/bool core, variables, assignment, arithmetic/comparisons, `if`, `while`, and `print`; unsupported constructs fail loudly instead of silently generating wrong code.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Windows:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

GitHub Actions builds and uploads artifacts for Linux x86_64, Linux x86, Windows x86_64, and Windows x86.

## Modules / libraries

Put reusable functions in `.jau` files and import them:

```jau
import "math.jau"
print(square(12));
```

Resolution checks the importing file directory, every `-I <path>`, `$JAU_HOME/stdlib`, then `./stdlib`. Circular/repeated imports are deduplicated.

## VM and optimization

JBC is a versioned binary bytecode (`JBC1`) containing constants, global symbol metadata, functions, local-slot counts and fixed-width instructions. JUR uses call frames and indexed local/global slots rather than name lookups on every instruction. `-O1/-O2` performs compile-time constant folding. On GNU-compatible x86_64 builds, hot integer add/sub/mul paths are implemented in hand-written assembly with a portable C++ fallback.

## Self-hosting status

The bootstrap directory is executable scaffolding, not a fake self-hosting badge. Stage0 is the C++ compiler. `bootstrap/jauc_stage1.jau` is a Jau-written bootstrap seed exercised by the VM. **Compiler-parity self-hosting is not yet complete**; it will only be marked complete when a Jau-written compiler can compile the full compiler source and stage2/stage3 output reproduces cleanly. See `bootstrap/README.md`.

## License

See `LICENSE`.
