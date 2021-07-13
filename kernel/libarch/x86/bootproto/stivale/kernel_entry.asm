[bits 64]
[extern stivale2_main]
[extern framebuffer_request]
[extern kernel_main]
[extern eggFrame]
section .start progbits alloc exec write align=1
global _start 
_start:
    ; mov rsp, stack_end ;stack grows top->down
    ; mov rbp, rsp
    call stivale2_main

    ; mov rdi, eggFrame
    ; call kernel_main


; section .data
; stivale2_framebuffer_tag:
;     dq 0x3ecc1bc43d0f7971
;     dq 0
;     dw 0
;     dw 0
;     dw 32

; section .stivale2hdr
; align 8
; mainHeader:
;     dq _start
;     dq stack_end
;     dq 0
;     dq stivale2_framebuffer_tag
    