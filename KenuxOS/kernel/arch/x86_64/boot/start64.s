section .text
bits 64

global _start
extern kernel_main

_start:
    mov [fb_config], rcx
    mov [mem_map], rdx

    mov rsp, stack_top

    mov rcx, [fb_config]
    mov rdx, [mem_map]
    call kernel_main

    cli
.halt:
    hlt
    jmp .halt

section .bss
align 16
stack_bottom:
    resb 32768
stack_top:

section .data
fb_config:
    dq 0
mem_map:
    dq 0
