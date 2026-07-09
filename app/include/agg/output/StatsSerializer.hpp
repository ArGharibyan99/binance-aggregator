#pragma once

#include <agg/output/StatsSnapshot.hpp>

#include <string>

namespace agg::output {

/// Formats a StatsSnapshot into the plain-text output format, e.g.:
///
///   timestamp=2026-01-12T14:23:20Z
///   symbol=BTCUSDT trades=154 volume=23.51 min=43012.1 max=43189.4 buy=82 sell=72
///   <blank line>
///
/// The timestamp reflects the exchange-time window start (UTC, ISO-8601),
/// not the time the snapshot happens to be written. A trailing blank line
/// follows the last symbol, separating this block from the next one
/// written to the same output.
class StatsSerializer {
public:
    /// Returns the serialized block for the snapshot (ending in a blank
    /// line), or an empty string if the snapshot has no windows (nothing
    /// to write).
    static std::string serialize(const StatsSnapshot& snapshot);
};

} // namespace agg::output
