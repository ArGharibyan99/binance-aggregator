#include <agg/aggregation/MarketDataAggregator.hpp>

#include <gtest/gtest.h>

namespace {

using agg::aggregation::MarketDataAggregator;
using agg::model::TradeEvent;

TradeEvent make_trade(
    std::string symbol,
    std::uint64_t trade_id,
    const std::string& price,
    const std::string& quantity,
    std::uint64_t trade_time_ms,
    bool buyer_is_maker)
{
    TradeEvent trade;
    trade.symbol = std::move(symbol);
    trade.trade_id = trade_id;
    trade.price = agg::util::Decimal::parse(price);
    trade.quantity = agg::util::Decimal::parse(quantity);
    trade.trade_time_ms = trade_time_ms;
    trade.buyer_is_maker = buyer_is_maker;
    return trade;
}

} // namespace

TEST(MarketDataAggregatorTest, OneSymbolAccumulatesStats)
{
    MarketDataAggregator aggregator(1000);

    aggregator.add_trade(make_trade("BTCUSDT", 1, "100.0", "1.0", 1200, false));
    aggregator.add_trade(make_trade("BTCUSDT", 2, "110.0", "2.0", 1500, true));

    const auto stats = aggregator.window_stats("BTCUSDT", 1000);

    ASSERT_TRUE(stats.has_value());
    EXPECT_EQ(stats->symbol, "BTCUSDT");
    EXPECT_EQ(stats->window_start_ms, 1000U);
    EXPECT_EQ(stats->trade_count, 2U);
}

TEST(MarketDataAggregatorTest, MultipleSymbolsAggregateIndependently)
{
    MarketDataAggregator aggregator(1000);

    aggregator.add_trade(make_trade("BTCUSDT", 1, "100.0", "1.0", 1200, false));
    aggregator.add_trade(make_trade("ETHUSDT", 1, "10.0", "5.0", 1300, true));
    aggregator.add_trade(make_trade("ETHUSDT", 2, "12.0", "1.0", 1400, true));

    const auto btc = aggregator.window_stats("BTCUSDT", 1000);
    const auto eth = aggregator.window_stats("ETHUSDT", 1000);

    ASSERT_TRUE(btc.has_value());
    ASSERT_TRUE(eth.has_value());
    EXPECT_EQ(btc->trade_count, 1U);
    EXPECT_EQ(eth->trade_count, 2U);
}

TEST(MarketDataAggregatorTest, BucketsTradesByExchangeTimeWindow)
{
    MarketDataAggregator aggregator(1000);

    aggregator.add_trade(make_trade("BTCUSDT", 1, "100.0", "1.0", 1500, false));
    aggregator.add_trade(make_trade("BTCUSDT", 2, "100.0", "1.0", 2500, false));

    const auto first_window = aggregator.window_stats("BTCUSDT", 1000);
    const auto second_window = aggregator.window_stats("BTCUSDT", 2000);
    const auto missing_window = aggregator.window_stats("BTCUSDT", 3000);

    ASSERT_TRUE(first_window.has_value());
    ASSERT_TRUE(second_window.has_value());
    EXPECT_EQ(first_window->trade_count, 1U);
    EXPECT_EQ(second_window->trade_count, 1U);
    EXPECT_FALSE(missing_window.has_value());
}

TEST(MarketDataAggregatorTest, TracksMinAndMaxPrice)
{
    MarketDataAggregator aggregator(1000);

    aggregator.add_trade(make_trade("BTCUSDT", 1, "100.0", "1.0", 1000, false));
    aggregator.add_trade(make_trade("BTCUSDT", 2, "50.0", "1.0", 1100, false));
    aggregator.add_trade(make_trade("BTCUSDT", 3, "200.0", "1.0", 1200, false));

    const auto stats = aggregator.window_stats("BTCUSDT", 1000);

    ASSERT_TRUE(stats.has_value());
    EXPECT_EQ(stats->min_price.to_string(), "50");
    EXPECT_EQ(stats->max_price.to_string(), "200");
}

TEST(MarketDataAggregatorTest, AccumulatesTotalVolumeAsPriceTimesQuantitySum)
{
    MarketDataAggregator aggregator(1000);

    aggregator.add_trade(make_trade("BTCUSDT", 1, "10.0", "2.0", 1000, false));
    aggregator.add_trade(make_trade("BTCUSDT", 2, "5.0", "3.0", 1100, false));

    const auto stats = aggregator.window_stats("BTCUSDT", 1000);

    ASSERT_TRUE(stats.has_value());
    EXPECT_EQ(stats->total_volume.to_string(), "35");
}

TEST(MarketDataAggregatorTest, CountsBuyerAndSellerInitiatedTrades)
{
    MarketDataAggregator aggregator(1000);

    aggregator.add_trade(make_trade("BTCUSDT", 1, "10.0", "1.0", 1000, false));
    aggregator.add_trade(make_trade("BTCUSDT", 2, "10.0", "1.0", 1100, false));
    aggregator.add_trade(make_trade("BTCUSDT", 3, "10.0", "1.0", 1200, true));

    const auto stats = aggregator.window_stats("BTCUSDT", 1000);

    ASSERT_TRUE(stats.has_value());
    EXPECT_EQ(stats->buyer_initiated_count, 2U);
    EXPECT_EQ(stats->seller_initiated_count, 1U);
}

TEST(MarketDataAggregatorTest, EmptyWindowIsNeverCreated)
{
    MarketDataAggregator aggregator(1000);

    EXPECT_FALSE(aggregator.window_stats("BTCUSDT", 1000).has_value());
    EXPECT_TRUE(aggregator.extract_all_windows().empty());
}

// Late-trade behavior is intentionally simple for now: a trade for a
// window that was already extracted (flushed) does not get merged back
// into history. It starts a brand-new bucket containing only itself,
// which will appear as a second, partial window on the next extraction.
TEST(MarketDataAggregatorTest, LateTradeAfterExtractionStartsFreshWindow)
{
    MarketDataAggregator aggregator(1000);

    aggregator.add_trade(make_trade("BTCUSDT", 1, "100.0", "1.0", 1000, false));
    auto first_flush = aggregator.extract_all_windows();
    ASSERT_EQ(first_flush.size(), 1U);
    EXPECT_EQ(first_flush[0].trade_count, 1U);

    aggregator.add_trade(make_trade("BTCUSDT", 2, "200.0", "1.0", 1000, false));
    auto second_flush = aggregator.extract_all_windows();

    ASSERT_EQ(second_flush.size(), 1U);
    EXPECT_EQ(second_flush[0].trade_count, 1U);
    EXPECT_EQ(second_flush[0].total_volume.to_string(), "200");
}

TEST(MarketDataAggregatorTest, ExtractCompletedWindowsOnlyReturnsWindowsBeforeWatermark)
{
    MarketDataAggregator aggregator(1000);

    aggregator.add_trade(make_trade("BTCUSDT", 1, "100.0", "1.0", 1000, false));
    aggregator.add_trade(make_trade("BTCUSDT", 2, "100.0", "1.0", 2000, false));
    aggregator.add_trade(make_trade("BTCUSDT", 3, "100.0", "1.0", 3000, false));

    // Watermark of 3000 means the window starting at 3000 is still "now"
    // and must be withheld; only 1000 and 2000 are strictly before it.
    auto completed = aggregator.extract_completed_windows(3000);

    ASSERT_EQ(completed.size(), 2U);
    EXPECT_EQ(completed[0].window_start_ms, 1000U);
    EXPECT_EQ(completed[1].window_start_ms, 2000U);

    EXPECT_FALSE(aggregator.window_stats("BTCUSDT", 1000).has_value());
    EXPECT_FALSE(aggregator.window_stats("BTCUSDT", 2000).has_value());
    ASSERT_TRUE(aggregator.window_stats("BTCUSDT", 3000).has_value());
    EXPECT_EQ(aggregator.window_stats("BTCUSDT", 3000)->trade_count, 1U);
}

TEST(MarketDataAggregatorTest, ExtractCompletedWindowsLeavesCurrentWindowAcrossSymbols)
{
    MarketDataAggregator aggregator(1000);

    aggregator.add_trade(make_trade("BTCUSDT", 1, "100.0", "1.0", 1000, false));
    aggregator.add_trade(make_trade("ETHUSDT", 1, "10.0", "1.0", 2000, false));

    auto completed = aggregator.extract_completed_windows(2000);

    ASSERT_EQ(completed.size(), 1U);
    EXPECT_EQ(completed[0].symbol, "BTCUSDT");

    ASSERT_TRUE(aggregator.window_stats("ETHUSDT", 2000).has_value());
}

TEST(MarketDataAggregatorTest, ExtractCompletedWindowsReturnsNothingWhenAllWindowsAreCurrent)
{
    MarketDataAggregator aggregator(1000);

    aggregator.add_trade(make_trade("BTCUSDT", 1, "100.0", "1.0", 5000, false));

    EXPECT_TRUE(aggregator.extract_completed_windows(5000).empty());
    ASSERT_TRUE(aggregator.window_stats("BTCUSDT", 5000).has_value());
}
