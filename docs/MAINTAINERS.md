# Jau Maintainer Notes

Copyright notice for Jau command tools: **DeathAmir Jau @ DeathAmir 2026 (C)**

## Correctness invariants

1. `func main()` is the application entry in VM and native executable mode. Do not require users to append `main();`.
2. `.jaux` native object members are binary linker inputs and must never be sent to the Jau parser.
3. Namespace-local `extern func` declarations retain the raw external C ABI symbol name.
4. Generated user artifacts must not receive the CLI copyright banner. Keep branding on actual Jau command-line tools/stderr.
5. Optimizer transforms stay disabled until each transform has side-effect, entry-point, loop, namespace, native-call, string/container and error-path semantic regressions.
6. Unsupported native syntax must fail with a diagnostic. Never silently drop a statement or fabricate a successful binary.
7. VM/JBC bytecode opcode numbering is compatibility-sensitive. Append new opcodes when possible instead of shifting existing values.
8. Array indexing and indexed assignment are bounds-checked in VM/JBC.
9. Windows system DLL dependencies flow from CLI/package metadata to `jauld`; do not hard-code application dependencies into codegen.
10. `jauc shared` and `jauc static` are validated by external clients, not only by checking that output files exist.
11. JauM must invoke the same compiler/linker pipeline as direct `jauc` usage; avoid a second incompatible dependency model.

## Library rules

- Windows shared: real PE export directory and `IMAGE_FILE_DLL`.
- Linux shared: public exported symbol aliases must match requested `--export` names.
- Windows static `.lib`: COFF archive linker members must remain consumable by MSVC.
- Linux static `.a`: archive symbol index must remain consumable by the system linker.
- A static `.lib` is not automatically a DLL import library.

## Native ABI

Current machine integer ABI follows target word width. C/C++ fixtures should use `intptr_t`. Literal strings may cross as borrowed `const char*`; do not expose VM-owned dynamic strings/arrays as though ownership were defined.

## Release gates

Before a release, validate:

- VM and JBC language suite.
- Windows x64/x86 native PE execution.
- Linux x64/x86 build/execute path.
- real `.jaux` C++ package flow.
- system DLL import flow (`user32` regression).
- DLL/.so external load and calls.
- `.lib/.a` external static link and calls.
- JauM executable/shared/static projects.
- installer includes `jaum`.
- generated artifacts do not contain the CLI copyright banner.
- invalid source/JSON/index operations return diagnostics, not process crashes.
