#include "passes/Passes.h"
#include "elf/ElfTypes.h"
#include "util/Error.h"

#include <algorithm>

namespace lnk {

namespace {

constexpr uint64_t PAGE = 0x1000;
constexpr uint64_t BASE = 0x400000;

uint64_t align_up(uint64_t v, uint64_t a) {
    if (a <= 1) return v;
    return (v + a - 1) & ~(a - 1);
}

void layout_contributors(OutputSection* o) {
    if (!o || o->contributors.empty()) { o->size = 0; o->file_size = 0; return; }
    uint64_t off = 0;
    for (auto* c : o->contributors) {
        uint64_t a = c->sh_addralign ? c->sh_addralign : 1;
        off = align_up(off, a);
        c->offset_in_output = off;
        off += c->size();
    }
    o->size      = off;
    o->file_size = o->is_nobits ? 0 : off;
}

} // namespace

void assign_addresses(OutputImage& img) {
    auto* text   = img.find_output(".text");
    auto* rodata = img.find_output(".rodata");
    auto* data   = img.find_output(".data");
    auto* bss    = img.find_output(".bss");

    for (auto* o : {text, rodata, data, bss}) layout_contributors(o);

    // Each alloc output section starts on its own page so PT_LOAD permissions can differ.
    uint64_t cur_va  = BASE + PAGE;
    uint64_t cur_off = PAGE;

    auto place_page = [&](OutputSection* o) {
        if (!o || o->contributors.empty()) return;
        cur_va  = align_up(cur_va,  PAGE);
        cur_off = align_up(cur_off, PAGE);
        o->vaddr       = cur_va;
        o->file_offset = cur_off;
        cur_va  += o->size;
        if (!o->is_nobits) cur_off += o->size;
    };

    place_page(text);
    place_page(rodata);
    place_page(data);
    // .bss sits in the same PT_LOAD as .data, immediately after in VA, with no file bytes.
    if (bss && !bss->contributors.empty()) {
        if (!data || data->contributors.empty()) {
            // bss-only RW segment
            cur_va  = align_up(cur_va,  PAGE);
            cur_off = align_up(cur_off, PAGE);
            bss->vaddr       = cur_va;
            bss->file_offset = cur_off;
            cur_va += bss->size;
        } else {
            bss->vaddr       = cur_va;          // follows .data end in VA
            bss->file_offset = cur_off;         // file offset unused (NOBITS)
            cur_va += bss->size;
        }
    }

    // Debug sections: contiguous file offsets after alloc; no vaddr.
    for (auto& o : img.outputs) {
        if (o->name.rfind(".debug_", 0) != 0) continue;
        layout_contributors(o.get());
        if (o->size == 0) continue;
        cur_off = align_up(cur_off, std::max<uint64_t>(1, o->align));
        o->vaddr       = 0;
        o->file_offset = cur_off;
        cur_off += o->file_size;
    }

    // Assign final VA to globals/weaks that are defined.
    for (auto& [name, sym] : img.globals) {
        if (sym.kind == SymKind::Defined && sym.defined_in && sym.defined_in->output) {
            sym.value = sym.defined_in->output->vaddr
                      + sym.defined_in->offset_in_output
                      + sym.sec_offset;
        }
        // Absolute: value already set.
        // Undef weak: keep value = 0.
    }

    // Populate per-object sym_value vector used by ApplyRelocations.
    for (auto& obj : img.objects) {
        obj->sym_value.assign(obj->symtab.size(), 0);
        for (size_t i = 0; i < obj->symtab.size(); ++i) {
            const auto& s = obj->symtab[i];
            unsigned bind = ELF64_ST_BIND(s.st_info);
            if (bind == STB_LOCAL) {
                if (s.st_shndx == SHN_ABS) {
                    obj->sym_value[i] = s.st_value;
                } else if (s.st_shndx != SHN_UNDEF &&
                           s.st_shndx < obj->sections.size() &&
                           obj->sections[s.st_shndx] &&
                           obj->sections[s.st_shndx]->output) {
                    auto& sec = *obj->sections[s.st_shndx];
                    obj->sym_value[i] = sec.output->vaddr + sec.offset_in_output + s.st_value;
                }
            } else {
                std::string name = obj->sym_name(s);
                auto it = img.globals.find(name);
                if (it != img.globals.end()) obj->sym_value[i] = it->second.value;
            }
        }
    }

    auto it = img.globals.find(img.entry_name);
    verify(it != img.globals.end() && it->second.kind == SymKind::Defined,
           "entry symbol '" + img.entry_name + "' not found");
    img.entry_vaddr = it->second.value;
}

} // namespace lnk
