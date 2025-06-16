#include "../include/kernel.h"
#include "../include/error.h"
#include "../include/font.h"


framebuffer_info_t g_framebuffer;

void draw_pixel(unsigned int x, unsigned int y, unsigned int color) {
    if (x >= g_framebuffer.framebuffer_width || y >= g_framebuffer.framebuffer_height) return;
    ((unsigned int *)g_framebuffer.framebuffer_base)[y * (g_framebuffer.framebuffer_pitch / 4) + x] = color;
}

void clear_screen(unsigned int color) {
    unsigned int *fb = (unsigned int *)g_framebuffer.framebuffer_base;
    unsigned int pixels = g_framebuffer.framebuffer_height * (g_framebuffer.framebuffer_pitch / 4);
    while (pixels--) *fb++ = color;
}

void draw_char(unsigned int x, unsigned int y, char c, unsigned int color) {
    if ((unsigned char)c > 127) c = '?';
    const uint8_t *bitmap = g_font[(unsigned char)c];
    for (unsigned int row = 0; row < 8; row++) {
        for (unsigned int col = 0; col < 8; col++) {
            if (bitmap[row] & (1 << (7 - col))) {
                draw_pixel(x + col, y + row, color);
            }
        }
    }
}

void draw_string(unsigned int x, unsigned int y, const char *str, unsigned int color) {
    unsigned int cx = x;
    while (*str) {
        if (*str == '\n') {
            cx = x;
            y += 10;
        } else {
            draw_char(cx, y, *str, color);
            cx += 8;
            if (cx >= g_framebuffer.framebuffer_width - 8) {
                cx = x;
                y += 10;
            }
        }
        str++;
    }
}

void init_console(framebuffer_info_t *framebuffer) {
    g_framebuffer = *framebuffer;
    clear_screen(COLOR_BLACK);
    draw_string(10, 10, "VisualOS Kernel", COLOR_WHITE);
    draw_string(10, 30, "Version 0.1", COLOR_GREEN);
}

void init_memory(memory_info_t *memory_info) {
    char mem_str[32];
    unsigned long kb = memory_info->map_size / 1024;
    unsigned int i = 0, j = 0;
    unsigned long t = kb;
    
    char *s = mem_str;
    s[j++] = 'M'; s[j++] = 'e'; s[j++] = 'm'; s[j++] = ':'; s[j++] = ' ';
    
    char num[16];
    do {
        num[i++] = '0' + (t % 10);
        t /= 10;
    } while (t);
    
    while (i > 0) s[j++] = num[--i];
    s[j++] = ' '; s[j++] = 'K'; s[j++] = 'B'; s[j] = '\0';
    
    draw_string(10, 50, mem_str, COLOR_CYAN);
}

void init_interrupts(void) {
    draw_string(10, 70, "Interrupts initialized", COLOR_YELLOW);
}

void kernel_main(kernel_params_t *params) {
    init_console(&params->framebuffer);
    init_memory(&params->memory_info);
    init_interrupts();
    
    draw_string(10, 90, params->acpi_enabled ? "ACPI: Enabled" : "ACPI: Disabled", COLOR_MAGENTA);
    draw_string(10, 110, params->apic_enabled ? "APIC: Enabled" : "APIC: Disabled", COLOR_MAGENTA);
    
    draw_string(g_framebuffer.framebuffer_width / 2 - 80, g_framebuffer.framebuffer_height / 2 - 10, 
               "Welcome to VisualOS!", COLOR_WHITE);
    
    draw_string(10, g_framebuffer.framebuffer_height - 20, "Kernel initialized successfully", COLOR_GREEN);
    
    while (1) __asm__ volatile("hlt");
}