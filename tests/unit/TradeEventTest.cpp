#include <agg/model/TradeEvent.hpp>

#include <gtest/gtest.h>

TEST(TradeEventTest, ConstructsWithExpectedFields)
{
    agg::model::TradeEvent event;
    event.symbol = "BTCUSDT";
    event.trade_id = 12345;
    event.price = agg::util::Decimal::parse("43012.10");
    event.quantity = agg::util::Decimal::parse("0.5");
    event.trade_time_ms = 1700000000000ULL;
    event.buyer_is_maker = true;

    EXPECT_EQ(event.symbol, "BTCUSDT");
    EXPECT_EQ(event.trade_id, 12345U);
    EXPECT_EQ(event.price.to_string(), "43012.1");
    EXPECT_EQ(event.quantity.to_string(), "0.5");
    EXPECT_EQ(event.trade_time_ms, 1700000000000ULL);
    EXPECT_TRUE(event.buyer_is_maker);
}

TEST(TradeEventTest, DefaultConstructedFieldsAreZeroed)
{
    agg::model::TradeEvent event;

    EXPECT_TRUE(event.symbol.empty());
    EXPECT_EQ(event.trade_id, 0U);
    EXPECT_EQ(event.price.raw(), 0);
    EXPECT_EQ(event.quantity.raw(), 0);
    EXPECT_EQ(event.trade_time_ms, 0U);
    EXPECT_FALSE(event.buyer_is_maker);
}
