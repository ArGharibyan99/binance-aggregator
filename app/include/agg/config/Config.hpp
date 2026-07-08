#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace agg::config {

struct ReconnectConfig {
    std::uint64_t initial_backoff_ms = 500;
    std::uint64_t max_backoff_ms = 30000;
    double jitter_ratio = 0.2;
};

struct Config {
    std::vector<std::string> symbols;

    std::uint64_t window_ms = 1000;
    std::uint64_t flush_interval_ms = 1000;

    std::string output_file;

    std::string ws_host = "stream.binance.com";
    std::string ws_port = "9443";

    ReconnectConfig reconnect;
};

} // namespace agg::config