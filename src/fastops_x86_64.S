.text
.globl jau_fast_add_i64
.type jau_fast_add_i64,@function
jau_fast_add_i64:
    movq %rdi, %rax
    addq %rsi, %rax
    ret
.globl jau_fast_sub_i64
.type jau_fast_sub_i64,@function
jau_fast_sub_i64:
    movq %rdi, %rax
    subq %rsi, %rax
    ret
.globl jau_fast_mul_i64
.type jau_fast_mul_i64,@function
jau_fast_mul_i64:
    movq %rdi, %rax
    imulq %rsi, %rax
    ret
