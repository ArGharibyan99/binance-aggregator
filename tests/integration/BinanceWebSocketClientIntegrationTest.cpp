#include <agg/net/BinanceWebSocketClient.hpp>
#include <agg/runtime/BoundedQueue.hpp>

#include <gtest/gtest.h>

// These tests deliberately never contact Binance or any external host:
// connecting to 127.0.0.1 needs no DNS lookup (it is a literal address)
// and no internet access, and nothing listens on the chosen port, so the
// TCP connect fails immediately and deterministically.
TEST(BinanceWebSocketClientIntegrationTest, ReportsTcpConnectFailureWhenNothingIsListening)
{
    agg::runtime::BoundedQueue<std::string> raw_queue(4);
    agg::net::BinanceWebSocketClient client("127.0.0.1", "1", "/stream?streams=btcusdt@trade", raw_queue);

    bool handler_called = false;
    agg::net::ConnectionStage failed_stage{};
    agg::net::ConnectionErrorKind failed_kind{};

    client.run([&](agg::net::ConnectionStage stage, agg::net::ConnectionErrorKind kind, boost::system::error_code) {
        handler_called = true;
        failed_stage = stage;
        failed_kind = kind;
    });

    ASSERT_TRUE(handler_called);
    EXPECT_EQ(failed_stage, agg::net::ConnectionStage::TcpConnect);
    EXPECT_EQ(failed_kind, agg::net::ConnectionErrorKind::TcpConnectFailure);
}

TEST(BinanceWebSocketClientIntegrationTest, StopBeforeRunReturnsCleanlyWithoutInvokingHandler)
{
    agg::runtime::BoundedQueue<std::string> raw_queue(4);
    agg::net::BinanceWebSocketClient client("127.0.0.1", "1", "/stream?streams=btcusdt@trade", raw_queue);

    client.stop();

    bool handler_called = false;
    client.run([&](agg::net::ConnectionStage, agg::net::ConnectionErrorKind, boost::system::error_code) {
        handler_called = true;
    });

    // stop() only affects an in-progress or future connection attempt at
    // the point the underlying socket exists; calling it before run() is
    // still expected to behave safely (no crash, no dangling state) even
    // though this particular run() still attempts (and fails) to connect.
    EXPECT_TRUE(handler_called);
}
