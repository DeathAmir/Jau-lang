# HTTP and JSON

Jau's VM is the current high-level networking/data runtime.

## HTTP

```jau
func main() {
    let body = http_get("http://example.com/api");
    print(body);
}
```

`download(url, path)` writes a response to disk.

Transport behavior is platform-dependent. Plain HTTP has a POSIX socket path. HTTPS currently falls back to platform tools (for example curl on Linux or PowerShell on Windows) where Jau does not yet provide a native TLS implementation. Do not treat this as a promise of an embedded TLS stack.

## JSON validation

```jau
print(json_valid("{\"ok\":true}"));
```

## Query paths

```jau
let body = "{\"user\":{\"name\":\"Jau\"},\"items\":[3,7]}";
print(json_string(body, "user.name"));
print(json_get(body, "items[1]"));
```

Paths support object keys separated by `.` and array indexes in brackets.

## Functions

`json_valid(text)` returns a boolean and does not throw for malformed JSON.

`json_get(text, path)` returns a decoded string scalar or serialized JSON for numbers, booleans, null, arrays and objects.

`json_string`, `json_int` and `json_bool` require the value to have the requested JSON type and produce a diagnostic otherwise.

`json_type(text, path)` returns `null`, `bool`, `number`, `string`, `array`, `object` or `missing`.

`json_has(text, path)` checks path existence.

`json_escape(text)` escapes text for insertion into a JSON string value.

The parser handles objects, arrays, numbers, booleans, null, standard string escapes and Unicode `\uXXXX` escapes including surrogate pairs.

## Native status

These JSON helpers operate in the VM/JBC runtime in 0.9. Native AOT has no stable dynamic JSON/object ABI yet and should reject programs that require VM-owned dynamic containers rather than silently dropping operations.
