#!/usr/bin/env sh
set -eu
JUC="$1"
ROOT="$2"
TMP="${TMPDIR:-/tmp}/jau-vm-ffi-$$"
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT
c++ -std=c++17 -O2 -fPIC -shared "$ROOT/tests/vm_ffi.cpp" -o "$TMP/libjau_vm_ffi.so"
OUT="$($JUC run "$ROOT/tests/vm_ffi.jau" -I "$ROOT/stdlib" -- "$TMP/libjau_vm_ffi.so")"
printf '%s\n' "$OUT"
printf '%s\n' "$OUT" | grep -qx '42\n42' 2>/dev/null || {
  FIRST="$(printf '%s\n' "$OUT" | sed -n '1p')"
  SECOND="$(printf '%s\n' "$OUT" | sed -n '2p')"
  [ "$FIRST" = "42" ] && [ "$SECOND" = "42" ]
}
