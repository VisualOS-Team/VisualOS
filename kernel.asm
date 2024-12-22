[BITS 32]
[ORG 0x1000]

section .text
global _start

_start:
    ; Set up segment registers
    mov ax, 0x10                 ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9C00              ; Setup stack

    ; Write "Kernel Running" to VGA
    mov edi, 0xB8000
    mov esi, kernel_message
    mov ecx, kernel_message_len

write_vga:
    lodsb
    or al, al
    jz done
    mov [edi], al
    mov byte [edi + 1], 0x0A     ; Green text on black
    add edi, 2
    loop write_vga

done:
    cli
    hlt

section .data
kernel_message db "Kernel Running.", 0
kernel_message_len equ $ - kernel_message
