#pragma once

#include "model/InputSection.h"

#include <cstdint>
#include <string>
#include <vector>

namespace lnk {

struct OutputSection {
    std::string                name;
    uint64_t                   flags       = 0;
    uint64_t                   align       = 1;
    uint64_t                   vaddr       = 0;     // 0 for debug/non-alloc
    uint64_t                   file_offset = 0;
    uint64_t                   size        = 0;     // in-memory size (incl. bss)
    uint64_t                   file_size   = 0;     // bytes on disk (excl. bss tail)
    bool                       is_nobits   = false;
    bool                       alloc       = false;
    std::vector<InputSection*> contributors;
};

} // namespace lnk
