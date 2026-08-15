#!/bin/sh
set -eu
JAUPM="$1"
JAUC="$2"
ROOT="$3"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/pkg"
cd "$TMP/pkg"
"$JAUPM" init iRx
"$JAUPM" pack
PKG="$TMP/pkg/dist/iRx-0.1.0.jaup"
test -f "$PKG"
"$JAUPM" verify "$PKG" | grep -q "Package OK"
export JAU_HOME="$TMP/home"
"$JAUPM" install "$PKG"
test -f "$JAU_HOME/packages/iRx/jau.pkg"
test -f "$JAU_HOME/packages/iRx/src/main.jau"
cat > "$TMP/use.jau" <<'JEOF'
import "pkg:iRx"
print(hello("Jau"));
JEOF
"$JAUC" run "$TMP/use.jau" -I "$ROOT/stdlib" | grep -q "hello Jau from iRx"
"$JAUPM" list | grep -q "iRx@0.1.0"
"$JAUPM" where iRx | grep -q "src/main.jau"
"$JAUPM" info iRx | grep -q "version: 0.1.0"
"$JAUPM" remove iRx
