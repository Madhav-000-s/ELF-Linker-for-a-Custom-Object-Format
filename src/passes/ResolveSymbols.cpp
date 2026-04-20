#include "passes/Passes.h"
#include "elf/ElfTypes.h"
#include "util/Error.h"

#include <string>

namespace lnk {

namespace {

Symbol make_symbol(const InputObject& obj, const Elf64_Sym& s, const std::string& name) {
    Symbol sym;
    sym.name    = name;
    unsigned b  = ELF64_ST_BIND(s.st_info);
    sym.binding = (b == STB_WEAK) ? Binding::Weak : Binding::Global;
    sym.origin  = const_cast<InputObject*>(&obj);
    sym.size    = s.st_size;

    if (s.st_shndx == SHN_UNDEF) {
        sym.kind = SymKind::Undef;
    } else if (s.st_shndx == SHN_ABS) {
        sym.kind  = SymKind::Absolute;
        sym.value = s.st_value;
    } else if (s.st_shndx < obj.sections.size() && obj.sections[s.st_shndx]) {
        sym.kind       = SymKind::Defined;
        sym.defined_in = obj.sections[s.st_shndx].get();
        sym.sec_offset = s.st_value;
    } else {
        sym.kind = SymKind::Undef;
    }
    return sym;
}

} // namespace

void resolve_symbols(OutputImage& img) {
    for (auto& obj : img.objects) {
        for (size_t i = 0; i < obj->symtab.size(); ++i) {
            const auto& s = obj->symtab[i];
            unsigned bind = ELF64_ST_BIND(s.st_info);
            if (bind == STB_LOCAL) continue;     // per-object, resolved in AssignAddresses

            std::string name = obj->sym_name(s);
            if (name.empty()) continue;

            Symbol sym = make_symbol(*obj, s, name);

            auto it = img.globals.find(name);
            if (it == img.globals.end()) {
                img.globals.emplace(name, std::move(sym));
                continue;
            }
            Symbol& existing = it->second;

            if (sym.kind == SymKind::Undef) {
                // Existing wins if defined; otherwise reconcile binding.
                if (existing.kind == SymKind::Undef) {
                    // If any sighting is strong, result is strong.
                    if (sym.binding == Binding::Global) existing.binding = Binding::Global;
                }
                continue;
            }

            // New is defined.
            if (existing.kind == SymKind::Undef) {
                existing = std::move(sym);                         // resolves the undef
                continue;
            }

            const bool ex_strong  = existing.is_strong();
            const bool new_strong = sym.is_strong();

            if (ex_strong && new_strong) {
                fatal("multiple definition of '" + name + "' in " +
                      (existing.origin ? existing.origin->path : "?") +
                      " and " + obj->path);
            }
            if (!ex_strong && new_strong) {
                existing = std::move(sym);                         // strong overrides weak
            }
            // else keep existing (strong beats weak; first weak wins)
        }
    }

    // Post-resolve sweep: every non-weak Undef is an error.
    for (auto& [name, sym] : img.globals) {
        if (sym.kind == SymKind::Undef && sym.binding != Binding::Weak) {
            fatal("undefined symbol: " + name);
        }
    }
}

} // namespace lnk
