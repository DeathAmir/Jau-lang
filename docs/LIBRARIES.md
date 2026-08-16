# Shared and Static Libraries

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
