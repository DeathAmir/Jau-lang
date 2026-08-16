# Jau ↔ C/C++ interoperability

Jau 0.6 provides relocatable object generation and a platform C ABI boundary for the AOT backend.

## Jau object → C/C++

Jau source:

```jau
func add(a, b) {
    return a + b;
}
```

Linux:

```bash
jauc obj math.jau -o math.o --target linux-x86_64
```

Windows:

```cmd
jauc obj math.jau -o math.obj --target windows-x86_64
```

The exported symbol is:

```text
jau_fn_add
```

C declaration:

```c
extern long long jau_fn_add(long long, long long);
```

C++ declaration:

```cpp
extern "C" long long jau_fn_add(long long, long long);
```

The resulting `.o` / `.obj` can be added to an ordinary CMake, GCC, Clang or MSVC target.

## Jau → C/C++ foreign calls

Jau:

```jau
extern func cpp_mul(a, b);

func answer() {
    return cpp_mul(6, 7);
}
```

C++:

```cpp
extern "C" long long cpp_mul(long long a, long long b) {
    return a * b;
}
```

One-command Linux build:

```bash
jauc native app.jau -o app --target linux-x86_64 --link helper.cpp
```

C is also accepted:

```bash
jauc native app.jau -o app --target linux-x86_64 --link helper.c
```

Prebuilt object files can be passed through the same `--link` option.

## ABI

Current AOT interop uses the platform C ABI:

- Windows x86-64: Microsoft x64 integer register ABI;
- Linux x86-64: System V AMD64 integer register ABI;
- x86 targets: cdecl stack arguments.

The stable exported function naming convention is `jau_fn_<qualified-name>` with namespace punctuation normalized to underscores.

Current FFI values are integer/bool-style machine values. String, array, struct, callback and ownership-safe FFI are future work.

## Protected packages in object builds

Package modules do not have to exist as plaintext source after installation. For:

```jau
import "pkg:NumPkg"
```

`jauc asm` and `jauc obj` read the package module from `$JAU_HOME/packages/NumPkg/package.jaup`, decode it in memory, parse the recovered source text, and compile it into the same AOT/object compilation unit.

That means code from protected packages can contribute exported Jau symbols to a `.o` / `.obj` without permanently extracting source files to disk.

## Windows assembler/object path

`jauas` supports:

```cmd
jauas input.s -o output.obj --target windows-x86_64 --object
jauas input.s -o output.obj --target windows-x86 --object
```

Windows targets produce COFF relocatable objects. Final PE executable linking is performed by a normal platform linker/compiler driver so CRT and external library imports remain standards-compatible.


## Native package source/binary separation (0.7.1)

For `type="native"` packages, the manifest `main` member is parsed as Jau source while `native_windows_*` members are opaque object bytes. `.obj`/`.o` members never enter the lexer/parser; `jauc native` extracts them and passes them directly to `jauld`.

Optional annotations such as `extern func f(a:int):int;` are accepted as syntax metadata. Full static type checking is not implied yet.
