#pragma once

#include <agg/util/Decimal.hpp>

#include <cstdint>
#include <string>

namespace agg::aggregation {

/// Aggregated trade statistics for one symbol over one exchange-time
/// window (see MarketDataAggregator for how windows are assigned).
struct WindowStats {
    std::string symbol;
    std::uint64_t window_start_ms = 0;

    std::uint64_t trade_count = 0;
    agg::util::Decimal total_volume;
    agg::util::Decimal min_price;
    agg::util::Decimal max_price;

    /// Trades where the buyer was the taker (m == false).
    std::uint64_t buyer_initiated_count = 0;
    /// Trades where the seller was the taker, i.e. the buyer was the
    /// market maker (m == true).
    std::uint64_t seller_initiated_count = 0;
};

} // namespace agg::aggregation
