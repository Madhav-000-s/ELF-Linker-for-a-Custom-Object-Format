// tests/programs/03_weak_symbols/main.c
extern int version(void);

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
    int v = version();
    if (v == 1) {
        syscall3(1, 1, (long)"v1\n", 3);
    } else if (v == 2) {
        syscall3(1, 1, (long)"v2\n", 3);
    } else {
        syscall3(1, 1, (long)"err\n", 4);
    }
    syscall1(60, 0);
}
