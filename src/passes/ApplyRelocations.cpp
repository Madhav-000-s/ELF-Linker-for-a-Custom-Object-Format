#include "passes/Passes.h"
#include "elf/ElfTypes.h"
#include "util/Error.h"

#include <climits>
#include <cstring>
#include <sstream>

namespace lnk {

namespace {

std::string site_desc(const InputObject& obj, const InputSection& sec, uint64_t off) {
    std::ostringstream os;
    os << obj.path << ":" << sec.name << "+0x" << std::hex << off;
    return os.str();
}

} // namespace

void apply_relocations(OutputImage& img) {
    for (auto& obj : img.objects) {
        for (auto& sp : obj->sections) {
            if (!sp || sp->discarded || !sp->output) continue;
            auto& s = *sp;
            if (s.sh_type == SHT_NOBITS) continue;

            for (const auto& r : s.relocs) {
                verify(r.r_sym < obj->sym_value.size(), "reloc: sym idx OOR");
                verify(r.r_offset + 4 <= s.data.size() ||
                       (r.r_type == R_X86_64_64 && r.r_offset + 8 <= s.data.size()) ||
                       r.r_type == R_X86_64_NONE,
                       "reloc: offset OOR in " + site_desc(*obj, s, r.r_offset));

                const uint64_t S = obj->sym_value[r.r_sym];
                const int64_t  A = r.r_addend;
                const uint64_t P = s.output->vaddr + s.offset_in_output + r.r_offset;
                uint8_t* site    = s.data.data() + r.r_offset;

                switch (r.r_type) {
                    case R_X86_64_NONE:
                        break;

                    case R_X86_64_64: {
                        verify(r.r_offset + 8 <= s.data.size(),
                               "reloc 64 OOR at " + site_desc(*obj, s, r.r_offset));
                        uint64_t v = S + static_cast<uint64_t>(A);
                        std::memcpy(site, &v, 8);
                        break;
                    }

                    case R_X86_64_PC32:
                    case R_X86_64_PLT32: {
                        int64_t v = static_cast<int64_t>(S) + A - static_cast<int64_t>(P);
                        verify(v >= INT32_MIN && v <= INT32_MAX,
                               "PC32/PLT32 overflow at " + site_desc(*obj, s, r.r_offset));
                        int32_t v32 = static_cast<int32_t>(v);
                        std::memcpy(site, &v32, 4);
                        break;
                    }

                    case R_X86_64_32: {
                        uint64_t v = S + static_cast<uint64_t>(A);
                        verify(v <= UINT32_MAX,
                               "R_X86_64_32 unsigned overflow at " + site_desc(*obj, s, r.r_offset));
                        uint32_t v32 = static_cast<uint32_t>(v);
                        std::memcpy(site, &v32, 4);
                        break;
                    }

                    case R_X86_64_32S: {
                        int64_t v = static_cast<int64_t>(S) + A;
                        verify(v >= INT32_MIN && v <= INT32_MAX,
                               "R_X86_64_32S overflow at " + site_desc(*obj, s, r.r_offset));
                        int32_t v32 = static_cast<int32_t>(v);
                        std::memcpy(site, &v32, 4);
                        break;
                    }

                    default:
                        fatal("unsupported relocation type " + std::to_string(r.r_type) +
                              " at " + site_desc(*obj, s, r.r_offset));
                }
            }
        }
    }
}

} // namespace lnk
