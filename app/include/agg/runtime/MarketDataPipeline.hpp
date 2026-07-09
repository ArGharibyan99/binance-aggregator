#pragma once

#include <agg/aggregation/MarketDataAggregator.hpp>
#include <agg/output/OutputSink.hpp>
#include <agg/runtime/BoundedQueue.hpp>

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace agg::runtime {

/// Wires the parser, aggregation, and writer stages together into a
/// running pipeline, without any real network source: raw messages are
/// fed in explicitly via submit_raw_message(), e.g. from a fake source
/// in tests or, later, from the Binance WebSocket client.
///
/// Stage layout (matches the planned runtime architecture):
///   submit_raw_message -> raw queue -> parser stage -> trade queue
///     -> aggregation stage -> aggregator -> writer stage (on a local
///     steady-clock timer) -> OutputSink
///
/// The aggregator is shared between the aggregation stage (adds trades)
/// and the writer stage (extracts completed windows), so access to it is
/// serialized internally by a mutex.
class MarketDataPipeline {
public:
    MarketDataPipeline(
        std::uint64_t window_ms,
        std::uint64_t flush_interval_ms,
        std::size_t queue_capacity,
        agg::output::OutputSink& sink);

    ~MarketDataPipeline();

    MarketDataPipeline(const MarketDataPipeline&) = delete;
    MarketDataPipeline& operator=(const MarketDataPipeline&) = delete;

    /// Starts the parser, aggregation, and writer threads.
    void start();

    /// Stops all stages in pipeline order (writer, then parser, then
    /// aggregation) so that messages already queued get a chance to
    /// drain through to the aggregator before their threads exit, then
    /// performs one final flush of anything accumulated since the last
    /// periodic flush. Safe to call more than once, and safe to call
    /// even if start() was never called.
    void stop();

    /// Feeds one raw message string into the pipeline, as if it had
    /// just been received from the network. Blocks if the raw-message
    /// queue is full; returns false if the pipeline is shutting down.
    bool submit_raw_message(std::string raw_message);

    /// Direct access to the raw-message queue, so an external message
    /// source (e.g. NetworkStage) can push into the same queue the
    /// parser stage reads from without going through submit_raw_message.
    BoundedQueue<std::string>& raw_message_queue() noexcept { return raw_queue_; }

private:
    void run_parser_stage();
    void run_aggregation_stage();
    void run_writer_stage();
    void flush_completed_windows();

    std::uint64_t flush_interval_ms_;

    BoundedQueue<std::string> raw_queue_;
    BoundedQueue<agg::model::TradeEvent> trade_queue_;

    std::mutex aggregator_mutex_;
    agg::aggregation::MarketDataAggregator aggregator_;

    agg::output::OutputSink& sink_;

    std::thread parser_thread_;
    std::thread aggregation_thread_;
    std::thread writer_thread_;

    std::mutex stop_mutex_;
    std::condition_variable stop_cv_;
    bool stop_requested_ = false;
};

} // namespace agg::runtime
