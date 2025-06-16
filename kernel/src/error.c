#include "../include/error.h"
#include "../include/kernel.h"

static cpu_state_t g_panic_state;
extern framebuffer_info_t g_framebuffer;

void capture_cpu_state(cpu_state_t *state) {
    __asm__ volatile (
        /* dump all GPRs into the struct */
        "movq  %%rax, %0\n\t"
        "movq  %%rbx, %1\n\t"
        "movq  %%rcx, %2\n\t"
        "movq  %%rdx, %3\n\t"
        "movq  %%rsi, %4\n\t"
        "movq  %%rdi, %5\n\t"
        "movq   %%r8, %6\n\t"
        "movq   %%r9, %7\n\t"
        "movq  %%r10, %8\n\t"
        "movq  %%r11, %9\n\t"
        "movq  %%r12, %10\n\t"
        "movq  %%r13, %11\n\t"
        "movq  %%r14, %12\n\t"
        "movq  %%r15, %13\n\t"
        /* flags */
        "pushfq\n\t"
        "popq   %14\n\t"
        /* get current RIP into RAX, then store */
        "leaq   0f(%%rip), %%rax\n\t"
        "movq   %%rax, %15\n\t"
        "0:\n\t"
        /* stack and base pointers */
        "movq  %%rsp, %16\n\t"
        "movq  %%rbp, %17\n\t"
        : /* outputs */
          "=m"(state->rax), "=m"(state->rbx), "=m"(state->rcx), "=m"(state->rdx),
          "=m"(state->rsi), "=m"(state->rdi), "=m"(state->r8),  "=m"(state->r9),
          "=m"(state->r10),"=m"(state->r11),"=m"(state->r12),"=m"(state->r13),
          "=m"(state->r14),"=m"(state->r15),"=m"(state->rflags),
          "=m"(state->rip), "=m"(state->rsp),  "=m"(state->rbp)
        : /* no inputs */
        : "rax", "memory"
    );
}

static void format_hex(char *buf, unsigned long val) {
    int i;
    buf[0] = '0'; buf[1] = 'x';
    for (i = 15; i >= 0; i--) {
        unsigned char n = (val >> (i * 4)) & 0xF;
        buf[17 - i] = n < 10 ? '0' + n : 'a' + (n - 10);
    }
    buf[18] = '\0';
}

static void format_reg(char *buf, const char *name, unsigned long val) {
    int i = 0;
    while (*name) buf[i++] = *name++;
    buf[i++] = ':'; buf[i++] = ' ';
    format_hex(buf + i, val);
}

void print_stacktrace(unsigned long rbp, unsigned int max_frames) {
    draw_string(10, 200, "Stack Trace:", COLOR_YELLOW);
    
    stack_frame_t *frame = (stack_frame_t *)rbp;
    unsigned int y = 210;
    
    for (unsigned int i = 0; i < max_frames && frame; i++) {
        if ((unsigned long)frame < 0x1000 || (unsigned long)frame & 0x7) break;
        
        char buf[32];
        format_hex(buf, frame->return_addr);
        draw_string(10, y, buf, COLOR_CYAN);
        
        stack_frame_t *next = (stack_frame_t *)frame->frame_pointer;
        if (next == frame) break;
        
        frame = next;
        y += 10;
    }
}

void display_error_screen(const char *message, cpu_state_t *state) {
    clear_screen(COLOR_BLACK);
    
    draw_string(10, 10, "KERNEL PANIC", COLOR_RED);
    draw_string(10, 30, "==========================================", COLOR_RED);
    draw_string(10, 50, message, COLOR_WHITE);
    draw_string(10, 80, "CPU State:", COLOR_YELLOW);
    
    char buf[32];
    format_reg(buf, "RIP", state->rip); draw_string(10, 100, buf, COLOR_CYAN);
    format_reg(buf, "RSP", state->rsp); draw_string(10, 110, buf, COLOR_CYAN);
    format_reg(buf, "RBP", state->rbp); draw_string(10, 120, buf, COLOR_CYAN);
    format_reg(buf, "RAX", state->rax); draw_string(10, 130, buf, COLOR_CYAN);
    format_reg(buf, "RBX", state->rbx); draw_string(210, 100, buf, COLOR_CYAN);
    format_reg(buf, "RCX", state->rcx); draw_string(210, 110, buf, COLOR_CYAN);
    format_reg(buf, "RDX", state->rdx); draw_string(210, 120, buf, COLOR_CYAN);
    format_reg(buf, "RSI", state->rsi); draw_string(210, 130, buf, COLOR_CYAN);
    format_reg(buf, "RDI", state->rdi); draw_string(410, 100, buf, COLOR_CYAN);
    format_reg(buf, "R8 ", state->r8);  draw_string(410, 110, buf, COLOR_CYAN);
    format_reg(buf, "R9 ", state->r9);  draw_string(410, 120, buf, COLOR_CYAN);
    
    print_stacktrace(state->rbp, MAX_STACKTRACE_DEPTH);
    draw_string(10, g_framebuffer.framebuffer_height - 20, "System halted", COLOR_RED);
}

void panic(const char *message) {
    __asm__ volatile ("cli");
    capture_cpu_state(&g_panic_state);
    display_error_screen(message, &g_panic_state);
    while (1) __asm__ volatile ("hlt");
}

void panic_with_state(const char *message, cpu_state_t *state) {
    __asm__ volatile ("cli");
    display_error_screen(message, state);
    while (1) __asm__ volatile ("hlt");
}