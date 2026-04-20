#pragma once

#include <iostream>
#include <string>

namespace lnk {

inline bool& verbose_flag() {
    static bool v = false;
    return v;
}

inline void log(const std::string& msg) {
    if (verbose_flag()) std::cerr << "[elf-linker] " << msg << '\n';
}

} // namespace lnk
