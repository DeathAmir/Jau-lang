#!/usr/bin/env sh
set -eu
JAUM="$1"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cat > "$TMP/main.jau" <<'JAU'
func main() {
    print(42);
    return 0;
}
JAU
cat > "$TMP/Jaum.yaml" <<'YAML'
project:
  name: yaml-demo
  source: main.jau
  type: exe
  output: build/{name}-{target}{ext}
  optimize: 0
targets:
  - linux-x86_64
links: []
system_libs: []
imports: []
exports: []
YAML
(
  cd "$TMP"
  "$JAUM" show -f Jaum.yaml | grep -q '^name=yaml-demo$'
  "$JAUM" build-all -f Jaum.yaml
  test -x build/yaml-demo-linux-x86_64
  test "$(./build/yaml-demo-linux-x86_64)" = "42"
)
