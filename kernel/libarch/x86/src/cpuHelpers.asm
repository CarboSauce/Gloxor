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
      ; Returns bitmask based on CPUID result values, stores it immediately in r10
      mov eax, 0x1
      cpuid
      xor r10, r10
      and edx, 1<<25
      jz .Lend
      or r10, 1
      mov rax, cr0 
      and eax, 0xFFFFFFFB
      or eax, 0x2 
      mov cr0, rax
      mov rax, cr4
      or eax, (3 << 9) + (1<<18)
      mov cr4, rax
      ;mov eax, 1 
      ; we test for xgetbv support before trying to access avx
      ;cpuid
      and ecx, (1 << 26)
      jz .Lend
      ; set the xsave flag
      or r10, 1 << 1
      ; apparently avx doesnt imply xsave in some vms
      and ecx, (1 << 28)
      jz .Lend
      xor ecx,ecx
      xgetbv 
      or eax, 7
      xsetbv
.Lend:
      mov rax, r10
      ret 