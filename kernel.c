#include <stdint.h>

void kernel_main() {
    // UEFI framebuffer (requires the bootloader to provide framebuffer address)
    volatile uint32_t *framebuffer = (uint32_t *)0xB8000; // Replace with actual framebuffer address
    const char *message = "Hello from the 64-bit kernel!";
    uint32_t color = 0xFFFFFFFF; // White color (ARGB: A=255, R=255, G=255, B=255)

    // Write message to framebuffer (assumes bootloader set graphics mode)
    for (int i = 0; message[i] != '\0'; i++) {
        framebuffer[i] = color; // Write color for each character
    }

    // Halt the CPU
    while (1) {
        __asm__("hlt");
    }
}
