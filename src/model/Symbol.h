#pragma once

#include <cstdint>
#include <string>

namespace lnk {

struct InputObject;
struct InputSection;

enum class Binding { Local, Global, Weak };
enum class SymKind { Undef, Defined, Common, Absolute };

struct Symbol {
    std::string    name;
    Binding        binding     = Binding::Global;
    SymKind        kind        = SymKind::Undef;
    InputSection*  defined_in  = nullptr;   // only if kind == Defined
    uint64_t       sec_offset  = 0;         // byte offset within defined_in
    uint64_t       value       = 0;         // final VA after AssignAddresses
    uint64_t       size        = 0;
    InputObject*   origin      = nullptr;

    bool is_strong() const { return binding != Binding::Weak; }
};

} // namespace lnk
