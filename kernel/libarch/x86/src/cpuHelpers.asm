bits 64
section .text
global gloxAsmLongJump
global gloxAsmInitBasics
global gloxAsmInitAvx

; caller saved: rbx rsp rbp r12 r13 r14 r15
gloxAsmLongJump:
      mov rax, rsp
      push 0x10
      push rax
      pushf
      push 0x8
      push .1
      iretq
      .1:
      mov eax, 0x10
      mov ds,eax 
      mov es,eax
      mov fs,eax 
      mov gs,eax 
      mov ss,eax
      ret
gloxAsmInitBasics:
      ; This function initializes basic CPU functionality like SSE
      mov rax, cr0 
      and eax, 0xFFFFFFFB
      or eax, 0x2 
      mov cr0, rax
      mov rax, cr4
      or eax, (3 << 9) + (1<<18)
      mov cr4, rax
      ret
gloxAsmInitAvx:
      xor r10, r10
      mov eax, 1 
      cpuid
      and ecx, 1 << 28
      jnz .L1
      or r10, 1
      xor ecx,ecx
      xgetbv 
      or eax, 7
      xsetbv
.L1:
      ret 