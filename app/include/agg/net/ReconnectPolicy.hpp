#pragma once

#include <agg/config/Config.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <random>

namespace agg::net {

/// Computes reconnect delays using exponential backoff capped at
/// max_backoff_ms, randomized by jitter_ratio, based on Config's
/// ReconnectConfig. Also owns an interruptible wait so a reconnect loop
/// can stop promptly during shutdown instead of blocking for up to
/// max_backoff_ms.
class ReconnectPolicy {
public:
    explicit ReconnectPolicy(agg::config::ReconnectConfig config);

    /// Returns the un-jittered base delay for a given (0-indexed) retry
    /// attempt: min(initial_backoff_ms * 2^attempt, max_backoff_ms).
    /// Exposed directly so the backoff/cap behavior can be tested
    /// independent of jitter and timing.
    std::chrono::milliseconds base_delay_for_attempt(unsigned attempt) const;

    /// Applies jitter_ratio to `base`, returning a value uniformly
    /// distributed in [base*(1-jitter_ratio), base*(1+jitter_ratio)]
    /// (clamped to non-negative). Exposed directly so jitter can be
    /// tested for staying within bounds without actually waiting out a
    /// delay.
    std::chrono::milliseconds apply_jitter(std::chrono::milliseconds base);

    /// Blocks for the delay corresponding to the current attempt count
    /// (with jitter applied), then advances the attempt count and
    /// returns true. Returns false immediately, without waiting out the
    /// full delay, if stop() is called while waiting.
    bool wait_for_next_attempt();

    /// Unblocks any in-progress wait_for_next_attempt() and causes
    /// future calls to return false immediately. Idempotent.
    void stop();

    /// Resets the attempt count back to zero (e.g. after a successful
    /// connection) and clears a prior stop(), so the policy can be
    /// reused for a fresh connect loop.
    void reset();

    unsigned current_attempt() const noexcept;

private:
    agg::config::ReconnectConfig config_;
    unsigned attempt_ = 0;
    std::mt19937_64 rng_{ std::random_device{}() };

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_requested_ = false;
};

} // namespace agg::net
