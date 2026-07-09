#pragma once

#include <agg/aggregation/WindowStats.hpp>
#include <agg/model/TradeEvent.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace agg::aggregation {

/// Buckets trades per symbol into fixed-size exchange-time windows and
/// accumulates per-window statistics.
///
/// Window membership is decided purely by each trade's own exchange
/// timestamp (`window_start_ms = trade_time_ms - trade_time_ms % window_ms`),
/// not by wall-clock arrival time. A trade that arrives late, after its
/// window has already been extracted and removed via extract_all_windows(),
/// is aggregated into a freshly created bucket for that same window rather
/// than being merged with the previously extracted stats or dropped. This
/// keeps the aggregator simple for now, at the cost of the late trade
/// producing a second, partial window on the next extraction.
class MarketDataAggregator {
public:
    explicit MarketDataAggregator(std::uint64_t window_ms) noexcept;

    /// Adds a trade to the bucket for its exchange-time window, creating
    /// the bucket if this is the first trade seen for it.
    void add_trade(const agg::model::TradeEvent& trade);

    /// Returns the stats accumulated so far for a symbol's window, or
    /// std::nullopt if no trade has been added for it. Windows are only
    /// ever created on-demand by add_trade, so an empty window can never
    /// be observed here.
    std::optional<WindowStats> window_stats(const std::string& symbol, std::uint64_t window_start_ms) const;

    /// Removes and returns every window currently held, across all
    /// symbols, in unspecified order.
    std::vector<WindowStats> extract_all_windows();

    /// Removes and returns every window whose window_start_ms is
    /// strictly before current_window_start_ms, leaving windows at or
    /// after it untouched. Used for periodic flushing: by withholding
    /// the window that is still in progress "now", it is not flushed
    /// (and thus not split by a subsequent late trade) while it could
    /// still receive more trades under normal message delay.
    std::vector<WindowStats> extract_completed_windows(std::uint64_t current_window_start_ms);

private:
    std::uint64_t window_ms_;
    std::map<std::string, std::map<std::uint64_t, WindowStats>> windows_;
};

} // namespace agg::aggregation
