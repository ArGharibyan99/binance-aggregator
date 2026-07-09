#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace agg::config {

/// WebSocket reconnect backoff settings; see agg::net::ReconnectPolicy.
struct ReconnectConfig {
    std::uint64_t initial_backoff_ms = 500;
    std::uint64_t max_backoff_ms = 30000;
    double jitter_ratio = 0.2;
};

/// Runtime configuration loaded from the service's config.json (see
/// ConfigLoader), covering symbols to subscribe to, aggregation/flush
/// timing, output location, Binance connection details, and reconnect
/// behavior.
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