# Jau bootstrap model

Jau uses staged bootstrapping.

1. **Stage-0** is the C++17 implementation. It provides the complete current lexer, parser, optimizer, bytecode compiler, VM, package loader and AOT backend.
2. **Stage-1** is `bootstrap/jauc_stage1.jau`, a compiler implemented in Jau. It recognizes a deliberately small bootstrap subset and currently compiles integer literal prints, string literal prints and integer returns to freestanding Intel assembly.
3. **jauas** is implemented in C++ without an external assembler dependency. It encodes the Stage-1 Intel subset and writes ELF32/ELF64 program headers and machine code directly.
4. CI runs Stage-1 on `stage2_input.jau`, passes the generated assembly to `jauas`, then executes Stage-2 and checks integer and string output.

Stage-1 is intentionally kept auditable while language infrastructure grows. It is a genuine compiler stage, but Jau is **not yet fully self-hosted**: the production lexer/parser/optimizer/package loader/VM/AOT backend remain Stage-0 C++.

The next self-host milestones are token arrays and token records in Jau, expression parsing, writable indexed data structures, JBC serialization, then a Jau implementation of the complete front end. Full self-host status should only be claimed after a Jau-written compiler compiles its own complete source and Stage-2/Stage-3 compiler outputs are equivalent.

## Stage-1 0.6 expansion

The Jau-written Stage-1 now collects `let` / `const` integer declarations, evaluates simple identifier/literal `+`, `-`, `*` expressions, compiles string/integer `print(...)` calls, and evaluates integer `return` expressions before emitting freestanding Linux x86/x86-64 assembly.

This is a meaningful bootstrap expansion, but it is not full parser/backend parity. Jau does not claim a numeric self-hosting percentage until a repeatable component-level metric and Stage-2/Stage-3 equivalence exist.
