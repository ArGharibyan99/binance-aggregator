#include <agg/util/Decimal.hpp>

#include <gtest/gtest.h>

#include <stdexcept>

namespace {
using agg::util::Decimal;
}

TEST(DecimalTest, ParsesIntegerValue)
{
    const auto value = Decimal::parse("1000");
    EXPECT_EQ(value.to_string(), "1000");
}

TEST(DecimalTest, ParsesFractionalValue)
{
    const auto value = Decimal::parse("43012.10");
    EXPECT_EQ(value.to_string(), "43012.1");
}

TEST(DecimalTest, ParsesMaxPrecisionValue)
{
    const auto value = Decimal::parse("0.00000001");
    EXPECT_EQ(value.to_string(), "0.00000001");
}

TEST(DecimalTest, FormatsTrimmedTrailingZeros)
{
    const auto value = Decimal::parse("23.510000");
    EXPECT_EQ(value.to_string(), "23.51");
}

TEST(DecimalTest, MultipliesPriceByQuantity)
{
    const auto price = Decimal::parse("43012.10");
    const auto quantity = Decimal::parse("0.5");

    const auto volume = price * quantity;

    EXPECT_EQ(volume.to_string(), "21506.05");
}

TEST(DecimalTest, AddsVolumesAtSameScale)
{
    const auto a = Decimal::parse("1.5");
    const auto b = Decimal::parse("2.25");

    EXPECT_EQ((a + b).to_string(), "3.75");
}

TEST(DecimalTest, RejectsEmptyString)
{
    EXPECT_THROW(Decimal::parse(""), std::invalid_argument);
}

TEST(DecimalTest, RejectsNonNumericString)
{
    EXPECT_THROW(Decimal::parse("abc"), std::invalid_argument);
}

TEST(DecimalTest, RejectsTrailingDotWithNoDigits)
{
    EXPECT_THROW(Decimal::parse("12."), std::invalid_argument);
}

TEST(DecimalTest, RejectsLeadingDotWithNoIntegerPart)
{
    EXPECT_THROW(Decimal::parse(".5"), std::invalid_argument);
}

TEST(DecimalTest, RejectsPrecisionBeyondScale)
{
    EXPECT_THROW(Decimal::parse("0.123456789"), std::invalid_argument);
}

TEST(DecimalTest, RejectsTrailingGarbageCharacters)
{
    EXPECT_THROW(Decimal::parse("1.5x"), std::invalid_argument);
}
