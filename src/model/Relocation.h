#pragma once

#include <cstdint>

namespace lnk {

struct Relocation {
    uint64_t r_offset;   // byte offset into the owning InputSection
    uint32_t r_type;     // R_X86_64_*
    uint32_t r_sym;      // index into InputObject::symtab
    int64_t  r_addend;
};

} // namespace lnk
