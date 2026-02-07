[bits 32]

global switch_context

; void switch_context(uint32_t **old_sp, uint32_t *new_sp)
; Saves registers/flags on current stack, stores ESP into *old_sp,
; loads ESP from new_sp, restores registers/flags, then returns.
switch_context:
    pushfd
    pusha
    mov eax, [esp + 40]
    mov edx, [esp + 44]
    mov [eax], esp
    mov esp, edx
    popa
    popfd
    ret
