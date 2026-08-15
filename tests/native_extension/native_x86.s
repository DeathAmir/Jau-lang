.intel_syntax noprefix
.text
.globl cppmath_mul
cppmath_mul:
  push ebp
  mov ebp, esp
  mov eax, DWORD PTR [ebp+8]
  mov ecx, DWORD PTR [ebp+12]
  imul eax, ecx
  leave
  ret
