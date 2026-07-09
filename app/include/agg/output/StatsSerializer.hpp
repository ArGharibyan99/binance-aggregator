#pragma once

#include <agg/output/StatsSnapshot.hpp>

#include <string>

namespace agg::output {

/// Formats a StatsSnapshot into the plain-text output format, e.g.:
///
///   timestamp=2026-01-12T14:23:20Z
///   symbol=BTCUSDT trades=154 volume=23.51 min=43012.1 max=43189.4 buy=82 sell=72
///
/// The timestamp reflects the exchange-time window start (UTC, ISO-8601),
/// not the time the snapshot happens to be written.
class StatsSerializer {
public:
    /// Returns the serialized block for the snapshot, or an empty string
    /// if the snapshot has no windows (nothing to write).
    static std::string serialize(const StatsSnapshot& snapshot);
};

} // namespace agg::output
