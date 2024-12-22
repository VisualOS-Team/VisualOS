[BITS 64]
[ORG 0x1000]

section .text
global _start

_start:
    ; Load framebuffer address (passed by bootloader in rdi)
    mov rbx, rdi             ; Framebuffer base address
    mov rcx, [frame_width]   ; Framebuffer width
    mov rdx, [frame_height]  ; Framebuffer height

    ; Write "Kernel Running" to framebuffer
    mov rsi, kernel_message
    call write_to_framebuffer

halt:
    cli
    hlt

; Writes a string to the framebuffer
write_to_framebuffer:
    mov rdi, rbx             ; Base address of framebuffer
    xor rax, rax             ; Clear index
write_loop:
    lodsb                    ; Load character from string
    or al, al                ; Check for null terminator
    jz write_done
    ; Calculate pixel position
    mov r10, rax             ; Save ASCII character in r10
    ; Display each character as a colored pixel block
    mov qword [rdi + rax * 4], 0x00FFFFFF ; White pixel
    add rdi, 4               ; Advance to the next pixel
    jmp write_loop
write_done:
    ret

section .data
kernel_message db "Kernel Running", 0
frame_width dq 1024          ; Example width (provided by bootloader)
frame_height dq 768          ; Example height (provided by bootloader)
