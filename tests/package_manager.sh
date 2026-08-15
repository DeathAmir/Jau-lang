#!/bin/sh
set -eu
JAUC="$1"
ROOT="$2"
PORT=18765
HOME_TMP="${TMPDIR:-/tmp}/jaupm_test_$$"
CONSUMER="${TMPDIR:-/tmp}/jaupm_consumer_$$.jau"
rm -rf "$HOME_TMP"
python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$ROOT/tests/http_pkg" >/dev/null 2>&1 &
PID=$!
trap 'kill "$PID" 2>/dev/null || true; rm -rf "$HOME_TMP" "$CONSUMER"' EXIT
sleep 1
JAU_HOME="$HOME_TMP" "$JAUC" run "$ROOT/tools/jaupm.jau" -I "$ROOT/stdlib" -- install demo "http://127.0.0.1:$PORT/main.jau"
cat > "$CONSUMER" <<'J'
import "pkg:demo"
print(remote_value());
J
OUT="$(JAU_HOME="$HOME_TMP" "$JAUC" run "$CONSUMER")"
[ "$OUT" = "314" ]
