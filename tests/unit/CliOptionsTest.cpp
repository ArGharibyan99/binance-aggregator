#include <agg/runtime/CliOptions.hpp>

#include <gtest/gtest.h>

#include <stdexcept>

TEST(CliOptionsTest, DefaultsToConfigJsonWhenNoArgsGiven)
{
    char program_name[] = "binance_aggregator";
    char* argv[] = { program_name };

    const auto options = agg::runtime::parse_cli_options(1, argv);

    EXPECT_EQ(options.config_path, "config/config.json");
}

TEST(CliOptionsTest, ParsesSeparateConfigFlagAndValue)
{
    char program_name[] = "binance_aggregator";
    char flag[] = "--config";
    char value[] = "custom.json";
    char* argv[] = { program_name, flag, value };

    const auto options = agg::runtime::parse_cli_options(3, argv);

    EXPECT_EQ(options.config_path, "custom.json");
}

TEST(CliOptionsTest, ParsesEqualsStyleConfigFlag)
{
    char program_name[] = "binance_aggregator";
    char flag[] = "--config=custom.json";
    char* argv[] = { program_name, flag };

    const auto options = agg::runtime::parse_cli_options(2, argv);

    EXPECT_EQ(options.config_path, "custom.json");
}

TEST(CliOptionsTest, ThrowsWhenConfigFlagMissingValue)
{
    char program_name[] = "binance_aggregator";
    char flag[] = "--config";
    char* argv[] = { program_name, flag };

    EXPECT_THROW(agg::runtime::parse_cli_options(2, argv), std::invalid_argument);
}
