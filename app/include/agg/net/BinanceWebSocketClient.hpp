#pragma once

#include <agg/net/ConnectionError.hpp>
#include <agg/runtime/BoundedQueue.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace agg::net {

/// Builds a Binance combined-stream request target from symbols, e.g.
/// {"BTCUSDT", "ETHUSDT"} -> "/stream?streams=btcusdt@trade/ethusdt@trade".
/// Symbols are lowercased, since Binance stream names are lowercase.
std::string build_combined_stream_target(const std::vector<std::string>& symbols);

/// Called when a connection ends abnormally, with the stage that failed,
/// the classified error kind, and the underlying error_code (for
/// logging). Not called when stop() causes a clean, requested shutdown.
using ConnectionErrorHandler =
    std::function<void(ConnectionStage, ConnectionErrorKind, boost::system::error_code)>;

/// Connects to a Binance combined-stream WebSocket endpoint over TLS and
/// pushes each received text message onto the given raw-message queue,
/// verbatim. This class only knows about the queue: it has no knowledge
/// of parsing or aggregation, and never touches aggregation state.
///
/// run() is synchronous and blocks the calling thread for the lifetime of
/// one connection attempt (matching the single network thread in the
/// planned runtime architecture). It does not retry on its own; retrying
/// after a failure is the caller's responsibility (see ReconnectPolicy).
class BinanceWebSocketClient {
public:
    BinanceWebSocketClient(
        std::string host,
        std::string port,
        std::string target,
        agg::runtime::BoundedQueue<std::string>& raw_message_queue);

    ~BinanceWebSocketClient();

    BinanceWebSocketClient(const BinanceWebSocketClient&) = delete;
    BinanceWebSocketClient& operator=(const BinanceWebSocketClient&) = delete;

    /// Resolves, connects, performs the TLS and WebSocket handshakes,
    /// then reads messages until the connection ends. Returns normally
    /// after a clean stop(); otherwise invokes error_handler (if set)
    /// exactly once with the stage/kind/error_code that ended it.
    ///
    /// Note: a stop() called during the initial DNS resolve (before the
    /// socket exists) is only noticed once resolve() itself returns,
    /// rather than interrupting it outright; making every phase
    /// immediately interruptible is left to the graceful-shutdown work.
    void run(ConnectionErrorHandler error_handler = {});

    /// Closes the underlying connection, causing a blocked run() on
    /// another thread to return. Safe to call from another thread, and
    /// safe to call even if run() has not been called or has already
    /// returned.
    void stop();

private:
    using SslStream = boost::beast::ssl_stream<boost::asio::ip::tcp::socket>;
    using WebSocketStream = boost::beast::websocket::stream<SslStream>;

    std::string host_;
    std::string port_;
    std::string target_;
    agg::runtime::BoundedQueue<std::string>& raw_message_queue_;

    boost::asio::io_context ioc_;
    boost::asio::ssl::context ssl_ctx_;

    std::atomic<bool> stop_requested_{ false };
    std::mutex stream_mutex_;
    std::optional<WebSocketStream> ws_;
};

} // namespace agg::net
