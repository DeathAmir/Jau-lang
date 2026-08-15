#!/bin/sh
set -eu
JAUC="$1"
ROOT="$2"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/lib.jau" <<'J'
extern func c_mul(a, b);

func add(a, b) {
    return a + b;
}

func mixed(a, b) {
    return c_mul(a, b) + 1;
}
J

"$JAUC" obj "$TMP/lib.jau" -o "$TMP/libjau.o" --target linux-x86_64 -I "$ROOT/stdlib"

cat > "$TMP/main.c" <<'C'
#include <stdio.h>
long long c_mul(long long a, long long b) { return a * b; }
extern long long jau_fn_add(long long, long long);
extern long long jau_fn_mixed(long long, long long);
int main(void) {
    long long a = jau_fn_add(20, 22);
    long long b = jau_fn_mixed(6, 7);
    printf("%lld %lld\n", a, b);
    return (a == 42 && b == 43) ? 0 : 1;
}
C

cc "$TMP/main.c" "$TMP/libjau.o" -o "$TMP/interop"
[ "$("$TMP/interop")" = "42 43" ]
