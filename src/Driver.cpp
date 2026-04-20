#include "Driver.h"
#include "elf/ElfReader.h"
#include "model/OutputImage.h"
#include "passes/Passes.h"
#include "util/Error.h"
#include "util/Log.h"

#include <iostream>

namespace lnk {

int run(const Options& opt) {
    verbose_flag() = opt.verbose;

    OutputImage img;
    img.out_path       = opt.output;
    img.entry_name     = opt.entry;
    img.keep_metadata  = opt.keep_metadata;

    try {
        parse_objects(img, opt.inputs);

        if (opt.dump_only) {
            for (const auto& obj : img.objects) dump_object(*obj, std::cout);
            return 0;
        }

        merge_sections(img);
        resolve_symbols(img);
        assign_addresses(img);
        apply_relocations(img);
        emit_executable(img);
    } catch (const LinkError& e) {
        std::cerr << "elf-linker: error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

} // namespace lnk
