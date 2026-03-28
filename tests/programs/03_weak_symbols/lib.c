// tests/programs/03_weak_symbols/lib.c
__attribute__((weak)) int version(void) {
    return 1;
}
