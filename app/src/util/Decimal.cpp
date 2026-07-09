#include <agg/util/Decimal.hpp>

#include <cctype>
#include <stdexcept>

namespace agg::util {

Decimal Decimal::parse(std::string_view text)
{
    if (text.empty()) {
        throw std::invalid_argument("Decimal string must not be empty");
    }

    std::size_t idx = 0;
    bool negative = false;

    if (text[idx] == '+' || text[idx] == '-') {
        negative = (text[idx] == '-');
        ++idx;
    }

    if (idx >= text.size() || std::isdigit(static_cast<unsigned char>(text[idx])) == 0) {
        throw std::invalid_argument("Decimal string must contain an integer part: " + std::string(text));
    }

    std::int64_t integer_part = 0;
    while (idx < text.size() && std::isdigit(static_cast<unsigned char>(text[idx])) != 0) {
        integer_part = integer_part * 10 + (text[idx] - '0');
        ++idx;
    }

    std::int64_t fractional_part = 0;
    int fractional_digits = 0;

    if (idx < text.size() && text[idx] == '.') {
        ++idx;

        if (idx >= text.size() || std::isdigit(static_cast<unsigned char>(text[idx])) == 0) {
            throw std::invalid_argument("Decimal string must have digits after '.': " + std::string(text));
        }

        while (idx < text.size() && std::isdigit(static_cast<unsigned char>(text[idx])) != 0) {
            if (fractional_digits >= kScale) {
                throw std::invalid_argument("Decimal string exceeds supported precision: " + std::string(text));
            }

            fractional_part = fractional_part * 10 + (text[idx] - '0');
            ++fractional_digits;
            ++idx;
        }
    }

    if (idx != text.size()) {
        throw std::invalid_argument("Decimal string contains invalid characters: " + std::string(text));
    }

    for (int i = fractional_digits; i < kScale; ++i) {
        fractional_part *= 10;
    }

    auto raw = integer_part * kScaleFactor + fractional_part;
    if (negative) {
        raw = -raw;
    }

    return Decimal::from_raw(raw);
}

std::string Decimal::to_string() const
{
    const bool negative = raw_ < 0;
    const auto magnitude = static_cast<std::uint64_t>(negative ? -raw_ : raw_);

    const auto integer_part = magnitude / static_cast<std::uint64_t>(kScaleFactor);
    const auto fractional_part = magnitude % static_cast<std::uint64_t>(kScaleFactor);

    auto fractional_str = std::to_string(fractional_part);
    fractional_str.insert(0, kScale - fractional_str.size(), '0');

    while (!fractional_str.empty() && fractional_str.back() == '0') {
        fractional_str.pop_back();
    }

    std::string result = negative ? "-" : "";
    result += std::to_string(integer_part);

    if (!fractional_str.empty()) {
        result += '.';
        result += fractional_str;
    }

    return result;
}

Decimal operator*(const Decimal& lhs, const Decimal& rhs) noexcept
{
    // Two values scaled by kScaleFactor multiply to a product scaled by
    // kScaleFactor^2; divide back down to keep the result at kScale.
    // Truncates rather than rounds, consistent with parse() rejecting
    // input precision beyond kScale digits instead of rounding it.
    //
    // __int128 is a GCC/Clang extension, not ISO C++; this project only
    // targets those compilers on Linux, and it is the simplest correct way
    // to hold the intermediate product of two 64-bit scaled values.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
    const auto product = static_cast<__int128>(lhs.raw()) * static_cast<__int128>(rhs.raw());
    return Decimal::from_raw(static_cast<std::int64_t>(product / Decimal::kScaleFactor));
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

} // namespace agg::util
