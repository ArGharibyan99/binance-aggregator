#include <agg/parse/BinanceTradeParser.hpp>

#include <gtest/gtest.h>

namespace {
using agg::parse::BinanceTradeParser;
}

TEST(BinanceTradeParserTest, ParsesValidRawTradeMessage)
{
    const auto event = BinanceTradeParser::parse(R"json(
{
  "e": "trade",
  "E": 123456789,
  "s": "BTCUSDT",
  "t": 12345,
  "p": "43012.10",
  "q": "0.50000000",
  "b": 88,
  "a": 50,
  "T": 123456785,
  "m": true,
  "M": true
}
)json");

    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->symbol, "BTCUSDT");
    EXPECT_EQ(event->trade_id, 12345U);
    EXPECT_EQ(event->price.to_string(), "43012.1");
    EXPECT_EQ(event->quantity.to_string(), "0.5");
    EXPECT_EQ(event->trade_time_ms, 123456785U);
    EXPECT_TRUE(event->buyer_is_maker);
}

TEST(BinanceTradeParserTest, ParsesValidCombinedStreamMessage)
{
    const auto event = BinanceTradeParser::parse(R"json(
{
  "stream": "btcusdt@trade",
  "data": {
    "e": "trade",
    "E": 123456789,
    "s": "BTCUSDT",
    "t": 12345,
    "p": "43012.10",
    "q": "0.5",
    "b": 88,
    "a": 50,
    "T": 123456785,
    "m": false,
    "M": true
  }
}
)json");

    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->symbol, "BTCUSDT");
    EXPECT_EQ(event->trade_id, 12345U);
    EXPECT_FALSE(event->buyer_is_maker);
}

TEST(BinanceTradeParserTest, ReturnsNulloptForMalformedJson)
{
    const auto event = BinanceTradeParser::parse("{not valid json");

    EXPECT_FALSE(event.has_value());
}

TEST(BinanceTradeParserTest, ReturnsNulloptForMissingRequiredField)
{
    const auto event = BinanceTradeParser::parse(R"json(
{
  "e": "trade",
  "s": "BTCUSDT",
  "t": 12345,
  "p": "43012.10",
  "T": 123456785,
  "m": true
}
)json");

    EXPECT_FALSE(event.has_value());
}

TEST(BinanceTradeParserTest, ReturnsNulloptForInvalidPrice)
{
    const auto event = BinanceTradeParser::parse(R"json(
{
  "e": "trade",
  "s": "BTCUSDT",
  "t": 12345,
  "p": "not-a-number",
  "q": "0.5",
  "T": 123456785,
  "m": true
}
)json");

    EXPECT_FALSE(event.has_value());
}

TEST(BinanceTradeParserTest, ReturnsNulloptForInvalidQuantity)
{
    const auto event = BinanceTradeParser::parse(R"json(
{
  "e": "trade",
  "s": "BTCUSDT",
  "t": 12345,
  "p": "43012.10",
  "q": "12.3.4",
  "T": 123456785,
  "m": true
}
)json");

    EXPECT_FALSE(event.has_value());
}

TEST(BinanceTradeParserTest, ReturnsNulloptForUnsupportedEventType)
{
    const auto event = BinanceTradeParser::parse(R"json(
{
  "e": "kline",
  "s": "BTCUSDT",
  "t": 12345,
  "p": "43012.10",
  "q": "0.5",
  "T": 123456785,
  "m": true
}
)json");

    EXPECT_FALSE(event.has_value());
}

TEST(BinanceTradeParserTest, MapsBuyerIsMakerFieldFromSourceMField)
{
    const auto buyer_is_maker_true = BinanceTradeParser::parse(R"json(
{"e":"trade","s":"BTCUSDT","t":1,"p":"1.0","q":"1.0","T":1,"m":true}
)json");
    const auto buyer_is_maker_false = BinanceTradeParser::parse(R"json(
{"e":"trade","s":"BTCUSDT","t":2,"p":"1.0","q":"1.0","T":1,"m":false}
)json");

    ASSERT_TRUE(buyer_is_maker_true.has_value());
    ASSERT_TRUE(buyer_is_maker_false.has_value());
    EXPECT_TRUE(buyer_is_maker_true->buyer_is_maker);
    EXPECT_FALSE(buyer_is_maker_false->buyer_is_maker);
}
