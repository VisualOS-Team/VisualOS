typedef unsigned long long size_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

// Video memory address
volatile uint16_t* const VIDEO_MEMORY = (uint16_t*)0xB8000;

// Screen dimensions
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25

// Colors
#define COLOR_BLACK 0
#define COLOR_WHITE 15
#define MAKE_COLOR(fg, bg) ((bg << 4) | fg)

// Position tracking
static size_t terminal_row = 0;
static size_t terminal_column = 0;
static uint8_t terminal_color = MAKE_COLOR(COLOR_WHITE, COLOR_BLACK);

// Function to initialize the terminal
void terminal_initialize() {
    // Clear screen
    for (size_t y = 0; y < SCREEN_HEIGHT; y++) {
        for (size_t x = 0; x < SCREEN_WIDTH; x++) {
            const size_t index = y * SCREEN_WIDTH + x;
            VIDEO_MEMORY[index] = (terminal_color << 8) | ' ';
        }
    }
}

// Function to put a character at specific position
void terminal_putchar(char c, size_t x, size_t y) {
    const size_t index = y * SCREEN_WIDTH + x;
    VIDEO_MEMORY[index] = (terminal_color << 8) | c;
}

// Function to write a string
void terminal_writestring(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            terminal_row++;
            terminal_column = 0;
            continue;
        }
        
        terminal_putchar(str[i], terminal_column, terminal_row);
        terminal_column++;
        
        if (terminal_column >= SCREEN_WIDTH) {
            terminal_column = 0;
            terminal_row++;
        }
        
        if (terminal_row >= SCREEN_HEIGHT) {
            terminal_row = 0;
        }
    }
}

// Kernel entry point
void _start() {
    terminal_initialize();
    terminal_writestring("Kernel loaded successfully!\n");
    terminal_writestring("Hello from C kernel!");
    
    // Halt the CPU
    while(1) {
        __asm__ volatile("hlt");
    }
}