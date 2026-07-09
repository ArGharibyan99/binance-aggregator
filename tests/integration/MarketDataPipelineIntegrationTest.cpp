#include <agg/output/FileOutputSink.hpp>
#include <agg/runtime/MarketDataPipeline.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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
        "symbol=ETHUSDT trades=1 volume=50 min=10 max=10 buy=1 sell=0\n");

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
