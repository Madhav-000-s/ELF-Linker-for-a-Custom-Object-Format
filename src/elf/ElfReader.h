#pragma once

#include "model/InputObject.h"

#include <memory>
#include <string>

namespace lnk {

std::unique_ptr<InputObject> read_object(const std::string& path);

void dump_object(const InputObject& obj, std::ostream& os);

} // namespace lnk
