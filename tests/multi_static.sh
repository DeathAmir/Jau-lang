#!/usr/bin/env sh
set -eu
JAUC="$1"
ROOT="$2"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

c++ -std=c++17 -O2 -c "$ROOT/tests/multi_native_left.cpp" -o "$TMP/left.o"
c++ -std=c++17 -O2 -c "$ROOT/tests/multi_native_right.cpp" -o "$TMP/right.o"

"$JAUC" static "$ROOT/tests/multi_library.jau" -o "$TMP/libmulti.a" --target linux-x86_64 \
  --link "$TMP/left.o" \
  --link "$TMP/right.o"

test -s "$TMP/libmulti.a"
COUNT="$(ar t "$TMP/libmulti.a" | wc -l | tr -d ' ')"
[ "$COUNT" -ge 3 ]

cat > "$TMP/client.cpp" <<'CPP'
#include <cstdint>
extern "C" std::intptr_t jau_fn_calculate(std::intptr_t, std::intptr_t);
int main() {
    return jau_fn_calculate(20, 22) == 42 ? 0 : 1;
}
CPP

c++ -std=c++17 -O2 "$TMP/client.cpp" "$TMP/libmulti.a" -o "$TMP/client"
"$TMP/client"
