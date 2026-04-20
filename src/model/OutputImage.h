#pragma once

#include "elf/ElfTypes.h"
#include "model/InputObject.h"
#include "model/OutputSection.h"
#include "model/Symbol.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace lnk {

struct OutputImage {
    std::vector<std::unique_ptr<InputObject>>   objects;
    std::vector<std::unique_ptr<OutputSection>> outputs;
    std::vector<Elf64_Phdr>                     phdrs;
    std::unordered_map<std::string, Symbol>     globals;

    std::string entry_name  = "_start";
    uint64_t    entry_vaddr = 0;
    std::string out_path    = "a.out";
    bool        keep_metadata = false;

    OutputSection* find_output(const std::string& name) {
        for (auto& p : outputs) if (p->name == name) return p.get();
        return nullptr;
    }
    OutputSection* get_or_create(const std::string& name) {
        if (auto* p = find_output(name)) return p;
        auto up = std::make_unique<OutputSection>();
        up->name = name;
        outputs.push_back(std::move(up));
        return outputs.back().get();
    }
};

} // namespace lnk
