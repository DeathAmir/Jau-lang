# Jau packages and JauPM

JauPM 0.6 is implemented in Jau (`tools/jaupm.jau`) and is compiled by `jauc standalone` into a real `jaupm.exe`/`jaupm` executable during the normal toolchain build.

## Protected package format

`jaupm pack` creates a `JAUPKG2` binary archive. Manifest text, archive paths and file payloads are stored through Jau's reversible protected binary stream format, while integrity is checked against the original plaintext file hashes. The format is intended to prevent casual source disclosure through Notepad/`strings`; it is obfuscation/protection rather than secret-key cryptography or code signing.

Normal installation keeps only the protected archive at:

```text
$JAU_HOME/packages/<name>/package.jaup
```

Jau source files are not extracted into the package store. The compiler resolves `import "pkg:<name>"` by reading the requested module directly from the archive into memory. Explicit `jaupm unpack` is the developer escape hatch that writes source files to a chosen directory.

Legacy `JAUPKG1` archives remain readable.

## Registry

The default package registry is:

```text
https://irautox.ir/package
```

`jaupm install JTTP` requests:

```text
https://irautox.ir/package/JTTP
```

The endpoint may return a `.jaup` file even when the URL has no `.jaup` extension.

Configuration precedence:

1. `JAU_REGISTRY` environment variable.
2. `$JAU_HOME/jaupm.conf`.
3. JauPM default registry.

Commands:

```text
jaupm registry
jaupm config get registry
jaupm config set registry https://example.org/packages
```

## Create, pack and install

```text
jaupm init iRx
cd iRx
jaupm pack
jaupm verify dist/iRx-0.1.0.jaup
jaupm install dist/iRx-0.1.0.jaup
```

A manifest uses the simple Jau key/value format:

```text
name="iRx"
version="1.0.0"
main="src/main.jau"
author="DeathAmir"
description="Example Jau library"
dependencies="JTTP,OtherPackage"
```

Comma-separated dependencies are installed from the configured registry when missing.

## Namespace packages

Packages can expose qualified functions:

```jau
namespace JTTP {
    func fetch(url) {
        return http_get(url);
    }
}
```

Consumers can use normal call syntax or Jau's namespace bracket-call shorthand:

```jau
import "pkg:JTTP"

let a = JTTP.fetch("http://127.0.0.1:8080/data");
let b = JTTP.fetch["http://127.0.0.1:8080/data"];
```

A dotted value followed by `[...]` is treated as a function call; ordinary `array[index]` remains array indexing.

## Build integration

Package imports are expanded before bytecode or AOT compilation. Therefore installed protected packages participate in all compiler modes without being extracted to disk:

```text
jauc run app.jau
jauc build app.jau -o app.jbc
jauc standalone app.jau -o app.exe
jauc asm app.jau -o app.s --target windows-x86_64
jauc obj app.jau -o app.obj --target windows-x86_64
jauc native app.jau -o app --target linux-x86_64
```

For AOT, functions imported from packages are emitted into the same assembly unit and linked together with the application. A missing package produces a direct `package not installed` compiler error.

## Commands

```text
jaupm help
jaupm version
jaupm init NAME
jaupm pack [OUTPUT.jaup]
jaupm install NAME|FILE.jaup|URL [URL]
jaupm install-manifest URL
jaupm verify FILE.jaup
jaupm info NAME|FILE.jaup
jaupm unpack FILE.jaup DIR
jaupm update NAME
jaupm remove NAME
jaupm list
jaupm where NAME
jaupm registry
jaupm config get registry
jaupm config set registry URL
jaupm cache-clean
jaupm home
jaupm doctor
```
