#include <agg/runtime/MarketDataPipeline.hpp>

#include <agg/output/StatsSerializer.hpp>
#include <agg/output/StatsSnapshot.hpp>
#include <agg/parse/BinanceTradeParser.hpp>

#include <chrono>
#include <map>
#include <utility>
#include <vector>

namespace agg::runtime {

MarketDataPipeline::MarketDataPipeline(
    std::uint64_t window_ms,
    std::uint64_t flush_interval_ms,
    std::size_t queue_capacity,
    agg::output::OutputSink& sink)
    : flush_interval_ms_(flush_interval_ms)
    , raw_queue_(queue_capacity)
    , trade_queue_(queue_capacity)
    , aggregator_(window_ms)
    , sink_(sink)
{
}

MarketDataPipeline::~MarketDataPipeline()
{
    stop();
}

void MarketDataPipeline::start()
{
    parser_thread_ = std::thread(&MarketDataPipeline::run_parser_stage, this);
    aggregation_thread_ = std::thread(&MarketDataPipeline::run_aggregation_stage, this);
    writer_thread_ = std::thread(&MarketDataPipeline::run_writer_stage, this);
}

void MarketDataPipeline::stop()
{
    {
        std::lock_guard<std::mutex> lock(stop_mutex_);
        stop_requested_ = true;
    }
    stop_cv_.notify_all();
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }

    raw_queue_.shutdown();
    if (parser_thread_.joinable()) {
        parser_thread_.join();
    }

    trade_queue_.shutdown();
    if (aggregation_thread_.joinable()) {
        aggregation_thread_.join();
    }

    flush_completed_windows();
}

bool MarketDataPipeline::submit_raw_message(std::string raw_message)
{
    return raw_queue_.push(std::move(raw_message));
}

void MarketDataPipeline::run_parser_stage()
{
    while (auto raw = raw_queue_.pop()) {
        auto trade = agg::parse::BinanceTradeParser::parse(*raw);
        if (trade) {
            trade_queue_.push(std::move(*trade));
        }
    }
}

void MarketDataPipeline::run_aggregation_stage()
{
    while (auto trade = trade_queue_.pop()) {
        std::lock_guard<std::mutex> lock(aggregator_mutex_);
        aggregator_.add_trade(*trade);
    }
}

void MarketDataPipeline::run_writer_stage()
{
    std::unique_lock<std::mutex> lock(stop_mutex_);

    while (!stop_cv_.wait_for(
        lock, std::chrono::milliseconds(flush_interval_ms_), [this] { return stop_requested_; })) {
        lock.unlock();
        flush_completed_windows();
        lock.lock();
    }
}

void MarketDataPipeline::flush_completed_windows()
{
    std::vector<agg::aggregation::WindowStats> windows;
    {
        std::lock_guard<std::mutex> lock(aggregator_mutex_);
        windows = aggregator_.extract_all_windows();
    }

    if (windows.empty()) {
        return;
    }

    std::map<std::uint64_t, agg::output::StatsSnapshot> snapshots_by_window;
    for (auto& window : windows) {
        auto& snapshot = snapshots_by_window[window.window_start_ms];
        snapshot.window_start_ms = window.window_start_ms;
        snapshot.windows.push_back(std::move(window));
    }

    for (auto& [window_start_ms, snapshot] : snapshots_by_window) {
        auto text = agg::output::StatsSerializer::serialize(snapshot);
        if (!text.empty()) {
            sink_.write(text);
        }
    }
}

} // namespace agg::runtime
