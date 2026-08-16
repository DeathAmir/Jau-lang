# Jau Maintainer Notes

Copyright: **DeathAmir Jau @ DeathAmir 2026 (C)**

## Native correctness rules

1. `func main()` is a language entry function. VM and native executable codegen must invoke the user `main` exactly once when there is no explicit top-level `main()` call. Never rely on users adding `main();` manually.
2. `.jaux` object files are binary linker inputs. They must never enter the lexer/parser. Package wrapper `.jau` source is parsed; target `.obj/.o` files are only scanned for symbols and handed to the linker.
3. Native C ABI `extern func` declarations inside namespaces retain their raw external symbol names. Namespace qualification belongs to Jau wrapper functions, not C ABI symbols.
4. VM and AOT optimization are disabled in v0.8.2. Re-enable transformations only after side-effect, entry-point, loop, namespace, native-call, string-print and error-path regressions exist for each transformation. Correct code is more important than smaller/faster code.
5. Never silently drop unsupported syntax. Return a stage-specific compiler error. All public compiler entry points must catch exceptions and convert them into diagnostics rather than process crashes.
6. AOT literal string `print` is supported directly; broader dynamic string/array AOT support still requires a stable native runtime ABI. Do not pretend VM-only builtins are native-capable until they have an implementation and tests.

## Required release smoke tests

- `func main()` with no top-level `main();` prints from a native PE.
- Literal string and integer printing work in native PE32+ and PE32.
- `for`, `if`, compound assignment, boolean aliases, bitwise operators and numeric literals survive AOT.
- MathX `.jaux` resolves C ABI symbols and executes on x64/x86.
- Invalid source returns an error code and diagnostic, never an access violation or uncaught exception.
- `jauc`, `jur`, `jauas`, `jauld`, setup and bundled JauPM retain the DeathAmir copyright banner on stderr; generated AOT assembly embeds the copyright notice.
