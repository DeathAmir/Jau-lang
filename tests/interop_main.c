#include <stdio.h>

long long c_mul(long long a, long long b) {
    return a * b;
}

extern long long jau_fn_add(long long a, long long b);
extern long long jau_fn_mixed(long long a, long long b);

int main(void) {
    long long a = jau_fn_add(20, 22);
    long long b = jau_fn_mixed(6, 7);
    printf("%lld %lld\n", a, b);
    return (a == 42 && b == 43) ? 0 : 1;
}
