// tests/programs/02_two_tu_extern/main.c
extern int add(int a, int b);

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
    int res = add(10, 32); // should be 42
    if (res == 42) {
        syscall3(1, 1, (long)"ok\n", 3);
        syscall1(60, 0);
    } else {
        syscall3(1, 1, (long)"err\n", 4);
        syscall1(60, 1);
    }
}
