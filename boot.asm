[BITS 16]
[ORG 0x7C00]

start:
    cli
    mov ax, 0x07C0              ; Setup stack at 0x7C00
    mov ss, ax
    mov sp, 0x7C00

    ; Display boot message
    mov si, boot_msg
print_message:
    lodsb
    or al, al
    jz load_kernel
    mov ah, 0x0E
    int 0x10
    jmp print_message

load_kernel:
    mov ax, 0x1000              ; Load kernel at 0x1000:0x0000
    mov es, ax
    xor bx, bx
    mov ah, 0x02                ; Read sectors using BIOS
    mov al, 5                   ; Number of sectors to read
    mov ch, 0
    mov cl, 2
    mov dh, 0
    int 0x13
    jc error                    ; Jump if disk read fails

    ; Jump to kernel entry point
    jmp 0x1000:0x0000

error:
    ; Display error message
    mov si, error_msg
print_error:
    lodsb
    or al, al
    jz halt
    mov ah, 0x0E
    int 0x10
    jmp print_error

halt:
    cli
    hlt

boot_msg db "Loading Kernel...", 0
error_msg db "Disk read error! Halting.", 0

times 510-($-$$) db 0
dw 0xAA55                       ; Boot signature
