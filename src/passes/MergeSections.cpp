#include "passes/Passes.h"
#include "elf/ElfTypes.h"

#include <algorithm>

namespace lnk {

namespace {

bool starts_with(const std::string& s, const char* p) {
    return s.rfind(p, 0) == 0;
}

bool is_metadata(const std::string& n) {
    return n == ".comment" || n == ".note.GNU-stack" || n == ".eh_frame" ||
           n == ".eh_frame_hdr";
}

} // namespace

void merge_sections(OutputImage& img) {
    // Pre-create in fixed order so .text/.rodata/.data/.bss layout is stable.
    for (const char* n : {".text", ".rodata", ".data", ".bss"}) img.get_or_create(n);

    for (auto& obj : img.objects) {
        for (auto& sp : obj->sections) {
            if (!sp) continue;
            auto& s = *sp;
            if (s.sh_type == SHT_NULL || s.sh_type == SHT_SYMTAB ||
                s.sh_type == SHT_STRTAB || s.sh_type == SHT_RELA || s.sh_type == SHT_REL) {
                s.discarded = true;
                continue;
            }
            const bool is_alloc = (s.sh_flags & SHF_ALLOC) != 0;
            const bool is_debug = starts_with(s.name, ".debug_");
            if (!is_alloc && !is_debug) {
                if (!(img.keep_metadata && is_metadata(s.name))) {
                    s.discarded = true;
                    continue;
                }
            }

            std::string out_name;
            if (is_debug) {
                out_name = s.name;
            } else if (s.sh_flags & SHF_EXECINSTR) {
                out_name = ".text";
            } else if (s.sh_type == SHT_NOBITS) {
                out_name = ".bss";
            } else if (s.sh_flags & SHF_WRITE) {
                out_name = ".data";
            } else {
                out_name = ".rodata";
            }

            auto* out = img.get_or_create(out_name);
            out->flags     = s.sh_flags;            // last-writer; all contributors should match
            out->align     = std::max<uint64_t>(out->align, s.sh_addralign);
            out->is_nobits = (s.sh_type == SHT_NOBITS);
            out->alloc     = is_alloc;
            out->contributors.push_back(&s);
            s.output       = out;
        }
    }
}

} // namespace lnk
