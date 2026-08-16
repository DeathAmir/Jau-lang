# JGui

JGui is the standard experimental GUI package for Jau. It exposes a stable C ABI over Dear ImGui and uses platform-native rendering backends:

- Windows: Win32 + DirectX 11
- Linux: GLFW + OpenGL 3

The package runtime is loaded by Jau VM through `stdlib/ffi.jau`, so application code uses one `JGui` namespace on both platforms.

## Build the native runtime

Windows x64:

```cmd
cmake -S packages/JGui -B packages/JGui/build -A x64
cmake --build packages/JGui/build --config Release --parallel 2
```

Windows x86:

```cmd
cmake -S packages/JGui -B packages/JGui/build32 -A Win32
cmake --build packages/JGui/build32 --config Release --parallel 2
```

Linux x64:

```sh
cmake -S packages/JGui -B packages/JGui/build -DCMAKE_BUILD_TYPE=Release
cmake --build packages/JGui/build -j2
```

The runtime is written under `native/bin/<platform>-<arch>/` so `jaupm pack` can include it using the `runtime_*` keys in `jau.pkg`.

## Package and install

```sh
cd packages/JGui
jaupm pack dist/JGui-0.1.0.jaup
jaupm install dist/JGui-0.1.0.jaup
```

## Jau API

```jau
import "pkg:JGui"

func main() {
    if (!JGui.available()) { return 1; }
    if (!JGui.init("Jau UI", 960, 640)) { return 2; }

    let enabled = true;
    let value = 25;
    while (JGui.begin_frame()) {
        JGui.begin_window("Demo");
        JGui.text("Hello from Jau + Dear ImGui");
        if (JGui.button("Click")) { print("clicked"); }
        enabled = JGui.checkbox("Enabled", enabled);
        value = JGui.slider_int("Value", value, 0, 100);
        JGui.end_window();
        JGui.render();
    }

    JGui.shutdown();
    return 0;
}
```

Current bindings intentionally cover a focused stable surface. More Dear ImGui widgets can be added without changing Jau's package/runtime ABI.
