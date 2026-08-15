#!/bin/sh
set -eu
JAUPM="$1"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/bin" "$TMP/work" "$TMP/home"
cp "$JAUPM" "$TMP/bin/jaupm"
chmod +x "$TMP/bin/jaupm"
cd "$TMP/work"
OUT="$(PATH="$TMP/bin:$PATH" JAU_HOME="$TMP/home" jaupm version)"
[ "$OUT" = "JauPM 0.6" ]
