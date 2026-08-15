.intel_syntax noprefix
.text
.globl cppmath_mul
cppmath_mul:
  mov rax, rcx
  imul rax, rdx
  ret
