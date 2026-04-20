#include "passes/Passes.h"
#include "elf/ElfTypes.h"
#include "util/Error.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace lnk {

namespace {

constexpr uint64_t PAGE = 0x1000;
constexpr uint64_t BASE = 0x400000;

uint64_t align_up(uint64_t v, uint64_t a) {
    if (a <= 1) return v;
    return (v + a - 1) & ~(a - 1);
}

struct ShstrBuilder {
    std::vector<char>                         bytes {'\0'};  // index 0 = empty name
    std::unordered_map<std::string, uint32_t> cache;

    uint32_t add(const std::string& s) {
        if (s.empty()) return 0;
        auto it = cache.find(s);
        if (it != cache.end()) return it->second;
        uint32_t off = static_cast<uint32_t>(bytes.size());
        for (char c : s) bytes.push_back(c);
        bytes.push_back('\0');
        cache.emplace(s, off);
        return off;
    }
};

} // namespace

void emit_executable(OutputImage& img) {
    auto* text   = img.find_output(".text");
    auto* rodata = img.find_output(".rodata");
    auto* data   = img.find_output(".data");
    auto* bss    = img.find_output(".bss");

    auto is_live = [](OutputSection* o) {
        return o && !o->contributors.empty() && (o->size > 0 || o->is_nobits);
    };

    const bool has_text   = is_live(text);
    const bool has_rodata = is_live(rodata);
    const bool has_data   = is_live(data);
    const bool has_bss    = is_live(bss);

    uint32_t num_phdrs = 2;                  // PT_PHDR + PT_LOAD #0 (ehdr+phdrs)
    if (has_text)                num_phdrs++;
    if (has_rodata)              num_phdrs++;
    if (has_data || has_bss)     num_phdrs++;

    const uint64_t hdr_size = sizeof(Elf64_Ehdr) + num_phdrs * sizeof(Elf64_Phdr);
    verify(hdr_size <= PAGE, "ehdr+phdrs exceed a page");

    std::vector<Elf64_Phdr> phdrs;
    phdrs.reserve(num_phdrs);

    auto add_phdr = [&](uint32_t t, uint32_t f, uint64_t off, uint64_t va,
                        uint64_t filesz, uint64_t memsz, uint64_t align) {
        Elf64_Phdr p{};
        p.p_type = t; p.p_flags = f;
        p.p_offset = off; p.p_vaddr = va; p.p_paddr = va;
        p.p_filesz = filesz; p.p_memsz = memsz; p.p_align = align;
        phdrs.push_back(p);
    };

    add_phdr(PT_PHDR, PF_R,
             sizeof(Elf64_Ehdr), BASE + sizeof(Elf64_Ehdr),
             num_phdrs * sizeof(Elf64_Phdr),
             num_phdrs * sizeof(Elf64_Phdr), 8);

    add_phdr(PT_LOAD, PF_R, 0, BASE, hdr_size, hdr_size, PAGE);

    if (has_text) {
        add_phdr(PT_LOAD, PF_R | PF_X,
                 text->file_offset, text->vaddr,
                 text->size, text->size, PAGE);
    }
    if (has_rodata) {
        add_phdr(PT_LOAD, PF_R,
                 rodata->file_offset, rodata->vaddr,
                 rodata->size, rodata->size, PAGE);
    }
    if (has_data || has_bss) {
        uint64_t va   = has_data ? data->vaddr       : bss->vaddr;
        uint64_t off  = has_data ? data->file_offset : bss->file_offset;
        uint64_t fsz  = has_data ? data->size        : 0;
        uint64_t msz  = fsz + (has_bss ? bss->size : 0);
        add_phdr(PT_LOAD, PF_R | PF_W, off, va, fsz, msz, PAGE);
    }

    // Section headers.
    ShstrBuilder shb;
    struct ShdrPlan {
        std::string    name;
        OutputSection* out   = nullptr;
        uint32_t       type  = SHT_NULL;
        uint64_t       flags = 0;
    };
    std::vector<ShdrPlan> plan;
    plan.push_back({"", nullptr, SHT_NULL, 0});

    auto push_out = [&](OutputSection* o) {
        if (!is_live(o)) return;
        ShdrPlan p;
        p.name  = o->name;
        p.out   = o;
        p.type  = o->is_nobits ? SHT_NOBITS : SHT_PROGBITS;
        p.flags = o->flags;
        plan.push_back(p);
    };

    push_out(text);
    push_out(rodata);
    push_out(data);
    push_out(bss);
    for (auto& o : img.outputs) {
        if (o->name.rfind(".debug_", 0) == 0 && o->size > 0) push_out(o.get());
    }

    // Compute end-of-content file offset (after all written sections).
    uint64_t max_off = hdr_size;
    auto update_max = [&](uint64_t off, uint64_t sz) {
        if (off + sz > max_off) max_off = off + sz;
    };
    for (const auto& p : plan) {
        if (!p.out) continue;
        if (!p.out->is_nobits) update_max(p.out->file_offset, p.out->size);
    }

    // Pre-register all shstrtab names so positions are known.
    for (const auto& p : plan) shb.add(p.name);
    shb.add(".shstrtab");

    const uint64_t shstrtab_off  = align_up(max_off, 1);
    const uint64_t shstrtab_size = shb.bytes.size();
    const uint64_t shoff         = align_up(shstrtab_off + shstrtab_size, 8);

    std::vector<Elf64_Shdr> shdrs;
    for (const auto& p : plan) {
        Elf64_Shdr sh{};
        sh.sh_name = shb.add(p.name);
        sh.sh_type = p.type;
        if (p.out) {
            sh.sh_flags     = p.flags;
            sh.sh_addr      = p.out->vaddr;
            sh.sh_offset    = p.out->file_offset;
            sh.sh_size      = p.out->size;
            sh.sh_addralign = p.out->align;
        }
        shdrs.push_back(sh);
    }
    Elf64_Shdr sh_shstr{};
    sh_shstr.sh_name      = shb.add(".shstrtab");
    sh_shstr.sh_type      = SHT_STRTAB;
    sh_shstr.sh_offset    = shstrtab_off;
    sh_shstr.sh_size      = shstrtab_size;
    sh_shstr.sh_addralign = 1;
    shdrs.push_back(sh_shstr);
    const uint16_t shstrndx = static_cast<uint16_t>(shdrs.size() - 1);

    Elf64_Ehdr eh{};
    eh.e_ident[EI_MAG0]    = ELFMAG0;
    eh.e_ident[EI_MAG1]    = ELFMAG1;
    eh.e_ident[EI_MAG2]    = ELFMAG2;
    eh.e_ident[EI_MAG3]    = ELFMAG3;
    eh.e_ident[EI_CLASS]   = ELFCLASS64;
    eh.e_ident[EI_DATA]    = ELFDATA2LSB;
    eh.e_ident[EI_VERSION] = EV_CURRENT;
    eh.e_type              = ET_EXEC;
    eh.e_machine           = EM_X86_64;
    eh.e_version           = EV_CURRENT;
    eh.e_entry             = img.entry_vaddr;
    eh.e_phoff             = sizeof(Elf64_Ehdr);
    eh.e_shoff             = shoff;
    eh.e_flags             = 0;
    eh.e_ehsize            = sizeof(Elf64_Ehdr);
    eh.e_phentsize         = sizeof(Elf64_Phdr);
    eh.e_phnum             = static_cast<uint16_t>(phdrs.size());
    eh.e_shentsize         = sizeof(Elf64_Shdr);
    eh.e_shnum             = static_cast<uint16_t>(shdrs.size());
    eh.e_shstrndx          = shstrndx;

    const uint64_t final_size = shoff + shdrs.size() * sizeof(Elf64_Shdr);
    std::vector<uint8_t> buf(final_size, 0);

    auto write_at = [&](uint64_t off, const void* src, size_t n) {
        verify(off + n <= buf.size(), "write OOR");
        std::memcpy(buf.data() + off, src, n);
    };

    write_at(0, &eh, sizeof(eh));
    for (size_t i = 0; i < phdrs.size(); ++i) {
        write_at(sizeof(eh) + i * sizeof(Elf64_Phdr), &phdrs[i], sizeof(Elf64_Phdr));
    }

    // Contributor bytes for each output section (skip NOBITS).
    auto write_out = [&](OutputSection* o) {
        if (!is_live(o) || o->is_nobits) return;
        for (auto* c : o->contributors) {
            if (c->sh_type == SHT_NOBITS) continue;
            if (!c->data.empty()) {
                write_at(o->file_offset + c->offset_in_output, c->data.data(), c->data.size());
            }
        }
    };
    write_out(text);
    write_out(rodata);
    write_out(data);
    for (auto& o : img.outputs) {
        if (o->name.rfind(".debug_", 0) == 0) write_out(o.get());
    }

    // shstrtab + shdrs.
    write_at(shstrtab_off, shb.bytes.data(), shb.bytes.size());
    for (size_t i = 0; i < shdrs.size(); ++i) {
        write_at(shoff + i * sizeof(Elf64_Shdr), &shdrs[i], sizeof(Elf64_Shdr));
    }

    std::ofstream out(img.out_path, std::ios::binary | std::ios::trunc);
    verify(out.good(), "cannot open output: " + img.out_path);
    out.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    verify(out.good(), "write failed: " + img.out_path);
    out.close();

    namespace fs = std::filesystem;
    fs::permissions(img.out_path,
                    fs::perms::owner_read  | fs::perms::owner_write | fs::perms::owner_exec |
                    fs::perms::group_read  | fs::perms::group_exec  |
                    fs::perms::others_read | fs::perms::others_exec,
                    fs::perm_options::replace);
}

} // namespace lnk
