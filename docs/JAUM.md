# JauM Project Builder

`jaum` is Jau's project-oriented front end. It reads `jaum.txt` and invokes the same `jauc` pipeline used manually, so project builds do not invent a second linker/package model.

## Create a project

```cmd
jaum init MyApp
```

Typical configuration:

```ini
name=MyApp
source=src/main.jau
type=exe
target=windows-x86_64
targets=windows-x86_64,windows-x86,linux-x86_64,linux-x86
output=build/{name}-{target}{ext}
optimize=0
subsystem=console
links=
system_libs=
imports=
include_paths=stdlib
```

## Commands

```text
jaum build
jaum build --target windows-x86
jaum build-all
jaum clean
jaum show
```

## Output types

```text
type=exe
type=shared
type=static
type=obj
type=asm
```

Shared libraries can declare:

```ini
exports=add,answer
```

Native project dependencies:

```ini
links=native/math.obj,native/window.obj
system_libs=user32,gdi32
imports=SpecialCall=special.dll
```

## Multi-target builds

`build-all` uses the `targets=` list. A target is only valid if the compiler/toolchain supports it; JauM does not pretend to create macOS/ARM output from unsupported backends.

## Windows process execution

JauM launches `jauc` with `CreateProcess`, not fragile `cmd.exe` command concatenation. This keeps paths containing spaces working correctly.
