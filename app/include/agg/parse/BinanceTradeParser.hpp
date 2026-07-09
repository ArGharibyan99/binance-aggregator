#pragma once

#include <agg/model/TradeEvent.hpp>

#include <optional>
#include <string_view>

namespace agg::parse {

/// Parses raw Binance trade-stream JSON messages into TradeEvent values.
///
/// Accepts both a single-stream raw trade message and a combined-stream
/// envelope (`{"stream": ..., "data": <raw message>}`). Malformed JSON,
/// missing or invalid fields, and unsupported event types are reported by
/// returning std::nullopt rather than throwing, since a single bad market
/// message must not interrupt the running service.
class BinanceTradeParser {
public:
    static std::optional<agg::model::TradeEvent> parse(std::string_view raw_message);
};

} // namespace agg::parse
