// tests/programs/04_rodata_data_bss/main.c
static const char* msg = "data test\n";
static int initialized_var = 42;
static int uninitialized_var; // .bss

static long syscall3(long n, long a, long b, long c) {
    long r;
    __asm__ volatile ("syscall" : "=a"(r) : "0"(n), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return r;
}

static long syscall1(long n, long a) {
    long r;
    __asm__ volatile ("syscall" : "=a"(r) : "0"(n), "D"(a) : "rcx", "r11", "memory");
    return r;
}

void _start(void) {
    if (initialized_var == 42 && uninitialized_var == 0) {
        syscall3(1, 1, (long)msg, 10);
        syscall1(60, 0);
    } else {
        syscall1(60, 1);
    }
}
