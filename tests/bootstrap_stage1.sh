#!/bin/sh
set -eu
JAUC="$1"
JAUAS="$2"
ROOT="$3"
TARGET="${4:-linux-x86_64}"
OUT="${TMPDIR:-/tmp}/jau_stage2_$$.s"
BIN="${TMPDIR:-/tmp}/jau_stage2_$$"
"$JAUC" run "$ROOT/bootstrap/jauc_stage1.jau" -- "$ROOT/bootstrap/stage2_input.jau" "$OUT" "$TARGET"
"$JAUAS" "$OUT" -o "$BIN" --target "$TARGET"
RESULT="$($BIN)"
rm -f "$OUT" "$BIN"
[ "$RESULT" = "73
19" ]
