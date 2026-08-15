#!/bin/sh
set -eu
JAUC="$1"
ROOT="$2"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
"$JAUC" native "$ROOT/tests/native_cpp.jau" -o "$TMP/native-link" --target linux-x86_64 --link "$ROOT/tests/native_cpp.cpp"
[ "$("$TMP/native-link")" = "42" ]
