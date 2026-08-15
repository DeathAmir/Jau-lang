# Jau packages and JauPM

JauPM 0.4 is written in Jau and is built as a standalone executable by `jauc`.

## Create and pack

```text
jaupm init iRx
cd iRx
jaupm pack
```

`pack` creates `dist/iRx-0.1.0.jaup`. `.jaup` is a deterministic binary archive with the `JAUPKG1` header, package metadata, a file table, file sizes and per-file FNV-1a hashes. Symlinks are skipped and extraction rejects absolute paths and `..` traversal.

A package manifest uses the simple Jau format:

```text
name="iRx"
version="1.0.0"
main="src/main.jau"
author="DeathAmir"
description="Example Jau library"
```

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
jaupm cache-clean
jaupm home
jaupm doctor
```

Installed packages live under `$JAU_HOME/packages/<name>` and `import "pkg:<name>"` resolves the `main` entry declared by that package's `jau.pkg`.

For registry installation set `JAU_REGISTRY`; `jaupm install foo` downloads `<registry>/foo/latest.jaup`, verifies it and extracts it into the package store. The older `jaupm install NAME URL_TO_MAIN.JAU` source-package form remains supported for compatibility.
