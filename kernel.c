void kernel_main() {
    char *vga = (char *)0xB8000;  // VGA text buffer address
    const char *message = "Hello from the 64-bit kernel!";
    
    // Write message to VGA text buffer
    for (int i = 0; message[i] != '\0'; i++) {
        vga[i * 2] = message[i];        // Character
        vga[i * 2 + 1] = 0x0F;         // Attribute: white text on black background
    }

    // Halt the CPU
    while (1) {
        __asm__("hlt");
    }
}
