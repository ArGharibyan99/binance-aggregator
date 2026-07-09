#pragma once

#include <agg/util/Decimal.hpp>

#include <cstdint>
#include <string>

namespace agg::model {

/// A single normalized trade event derived from a Binance trade message.
struct TradeEvent {
    std::string symbol;
    std::uint64_t trade_id = 0;
    agg::util::Decimal price;
    agg::util::Decimal quantity;
    std::uint64_t trade_time_ms = 0;

    /// True when the buyer is the market maker, meaning the trade was
    /// seller-initiated; false means the trade was buyer-initiated.
    bool buyer_is_maker = false;
};

} // namespace agg::model
