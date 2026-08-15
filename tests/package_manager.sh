#!/bin/sh
set -eu
JAUC="$1"
ROOT="$2"
PORT=18765
TMP="$(mktemp -d)"
trap 'kill "${PID:-0}" 2>/dev/null || true; rm -rf "$TMP"' EXIT
mkdir -p "$TMP/dep/src" "$TMP/demo/src" "$TMP/server/package" "$TMP/home"
cat > "$TMP/dep/jau.pkg" <<'P'
name="dep"
version="1.0.0"
main="src/main.jau"
P
cat > "$TMP/dep/src/main.jau" <<'J'
namespace Dep {
    func value() { return 300; }
}
J
cat > "$TMP/demo/jau.pkg" <<'P'
name="demo"
version="1.2.3"
main="src/main.jau"
author="Jau Test"
description="registry test package"
dependencies="dep"
P
cat > "$TMP/demo/src/main.jau" <<'J'
import "pkg:dep"
namespace Demo {
    func value() { return Dep.value() + 14; }
}
J
(
  cd "$TMP/dep"
  JAU_HOME="$TMP/home" "$JAUC" run "$ROOT/tools/jaupm.jau" -I "$ROOT/stdlib" -- pack "$TMP/server/package/dep"
)
(
  cd "$TMP/demo"
  JAU_HOME="$TMP/home" "$JAUC" run "$ROOT/tools/jaupm.jau" -I "$ROOT/stdlib" -- pack "$TMP/server/package/demo"
)
python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$TMP/server" >/dev/null 2>&1 &
PID=$!
sleep 1
JAU_HOME="$TMP/home" "$JAUC" run "$ROOT/tools/jaupm.jau" -I "$ROOT/stdlib" -- config set registry "http://127.0.0.1:$PORT/package"
JAU_HOME="$TMP/home" "$JAUC" run "$ROOT/tools/jaupm.jau" -I "$ROOT/stdlib" -- install demo
test -f "$TMP/home/packages/demo/package.jaup"
test -f "$TMP/home/packages/dep/package.jaup"
cat > "$TMP/consumer.jau" <<'J'
import "pkg:demo"
print(Demo.value[]);
J
OUT="$(cd "$TMP" && JAU_HOME="$TMP/home" "$JAUC" run consumer.jau -I "$ROOT/stdlib")"
[ "$OUT" = "314" ]
test ! -f "$TMP/home/packages/demo/src/main.jau"
test ! -f "$TMP/home/packages/dep/src/main.jau"
! grep -a -q 'namespace Demo' "$TMP/home/packages/demo/package.jaup"
! grep -a -q 'namespace Dep' "$TMP/home/packages/dep/package.jaup"
