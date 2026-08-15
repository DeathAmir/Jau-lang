#!/bin/sh
set -eu
JAUPM="$1"
JAUC="$2"
ROOT="$3"
PORT=18766
TMP="$(mktemp -d)"
trap 'kill "${PID:-0}" 2>/dev/null || true; rm -rf "$TMP"' EXIT
cp -R "$ROOT/packages/JTTP" "$TMP/JTTP"
mkdir -p "$TMP/home" "$TMP/web"
printf 'JTTP-NET-OK' > "$TMP/web/data.txt"
(
 cd "$TMP/JTTP"
 "$JAUPM" pack
 JAU_HOME="$TMP/home" "$JAUPM" install dist/JTTP-1.0.0.jaup
)
python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$TMP/web" >/dev/null 2>&1 &
PID=$!
sleep 1
cat > "$TMP/app.jau" <<J
import "pkg:JTTP"
print(JTTP.fetch["http://127.0.0.1:$PORT/data.txt"]);
J
OUT="$(cd "$TMP" && JAU_HOME="$TMP/home" "$JAUC" run app.jau -I "$ROOT/stdlib")"
[ "$OUT" = "JTTP-NET-OK" ]
