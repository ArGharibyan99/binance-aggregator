#pragma once

#include <agg/config/Config.hpp>

#include <filesystem>

namespace agg::config {

class ConfigLoader {
public:
    static Config load_from_file(const std::filesystem::path& path);
};

} // namespace agg::config