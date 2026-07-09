#include <agg/output/FileOutputSink.hpp>
#include <agg/runtime/MarketDataPipeline.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::filesystem::path make_temp_file_path(const std::string& name)
{
    const auto unique =
        std::chrono::steady_clock::now().time_since_epoch().count();

    return std::filesystem::temp_directory_path() /
        ("binance_aggregator_pipeline_" + name + "_" + std::to_string(unique) + ".log");
}

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

} // namespace

// Feeds a fixed set of fixture messages (no real Binance connection) through
// the full parser -> aggregation -> writer pipeline and checks the exact
// file contents produced. The flush interval is set far longer than the
// test itself so the periodic writer timer never fires; stop() performs
// its own deterministic final flush, which is what this test relies on to
// avoid timing-dependent behavior.
TEST(MarketDataPipelineIntegrationTest, ProcessesFixtureMessagesIntoExpectedOutput)
{
    const auto output_path = make_temp_file_path("output");
    agg::output::FileOutputSink sink(output_path);

    agg::runtime::MarketDataPipeline pipeline(
        /*window_ms=*/1000,
        /*flush_interval_ms=*/60000,
        /*queue_capacity=*/16,
        sink);

    pipeline.start();

    // Two BTCUSDT trades and one ETHUSDT trade, all in exchange-time
    // window [1000, 2000).
    ASSERT_TRUE(pipeline.submit_raw_message(R"json(
{"e":"trade","s":"BTCUSDT","t":1,"p":"100.0","q":"1.0","T":1200,"m":false}
)json"));

    ASSERT_TRUE(pipeline.submit_raw_message(R"json(
{"stream":"ethusdt@trade","data":{"e":"trade","s":"ETHUSDT","t":1,"p":"10.0","q":"5.0","T":1300,"m":false}}
)json"));

    ASSERT_TRUE(pipeline.submit_raw_message(R"json(
{"e":"trade","s":"BTCUSDT","t":2,"p":"110.0","q":"2.0","T":1500,"m":true}
)json"));

    // Malformed / unsupported messages must be dropped without disrupting
    // the pipeline.
    ASSERT_TRUE(pipeline.submit_raw_message("{not valid json"));
    ASSERT_TRUE(pipeline.submit_raw_message(R"json(
{"e":"kline","s":"BTCUSDT","t":3,"p":"999.0","q":"1.0","T":1600,"m":false}
)json"));

    pipeline.stop();

    EXPECT_EQ(read_file(output_path),
        "timestamp=1970-01-01T00:00:01Z\n"
        "symbol=BTCUSDT trades=2 volume=320 min=100 max=110 buy=1 sell=1\n"
        "symbol=ETHUSDT trades=1 volume=50 min=10 max=10 buy=1 sell=0\n"
        "\n");

    std::filesystem::remove(output_path);
}

// Demonstrates the fix for the "duplicate timestamp" issue: a trade whose
// window is still the currently-in-progress one (by wall clock) must not
// be written out by a periodic flush tick, since a further trade for that
// same window could still arrive. It should only appear once the pipeline
// is stopped (the final flush is unconditional, since no more trades can
// arrive after that).
TEST(MarketDataPipelineIntegrationTest, PeriodicFlushWithholdsCurrentWindowUntilStop)
{
    const auto output_path = make_temp_file_path("withhold");
    agg::output::FileOutputSink sink(output_path);

    constexpr std::uint64_t kWindowMs = 1000;

    const auto wall_clock_ms = [] {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
    };

    // Align to just past the start of a fresh window, so the trade below
    // has close to a full window_ms of margin before its window naturally
    // closes -- otherwise, if the test happened to start near the end of
    // a second, the window could close for real before the assertions
    // below run, making the test flaky.
    const auto ms_into_window = wall_clock_ms() % kWindowMs;
    std::this_thread::sleep_for(std::chrono::milliseconds((kWindowMs - ms_into_window) + 20));

    const auto trade_time_ms = wall_clock_ms();

    agg::runtime::MarketDataPipeline pipeline(
        kWindowMs,
        /*flush_interval_ms=*/50,
        /*queue_capacity=*/16,
        sink);

    pipeline.start();

    const auto message = "{\"e\":\"trade\",\"s\":\"BTCUSDT\",\"t\":1,\"p\":\"100.0\",\"q\":\"1.0\",\"T\":"
        + std::to_string(trade_time_ms) + ",\"m\":false}";
    ASSERT_TRUE(pipeline.submit_raw_message(message));

    // Let several periodic flush ticks (every 50ms) happen while the
    // window is still current.
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    EXPECT_TRUE(read_file(output_path).empty());

    pipeline.stop();

    EXPECT_FALSE(read_file(output_path).empty());

    std::filesystem::remove(output_path);
}

// Sharding must be behavior-preserving: the same fixture messages fed
// through a single-shard pipeline and a multi-shard pipeline must
// produce byte-for-byte identical output (write_windows() sorts each
// window's symbols so this holds regardless of which shard a symbol
// happened to land in).
TEST(MarketDataPipelineIntegrationTest, ShardedAggregationProducesIdenticalOutputToSingleShard)
{
    const std::vector<std::string> messages = {
        R"json({"e":"trade","s":"BTCUSDT","t":1,"p":"100.0","q":"1.0","T":1200,"m":false})json",
        R"json({"e":"trade","s":"ETHUSDT","t":1,"p":"10.0","q":"5.0","T":1300,"m":false})json",
        R"json({"e":"trade","s":"BTCUSDT","t":2,"p":"110.0","q":"2.0","T":1500,"m":true})json",
        R"json({"e":"trade","s":"BNBUSDT","t":1,"p":"300.0","q":"1.0","T":1250,"m":true})json",
        R"json({"e":"trade","s":"ETHUSDT","t":2,"p":"11.0","q":"1.0","T":1600,"m":false})json",
        R"json({"e":"trade","s":"SOLUSDT","t":1,"p":"20.0","q":"4.0","T":1700,"m":false})json",
    };

    const auto run_pipeline = [&](std::size_t aggregator_threads, const std::string& name) {
        const auto output_path = make_temp_file_path(name);
        agg::output::FileOutputSink sink(output_path);

        agg::runtime::MarketDataPipeline pipeline(
            /*window_ms=*/1000,
            /*flush_interval_ms=*/60000,
            /*queue_capacity=*/16,
            sink,
            aggregator_threads);

        pipeline.start();
        for (const auto& message : messages) {
            EXPECT_TRUE(pipeline.submit_raw_message(message));
        }
        pipeline.stop();

        const auto contents = read_file(output_path);
        std::filesystem::remove(output_path);
        return contents;
    };

    const auto single_shard_output = run_pipeline(1, "single_shard");
    const auto multi_shard_output = run_pipeline(4, "multi_shard");

    EXPECT_FALSE(single_shard_output.empty());
    EXPECT_EQ(single_shard_output, multi_shard_output);
}

// A single symbol's trades must always route to the same shard, so they
// stay together in one WindowStats entry no matter how many shards are
// configured -- not split into multiple partial entries.
TEST(MarketDataPipelineIntegrationTest, SingleSymbolTradesNeverSplitAcrossShards)
{
    const auto output_path = make_temp_file_path("single_symbol_many_shards");
    agg::output::FileOutputSink sink(output_path);

    agg::runtime::MarketDataPipeline pipeline(
        /*window_ms=*/1000,
        /*flush_interval_ms=*/60000,
        /*queue_capacity=*/16,
        sink,
        /*aggregator_threads=*/8);

    pipeline.start();

    for (int i = 0; i < 20; ++i) {
        const auto message = "{\"e\":\"trade\",\"s\":\"BTCUSDT\",\"t\":" + std::to_string(i)
            + ",\"p\":\"100.0\",\"q\":\"1.0\",\"T\":1200,\"m\":" + (i % 2 == 0 ? "true" : "false") + "}";
        ASSERT_TRUE(pipeline.submit_raw_message(message));
    }

    pipeline.stop();

    EXPECT_EQ(read_file(output_path),
        "timestamp=1970-01-01T00:00:01Z\n"
        "symbol=BTCUSDT trades=20 volume=2000 min=100 max=100 buy=10 sell=10\n"
        "\n");

    std::filesystem::remove(output_path);
}

TEST(MarketDataPipelineIntegrationTest, StopWithoutAnyMessagesProducesNoOutput)
{
    const auto output_path = make_temp_file_path("empty");
    agg::output::FileOutputSink sink(output_path);

    agg::runtime::MarketDataPipeline pipeline(
        /*window_ms=*/1000,
        /*flush_interval_ms=*/60000,
        /*queue_capacity=*/16,
        sink);

    pipeline.start();
    pipeline.stop();

    EXPECT_TRUE(read_file(output_path).empty());

    std::filesystem::remove(output_path);
}
