section .text

extern int_handle

; Common handler body generator
; RDI = saved register pointer
; RSI = interrupt number
; RDX = error code

%macro INTERRUPT_COMMON 1
    cld

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Arguments
    mov rdi, rsp
    mov rsi, %1
    mov rdx, [rsp + 120]

    ; Preserve stack pointer and align
    mov rbx, rsp
    and rsp, -16

    call int_handle

    mov rsp, rbx

    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove error code
    add rsp, 8

    iretq
%endmacro


%macro INTERRUPT_ERR 1
global _intHandler%1
_intHandler%1:
    ; CPU already pushed error code
    INTERRUPT_COMMON %1
%endmacro


%macro INTERRUPT_NOERR 1
global _intHandler%1
_intHandler%1:
    push qword 0
    INTERRUPT_COMMON %1
%endmacro


INTERRUPT_NOERR 0
INTERRUPT_NOERR 1
INTERRUPT_NOERR 2
INTERRUPT_NOERR 3
INTERRUPT_NOERR 4
INTERRUPT_NOERR 5
INTERRUPT_NOERR 6
INTERRUPT_NOERR 7
INTERRUPT_ERR   8
INTERRUPT_NOERR 9
INTERRUPT_ERR   10
INTERRUPT_ERR   11
INTERRUPT_ERR   12
INTERRUPT_ERR   13
INTERRUPT_ERR   14
INTERRUPT_NOERR 15
INTERRUPT_NOERR 16
INTERRUPT_ERR   17
INTERRUPT_NOERR 18
INTERRUPT_NOERR 19
INTERRUPT_ERR 20
INTERRUPT_ERR   21
INTERRUPT_NOERR 22
INTERRUPT_NOERR 23
INTERRUPT_NOERR 24
INTERRUPT_NOERR 25
INTERRUPT_NOERR 26
INTERRUPT_NOERR 27
INTERRUPT_NOERR 28
INTERRUPT_ERR   29
INTERRUPT_ERR   30
INTERRUPT_NOERR 31


%assign i 32
%rep 224
    INTERRUPT_NOERR i
    %assign i i+1
%endrep


section .rodata
align 8

global intVector

intVector:
%assign i 0
%rep 256
    dq _intHandler %+ i
    %assign i i+1
%endrep

