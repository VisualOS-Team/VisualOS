#ifndef KERNEL_H
#define KERNEL_H

// Forward declaration for error handling
struct cpu_state_t;

// Memory map information structure
typedef struct {
    void *memory_map;
    unsigned long map_size;
    unsigned long map_key;
    unsigned long descriptor_size;
    unsigned int descriptor_version;
} memory_info_t;

// Framebuffer information structure
typedef struct {
    unsigned long long framebuffer_base;
    unsigned int framebuffer_width;
    unsigned int framebuffer_height;
    unsigned int framebuffer_pitch;
    unsigned int framebuffer_bpp;
} framebuffer_info_t;

// Kernel parameters structure passed from bootloader
typedef struct {
    memory_info_t memory_info;
    framebuffer_info_t framebuffer;
    unsigned char acpi_enabled;
    unsigned char apic_enabled;
} kernel_params_t;

// Function prototypes
void kernel_main(kernel_params_t *params);
void init_console(framebuffer_info_t *framebuffer);
void clear_screen(unsigned int color);
void draw_pixel(unsigned int x, unsigned int y, unsigned int color);
void draw_string(unsigned int x, unsigned int y, const char *str, unsigned int color);
void init_memory(memory_info_t *memory_info);
void init_interrupts(void);

// Color definitions
#define COLOR_BLACK   0x00000000
#define COLOR_BLUE    0x000000FF
#define COLOR_GREEN   0x0000FF00
#define COLOR_CYAN    0x0000FFFF
#define COLOR_RED     0x00FF0000
#define COLOR_MAGENTA 0x00FF00FF
#define COLOR_YELLOW  0x00FFFF00
#define COLOR_WHITE   0x00FFFFFF

#endif // KERNEL_H