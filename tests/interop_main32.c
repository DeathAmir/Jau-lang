#include <stdio.h>

int c_mul(int a, int b) {
    return a * b;
}

extern int jau_fn_add(int a, int b);
extern int jau_fn_mixed(int a, int b);

int main(void) {
    int a = jau_fn_add(20, 22);
    int b = jau_fn_mixed(6, 7);
    printf("%d %d\n", a, b);
    return (a == 42 && b == 43) ? 0 : 1;
}
