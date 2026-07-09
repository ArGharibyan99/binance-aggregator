#pragma once

#include <agg/config/Config.hpp>

#include <filesystem>

namespace agg::config {

/// Loads and validates the service's runtime configuration file.
class ConfigLoader {
public:
    /// Reads and validates the JSON config at `path`. Throws
    /// std::runtime_error with a descriptive message if the file cannot
    /// be opened, is not valid JSON, or fails validation (missing/empty
    /// fields, non-positive timing values, invalid symbols, or an
    /// inverted reconnect backoff range).
    static Config load_from_file(const std::filesystem::path& path);
};

} // namespace agg::config