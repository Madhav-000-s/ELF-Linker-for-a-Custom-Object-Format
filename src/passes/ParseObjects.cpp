#include "passes/Passes.h"
#include "elf/ElfReader.h"
#include "util/Log.h"

namespace lnk {

void parse_objects(OutputImage& img, const std::vector<std::string>& inputs) {
    for (const auto& p : inputs) {
        log("parse " + p);
        img.objects.push_back(read_object(p));
    }
}

} // namespace lnk
