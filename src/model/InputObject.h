#pragma once

#include "elf/ElfTypes.h"
#include "model/InputSection.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace lnk {

struct InputObject {
    std::string                       path;
    std::vector<uint8_t>              raw;
    Elf64_Ehdr                        ehdr{};
    std::vector<Elf64_Shdr>           shdrs;
    std::string                       shstrtab;
    std::string                       strtab;
    std::vector<Elf64_Sym>            symtab;
    // Sparse: one slot per shdr, pointer may be null for ignored sections.
    std::vector<std::unique_ptr<InputSection>> sections;
    // Parallel to symtab; filled by AssignAddresses with the final VA of each symbol.
    std::vector<uint64_t>             sym_value;

    const char* shdr_name(const Elf64_Shdr& s) const {
        if (s.sh_name >= shstrtab.size()) return "";
        return shstrtab.data() + s.sh_name;
    }
    const char* sym_name(const Elf64_Sym& sym) const {
        if (sym.st_name >= strtab.size()) return "";
        return strtab.data() + sym.st_name;
    }
};

} // namespace lnk
