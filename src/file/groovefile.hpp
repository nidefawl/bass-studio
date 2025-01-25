#pragma once
#include "types.hpp"
#include <vector>
#include <optional>
#include <variant>
#include "host/project/project.hpp"

struct groove_file_t {
    int32_t version = -1;
    std::vector<groove_data_t> grooves;
};

namespace DAW::ProjectFileV1 {
std::variant<groove_file_t, String> loadGrooveFile(const String& path);
} // namespace DAW::ProjectFileV1

namespace DAW::ProjectFileV2 {
std::optional<String> saveGrooveFile(const groove_file_t& grooveFile, const String& path);
std::variant<groove_file_t, String> loadGrooveFile(const String& path);
} // namespace DAW::ProjectFileV2
