# Jau 0.7 Language Reference

This document describes the language surface implemented by the current Jau toolchain. It distinguishes VM features from the smaller native AOT subset so examples do not promise behavior the compiler does not yet provide.

## Program structure

A Jau source file uses the `.jau` extension. Statements are terminated with `;` in normal style. Blocks use braces.

```jau
let x = 10;
let y = 20;
print(x + y);
```

## Values

The VM supports:

- signed integers;
- floating-point values;
- booleans;
- strings;
- `null`;
- arrays.

The current native AOT backend focuses on integer and boolean machine values. Strings and arrays are VM-first and are not yet a stable native ABI.

## Variables

Mutable declaration:

```jau
let score = 10;
score = score + 1;
```

Constant declaration:

```jau
const max_score = 100;
```

Reassigning a `const` is a compile error.

## Functions

```jau
func add(a, b) {
    return a + b;
}

let answer = add(20, 22);
```

Recursion is supported by the current compiler/runtime subset.

## Native external functions

An AOT/object build can call a C ABI symbol declared with `extern func`:

```jau
extern func native_mul(a, b);

func calculate() {
    return native_mul(6, 7);
}
```

The current stable native FFI boundary is machine-sized integer/boolean values.

## Namespaces

```jau
namespace MathBox {
    func add(a, b) {
        return a + b;
    }
}

print(MathBox.add(20, 22));
```

Jau also supports bracket-call syntax:

```jau
print(MathBox.add[20, 22]);
```

Qualified names such as `MathBox.add` and `time.now_ms` are parsed as named calls.

## Conditionals

```jau
if (score >= 10) {
    print(1);
} else {
    print(0);
}
```

## Loops

```jau
let i = 0;

while (i < 10) {
    print(i);
    i = i + 1;
}
```

`break` and `continue` are supported inside loops.

## Arrays

```jau
let values = [10, 20, 30];
print(values[0]);
push(values, 40);
print(pop(values));
```

Current limitation: indexed assignment such as `values[1] = 99;` is not part of the current assignment grammar. Rebuild the array or use library helpers instead of relying on indexed l-values.

Useful array/string helpers include `push`, `pop`, `join`, `split`, `len`, `contains`, `starts_with`, `substr`, `char_at`, `find`, `trim`, `upper`, `lower`, and `replace`.

## Imports

Local source import:

```jau
import "math.jau"
```

Installed package import:

```jau
import "pkg:MathBox"
```

Protected `JAUPKG2` package source is read into the compilation unit in memory. The installed package does not need plaintext source beside the archive.

## Time API

Jau 0.7 adds a monotonic time namespace suitable for elapsed-time measurement:

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

`time.now_ms()` is the recommended benchmark clock. In the VM it is based on `std::chrono::steady_clock`. Windows AOT maps it to the platform monotonic tick API (`GetTickCount64` on x64 and `GetTickCount` on x86).

`time.now_ns()` returns an integer nanosecond-scale value. In the VM it has nanosecond clock resolution where the platform provides it. The current Windows AOT implementation scales the monotonic millisecond tick count by `1,000,000`, so its unit is nanoseconds but its effective resolution is still milliseconds. Use `time.now_ms()` when comparing native benchmark durations.

## Runtime/system builtins

The current VM/compiler surface includes these commonly used builtins:

```text
print
clock
clock_ms / clock_ns
time.now_ms / time.now_ns / time.sleep_ms
len / str / int
read_file / write_file
contains / starts_with
argc / arg
getenv
file_exists / mkdir / remove_file / remove_tree
path_join / cwd / temp_dir
http_get / download
substr / char_at / find / trim / upper / lower / replace
manifest_value / hash64
push / pop / join / split
read_line / sleep_ms
platform / arch / random_int
list_dir / file_size
jaup_pack / jaup_extract / jaup_manifest / jaup_verify
```

Not every VM builtin is available in native AOT yet. Native compilation reports an explicit `AOT unknown function`/unsupported-expression error instead of silently changing behavior.

## Build modes

Run source in the VM:

```bash
jauc run app.jau
```

Build JBC bytecode:

```bash
jauc build app.jau -o app.jbc
jur app.jbc
```

Emit assembly:

```bash
jauc asm app.jau -o app.s --target windows-x86_64 -O3
```

Emit an object:

```bash
jauc obj app.jau -o app.obj --target windows-x86_64 -O3
```

Build a native Windows executable with the Jau-owned assembler/linker path:

```bash
jauc native app.jau -o app.exe --target windows-x86_64 -O3
```

Supported AOT targets:

```text
linux-x86_64
linux-x86
windows-x86_64
windows-x86
```

## Optimization levels

```text
-O0  minimal/no optimizer passes
-O1  AST simplification
-O2  AST optimization + native register/peephole improvements
-O3  current maximum Jau optimization level
```

`jauc native` defaults to `-O3` when no explicit optimization level is supplied. Other commands keep their explicit/default optimization behavior.

## Current native limitations

The current AOT backend intentionally remains smaller than the VM language:

- integer/bool-focused native values;
- no stable native string/array ownership ABI yet;
- limited C ABI argument counts according to target register conventions;
- native time intrinsic is currently implemented for Windows targets;
- `jauas` implements the assembly subset generated by Jau, not every MASM/NASM/GAS instruction;
- `jauld` implements the COFF/PE subset required by Jau and supported native extension objects, not every feature of industrial Windows linkers.

These boundaries are documented so future versions can expand them without pretending unsupported syntax already works.
