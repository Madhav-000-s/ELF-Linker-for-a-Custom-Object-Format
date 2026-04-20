#pragma once

#include <string>
#include <vector>

namespace lnk {

struct Options {
    std::vector<std::string> inputs;
    std::string              output      = "a.out";
    std::string              entry       = "_start";
    bool                     dump_only   = false;
    bool                     verbose     = false;
    bool                     keep_metadata = false;
};

int run(const Options& opt);

} // namespace lnk
