#include <agg/output/FileOutputSink.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::filesystem::path make_temp_file_path(const std::string& name)
{
    const auto unique =
        std::chrono::steady_clock::now().time_since_epoch().count();

    return std::filesystem::temp_directory_path() /
        ("binance_aggregator_" + name + "_" + std::to_string(unique) + ".log");
}

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

} // namespace

TEST(FileOutputSinkTest, WritesTextToFile)
{
    const auto path = make_temp_file_path("write");

    {
        agg::output::FileOutputSink sink(path);
        sink.write("timestamp=2026-01-12T14:23:20Z\n");
        sink.write("symbol=BTCUSDT trades=1 volume=1 min=1 max=1 buy=1 sell=0\n");
    }

    EXPECT_EQ(read_file(path),
        "timestamp=2026-01-12T14:23:20Z\n"
        "symbol=BTCUSDT trades=1 volume=1 min=1 max=1 buy=1 sell=0\n");

    std::filesystem::remove(path);
}

TEST(FileOutputSinkTest, ThrowsWhenFileCannotBeOpened)
{
    const auto path = std::filesystem::path("/nonexistent_binance_aggregator_dir/output.log");

    EXPECT_THROW(agg::output::FileOutputSink sink(path), std::runtime_error);
}
