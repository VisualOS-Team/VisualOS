#ifndef ERROR_H
#define ERROR_H

#include "kernel.h"

// Maximum depth of stack trace to display
#define MAX_STACKTRACE_DEPTH 16

// Error codes
#define ERROR_NONE              0
#define ERROR_UNKNOWN           1
#define ERROR_MEMORY_FAULT      2
#define ERROR_DIVIDE_BY_ZERO    3
#define ERROR_INVALID_OPCODE    4
#define ERROR_GENERAL_PROTECTION 5
#define ERROR_PAGE_FAULT        6
#define ERROR_DOUBLE_FAULT      7

// Debug structure to track CPU state
typedef struct {
    unsigned long rip;  // Instruction pointer
    unsigned long rsp;  // Stack pointer
    unsigned long rbp;  // Base pointer
    unsigned long rax;  // General purpose registers
    unsigned long rbx;
    unsigned long rcx;
    unsigned long rdx;
    unsigned long rsi;
    unsigned long rdi;
    unsigned long r8;
    unsigned long r9;
    unsigned long r10;
    unsigned long r11;
    unsigned long r12;
    unsigned long r13;
    unsigned long r14;
    unsigned long r15;
    unsigned long rflags;  // CPU flags
    unsigned int error_code; // Error code if applicable
} cpu_state_t;

// Stack frame for unwinding
typedef struct {
    unsigned long return_addr;
    unsigned long frame_pointer;
} stack_frame_t;

// Function prototypes
void panic(const char *message);
void panic_with_state(const char *message, cpu_state_t *state);
void capture_cpu_state(cpu_state_t *state);
void print_stacktrace(unsigned long rbp, unsigned int max_frames);
void display_error_screen(const char *message, cpu_state_t *state);

#endif // ERROR_H