#pragma once

#include <string>
#include <stdexcept>

namespace lnk {

class LinkError : public std::runtime_error {
public:
    explicit LinkError(const std::string& msg) : std::runtime_error(msg) {}
};

[[noreturn]] void fatal(const std::string& msg);

void verify(bool cond, const std::string& msg);

} // namespace lnk
