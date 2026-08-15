# Jau bootstrap

Jau currently uses a conventional two-stage bootstrap:

1. **stage0**: the production compiler/VM in C++17 (`jauc`/`jur`).
2. **stage1 seed**: `jauc_stage1.jau`, executed by stage0 and able to emit target text files using Jau runtime primitives.

This repository does **not** claim compiler-parity self-hosting yet. Real self-hosting is considered complete only when a Jau-written compiler accepts the same language, recompiles itself, and the stage2/stage3 outputs are reproducible. The CI contains the bootstrap seed check so that the path toward that goal stays executable rather than being roadmap-only.
