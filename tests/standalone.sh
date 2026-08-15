#!/bin/sh
set -eu
JAUC="$1"
JUR="$2"
ROOT="$3"
OUT="${TMPDIR:-/tmp}/jau_standalone_test_$$"
"$JAUC" standalone "$ROOT/examples/fibonacci.jau" -o "$OUT" --runtime "$JUR"
RESULT="$($OUT)"
rm -f "$OUT"
[ "$RESULT" = "55" ]
