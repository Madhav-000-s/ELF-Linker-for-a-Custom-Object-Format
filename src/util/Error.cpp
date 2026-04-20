#include "Error.h"

namespace lnk {

void fatal(const std::string& msg) {
    throw LinkError(msg);
}

void verify(bool cond, const std::string& msg) {
    if (!cond) throw LinkError(msg);
}

} // namespace lnk
