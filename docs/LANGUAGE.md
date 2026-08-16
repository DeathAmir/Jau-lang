# Jau Language Reference — 0.9

This document describes implemented source-language behavior. VM-only or AOT-only differences are called out instead of hidden.

## Program entry

```jau
func main() {
    print("hello");
    return 0;
}
```

A zero-argument `main` is invoked automatically when there is no explicit top-level call. Do not append `main();` to ordinary applications.

## Comments

```jau
// one line
/* block comment */
```

## Variables and constants

```jau
let count = 1;
var other = 2;
const limit = 10;
count += 1;
count++;
```

`let` and `var` create mutable bindings. `const` rejects reassignment.

## Values

The VM supports null, booleans, integers, floating-point values, strings and arrays. Current native AOT is intentionally narrower and primarily targets machine-word integer/bool values plus literal string pointers at the C ABI boundary.

```jau
let nothing = null;
let also_nothing = nil;
let ok = true;
let n = 42;
let pi = 3.14;
let text = "Jau";
let list = [1, 2, 3];
```

## Numeric literals

```jau
let decimal = 1_000_000;
let hex = 0xff;
let binary = 0b1010;
let octal = 0o755;
```

## Operators

Arithmetic: `+ - * / %`

Comparison: `== != < <= > >=`

Logical: `&& || !` and aliases `and or not`

Bitwise: `& | ^ ~ << >>`

Assignment: `= += -= *= /= %=`

Increment/decrement: `++ --`

## Functions

```jau
func add(a:int, b:int):int {
    return a + b;
}
```

`fn`, `def` and `function` are accepted aliases for `func`. Type annotations are currently syntax/ABI metadata, not a complete static type checker.

## Control flow

```jau
if (score >= 50) {
    print("pass");
} else {
    print("fail");
}

let i = 0;
while (i < 10) {
    i++;
}

for (let j = 0; j < 10; j++) {
    if (j == 3) { continue; }
    if (j == 8) { break; }
}
```

Recursion is supported by the VM and supported AOT function subset.

## Arrays

```jau
let values = [10, 20, 30];
print(values[0]);
values[1] = 42;
push(values, 99);
print(pop(values));
print(join(values, ","));
```

VM/JBC indexed assignment is bounds-checked. Compound indexed assignment is not yet syntax sugar; write:

```jau
values[i] = values[i] + 1;
```

instead of `values[i] += 1`.

Arrays and indexed mutation are VM/JBC features in 0.9. Native AOT reports an error rather than silently compiling invalid container code.

## Strings

Common VM helpers include:

```text
len str int contains starts_with substr char_at find
trim upper lower replace split join read_line
```

Literal strings can be emitted in native AOT `.rodata` and passed to C ABI functions as borrowed `const char*`. Dynamic ownership across the native ABI is not standardized yet.

## Namespaces

```jau
namespace Math {
    func add(a, b) { return a + b; }
}

func main() {
    print(Math.add(20, 22));
}
```

The bracket call form remains accepted for compatibility:

```jau
Math.add[20, 22]
```

## Imports

```jau
import "local.jau"
import "pkg:MathX"
```

Package imports read the package manifest to find Jau wrapper source. Native package object members never enter the lexer/parser.

## Native C ABI declarations

```jau
extern func native_add(a:int, b:int):int;
```

Inside a namespace, an `extern func` still keeps its raw C ABI linker symbol. The namespace applies to Jau wrapper functions, not to the external C name.

## JSON helpers

```jau
let doc = "{\"user\":{\"name\":\"Amir\"},\"items\":[3,7]}";
print(json_string(doc, "user.name"));
print(json_get(doc, "items[1]"));
```

See `HTTP_JSON.md` for the full helper list.

## Standard/builtin surface

Filesystem/system helpers include `read_file`, `write_file`, `file_exists`, `mkdir`, `remove_file`, `remove_tree`, `list_dir`, `file_size`, `path_join`, `cwd`, `temp_dir`, `getenv`, `platform`, `arch`, `random_int`.

Timing includes `clock_ms`, `clock_ns`, `time.now_ms`, `time.now_ns`, `sleep_ms`, `time.sleep_ms`.

Networking includes `http_get` and `download` with platform-specific transport behavior.

## VM vs native

The VM is the broad dynamic runtime. AOT is intentionally explicit: if a construct has no stable native ABI/codegen path, compilation should fail with a stage-specific diagnostic. This is a design rule, not something callers should work around by accepting wrong output.
