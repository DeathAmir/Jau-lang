#!/bin/sh
set -eu
JAUPM="$1"
JAUC="$2"
JUR="$3"
ROOT="$4"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/pkg/src" "$TMP/home"
cat > "$TMP/pkg/jau.pkg" <<'P'
name="NumPkg"
version="1.0.0"
main="src/main.jau"
P
cat > "$TMP/pkg/src/main.jau" <<'J'
namespace NumPkg {
    func add(a, b) { return a + b; }
}
J
(
 cd "$TMP/pkg"
 "$JAUPM" pack
 JAU_HOME="$TMP/home" "$JAUPM" install dist/NumPkg-1.0.0.jaup
)
cat > "$TMP/app.jau" <<'J'
import "pkg:NumPkg"
print(NumPkg.add[40, 2]);
J
JAU_HOME="$TMP/home" "$JAUC" build "$TMP/app.jau" -o "$TMP/app.jbc" -I "$ROOT/stdlib"
[ "$(JAU_HOME="$TMP/home" "$JUR" "$TMP/app.jbc")" = "42" ]
JAU_HOME="$TMP/home" "$JAUC" standalone "$TMP/app.jau" -o "$TMP/app" --runtime "$JUR" -I "$ROOT/stdlib"
[ "$(JAU_HOME="$TMP/home" "$TMP/app")" = "42" ]
JAU_HOME="$TMP/home" "$JAUC" asm "$TMP/app.jau" -o "$TMP/app.s" --target linux-x86_64 -I "$ROOT/stdlib"
grep -q 'jau_fn_NumPkg_add' "$TMP/app.s"
cc -no-pie "$TMP/app.s" -o "$TMP/app-native"
[ "$("$TMP/app-native")" = "42" ]

JAU_HOME="$TMP/home" "$JAUC" obj "$TMP/app.jau" -o "$TMP/pkg.o" --target linux-x86_64 -I "$ROOT/stdlib"
cat > "$TMP/pkg_call.c" <<'C'
extern long long jau_fn_NumPkg_add(long long, long long);
int main(void) { return jau_fn_NumPkg_add(40, 2) == 42 ? 0 : 1; }
C
cc "$TMP/pkg_call.c" "$TMP/pkg.o" -o "$TMP/pkg-call"
"$TMP/pkg-call"
