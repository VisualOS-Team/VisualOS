#include <stdint.h>
#include <stddef.h>

// VGA constants
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY (uint16_t *)0xB8000

// Terminal state
static uint16_t *terminal_buffer;
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;

// VGA entry creation
static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

// VGA color setup
static inline uint8_t vga_color(uint8_t fg, uint8_t bg) {
    return fg | bg << 4;
}

// Initialize terminal (clears the screen)
void terminal_initialize() {
    terminal_buffer = VGA_MEMORY;
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_color(15, 0); // White on black

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
        }
    }
}

// Write a single character to the terminal
void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_row = 0; // Wrap back to the top of the screen
        }
    } else {
        terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = vga_entry(c, terminal_color);
        if (++terminal_column == VGA_WIDTH) {
            terminal_column = 0;
            if (++terminal_row == VGA_HEIGHT) {
                terminal_row = 0;
            }
        }
    }
}

// Write a string to the terminal
void terminal_write(const char *str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        terminal_putchar(str[i]);
    }
}

// Kernel main function
void kernel_main() {
    terminal_initialize();
    terminal_write("Hello, World!\n");

    while (1) {
        __asm__("hlt");
    }
}

// Entry point for the kernel
void _start() {
    // Ensure the stack is set up correctly
    __asm__ volatile (
        "mov $0x90000, %esp\n"  // Set stack pointer to a known good value
        "xor %ebp, %ebp\n"      // Zero out the base pointer
    );

    // Call the kernel main function
    kernel_main();

    // This should not be reached
    __asm__ volatile ("hlt");
}
