section .text
global efi_main

efi_main:
    mov rsi, welcome_msg
    call print_message
    hlt

print_message:
    ret

section .data
welcome_msg db "UEFI Bootloader Starting...", 0
