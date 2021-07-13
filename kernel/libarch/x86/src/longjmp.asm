bits 64
section .text
global _gloxAsmLongJump
_gloxAsmLongJump:
      mov rax, rsp
      push 0x10
      push rax
      pushf
      push 0x8
      push .1
      iretq
      .1:

      mov eax, 0x10
      mov ds,eax; 
      mov es,eax; 
      mov fs,eax; 
      mov gs,eax 
      mov ss,eax
      ret       