#pragma once

#include "model/OutputImage.h"

#include <string>
#include <vector>

namespace lnk {

void parse_objects(OutputImage& img, const std::vector<std::string>& inputs);
void merge_sections(OutputImage& img);
void resolve_symbols(OutputImage& img);
void assign_addresses(OutputImage& img);
void apply_relocations(OutputImage& img);
void emit_executable(OutputImage& img);

} // namespace lnk
