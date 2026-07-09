#include <agg/config/Config.hpp>
#include <agg/config/ConfigLoader.hpp>
#include <agg/runtime/CliOptions.hpp>

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

namespace {

std::string join_symbols(const std::vector<std::string>& symbols)
{
    std::string joined;

    for (std::size_t i = 0; i < symbols.size(); ++i) {
        if (i != 0) {
            joined += ",";
        }
        joined += symbols[i];
    }

    return joined;
}

} // namespace

int main(int argc, char** argv)
{
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

    spdlog::info("Binance Aggregator service starting");

    agg::runtime::CliOptions options;

    try {
        options = agg::runtime::parse_cli_options(argc, argv);
    } catch (const std::exception& error) {
        spdlog::error("Failed to parse command-line arguments: {}", error.what());
        return EXIT_FAILURE;
    }

    spdlog::info("Loading configuration from '{}'", options.config_path.string());

    agg::config::Config config;

    try {
        config = agg::config::ConfigLoader::load_from_file(options.config_path);
    } catch (const std::exception& error) {
        spdlog::error("Failed to load configuration: {}", error.what());
        return EXIT_FAILURE;
    }

    spdlog::info("Config: symbols=[{}]", join_symbols(config.symbols));
    spdlog::info("Config: window_ms={} flush_interval_ms={}", config.window_ms, config.flush_interval_ms);
    spdlog::info("Config: output_file={}", config.output_file);
    spdlog::info("Config: ws_host={} ws_port={}", config.ws_host, config.ws_port);
    spdlog::info(
        "Config: reconnect initial_backoff_ms={} max_backoff_ms={} jitter_ratio={}",
        config.reconnect.initial_backoff_ms,
        config.reconnect.max_backoff_ms,
        config.reconnect.jitter_ratio);

    spdlog::info("Configuration loaded successfully; Binance connection not yet implemented");

    return EXIT_SUCCESS;
}
