# DWARF Notes: How Line Tables Work

This document explains how our linker preserves DWARF debug information, specifically the `.debug_line` section, to allow tools like `gdb` to map runtime PC values back to source code.

## The Problem
When the compiler (`gcc -c`) generates an object file, it doesn't know the final virtual address (VA) where the code will live. However, the DWARF `.debug_line` section needs to contain these VAs to map them to line numbers.

## The Solution: Relocations
The compiler emits `.debug_line` with placeholder addresses (usually 0) and creates relocations (typically `R_X86_64_64`) against the code symbols in `.text`.

### Linker Responsibilities
1.  **Preservation**: The linker must recognize `.debug_*` sections and copy them into the output file. Unlike code and data sections, these are not "allocatable" (`SHF_ALLOC` is not set), meaning they are not loaded into memory at runtime.
2.  **Concatenation**: Since multiple input files might have debug info, the linker concatenates these sections.
3.  **Relocation Application**: This is the most critical step. After the linker has assigned final VAs to all symbols (in the `AssignAddresses` pass), it must walk through the relocations for the `.debug_line` section and patch in the real addresses.

## `.debug_line` State Machine
The `.debug_line` section doesn't just contain a simple table. It's actually a bytecode program for a tiny state machine. The state machine tracks:
-   `address`: The current runtime PC.
-   `file`: The current source file index.
-   `line`: The current line number.
-   `column`: The current column number.
-   `is_stmt`: Whether this address is a recommended breakpoint location.

The bytecode contains instructions like:
-   `DW_LNS_advance_pc`: Increment the `address`.
-   `DW_LNS_advance_line`: Increment the `line`.
-   `DW_LNS_copy`: Append a row to the logical table using the current state.
-   `DW_LNE_set_address`: Explicitly set the `address` (this is what the `R_X86_64_64` relocation targets).

## Verification with `readelf`
You can inspect the resulting line table using:
```bash
readelf --debug-dump=line <executable>
```
If the linker worked correctly, the "Address" column will contain values in the `0x400000` range (matching the `.text` segment), and `gdb` will be able to translate those addresses back to your `.c` files.
