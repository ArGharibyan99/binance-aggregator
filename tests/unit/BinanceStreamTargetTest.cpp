#include <agg/net/BinanceWebSocketClient.hpp>

#include <gtest/gtest.h>

TEST(BinanceStreamTargetTest, BuildsTargetForSingleSymbol)
{
    EXPECT_EQ(agg::net::build_combined_stream_target({ "BTCUSDT" }), "/stream?streams=btcusdt@trade");
}

TEST(BinanceStreamTargetTest, BuildsTargetForMultipleSymbolsLowercased)
{
    EXPECT_EQ(
        agg::net::build_combined_stream_target({ "BTCUSDT", "ETHUSDT" }),
        "/stream?streams=btcusdt@trade/ethusdt@trade");
}

TEST(BinanceStreamTargetTest, BuildsEmptyStreamsTargetForNoSymbols)
{
    EXPECT_EQ(agg::net::build_combined_stream_target({}), "/stream?streams=");
}
