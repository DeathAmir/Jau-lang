# Jau Programming Language

Jau is an experimental programming language and toolchain implemented from scratch in C++17, with a growing self-hosted layer written in Jau itself. Version **0.4.0** includes a bytecode compiler, JUR VM, x86/x86-64 AOT assembly backend, a dependency-free Linux ELF assembler (`jauas`), a Jau-written package manager (`jaupm`), a cross-platform setup program, a standard library, and a tested bootstrap compiler stage written in Jau.

## Language

```jau
import "arrays.jau"

func fib(n) {
    if (n <= 1) { return n; }
    return fib(n - 1) + fib(n - 2);
}

const name = "Jau";
let values = [fib(8), 21, 34];
push(values, 55);

let i = 0;
while (i < len(values)) {
    if (i == 1) { i = i + 1; continue; }
    print(values[i]);
    i = i + 1;
}
```

Implemented today: integers, floats, booleans, strings, null, shared arrays, indexing, mutable `let`, enforced `const`, functions, recursion, globals/locals, imports, `if/else`, `while`, `break`, `continue`, `return`, arithmetic/comparison/boolean operators and builtins.

## Toolchain

- `jauc build app.jau -o app.jbc` — compile to JBC bytecode.
- `jur app.jbc` — run bytecode in JUR.
- `jauc run app.jau -- arg1 arg2` — compile/run source with program arguments.
- `jauc asm app.jau -o app.s --target linux-x86_64` — emit Intel/GNU assembly.
- `jauc native app.jau -o app --target ...` — AOT assemble/link through the platform toolchain.
- `jauc standalone app.jau -o app` — produce a single executable bundle without GCC/NASM/MinGW at application build time.
- `jauas input.s -o app --target linux-x86_64` — Jau's own dependency-free assembler/linker for the freestanding bootstrap subset; emits ELF32/ELF64 directly.
- `jaupm ...` — package manager whose command logic is written in Jau.
- `jau-setup` — installs the toolchain, stdlib and JauPM and configures `JAU_HOME`/`PATH`.

AOT assembly targets: `linux-x86_64`, `linux-x86`, `windows-x86_64`, `windows-x86`. The internal dependency-free `jauas` path currently targets Linux x86/x86-64. Windows gets a zero-toolchain **standalone bundle** path while the Windows AOT assembly path still uses a system linker.

## Optimizer

The front end performs constant folding, unary folding, algebraic simplification (`x+0`, `x*1`, zero multiplication), fixed-branch elimination, false-loop removal and unreachable-statement removal. `const` assignment is rejected during compilation. AOT now supports integer/bool functions, recursion, calls, return values, loops, break/continue and comparisons instead of only top-level expressions.

## Packages

JauPM is implemented in `tools/jaupm.jau`, then compiled by Jau itself into a standalone `jaupm.exe`/`jaupm` executable during the normal toolchain build.

```bash
jaupm init iRx
cd iRx
jaupm pack
jaupm verify dist/iRx-0.1.0.jaup
jaupm install dist/iRx-0.1.0.jaup
jaupm list
```

`.jaup` is Jau's binary package archive (`JAUPKG1`) with deterministic file ordering, metadata, file sizes and per-file hashes. Extraction rejects absolute paths and `..` traversal. Installed packages live under `$JAU_HOME/packages/<name>` and are imported with `import "pkg:name"`.

`JAU_REGISTRY` can point to a registry base URL; `jaupm install NAME` resolves `$JAU_REGISTRY/NAME/latest.jaup`. Local archives, direct package URLs, legacy source-package URLs, info/verify/unpack/update/remove/cache/doctor commands are supported.

## Internet and standard library

Runtime networking includes a direct socket HTTP client on POSIX, redirects/chunked transfer decoding, downloads, and HTTPS platform fallback. TLS is intentionally not reimplemented with home-grown cryptography. The installed stdlib currently contains modules for arrays, config, environment, filesystem, HTTP, IO, math, networking, package paths, filesystem paths, randomness, strings, system information and testing.

## Bootstrap / self-hosting

`bootstrap/jauc_stage1.jau` is now an actual compiler stage written in Jau. It reads Jau source, parses an auditable bootstrap subset, emits freestanding Intel assembly, then `jauas` turns that assembly directly into an ELF executable without GCC, `as`, `ld` or NASM. CI executes the resulting stage-2 program.

This is **real staged self-hosting work, but not yet full compiler parity**: the complete lexer/parser/optimizer/backend are still Stage-0 C++. Full self-hosting means porting those components to Jau and proving Stage-2/Stage-3 compiler equivalence. See `docs/BOOTSTRAP.md`.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

GitHub Actions builds Linux x86-64, Linux x86, Windows x86-64 and Windows x86 artifacts.


## JauPM 0.4

JauPM is written in Jau and built by `jauc standalone` into a real `jaupm.exe`/`jaupm` binary. It can create deterministic `.jaup` archives, verify hashes, install from local files/URLs/registries, inspect/unpack/update/remove packages, and resolve package entry points through `import "pkg:name"`. See `docs/PACKAGES.md`.
