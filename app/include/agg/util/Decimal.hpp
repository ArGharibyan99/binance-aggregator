#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

namespace agg::util {

/// Fixed-point decimal value with 8 fractional digits.
///
/// Prices, quantities, and accumulated volume are represented with this
/// type instead of `double` so that repeated parsing, formatting, and
/// summation of Binance trade data stays exact.
class Decimal {
public:
    /// Number of fractional digits represented.
    static constexpr int kScale = 8;
    static constexpr std::int64_t kScaleFactor = 100'000'000;

    constexpr Decimal() noexcept = default;

    /// Constructs a Decimal from an already-scaled integer value.
    static constexpr Decimal from_raw(std::int64_t raw) noexcept
    {
        Decimal value;
        value.raw_ = raw;
        return value;
    }

    /// Parses a plain decimal string such as "43012.10".
    /// Throws std::invalid_argument if the string is empty, malformed, or
    /// has more than kScale fractional digits.
    static Decimal parse(std::string_view text);

    /// Formats the value with trailing fractional zeros trimmed, e.g. the
    /// value parsed from "43012.10" formats back as "43012.1".
    std::string to_string() const;

    constexpr std::int64_t raw() const noexcept { return raw_; }

    constexpr bool operator==(const Decimal& other) const noexcept = default;
    constexpr auto operator<=>(const Decimal& other) const noexcept = default;

    friend constexpr Decimal operator+(const Decimal& lhs, const Decimal& rhs) noexcept
    {
        return Decimal::from_raw(lhs.raw_ + rhs.raw_);
    }

    friend Decimal operator*(const Decimal& lhs, const Decimal& rhs) noexcept;

private:
    std::int64_t raw_ = 0;
};

} // namespace agg::util
