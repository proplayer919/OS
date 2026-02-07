[bits 32]
[section .text.entry]

global _start
extern kernel_main

_start:
    call kernel_main

.hang:
    hlt
    jmp .hang
