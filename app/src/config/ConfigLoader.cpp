#include <agg/config/ConfigLoader.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>

namespace agg::config {
namespace {

using json = nlohmann::json;

std::string to_upper_ascii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });

    return value;
}

bool is_valid_symbol_char(char ch)
{
    const auto uch = static_cast<unsigned char>(ch);
    return std::isalnum(uch) != 0;
}

void require_field(const json& root, const char* field)
{
    if (!root.contains(field)) {
        throw std::runtime_error(std::string("Config is missing required field: ") + field);
    }
}

std::vector<std::string> parse_symbols(const json& root)
{
    require_field(root, "symbols");

    if (!root["symbols"].is_array()) {
        throw std::runtime_error("Config field 'symbols' must be an array");
    }

    std::vector<std::string> symbols;
    symbols.reserve(root["symbols"].size());

    for (const auto& item : root["symbols"]) {
        if (!item.is_string()) {
            throw std::runtime_error("Each symbol must be a string");
        }

        auto symbol = to_upper_ascii(item.get<std::string>());

        if (symbol.empty()) {
            throw std::runtime_error("Symbol must not be empty");
        }

        const auto valid = std::all_of(symbol.begin(), symbol.end(), is_valid_symbol_char);
        if (!valid) {
            throw std::runtime_error("Symbol contains invalid characters: " + symbol);
        }

        symbols.push_back(std::move(symbol));
    }

    if (symbols.empty()) {
        throw std::runtime_error("Config field 'symbols' must not be empty");
    }

    return symbols;
}

std::uint64_t parse_positive_u64(const json& root, const char* field)
{
    require_field(root, field);

    if (!root[field].is_number_unsigned()) {
        throw std::runtime_error(std::string("Config field must be a positive integer: ") + field);
    }

    const auto value = root[field].get<std::uint64_t>();

    if (value == 0) {
        throw std::runtime_error(std::string("Config field must be greater than zero: ") + field);
    }

    return value;
}

std::string parse_required_string(const json& root, const char* field)
{
    require_field(root, field);

    if (!root[field].is_string()) {
        throw std::runtime_error(std::string("Config field must be a string: ") + field);
    }

    auto value = root[field].get<std::string>();

    if (value.empty()) {
        throw std::runtime_error(std::string("Config field must not be empty: ") + field);
    }

    return value;
}

ReconnectConfig parse_reconnect_config(const json& root)
{
    ReconnectConfig reconnect;

    if (!root.contains("reconnect")) {
        return reconnect;
    }

    const auto& reconnect_json = root["reconnect"];

    if (!reconnect_json.is_object()) {
        throw std::runtime_error("Config field 'reconnect' must be an object");
    }

    if (reconnect_json.contains("initial_backoff_ms")) {
        reconnect.initial_backoff_ms =
            parse_positive_u64(reconnect_json, "initial_backoff_ms");
    }

    if (reconnect_json.contains("max_backoff_ms")) {
        reconnect.max_backoff_ms =
            parse_positive_u64(reconnect_json, "max_backoff_ms");
    }

    if (reconnect.initial_backoff_ms > reconnect.max_backoff_ms) {
        throw std::runtime_error(
            "Reconnect initial_backoff_ms must not be greater than max_backoff_ms"
        );
    }

    if (reconnect_json.contains("jitter_ratio")) {
        if (!reconnect_json["jitter_ratio"].is_number()) {
            throw std::runtime_error("Reconnect jitter_ratio must be a number");
        }

        reconnect.jitter_ratio = reconnect_json["jitter_ratio"].get<double>();

        if (reconnect.jitter_ratio < 0.0 || reconnect.jitter_ratio > 1.0) {
            throw std::runtime_error("Reconnect jitter_ratio must be between 0.0 and 1.0");
        }
    }

    return reconnect;
}

Config parse_config_json(const json& root)
{
    if (!root.is_object()) {
        throw std::runtime_error("Config root must be a JSON object");
    }

    Config config;

    config.symbols = parse_symbols(root);
    config.window_ms = parse_positive_u64(root, "window_ms");
    config.flush_interval_ms = parse_positive_u64(root, "flush_interval_ms");
    config.output_file = parse_required_string(root, "output_file");
    config.ws_host = parse_required_string(root, "ws_host");
    config.ws_port = parse_required_string(root, "ws_port");
    config.reconnect = parse_reconnect_config(root);

    return config;
}

} // namespace

Config ConfigLoader::load_from_file(const std::filesystem::path& path)
{
    std::ifstream input(path);

    if (!input.is_open()) {
        throw std::runtime_error("Failed to open config file: " + path.string());
    }

    json root;

    try {
        input >> root;
    } catch (const json::exception& error) {
        throw std::runtime_error(
            "Failed to parse config file '" + path.string() + "': " + error.what()
        );
    }

    return parse_config_json(root);
}

} // namespace agg::config