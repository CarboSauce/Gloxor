[bits 64]
[extern kernel_main]
section .start progbits alloc exec write align=1
global _start 
_start:
    mov rsp, stack_end ;stakc grows top->down
    call kernel_main
    jmp $ 


section .data
stack_beg: 
times 0x100000 db 0
stack_end: 

