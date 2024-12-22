[BITS 16]
ORG 0x7C00

start:
    cli                     ; Disable interrupts
    xor ax, ax
    mov ds, ax              ; Clear segment registers
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; Initialize stack

    ; Print debug message
    mov ah, 0x0E
    mov al, 'B'
    int 0x10

    ; Load GDT
    lgdt [gdt_descriptor]

    ; Enable protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to protected mode
    jmp 0x08:protected_mode

[BITS 32]
protected_mode:
    ; Set data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9C00

    ; Print debug message
    mov ah, 0x0E
    mov al, 'P'
    int 0x10

    ; Halt the CPU
    hlt

gdt:
    dq 0x0000000000000000   ; Null descriptor
    dq 0x00CF9A000000FFFF   ; Code segment descriptor
    dq 0x00CF92000000FFFF   ; Data segment descriptor

gdt_descriptor:
    dw gdt_end - gdt - 1
    dd gdt

gdt_end:
