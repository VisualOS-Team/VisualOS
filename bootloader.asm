section .text
global efi_main

efi_main:
    ; Entry point for UEFI application
    mov rsi, welcome_msg
    call print_message
    hlt

print_message:
    ; Example function
    ret

section .data
welcome_msg db "UEFI Bootloader Starting...", 0
