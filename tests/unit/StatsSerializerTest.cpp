#include <agg/output/OutputSink.hpp>
#include <agg/output/StatsSerializer.hpp>

#include <gtest/gtest.h>

namespace {

using agg::aggregation::WindowStats;
using agg::output::StatsSerializer;
using agg::output::StatsSnapshot;

WindowStats make_window(
    std::string symbol,
    std::uint64_t trades,
    const std::string& volume,
    const std::string& min_price,
    const std::string& max_price,
    std::uint64_t buy,
    std::uint64_t sell)
{
    WindowStats stats;
    stats.symbol = std::move(symbol);
    stats.trade_count = trades;
    stats.total_volume = agg::util::Decimal::parse(volume);
    stats.min_price = agg::util::Decimal::parse(min_price);
    stats.max_price = agg::util::Decimal::parse(max_price);
    stats.buyer_initiated_count = buy;
    stats.seller_initiated_count = sell;
    return stats;
}

class FakeOutputSink : public agg::output::OutputSink {
public:
    void write(const std::string& text) override { writes.push_back(text); }

    std::vector<std::string> writes;
};

// 2026-01-12T14:23:20Z
constexpr std::uint64_t kExampleWindowStartMs = 1768227800000ULL;

} // namespace

TEST(StatsSerializerTest, FormatsSingleSymbolDeterministically)
{
    StatsSnapshot snapshot;
    snapshot.window_start_ms = kExampleWindowStartMs;
    snapshot.windows.push_back(
        make_window("BTCUSDT", 154, "23.51", "43012.1", "43189.4", 82, 72));

    const auto text = StatsSerializer::serialize(snapshot);

    EXPECT_EQ(text,
        "timestamp=2026-01-12T14:23:20Z\n"
        "symbol=BTCUSDT trades=154 volume=23.51 min=43012.1 max=43189.4 buy=82 sell=72\n");
}

TEST(StatsSerializerTest, FormatsMultipleSymbolsUnderOneTimestamp)
{
    StatsSnapshot snapshot;
    snapshot.window_start_ms = kExampleWindowStartMs;
    snapshot.windows.push_back(
        make_window("BTCUSDT", 154, "23.51", "43012.1", "43189.4", 82, 72));
    snapshot.windows.push_back(
        make_window("ETHUSDT", 231, "112.7", "2289.2", "2301.8", 120, 111));

    const auto text = StatsSerializer::serialize(snapshot);

    EXPECT_EQ(text,
        "timestamp=2026-01-12T14:23:20Z\n"
        "symbol=BTCUSDT trades=154 volume=23.51 min=43012.1 max=43189.4 buy=82 sell=72\n"
        "symbol=ETHUSDT trades=231 volume=112.7 min=2289.2 max=2301.8 buy=120 sell=111\n");
}

TEST(StatsSerializerTest, SkipsEmptyOutputForSnapshotWithNoWindows)
{
    StatsSnapshot snapshot;
    snapshot.window_start_ms = kExampleWindowStartMs;

    EXPECT_TRUE(StatsSerializer::serialize(snapshot).empty());
}

TEST(StatsSerializerTest, WritesSerializedTextThroughSinkVerbatim)
{
    StatsSnapshot snapshot;
    snapshot.window_start_ms = kExampleWindowStartMs;
    snapshot.windows.push_back(
        make_window("BTCUSDT", 1, "1.0", "1.0", "1.0", 1, 0));

    FakeOutputSink sink;
    sink.write(StatsSerializer::serialize(snapshot));

    ASSERT_EQ(sink.writes.size(), 1U);
    EXPECT_EQ(sink.writes[0],
        "timestamp=2026-01-12T14:23:20Z\n"
        "symbol=BTCUSDT trades=1 volume=1 min=1 max=1 buy=1 sell=0\n");
}
