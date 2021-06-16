[bits 64]
[extern kernel_main]
section .start progbits alloc exec write align=1
global _start 
_start:
    mov rsp, stack_end ;stack grows top->down
    mov rbp, rsp
    call kernel_main
    jmp $ 


section .bss
stack_beg: 
resb 0x100000 
stack_end: 

