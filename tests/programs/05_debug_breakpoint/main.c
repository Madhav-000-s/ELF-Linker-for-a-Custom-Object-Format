// tests/programs/05_debug_breakpoint/main.c
static void unused(void) {
    __asm__ volatile ("nop");
}

void _start(void) {
    int x = 1;
    x += 2; // Breakpoint here
    unused();
    
    __asm__ volatile (
        "mov $60, %%rax\n"
        "mov $0, %%rdi\n"
        "syscall"
        : : : "rax", "rdi"
    );
}
