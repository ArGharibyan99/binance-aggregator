#pragma once

#include <agg/aggregation/WindowStats.hpp>

#include <cstdint>
#include <vector>

namespace agg::output {

/// One flushed group of window statistics, all sharing the same
/// exchange-time window start, ready to be serialized to a single
/// timestamped block of output lines.
struct StatsSnapshot {
    std::uint64_t window_start_ms = 0;
    std::vector<agg::aggregation::WindowStats> windows;
};

} // namespace agg::output
