# Bootstrap

`jauc_stage1.jau` is a compiler stage implemented in Jau itself. It reads a Jau source file, parses the bootstrap literal `print(...)` / `return` subset, and emits freestanding Intel assembly for Linux x86 or x86-64.

`jauas` assembles that output directly to ELF32/ELF64 without GCC, `as`, `ld` or NASM. The CI bootstrap test executes the x86-64 Stage-2 result.

This is a staged self-host foundation rather than full compiler parity. See `docs/BOOTSTRAP.md` for the exact status and next parity milestones.
