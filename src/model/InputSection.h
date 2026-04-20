#pragma once

#include "model/Relocation.h"

#include <cstdint>
#include <string>
#include <vector>

namespace lnk {

struct InputObject;
struct OutputSection;

struct InputSection {
    InputObject*          owner           = nullptr;
    uint32_t              index_in_owner  = 0;
    std::string           name;
    uint32_t              sh_type         = 0;
    uint64_t              sh_flags        = 0;
    uint64_t              sh_addralign    = 1;
    std::vector<uint8_t>  data;                    // empty for SHT_NOBITS
    uint64_t              nobits_size     = 0;     // set for SHT_NOBITS
    std::vector<Relocation> relocs;

    OutputSection* output           = nullptr;
    uint64_t       offset_in_output = 0;
    bool           discarded        = false;

    uint64_t size() const {
        return sh_type == 8 /*SHT_NOBITS*/ ? nobits_size : data.size();
    }
};

} // namespace lnk
