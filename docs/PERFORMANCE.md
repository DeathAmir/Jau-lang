# Jau 0.7 Performance and Optimization

Jau's native backend is designed to keep simple integer code close to the operations written by the programmer. Performance claims in this project are benchmark-driven: Jau does not claim that every program is automatically faster than optimized C++.

## Native optimization levels

```text
-O0  minimal/no optimization
-O1  AST simplification and constant-oriented optimization
-O2  AST optimization plus native register/peephole improvements
-O3  current maximum optimization level
```

`jauc native` defaults to `-O3` when no optimization option is explicitly provided.

```cmd
jauc native bench.jau -o bench.exe --target windows-x86_64
```

is therefore equivalent to requesting the current maximum native optimization level.

You can force a level:

```cmd
jauc native bench.jau -o bench.exe --target windows-x86_64 -O0
jauc native bench.jau -o bench.exe --target windows-x86_64 -O3
```

## Jau 0.7 AOT improvements

The current AOT path applies the existing AST optimizer and adds a small native code-generation fast path for common integer expressions.

For expressions whose operands are simple variables or literals, Jau can load operands directly into registers instead of first materializing both through the temporary evaluation stack.

Typical source:

```jau
i = i + 1;
sum = sum + i;
while (i < limit) {
    // ...
}
```

The backend can reduce unnecessary `push`/`pop` traffic for these arithmetic and comparison operations. A final peephole pass also removes several adjacent stack operations that cancel each other.

This is intentionally a focused optimizer, not yet an LLVM-class optimizer. Future work includes a dedicated IR, SSA, register allocation, stronger data-flow analysis, loop transformations, inlining policy, alias analysis and vectorization.

## Inspect generated assembly

Never judge native performance only from source syntax. Emit optimized assembly:

```cmd
jauc asm bench.jau -o bench.s --target windows-x86_64 -O3
```

Compare with `-O0`:

```cmd
jauc asm bench.jau -o bench-o0.s --target windows-x86_64 -O0
```

Look for unnecessary stack traffic, repeated loads, avoidable branches and missed constant folding.

## Monotonic timing API

Recommended benchmark clock:

```jau
let start = time.now_ms();
// measured work
let finish = time.now_ms();
print(finish - start);
```

The clock is monotonic, so elapsed-time measurements are not affected by wall-clock changes.

VM:

```text
time.now_ms() → std::chrono::steady_clock milliseconds
time.now_ns() → std::chrono::steady_clock nanoseconds
```

Windows native AOT:

```text
x64 time.now_ms() → GetTickCount64
x86 time.now_ms() → GetTickCount
```

The current Windows AOT `time.now_ns()` implementation scales that millisecond tick value by `1,000,000`. It therefore has nanosecond units but millisecond effective resolution. Use `time.now_ms()` for native benchmark reporting in Jau 0.7.

## Billion-iteration example

```jau
func main() {
    let start = time.now_ms();
    let i = 0;
    let value = 0;

    while (i < 1000000000) {
        value = value + i;
        i = i + 1;
    }

    let finish = time.now_ms();

    print(value);
    print(finish - start);
    return 0;
}

main();
```

Build x64:

```cmd
jauc native bench.jau -o bench.exe --target windows-x86_64 -O3
bench.exe
```

For large integer loops use x64. A 32-bit target uses 32-bit machine arithmetic in the current AOT backend and can overflow values that fit naturally in the x64 benchmark.

## Comparing with C++ fairly

A useful comparison keeps all of these equal:

- same algorithm;
- same integer width;
- same number of iterations;
- same observable result so the compiler cannot remove the loop;
- release/optimized builds;
- same machine and power state;
- warm-up runs where appropriate;
- multiple repetitions and median/percentile results.

For C++ compare against an optimized build such as `/O2` or `-O3`, not a debug executable.

A benchmark where Jau wins is evidence for that workload, not proof that the language is universally faster than C++. Conversely, one slow benchmark should be inspected at the generated assembly level before deciding which compiler component needs improvement.

## Performance roadmap

The highest-value compiler work after the current peephole/register fast paths is:

1. introduce a real typed/native IR between AST and assembly;
2. convert the IR to SSA form;
3. constant propagation and dead-code elimination over the CFG;
4. global value numbering/common subexpression elimination;
5. function inlining with size/cost heuristics;
6. loop-invariant code motion and induction-variable optimization;
7. linear-scan or graph-coloring register allocation;
8. strength reduction and instruction selection improvements;
9. auto-vectorization for supported loops;
10. profile-guided optimization when the core compiler is stable.

Those changes matter much more than marketing a universal speed claim.
