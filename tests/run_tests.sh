#!/usr/bin/env bash
# Run linker tests inside the elf-linker-dev container.
# Usage (from project root):
#   MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd -W):/work" -w /work \
#       elf-linker-dev:1 bash tests/run_tests.sh [iteration]

set -euo pipefail

ROOT=/work
LINKER=$ROOT/build/elf-linker
PASS=0
FAIL=0

die()  { echo "FAIL: $*" >&2; FAIL=$((FAIL+1)); }
ok()   { echo "PASS: $*"; PASS=$((PASS+1)); }

build_linker() {
    echo "--- building linker ---"
    cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build "$ROOT/build" -j >/dev/null
    test -x "$LINKER" || { echo "linker not built"; exit 1; }
}

# ----------------------------------------------------------------------------
# Iteration 1: parser correctness vs readelf.
# ----------------------------------------------------------------------------
it1_parse() {
    echo "=== Iteration 1: parse + dump ==="
    local d=$ROOT/tests/programs/01_hello_freestanding
    ( cd "$d" && gcc -c -ffreestanding -fno-pic -fno-stack-protector -nostdlib -g -O0 hello.c -o hello.o )

    local our=$("$LINKER" --dump "$d/hello.o")
    local re=$(readelf -SshW "$d/hello.o")

    # Section count (from our "Sections (N):" header vs readelf header count)
    local our_sec=$(echo "$our" | awk '/^Sections \(/{gsub(/[^0-9]/,""); print; exit}')
    local re_sec=$(readelf -h "$d/hello.o" | awk '/section headers:/ && /Number/{print $NF; exit}')
    [[ "$our_sec" == "$re_sec" ]] && ok "section count ($our_sec)" || die "section count: ours=$our_sec re=$re_sec"

    # Symbol count
    local our_sym=$(echo "$our" | awk '/^Symbols \(/{gsub(/[^0-9]/,""); print; exit}')
    local re_sym=$(readelf -s "$d/hello.o" | awk '/contains/{gsub(/[^0-9]/,""); print; exit}')
    [[ "$our_sym" == "$re_sym" ]] && ok "symbol count ($our_sym)" || die "symbol count: ours=$our_sym re=$re_sym"

    # Relocation count (sum across all .rela.*)
    local our_rel=$(echo "$our" | grep -cE '^\s+off=0x')
    local re_rel=$(readelf -r "$d/hello.o" | grep -cE '^[0-9a-f]{12} ')
    [[ "$our_rel" == "$re_rel" ]] && ok "relocation count ($our_rel)" || die "reloc count: ours=$our_rel re=$re_rel"

    # Presence of key sections
    for s in .text .rodata .symtab .strtab .debug_info .debug_line; do
        echo "$our" | grep -q " $s " && ok "has $s" || die "missing $s"
    done

    # _start is GLOBAL FUNC in .text (shndx=1)
    echo "$our" | grep -qE '_start\s+bind=GLOBAL\s+type=\s*FUNC' \
        && ok "_start is GLOBAL FUNC" || die "_start not global/func"

    # Validate relocations include R_X86_64_32 (for msg), and PC32 in eh_frame
    echo "$our" | grep -q 'R_X86_64_32 ' && ok "has R_X86_64_32" || die "missing R_X86_64_32"
    echo "$our" | grep -q 'R_X86_64_PC32' && ok "has R_X86_64_PC32" || die "missing R_X86_64_PC32"

    # Validate that the linker rejects non-ELF input
    if "$LINKER" --dump /etc/hostname 2>/dev/null; then
        die "linker accepted non-ELF input"
    else
        ok "rejects non-ELF input"
    fi

    # Multi-file dump
    cat > /tmp/b.c <<'EOF'
int g_answer = 42;
int getter(void) { return g_answer; }
EOF
    gcc -c -ffreestanding -fno-pic -nostdlib /tmp/b.c -o /tmp/b.o
    "$LINKER" --dump "$d/hello.o" /tmp/b.o >/tmp/multi.dump
    grep -q 'hello.o ===' /tmp/multi.dump && grep -q 'b.o ===' /tmp/multi.dump \
        && ok "multi-input dump" || die "multi-input dump"
}

# ----------------------------------------------------------------------------
# Iteration 1 (M2): hello freestanding
# ----------------------------------------------------------------------------
it2_hello() {
    echo "=== Iteration 1 (M2): hello freestanding ==="
    local d=$ROOT/tests/programs/01_hello_freestanding
    ( cd "$d" && gcc -c -ffreestanding -fno-pic -fno-stack-protector -nostdlib -g -O0 hello.c -o hello.o )
    
    "$LINKER" -o "$d/hello" "$d/hello.o"
    chmod +x "$d/hello"
    
    local out=$("$d/hello")
    [[ "$out" == "hi" || "$out" == "hi\n" ]] && ok "hello output" || die "hello output: '$out'"
}

# ----------------------------------------------------------------------------
# Iteration 1 (M3): two TU extern
# ----------------------------------------------------------------------------
it3_two_tu() {
    echo "=== Iteration 1 (M3): two TU extern ==="
    local d=$ROOT/tests/programs/02_two_tu_extern
    ( cd "$d" && \
      gcc -c -ffreestanding -fno-pic -fno-stack-protector -nostdlib -g -O0 main.c -o main.o && \
      gcc -c -ffreestanding -fno-pic -fno-stack-protector -nostdlib -g -O0 math.c -o math.o )
    
    "$LINKER" -o "$d/two_tu" "$d/main.o" "$d/math.o"
    chmod +x "$d/two_tu"
    
    local out=$("$d/two_tu")
    [[ "$out" == "ok" || "$out" == "ok\n" ]] && ok "two_tu output" || die "two_tu output: '$out'"
}

# ----------------------------------------------------------------------------
# Iteration 2 (M4): rodata, data, bss
# ----------------------------------------------------------------------------
it4_data() {
    echo "=== Iteration 2 (M4): rodata, data, bss ==="
    local d=$ROOT/tests/programs/04_rodata_data_bss
    ( cd "$d" && gcc -c -ffreestanding -fno-pic -fno-stack-protector -nostdlib -g -O0 main.c -o main.o )
    
    "$LINKER" -o "$d/data_test" "$d/main.o"
    chmod +x "$d/data_test"
    
    local out=$("$d/data_test")
    [[ "$out" == "data test" || "$out" == "data test\n" ]] && ok "data_test output" || die "data_test output: '$out'"
    
    # Check segments with readelf
    local phdrs=$(readelf -l "$d/data_test")
    echo "$phdrs" | grep -q "LOAD" || die "no LOAD segments"
    # We expect at least 3 LOAD segments (R-X, R--, RW-) or combined if logic allows.
    # Our EmitExecutable creates up to 4 LOAD segments (header, text, rodata, data+bss).
    local load_count=$(echo "$phdrs" | grep -c "LOAD")
    [[ $load_count -ge 3 ]] && ok "segment count ($load_count)" || die "segment count: $load_count"
}

# ----------------------------------------------------------------------------
# Iteration 2 (M5): debug preservation
# ----------------------------------------------------------------------------
it5_debug() {
    echo "=== Iteration 2 (M5): debug preservation ==="
    local d=$ROOT/tests/programs/05_debug_breakpoint
    ( cd "$d" && gcc -c -ffreestanding -fno-pic -fno-stack-protector -nostdlib -g -O0 main.c -o main.o )
    
    "$LINKER" -o "$d/debug_test" "$d/main.o"
    chmod +x "$d/debug_test"
    
    # Use gdb to verify we can set a breakpoint on line 7 (x += 2)
    # We check if gdb successfully hits the breakpoint.
    if gdb --version >/dev/null 2>&1; then
        local gdb_out=$(gdb --batch -ex 'b main.c:7' -ex run "$d/debug_test" 2>&1)
        if echo "$gdb_out" | grep -q "Breakpoint 1, _start"; then
            ok "gdb breakpoint hit"
        else
            die "gdb failed to hit breakpoint: $gdb_out"
        fi
    else
        echo "gdb not found, skipping functional debug test"
        # Fallback: check if debug sections exist and have non-zero size
        readelf -S "$d/debug_test" | grep -q ".debug_info" && ok "has debug info" || die "missing debug info"
    fi
}

main() {
    build_linker
    local iter=${1:-1}
    case "$iter" in
        1|all) it1_parse ;&
        2) it2_hello ;;
        3) it3_two_tu ;;
        4) it4_data ;;
        5) it5_debug ;;
    esac
    case "$iter" in
        all) it2_hello ; it3_two_tu ; it4_data ; it5_debug ;;
    esac
    echo
    echo "=== $PASS passed, $FAIL failed ==="
    [[ "$FAIL" == "0" ]]
}

main "$@"
