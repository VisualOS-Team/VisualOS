section .text
[BITS 64]

global _start

; UEFI application entry point
_start:
    mov rsi, welcome_msg
    call print_message

    ; Load kernel into memory using UEFI services
    call load_kernel

    ; Set up kernel entry point and jump
    mov rax, [kernel_buffer] ; Address of kernel in memory
    mov rbx, kernel_entry    ; Entry point address offset
    add rax, rbx             ; Calculate kernel entry point
    jmp rax                  ; Jump to kernel

; Print a message using UEFI Write function
print_message:
    mov rdx, rsi              ; Pointer to message
    mov rcx, [console_out]    ; Console output handle
    mov rax, [write_output]   ; Address of Write function
    call rax                  ; Call Write function
    ret

; Load the kernel from the disk using UEFI services
load_kernel:
    ; Open the kernel file
    mov rdi, kernel_path      ; File path to kernel
    mov rax, [open_file]      ; Address of OpenFile function
    call rax                  ; Open kernel file

    ; Read the kernel into memory
    mov rsi, [kernel_buffer]  ; Destination buffer
    mov rdi, [file_handle]    ; File handle
    mov rax, [read_file]      ; Address of ReadFile function
    call rax                  ; Read file contents
    ret

; UEFI Application Exit
exit:
    mov rdi, [efi_exit]       ; Address of Exit function
    call rdi                  ; Exit application

section .data

welcome_msg db "Starting UEFI Bootloader...", 0
kernel_path db "\EFI\BOOT\kernel.bin", 0
kernel_buffer dq 0x100000 ; Address where kernel will be loaded
kernel_entry dq 0x0       ; Offset for kernel entry point

section .bss

console_out resq 1 ; Console output handle
file_handle resq 1 ; File handle for kernel
volume_handle resq 1 ; UEFI volume handle

section .uefi_services

; UEFI Boot Services Function Table
open_file dq 0          ; Address of OpenFile service
read_file dq 0          ; Address of ReadFile service
write_output dq 0       ; Address of Write service
efi_exit dq 0           ; Address of Exit service
