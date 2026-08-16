#!/usr/bin/env sh
set -eu
JAUPM="$1"
JAUC="$2"
ROOT="$3"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/pkg/src" "$TMP/pkg/native" "$TMP/home"

cat > "$TMP/pkg/native/jmath.cpp" <<'CPP'
#include <cstdint>
extern "C" std::intptr_t jmath_add(std::intptr_t a, std::intptr_t b) { return a + b; }
extern "C" std::intptr_t jmath_mul(std::intptr_t a, std::intptr_t b) { return a * b; }
CPP
c++ -std=c++17 -O3 -fPIC -shared "$TMP/pkg/native/jmath.cpp" -o "$TMP/pkg/native/libjmath.so"

cat > "$TMP/pkg/jau.pkg" <<'PKG'
name="JMathVM"
version="1.0.0"
main="src/main.jau"
runtime_linux_x86_64="native/libjmath.so"
PKG

cat > "$TMP/pkg/src/main.jau" <<'JAU'
import "ffi.jau"
import "pkg.jau"

let jmath_module = ffi.open(package_runtime("JMathVM"));
let jmath_add_pointer = ffi.symbol(jmath_module, "jmath_add");
let jmath_mul_pointer = ffi.symbol(jmath_module, "jmath_mul");

namespace JMath {
    func add(a, b) { return ffi.call2(jmath_add_pointer, a, b); }
    func multiply(a, b) { return ffi.call2(jmath_mul_pointer, a, b); }
}
JAU

(
  cd "$TMP/pkg"
  JAU_HOME="$TMP/home" "$JAUPM" pack "$TMP/JMathVM.jaup"
)
JAU_HOME="$TMP/home" "$JAUPM" install "$TMP/JMathVM.jaup"
test -s "$TMP/home/packages/JMathVM/runtime.so"
test -f "$TMP/home/packages/JMathVM/package.jaup"
test ! -f "$TMP/home/packages/JMathVM/src/main.jau"

cat > "$TMP/consumer.jau" <<'JAU'
import "pkg:JMathVM"
print(JMath.add[20, 22]);
print(JMath.multiply[6, 7]);
JAU

OUT="$(cd "$TMP" && JAU_HOME="$TMP/home" "$JAUC" run consumer.jau -I "$ROOT/stdlib")"
FIRST="$(printf '%s\n' "$OUT" | sed -n '1p')"
SECOND="$(printf '%s\n' "$OUT" | sed -n '2p')"
[ "$FIRST" = "42" ]
[ "$SECOND" = "42" ]
