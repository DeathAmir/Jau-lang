#!/bin/sh
set -eu
JAUC="$1"
JUR="$2"
ROOT="$3"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
"$JAUC" standalone "$ROOT/tests/args.jau" -o "$TMP/args-app" --runtime "$JUR"
OUT="$($TMP/args-app alpha beta)"
printf '%s\n' "$OUT" | grep -q '^2$'
printf '%s\n' "$OUT" | grep -q '^alpha$'
printf '%s\n' "$OUT" | grep -q '^beta$'
