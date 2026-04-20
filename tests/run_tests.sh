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

main() {
    build_linker
    local iter=${1:-1}
    case "$iter" in
        1|all) it1_parse ;;
    esac
    echo
    echo "=== $PASS passed, $FAIL failed ==="
    [[ "$FAIL" == "0" ]]
}

main "$@"
