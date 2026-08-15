# Jau bootstrap model

Jau uses staged bootstrapping.

1. **Stage-0** is the C++17 implementation. It provides the complete current lexer, parser, optimizer, bytecode compiler, VM and AOT backend.
2. **Stage-1** is `bootstrap/jauc_stage1.jau`, a compiler implemented in Jau. It currently recognizes a deliberately small literal-print/return bootstrap subset and emits freestanding Intel assembly.
3. **jauas** is implemented in C++ without an external assembler dependency. It encodes the Stage-1 Intel subset and writes ELF32/ELF64 program headers and machine code directly.
4. CI runs Stage-1 on `stage2_input.jau`, passes the generated assembly to `jauas`, then executes Stage-2 and checks its output.

The next self-host milestones are: arrays/strings in the Stage-1 parser, token objects, expression parsing, bytecode serialization, then porting the full optimizer and backends. Full self-host status should only be claimed after a Jau-written compiler compiles its own complete source and Stage-2/Stage-3 outputs are equivalent.
