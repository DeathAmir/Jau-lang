#!/usr/bin/env sh
set -eu
JAUC="$1"
ROOT="$2"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

"$JAUC" native "$ROOT/tests/multi_native.jau" -o "$TMP/multi-native" --target linux-x86_64 \
  --link "$ROOT/tests/multi_native_left.cpp" \
  --link "$ROOT/tests/multi_native_right.cpp"
[ "$("$TMP/multi-native")" = "42" ]

"$JAUC" shared "$ROOT/tests/multi_native.jau" -o "$TMP/libmulti.so" --target linux-x86_64 \
  --link "$ROOT/tests/multi_native_left.cpp" \
  --link "$ROOT/tests/multi_native_right.cpp" \
  --export calculate

test -s "$TMP/libmulti.so"
cat > "$TMP/client.cpp" <<'CPP'
#include <cstdint>
#include <cstdio>
#include <dlfcn.h>
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    void* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) return 3;
    using calculate_fn = std::intptr_t (*)(std::intptr_t, std::intptr_t);
    auto calculate = reinterpret_cast<calculate_fn>(dlsym(module, "calculate"));
    if (!calculate) return 4;
    std::printf("%lld\n", (long long)calculate(20, 22));
    dlclose(module);
    return 0;
}
CPP
c++ -std=c++17 -O2 "$TMP/client.cpp" -ldl -o "$TMP/client"
[ "$("$TMP/client" "$TMP/libmulti.so")" = "42" ]
