#include <agg/parse/BinanceTradeParser.hpp>

#include <agg/util/Decimal.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace agg::parse {
namespace {

using json = nlohmann::json;

const json& unwrap_payload(const json& root)
{
    if (root.contains("data")) {
        return root["data"];
    }

    return root;
}

bool has_required_fields(const json& payload)
{
    return payload.contains("s") && payload["s"].is_string()
        && payload.contains("t") && payload["t"].is_number_unsigned()
        && payload.contains("p") && payload["p"].is_string()
        && payload.contains("q") && payload["q"].is_string()
        && payload.contains("T") && payload["T"].is_number_unsigned()
        && payload.contains("m") && payload["m"].is_boolean();
}

bool is_unsupported_event_type(const json& payload)
{
    return payload.contains("e") && payload["e"].is_string()
        && payload["e"].get<std::string>() != "trade";
}

} // namespace

std::optional<agg::model::TradeEvent> BinanceTradeParser::parse(std::string_view raw_message)
{
    json root;

    try {
        root = json::parse(raw_message);
    } catch (const json::exception&) {
        return std::nullopt;
    }

    if (!root.is_object()) {
        return std::nullopt;
    }

    const json& payload = unwrap_payload(root);

    if (!payload.is_object() || is_unsupported_event_type(payload) || !has_required_fields(payload)) {
        return std::nullopt;
    }

    agg::model::TradeEvent event;
    event.symbol = payload["s"].get<std::string>();
    event.trade_id = payload["t"].get<std::uint64_t>();
    event.trade_time_ms = payload["T"].get<std::uint64_t>();
    event.buyer_is_maker = payload["m"].get<bool>();

    try {
        event.price = agg::util::Decimal::parse(payload["p"].get<std::string>());
        event.quantity = agg::util::Decimal::parse(payload["q"].get<std::string>());
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }

    return event;
}

} // namespace agg::parse
