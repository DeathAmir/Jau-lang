#!/bin/sh
set -eu
JAUPM="$1"
JAUC="$2"
ROOT="$3"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/isolated" "$TMP/pkg"
cp "$JAUPM" "$TMP/isolated/jaupm"
chmod +x "$TMP/isolated/jaupm"
[ "$("$TMP/isolated/jaupm" version)" = "JauPM 0.6" ]
cd "$TMP/pkg"
"$JAUPM" init iRx
"$JAUPM" pack
PKG="$TMP/pkg/dist/iRx-0.1.0.jaup"
test -f "$PKG"
"$JAUPM" verify "$PKG" | grep -q "Package OK"
grep -a -q 'JAUPKG2' "$PKG"
! grep -a -q 'hello Jau' "$PKG"
! grep -a -q 'namespace iRx' "$PKG"
export JAU_HOME="$TMP/home"
"$JAUPM" install "$PKG"
test -f "$JAU_HOME/packages/iRx/package.jaup"
test ! -f "$JAU_HOME/packages/iRx/jau.pkg"
test ! -f "$JAU_HOME/packages/iRx/src/main.jau"
cat > "$TMP/use.jau" <<'JEOF'
import "pkg:iRx"
print(iRx.hello["Jau"]);
JEOF
"$JAUC" run "$TMP/use.jau" -I "$ROOT/stdlib" | grep -q "hello Jau from iRx"
"$JAUPM" list | grep -q "iRx@0.1.0 \[protected\]"
"$JAUPM" where iRx | grep -q "package.jaup#src/main.jau"
"$JAUPM" info iRx | grep -q "version: 0.1.0"
"$JAUPM" remove iRx
