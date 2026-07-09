#pragma once

#include <filesystem>

namespace agg::runtime {

/// Parsed command-line options for the binance_aggregator executable.
struct CliOptions {
    std::filesystem::path config_path = "config/config.json";
};

/// Parses command-line arguments (as passed to main) into CliOptions.
/// Supports `--config <path>` and `--config=<path>`; any argument not
/// recognized is ignored. Throws std::invalid_argument if `--config` is
/// given without a following value.
CliOptions parse_cli_options(int argc, char** argv);

} // namespace agg::runtime
