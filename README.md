# ELF Linker for a Custom Object Format

A static linker for x86_64 ELF64 object files, written in C++17. This project demonstrates the core mechanics of a linker: ELF parsing, symbol resolution, section merging, address assignment, and relocation application.

## Features

- **ELF64 Parsing**: Reads `.o` files produced by `gcc`.
- **Symbol Resolution**: Supports strong and weak symbols across multiple translation units.
- **Section Merging**: Groups input sections into standard `.text`, `.rodata`, `.data`, and `.bss` output segments.
- **Relocation Support**: Implements `R_X86_64_64`, `R_X86_64_PC32`, `R_X86_64_PLT32`, `R_X86_64_32`, and `R_X86_64_32S`.
- **DWARF Preservation**: Copies `.debug_*` sections and applies their relocations so the output is debuggable in `gdb`.
- **Freestanding Output**: Emits non-PIE `ET_EXEC` binaries suitable for freestanding environments (e.g., OS kernels or simple assembly programs).

## Usage

### Prerequisites

- A Linux environment (or WSL) with `gcc`, `cmake`, `make`, and `readelf`.
- `gdb` (optional, for debugging tests).

### Building the Linker

```bash
cmake -B build
cmake --build build
```

### Linking an Object File

```bash
./build/elf-linker -o my_prog input1.o input2.o
```

### Running Tests

The test suite builds several freestanding C programs, links them with `elf-linker`, and verifies their execution and ELF structure.

```bash
./tests/run_tests.sh all
```

## DWARF and Debugging

This linker preserves DWARF debug information. This means you can use `gdb` to debug the resulting executables:

```bash
gdb ./my_prog
(gdb) break main.c:10
(gdb) run
```

For a detailed explanation of how we map PC values back to source lines using the `.debug_line` state machine, see [docs/dwarf-notes.md](docs/dwarf-notes.md).

## Project Structure

- `src/elf/`: Low-level ELF structure definitions and raw reader.
- `src/model/`: In-memory representation of the object files and the final image.
- `src/passes/`: The pipeline of linker stages (Merge, Resolve, Assign, Relocate, Emit).
- `tests/`: A suite of freestanding programs to verify linker correctness.
