#include "elf/ElfReader.h"
#include "elf/ElfTypes.h"
#include "util/Error.h"

#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace lnk {

namespace {

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    verify(f.good(), "cannot open: " + path);
    std::streamsize sz = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    verify(static_cast<std::streamsize>(buf.size()) == sz, "short read: " + path);
    return buf;
}

template <typename T>
T read_at(const std::vector<uint8_t>& buf, uint64_t off) {
    verify(off + sizeof(T) <= buf.size(), "out-of-range read");
    T v{};
    std::memcpy(&v, buf.data() + off, sizeof(T));
    return v;
}

void validate_ehdr(const Elf64_Ehdr& e, const std::string& path) {
    verify(e.e_ident[EI_MAG0] == ELFMAG0 && e.e_ident[EI_MAG1] == ELFMAG1 &&
           e.e_ident[EI_MAG2] == ELFMAG2 && e.e_ident[EI_MAG3] == ELFMAG3,
           path + ": not an ELF file");
    verify(e.e_ident[EI_CLASS] == ELFCLASS64, path + ": not ELF64");
    verify(e.e_ident[EI_DATA] == ELFDATA2LSB, path + ": not little-endian");
    verify(e.e_type == ET_REL, path + ": not a relocatable object (ET_REL)");
    verify(e.e_machine == EM_X86_64, path + ": not x86_64");
}

} // namespace

std::unique_ptr<InputObject> read_object(const std::string& path) {
    auto obj = std::make_unique<InputObject>();
    obj->path = path;
    obj->raw  = read_file(path);

    obj->ehdr = read_at<Elf64_Ehdr>(obj->raw, 0);
    validate_ehdr(obj->ehdr, path);

    const uint64_t shoff   = obj->ehdr.e_shoff;
    const uint16_t shnum   = obj->ehdr.e_shnum;
    const uint16_t shentsz = obj->ehdr.e_shentsize;
    verify(shentsz == sizeof(Elf64_Shdr), path + ": unexpected shentsize");
    obj->shdrs.resize(shnum);
    for (uint16_t i = 0; i < shnum; ++i) {
        obj->shdrs[i] = read_at<Elf64_Shdr>(obj->raw, shoff + static_cast<uint64_t>(i) * shentsz);
    }

    // .shstrtab
    verify(obj->ehdr.e_shstrndx < shnum, path + ": bad e_shstrndx");
    const auto& shstr = obj->shdrs[obj->ehdr.e_shstrndx];
    verify(shstr.sh_offset + shstr.sh_size <= obj->raw.size(), path + ": shstrtab OOR");
    obj->shstrtab.assign(reinterpret_cast<const char*>(obj->raw.data() + shstr.sh_offset),
                         shstr.sh_size);

    // Locate .symtab + its linked .strtab
    uint32_t symtab_idx = 0;
    for (uint16_t i = 0; i < shnum; ++i) {
        if (obj->shdrs[i].sh_type == SHT_SYMTAB) { symtab_idx = i; break; }
    }
    if (symtab_idx != 0) {
        const auto& sh = obj->shdrs[symtab_idx];
        verify(sh.sh_entsize == sizeof(Elf64_Sym), path + ": bad sym entsize");
        verify(sh.sh_offset + sh.sh_size <= obj->raw.size(), path + ": symtab OOR");
        const uint64_t n = sh.sh_size / sh.sh_entsize;
        obj->symtab.resize(n);
        for (uint64_t i = 0; i < n; ++i) {
            obj->symtab[i] = read_at<Elf64_Sym>(obj->raw, sh.sh_offset + i * sh.sh_entsize);
        }
        verify(sh.sh_link < shnum, path + ": bad strtab link");
        const auto& ss = obj->shdrs[sh.sh_link];
        verify(ss.sh_offset + ss.sh_size <= obj->raw.size(), path + ": strtab OOR");
        obj->strtab.assign(reinterpret_cast<const char*>(obj->raw.data() + ss.sh_offset),
                           ss.sh_size);
    }

    // Build InputSection for every shdr; fill data + nobits size.
    obj->sections.resize(shnum);
    for (uint16_t i = 0; i < shnum; ++i) {
        const auto& sh = obj->shdrs[i];
        if (sh.sh_type == SHT_NULL) continue;
        auto s = std::make_unique<InputSection>();
        s->owner          = obj.get();
        s->index_in_owner = i;
        s->name           = obj->shdr_name(sh);
        s->sh_type        = sh.sh_type;
        s->sh_flags       = sh.sh_flags;
        s->sh_addralign   = sh.sh_addralign ? sh.sh_addralign : 1;
        if (sh.sh_type == SHT_NOBITS) {
            s->nobits_size = sh.sh_size;
        } else if (sh.sh_size > 0 && sh.sh_type != SHT_NULL) {
            verify(sh.sh_offset + sh.sh_size <= obj->raw.size(),
                   path + ": section '" + s->name + "' OOR");
            s->data.assign(obj->raw.data() + sh.sh_offset,
                           obj->raw.data() + sh.sh_offset + sh.sh_size);
        }
        obj->sections[i] = std::move(s);
    }

    // Attach rela entries to their target sections.
    for (uint16_t i = 0; i < shnum; ++i) {
        const auto& sh = obj->shdrs[i];
        if (sh.sh_type != SHT_RELA) continue;
        verify(sh.sh_entsize == sizeof(Elf64_Rela), path + ": bad rela entsize");
        verify(sh.sh_info < shnum, path + ": bad rela sh_info");
        auto& tgt = obj->sections[sh.sh_info];
        verify(static_cast<bool>(tgt), path + ": rela targets null section");
        const uint64_t n = sh.sh_size / sh.sh_entsize;
        tgt->relocs.reserve(tgt->relocs.size() + n);
        for (uint64_t k = 0; k < n; ++k) {
            Elf64_Rela r = read_at<Elf64_Rela>(obj->raw, sh.sh_offset + k * sh.sh_entsize);
            Relocation rn;
            rn.r_offset = r.r_offset;
            rn.r_type   = static_cast<uint32_t>(ELF64_R_TYPE(r.r_info));
            rn.r_sym    = static_cast<uint32_t>(ELF64_R_SYM(r.r_info));
            rn.r_addend = r.r_addend;
            tgt->relocs.push_back(rn);
        }
    }

    return obj;
}

namespace {

const char* sht_name(uint32_t t) {
    switch (t) {
        case SHT_NULL:     return "NULL";
        case SHT_PROGBITS: return "PROGBITS";
        case SHT_SYMTAB:   return "SYMTAB";
        case SHT_STRTAB:   return "STRTAB";
        case SHT_RELA:     return "RELA";
        case SHT_NOBITS:   return "NOBITS";
        case SHT_REL:      return "REL";
        default:           return "OTHER";
    }
}
const char* reloc_name(uint32_t t) {
    switch (t) {
        case R_X86_64_NONE:  return "R_X86_64_NONE";
        case R_X86_64_64:    return "R_X86_64_64";
        case R_X86_64_PC32:  return "R_X86_64_PC32";
        case R_X86_64_PLT32: return "R_X86_64_PLT32";
        case R_X86_64_32:    return "R_X86_64_32";
        case R_X86_64_32S:   return "R_X86_64_32S";
        default:             return "OTHER";
    }
}
const char* bind_name(unsigned b) {
    switch (b) {
        case STB_LOCAL:  return "LOCAL";
        case STB_GLOBAL: return "GLOBAL";
        case STB_WEAK:   return "WEAK";
        default:         return "OTHER";
    }
}
const char* type_name(unsigned t) {
    switch (t) {
        case STT_NOTYPE:  return "NOTYPE";
        case STT_OBJECT:  return "OBJECT";
        case STT_FUNC:    return "FUNC";
        case STT_SECTION: return "SECTION";
        case STT_FILE:    return "FILE";
        default:          return "OTHER";
    }
}

} // namespace

void dump_object(const InputObject& obj, std::ostream& os) {
    os << "=== " << obj.path << " ===\n";
    os << "ELF header: type=ET_REL machine=EM_X86_64 shnum=" << obj.ehdr.e_shnum
       << " shstrndx=" << obj.ehdr.e_shstrndx << "\n";

    os << "Sections (" << obj.shdrs.size() << "):\n";
    for (size_t i = 0; i < obj.shdrs.size(); ++i) {
        const auto& sh = obj.shdrs[i];
        os << "  [" << std::setw(2) << i << "] "
           << std::left << std::setw(20) << (obj.shdr_name(sh)) << std::right
           << " type=" << std::setw(8) << sht_name(sh.sh_type)
           << " size=0x" << std::hex << sh.sh_size
           << " align=" << std::dec << sh.sh_addralign
           << " flags=0x" << std::hex << sh.sh_flags << std::dec
           << "\n";
    }

    os << "Symbols (" << obj.symtab.size() << "):\n";
    for (size_t i = 0; i < obj.symtab.size(); ++i) {
        const auto& s = obj.symtab[i];
        os << "  [" << std::setw(3) << i << "] "
           << std::left << std::setw(24) << obj.sym_name(s) << std::right
           << " bind=" << std::setw(6) << bind_name(ELF64_ST_BIND(s.st_info))
           << " type=" << std::setw(7) << type_name(ELF64_ST_TYPE(s.st_info))
           << " shndx=" << std::setw(5) << s.st_shndx
           << " value=0x" << std::hex << s.st_value
           << " size=0x" << s.st_size << std::dec
           << "\n";
    }

    os << "Relocations:\n";
    for (const auto& sp : obj.sections) {
        if (!sp || sp->relocs.empty()) continue;
        os << "  in '" << sp->name << "':\n";
        for (const auto& r : sp->relocs) {
            std::string sname = "?";
            if (r.r_sym < obj.symtab.size()) {
                const auto& sym = obj.symtab[r.r_sym];
                sname = obj.sym_name(sym);
                if (sname.empty() && sym.st_shndx < obj.shdrs.size()) {
                    sname = obj.shdr_name(obj.shdrs[sym.st_shndx]);
                }
            }
            os << "    off=0x" << std::hex << r.r_offset
               << " type=" << std::setw(14) << std::left << reloc_name(r.r_type) << std::right
               << " sym=" << std::setw(20) << std::left << sname << std::right
               << " addend=" << std::dec << r.r_addend << "\n";
        }
    }
    os << std::dec;
}

} // namespace lnk
