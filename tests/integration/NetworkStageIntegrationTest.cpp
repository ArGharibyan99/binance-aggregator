#include <agg/runtime/NetworkStage.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

// Connects to 127.0.0.1 (no DNS, no internet needed) on a port nothing
// listens on, so every attempt fails immediately with a real TCP connect
// failure. This exercises the actual reconnect-with-backoff loop and
// prompt-stop behavior without ever touching Binance.
TEST(NetworkStageIntegrationTest, RetriesWithBackoffAndStopsPromptly)
{
    agg::runtime::BoundedQueue<std::string> raw_queue(4);

    agg::config::ReconnectConfig reconnect_config;
    reconnect_config.initial_backoff_ms = 5;
    reconnect_config.max_backoff_ms = 20;
    reconnect_config.jitter_ratio = 0.0;

    std::atomic<int> failure_count{ 0 };

    agg::runtime::NetworkStage network_stage(
        "127.0.0.1", "1", "/stream?streams=btcusdt@trade", reconnect_config, raw_queue,
        [&](agg::net::ConnectionStage, agg::net::ConnectionErrorKind, boost::system::error_code) {
            failure_count.fetch_add(1);
        });

    network_stage.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto stop_start = std::chrono::steady_clock::now();
    network_stage.stop();
    const auto stop_duration = std::chrono::steady_clock::now() - stop_start;

    EXPECT_GE(failure_count.load(), 2);
    EXPECT_LT(stop_duration, std::chrono::milliseconds(500));
}

TEST(NetworkStageIntegrationTest, StopWithoutStartIsSafe)
{
    agg::runtime::BoundedQueue<std::string> raw_queue(4);

    agg::config::ReconnectConfig reconnect_config;
    reconnect_config.initial_backoff_ms = 5;
    reconnect_config.max_backoff_ms = 20;

    agg::runtime::NetworkStage network_stage("127.0.0.1", "1", "/stream?streams=btcusdt@trade", reconnect_config, raw_queue);

    network_stage.stop();
}
