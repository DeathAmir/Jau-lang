# Jau ↔ C/C++ interoperability

Jau 0.6 adds relocatable object generation and an integer/bool C ABI boundary for the AOT backend.

## Jau to C

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

The exported symbol is `jau_fn_add`.

```c
extern long long jau_fn_add(long long, long long);
```

For C++:

```cpp
extern "C" long long jau_fn_add(long long, long long);
```

## C to Jau

Declare an AOT-only foreign function:

```jau
extern func c_mul(a, b);

func mixed(a, b) {
    return c_mul(a, b) + 1;
}
```

Provide the implementation from C:

```c
long long c_mul(long long a, long long b) {
    return a * b;
}
```

Build the Jau object and the C object, then link them with the normal platform linker/compiler driver.

## ABI

Current object interoperability uses the platform C ABI:

- Windows x86-64: Microsoft x64 integer register ABI;
- Linux x86-64: System V AMD64 integer register ABI;
- x86 targets: cdecl stack arguments.

Current AOT functions accept integer/bool-style machine values. Rich string/array/struct FFI types are not implemented yet.

## Package imports

Protected packages are expanded in memory before AOT code generation, so an installed `package.jaup` can participate directly in `jauc asm` and `jauc obj` without writing plaintext package source to disk.
