#include <agg/output/StatsSerializer.hpp>

#include <ctime>

namespace agg::output {
namespace {

// Uses std::gmtime rather than the POSIX gmtime_r: it is not thread-safe,
// but the runtime only ever has a single writer thread producing output,
// so the shared static buffer is never accessed concurrently.
std::string format_iso8601_utc(std::uint64_t epoch_ms)
{
    const auto seconds = static_cast<std::time_t>(epoch_ms / 1000);
    const std::tm* tm_utc = std::gmtime(&seconds);

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", tm_utc);

    return std::string(buffer);
}

} // namespace

std::string StatsSerializer::serialize(const StatsSnapshot& snapshot)
{
    if (snapshot.windows.empty()) {
        return {};
    }

    std::string output = "timestamp=" + format_iso8601_utc(snapshot.window_start_ms) + "\n";

    for (const auto& window : snapshot.windows) {
        output += "symbol=" + window.symbol
            + " trades=" + std::to_string(window.trade_count)
            + " volume=" + window.total_volume.to_string()
            + " min=" + window.min_price.to_string()
            + " max=" + window.max_price.to_string()
            + " buy=" + std::to_string(window.buyer_initiated_count)
            + " sell=" + std::to_string(window.seller_initiated_count)
            + "\n";
    }

    // Blank line after the last symbol, separating this block from the
    // next one written to the same output.
    output += "\n";

    return output;
}

} // namespace agg::output
