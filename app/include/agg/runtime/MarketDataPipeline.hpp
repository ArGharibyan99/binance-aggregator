#pragma once

#include <agg/aggregation/MarketDataAggregator.hpp>
#include <agg/output/OutputSink.hpp>
#include <agg/runtime/BoundedQueue.hpp>

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace agg::runtime {

/// Wires the parser, (sharded) aggregation, and writer stages together
/// into a running pipeline, without any real network source: raw
/// messages are fed in explicitly via submit_raw_message(), e.g. from a
/// fake source in tests or, later, from the Binance WebSocket client.
///
/// Stage layout (matches the planned runtime architecture):
///   submit_raw_message -> raw queue -> parser stage
///     -> one of `aggregator_threads` independent (queue, aggregator)
///        shards, chosen by std::hash<string>{}(symbol) % shard_count,
///        so a given symbol always routes to the same shard for the
///        life of the process
///     -> one aggregation thread per shard
///     -> writer stage (on a local steady-clock timer), which merges
///        completed windows extracted from every shard before
///        serializing -> OutputSink
///
/// Sharding is purely a throughput mechanism: aggregation within one
/// (symbol, window) bucket is commutative, and a symbol never splits
/// across shards, so there is no cross-shard ordering or reconciliation
/// to manage -- each shard is an entirely independent single-threaded
/// aggregator/mutex pair, merged only at flush time.
class MarketDataPipeline {
public:
    MarketDataPipeline(
        std::uint64_t window_ms,
        std::uint64_t flush_interval_ms,
        std::size_t queue_capacity,
        agg::output::OutputSink& sink,
        std::size_t aggregator_threads = 1);

    ~MarketDataPipeline();

    MarketDataPipeline(const MarketDataPipeline&) = delete;
    MarketDataPipeline& operator=(const MarketDataPipeline&) = delete;

    /// Starts the parser thread, one aggregation thread per shard, and
    /// the writer thread.
    void start();

    /// Stops all stages in pipeline order (writer, then parser, then
    /// every aggregation shard) so that messages already queued get a
    /// chance to drain through to their shard's aggregator before
    /// threads exit, then performs one final flush merging every
    /// shard's remaining windows. Safe to call more than once, and safe
    /// to call even if start() was never called.
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
    struct Shard;

    void run_parser_stage();
    void run_aggregation_stage(Shard& shard);
    void run_writer_stage();
    void flush_completed_windows();
    void flush_remaining_windows();
    void write_windows(std::vector<agg::aggregation::WindowStats> windows);
    std::size_t shard_index_for_symbol(const std::string& symbol) const;

    std::uint64_t window_ms_;
    std::uint64_t flush_interval_ms_;

    BoundedQueue<std::string> raw_queue_;

    std::vector<std::unique_ptr<Shard>> shards_;

    agg::output::OutputSink& sink_;

    std::thread parser_thread_;
    std::vector<std::thread> aggregation_threads_;
    std::thread writer_thread_;

    std::mutex stop_mutex_;
    std::condition_variable stop_cv_;
    bool stop_requested_ = false;
};

} // namespace agg::runtime
