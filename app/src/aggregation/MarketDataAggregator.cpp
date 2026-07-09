#include <agg/aggregation/MarketDataAggregator.hpp>

#include <utility>

namespace agg::aggregation {

MarketDataAggregator::MarketDataAggregator(std::uint64_t window_ms) noexcept
    : window_ms_(window_ms)
{
}

void MarketDataAggregator::add_trade(const agg::model::TradeEvent& trade)
{
    const auto window_start_ms = trade.trade_time_ms - (trade.trade_time_ms % window_ms_);

    auto& symbol_windows = windows_[trade.symbol];
    const auto [it, inserted] = symbol_windows.try_emplace(window_start_ms);
    WindowStats& stats = it->second;

    if (inserted) {
        stats.symbol = trade.symbol;
        stats.window_start_ms = window_start_ms;
        stats.min_price = trade.price;
        stats.max_price = trade.price;
    } else {
        if (trade.price < stats.min_price) {
            stats.min_price = trade.price;
        }
        if (trade.price > stats.max_price) {
            stats.max_price = trade.price;
        }
    }

    stats.trade_count += 1;
    stats.total_volume = stats.total_volume + (trade.price * trade.quantity);

    if (trade.buyer_is_maker) {
        stats.seller_initiated_count += 1;
    } else {
        stats.buyer_initiated_count += 1;
    }
}

std::optional<WindowStats> MarketDataAggregator::window_stats(
    const std::string& symbol, std::uint64_t window_start_ms) const
{
    const auto symbol_it = windows_.find(symbol);
    if (symbol_it == windows_.end()) {
        return std::nullopt;
    }

    const auto window_it = symbol_it->second.find(window_start_ms);
    if (window_it == symbol_it->second.end()) {
        return std::nullopt;
    }

    return window_it->second;
}

std::vector<WindowStats> MarketDataAggregator::extract_all_windows()
{
    std::vector<WindowStats> result;

    for (auto& [symbol, symbol_windows] : windows_) {
        for (auto& [window_start_ms, stats] : symbol_windows) {
            result.push_back(std::move(stats));
        }
    }

    windows_.clear();

    return result;
}

std::vector<WindowStats> MarketDataAggregator::extract_completed_windows(std::uint64_t current_window_start_ms)
{
    std::vector<WindowStats> result;

    for (auto symbol_it = windows_.begin(); symbol_it != windows_.end();) {
        auto& symbol_windows = symbol_it->second;

        for (auto window_it = symbol_windows.begin(); window_it != symbol_windows.end();) {
            if (window_it->first < current_window_start_ms) {
                result.push_back(std::move(window_it->second));
                window_it = symbol_windows.erase(window_it);
            } else {
                ++window_it;
            }
        }

        if (symbol_windows.empty()) {
            symbol_it = windows_.erase(symbol_it);
        } else {
            ++symbol_it;
        }
    }

    return result;
}

} // namespace agg::aggregation
