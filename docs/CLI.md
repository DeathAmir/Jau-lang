# Jau CLI Reference

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
