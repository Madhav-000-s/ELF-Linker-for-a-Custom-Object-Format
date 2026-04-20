// Freestanding: write(1, "hi\n", 3); exit(0).
// No libc; inline syscalls only.

static long syscall3(long n, long a, long b, long c) {
    long r;
    __asm__ volatile (
        "syscall"
        : "=a"(r)
        : "0"(n), "D"(a), "S"(b), "d"(c)
        : "rcx", "r11", "memory"
    );
    return r;
}

static long syscall1(long n, long a) {
    long r;
    __asm__ volatile (
        "syscall"
        : "=a"(r)
        : "0"(n), "D"(a)
        : "rcx", "r11", "memory"
    );
    return r;
}

static const char msg[] = "hi\n";

void _start(void) {
    syscall3(1 /*sys_write*/, 1 /*stdout*/, (long)msg, 3);
    syscall1(60 /*sys_exit*/, 0);
}
