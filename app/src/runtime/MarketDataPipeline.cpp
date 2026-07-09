#include <agg/runtime/MarketDataPipeline.hpp>

#include <agg/output/StatsSerializer.hpp>
#include <agg/output/StatsSnapshot.hpp>
#include <agg/parse/BinanceTradeParser.hpp>

#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <utility>
#include <vector>

namespace agg::runtime {

struct MarketDataPipeline::Shard {
    Shard(std::uint64_t window_ms, std::size_t queue_capacity)
        : queue(queue_capacity)
        , aggregator(window_ms)
    {
    }

    BoundedQueue<agg::model::TradeEvent> queue;
    std::mutex mutex;
    agg::aggregation::MarketDataAggregator aggregator;
};

MarketDataPipeline::MarketDataPipeline(
    std::uint64_t window_ms,
    std::uint64_t flush_interval_ms,
    std::size_t queue_capacity,
    agg::output::OutputSink& sink,
    std::size_t aggregator_threads)
    : window_ms_(window_ms)
    , flush_interval_ms_(flush_interval_ms)
    , raw_queue_(queue_capacity)
    , sink_(sink)
{
    const auto shard_count = std::max<std::size_t>(aggregator_threads, 1);
    const auto per_shard_capacity = std::max<std::size_t>(queue_capacity / shard_count, 1);

    shards_.reserve(shard_count);
    for (std::size_t i = 0; i < shard_count; ++i) {
        shards_.push_back(std::make_unique<Shard>(window_ms, per_shard_capacity));
    }
}

MarketDataPipeline::~MarketDataPipeline()
{
    stop();
}

void MarketDataPipeline::start()
{
    parser_thread_ = std::thread(&MarketDataPipeline::run_parser_stage, this);

    aggregation_threads_.reserve(shards_.size());
    for (auto& shard : shards_) {
        aggregation_threads_.emplace_back(&MarketDataPipeline::run_aggregation_stage, this, std::ref(*shard));
    }

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

    for (auto& shard : shards_) {
        shard->queue.shutdown();
    }
    for (auto& thread : aggregation_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    flush_remaining_windows();
}

bool MarketDataPipeline::submit_raw_message(std::string raw_message)
{
    return raw_queue_.push(std::move(raw_message));
}

std::size_t MarketDataPipeline::shard_index_for_symbol(const std::string& symbol) const
{
    return std::hash<std::string>{}(symbol) % shards_.size();
}

void MarketDataPipeline::run_parser_stage()
{
    while (auto raw = raw_queue_.pop()) {
        auto trade = agg::parse::BinanceTradeParser::parse(*raw);
        if (trade) {
            const auto shard_index = shard_index_for_symbol(trade->symbol);
            shards_[shard_index]->queue.push(std::move(*trade));
        }
    }
}

void MarketDataPipeline::run_aggregation_stage(Shard& shard)
{
    while (auto trade = shard.queue.pop()) {
        std::lock_guard<std::mutex> lock(shard.mutex);
        shard.aggregator.add_trade(*trade);
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
    // Withhold the window currently in progress "now" (by wall clock) so
    // it is not flushed, and thus not split by a late trade, while it
    // could still receive more trades under normal message delay. This
    // relies on the host clock being reasonably close to exchange time.
    const auto now_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
    const auto current_window_start_ms = now_ms - (now_ms % window_ms_);

    std::vector<agg::aggregation::WindowStats> windows;
    for (auto& shard : shards_) {
        std::vector<agg::aggregation::WindowStats> shard_windows;
        {
            std::lock_guard<std::mutex> lock(shard->mutex);
            shard_windows = shard->aggregator.extract_completed_windows(current_window_start_ms);
        }
        windows.insert(
            windows.end(), std::make_move_iterator(shard_windows.begin()), std::make_move_iterator(shard_windows.end()));
    }

    write_windows(std::move(windows));
}

void MarketDataPipeline::flush_remaining_windows()
{
    // Unconditional: called only once, during shutdown, when no further
    // trades can possibly arrive to split whatever window is still open.
    std::vector<agg::aggregation::WindowStats> windows;
    for (auto& shard : shards_) {
        std::vector<agg::aggregation::WindowStats> shard_windows;
        {
            std::lock_guard<std::mutex> lock(shard->mutex);
            shard_windows = shard->aggregator.extract_all_windows();
        }
        windows.insert(
            windows.end(), std::make_move_iterator(shard_windows.begin()), std::make_move_iterator(shard_windows.end()));
    }

    write_windows(std::move(windows));
}

void MarketDataPipeline::write_windows(std::vector<agg::aggregation::WindowStats> windows)
{
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
        // A symbol always lives in exactly one shard, so this sort makes
        // the merged output deterministic and byte-for-byte identical to
        // a single-shard run for the same input, regardless of shard
        // count or which shard happened to flush first.
        std::sort(snapshot.windows.begin(), snapshot.windows.end(),
            [](const agg::aggregation::WindowStats& lhs, const agg::aggregation::WindowStats& rhs) {
                return lhs.symbol < rhs.symbol;
            });

        auto text = agg::output::StatsSerializer::serialize(snapshot);
        if (!text.empty()) {
            sink_.write(text);
        }
    }
}

} // namespace agg::runtime
