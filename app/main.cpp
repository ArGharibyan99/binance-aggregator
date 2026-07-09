#include <agg/config/Config.hpp>
#include <agg/config/ConfigLoader.hpp>
#include <agg/net/BinanceWebSocketClient.hpp>
#include <agg/net/ConnectionError.hpp>
#include <agg/output/FileOutputSink.hpp>
#include <agg/runtime/CliOptions.hpp>
#include <agg/runtime/MarketDataPipeline.hpp>
#include <agg/runtime/NetworkStage.hpp>

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> g_stop_requested{ false };

void handle_shutdown_signal(int)
{
    // Signal-safe: only a lock-free atomic store, nothing else.
    g_stop_requested.store(true);
}

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

std::string to_string(agg::net::ConnectionStage stage)
{
    switch (stage) {
    case agg::net::ConnectionStage::Resolve:
        return "resolve";
    case agg::net::ConnectionStage::TcpConnect:
        return "tcp_connect";
    case agg::net::ConnectionStage::TlsHandshake:
        return "tls_handshake";
    case agg::net::ConnectionStage::WebSocketHandshake:
        return "websocket_handshake";
    case agg::net::ConnectionStage::Read:
        return "read";
    }
    return "unknown";
}

std::string to_string(agg::net::ConnectionErrorKind kind)
{
    switch (kind) {
    case agg::net::ConnectionErrorKind::DnsFailure:
        return "dns_failure";
    case agg::net::ConnectionErrorKind::TcpConnectFailure:
        return "tcp_connect_failure";
    case agg::net::ConnectionErrorKind::TlsHandshakeFailure:
        return "tls_handshake_failure";
    case agg::net::ConnectionErrorKind::WebSocketHandshakeFailure:
        return "websocket_handshake_failure";
    case agg::net::ConnectionErrorKind::ReadFailure:
        return "read_failure";
    case agg::net::ConnectionErrorKind::Timeout:
        return "timeout";
    case agg::net::ConnectionErrorKind::ServerDisconnect:
        return "server_disconnect";
    case agg::net::ConnectionErrorKind::Unknown:
        return "unknown";
    }
    return "unknown";
}

// Queue capacity is not currently sourced from Config (no such field
// exists yet); this constant is generous relative to the configured
// flush_interval_ms/window_ms for the symbol counts this service targets.
constexpr std::size_t kQueueCapacity = 4096;

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
    spdlog::info("Config: aggregator_threads={}", config.aggregator_threads);
    spdlog::info("Config: output_file={}", config.output_file);
    spdlog::info("Config: ws_host={} ws_port={}", config.ws_host, config.ws_port);
    spdlog::info(
        "Config: reconnect initial_backoff_ms={} max_backoff_ms={} jitter_ratio={}",
        config.reconnect.initial_backoff_ms,
        config.reconnect.max_backoff_ms,
        config.reconnect.jitter_ratio);

    std::signal(SIGINT, handle_shutdown_signal);
    std::signal(SIGTERM, handle_shutdown_signal);

    agg::output::FileOutputSink sink(config.output_file);
    agg::runtime::MarketDataPipeline pipeline(
        config.window_ms, config.flush_interval_ms, kQueueCapacity, sink, config.aggregator_threads);

    agg::runtime::NetworkStage network_stage(
        config.ws_host,
        config.ws_port,
        agg::net::build_combined_stream_target(config.symbols),
        config.reconnect,
        pipeline.raw_message_queue(),
        [](agg::net::ConnectionStage stage, agg::net::ConnectionErrorKind kind, boost::system::error_code ec) {
            spdlog::warn(
                "WebSocket connection issue: stage={} kind={} error={}", to_string(stage), to_string(kind),
                ec.message());
        });

    spdlog::info("Starting market data pipeline and network stage");
    pipeline.start();
    network_stage.start();

    while (!g_stop_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    spdlog::info("Shutdown signal received; stopping stages");

    network_stage.stop();
    pipeline.stop();

    spdlog::info("Binance Aggregator service stopped");

    return EXIT_SUCCESS;
}
