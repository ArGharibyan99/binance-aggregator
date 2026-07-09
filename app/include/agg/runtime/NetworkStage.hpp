#pragma once

#include <agg/config/Config.hpp>
#include <agg/net/BinanceWebSocketClient.hpp>
#include <agg/net/ReconnectPolicy.hpp>
#include <agg/runtime/BoundedQueue.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace agg::runtime {

/// Runs the Binance WebSocket client on its own thread, reconnecting
/// with exponential backoff (via ReconnectPolicy) whenever a connection
/// ends abnormally, until stop() is called. The reconnect backoff is
/// reset after any connection that got far enough to successfully read
/// at least one message, so a long-lived connection dropping later does
/// not inherit an escalated delay from earlier connection attempts.
class NetworkStage {
public:
    NetworkStage(
        std::string host,
        std::string port,
        std::string target,
        agg::config::ReconnectConfig reconnect_config,
        agg::runtime::BoundedQueue<std::string>& raw_message_queue,
        agg::net::ConnectionErrorHandler error_handler = {});

    ~NetworkStage();

    NetworkStage(const NetworkStage&) = delete;
    NetworkStage& operator=(const NetworkStage&) = delete;

    /// Starts the network thread.
    void start();

    /// Stops the current connection attempt or backoff wait and joins
    /// the network thread. Safe to call more than once, and safe to
    /// call even if start() was never called.
    void stop();

private:
    void run();

    std::string host_;
    std::string port_;
    std::string target_;
    agg::runtime::BoundedQueue<std::string>& raw_message_queue_;
    agg::net::ConnectionErrorHandler error_handler_;

    agg::net::ReconnectPolicy reconnect_policy_;

    std::mutex client_mutex_;
    std::unique_ptr<agg::net::BinanceWebSocketClient> client_;

    std::atomic<bool> stop_requested_{ false };
    std::thread thread_;
};

} // namespace agg::runtime
