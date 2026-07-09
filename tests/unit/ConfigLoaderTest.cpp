#include <agg/config/ConfigLoader.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path make_temp_config_path(const std::string& name)
{
    const auto unique =
        std::chrono::steady_clock::now().time_since_epoch().count();

    return std::filesystem::temp_directory_path() /
        ("binance_aggregator_" + name + "_" + std::to_string(unique) + ".json");
}

void write_text_file(const std::filesystem::path& path, const std::string& body)
{
    std::ofstream output(path);
    ASSERT_TRUE(output.is_open());
    output << body;
}

} // namespace

TEST(ConfigLoaderTest, LoadsValidConfig)
{
    const auto path = make_temp_config_path("valid");

    write_text_file(path, R"json(
{
  "symbols": ["BTCUSDT", "ETHUSDT"],
  "window_ms": 1000,
  "flush_interval_ms": 2000,
  "output_file": "market_stats.log",
  "ws_host": "stream.binance.com",
  "ws_port": "9443",
  "reconnect": {
    "initial_backoff_ms": 500,
    "max_backoff_ms": 30000,
    "jitter_ratio": 0.2
  }
}
)json");

    const auto config = agg::config::ConfigLoader::load_from_file(path);

    EXPECT_EQ(config.symbols.size(), 2U);
    EXPECT_EQ(config.symbols[0], "BTCUSDT");
    EXPECT_EQ(config.symbols[1], "ETHUSDT");
    EXPECT_EQ(config.window_ms, 1000U);
    EXPECT_EQ(config.flush_interval_ms, 2000U);
    EXPECT_EQ(config.aggregator_threads, 1U);
    EXPECT_EQ(config.output_file, "market_stats.log");
    EXPECT_EQ(config.ws_host, "stream.binance.com");
    EXPECT_EQ(config.ws_port, "9443");
    EXPECT_EQ(config.reconnect.initial_backoff_ms, 500U);
    EXPECT_EQ(config.reconnect.max_backoff_ms, 30000U);
    EXPECT_DOUBLE_EQ(config.reconnect.jitter_ratio, 0.2);

    std::filesystem::remove(path);
}

TEST(ConfigLoaderTest, NormalizesSymbolsToUppercase)
{
    const auto path = make_temp_config_path("lowercase_symbols");

    write_text_file(path, R"json(
{
  "symbols": ["btcusdt", "ethusdt"],
  "window_ms": 1000,
  "flush_interval_ms": 1000,
  "output_file": "market_stats.log",
  "ws_host": "stream.binance.com",
  "ws_port": "9443"
}
)json");

    const auto config = agg::config::ConfigLoader::load_from_file(path);

    ASSERT_EQ(config.symbols.size(), 2U);
    EXPECT_EQ(config.symbols[0], "BTCUSDT");
    EXPECT_EQ(config.symbols[1], "ETHUSDT");

    std::filesystem::remove(path);
}

TEST(ConfigLoaderTest, RejectsMissingSymbols)
{
    const auto path = make_temp_config_path("missing_symbols");

    write_text_file(path, R"json(
{
  "window_ms": 1000,
  "flush_interval_ms": 1000,
  "output_file": "market_stats.log",
  "ws_host": "stream.binance.com",
  "ws_port": "9443"
}
)json");

    EXPECT_THROW(
        agg::config::ConfigLoader::load_from_file(path),
        std::runtime_error
    );

    std::filesystem::remove(path);
}

TEST(ConfigLoaderTest, RejectsEmptySymbols)
{
    const auto path = make_temp_config_path("empty_symbols");

    write_text_file(path, R"json(
{
  "symbols": [],
  "window_ms": 1000,
  "flush_interval_ms": 1000,
  "output_file": "market_stats.log",
  "ws_host": "stream.binance.com",
  "ws_port": "9443"
}
)json");

    EXPECT_THROW(
        agg::config::ConfigLoader::load_from_file(path),
        std::runtime_error
    );

    std::filesystem::remove(path);
}

TEST(ConfigLoaderTest, RejectsZeroWindow)
{
    const auto path = make_temp_config_path("zero_window");

    write_text_file(path, R"json(
{
  "symbols": ["BTCUSDT"],
  "window_ms": 0,
  "flush_interval_ms": 1000,
  "output_file": "market_stats.log",
  "ws_host": "stream.binance.com",
  "ws_port": "9443"
}
)json");

    EXPECT_THROW(
        agg::config::ConfigLoader::load_from_file(path),
        std::runtime_error
    );

    std::filesystem::remove(path);
}

TEST(ConfigLoaderTest, ParsesExplicitAggregatorThreads)
{
    const auto path = make_temp_config_path("aggregator_threads");

    write_text_file(path, R"json(
{
  "symbols": ["BTCUSDT"],
  "window_ms": 1000,
  "flush_interval_ms": 1000,
  "aggregator_threads": 4,
  "output_file": "market_stats.log",
  "ws_host": "stream.binance.com",
  "ws_port": "9443"
}
)json");

    const auto config = agg::config::ConfigLoader::load_from_file(path);

    EXPECT_EQ(config.aggregator_threads, 4U);

    std::filesystem::remove(path);
}

TEST(ConfigLoaderTest, RejectsZeroAggregatorThreads)
{
    const auto path = make_temp_config_path("zero_aggregator_threads");

    write_text_file(path, R"json(
{
  "symbols": ["BTCUSDT"],
  "window_ms": 1000,
  "flush_interval_ms": 1000,
  "aggregator_threads": 0,
  "output_file": "market_stats.log",
  "ws_host": "stream.binance.com",
  "ws_port": "9443"
}
)json");

    EXPECT_THROW(
        agg::config::ConfigLoader::load_from_file(path),
        std::runtime_error
    );

    std::filesystem::remove(path);
}

TEST(ConfigLoaderTest, RejectsInvalidReconnectBackoff)
{
    const auto path = make_temp_config_path("invalid_reconnect");

    write_text_file(path, R"json(
{
  "symbols": ["BTCUSDT"],
  "window_ms": 1000,
  "flush_interval_ms": 1000,
  "output_file": "market_stats.log",
  "ws_host": "stream.binance.com",
  "ws_port": "9443",
  "reconnect": {
    "initial_backoff_ms": 30000,
    "max_backoff_ms": 500
  }
}
)json");

    EXPECT_THROW(
        agg::config::ConfigLoader::load_from_file(path),
        std::runtime_error
    );

    std::filesystem::remove(path);
}