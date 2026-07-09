#include <agg/net/ReconnectPolicy.hpp>

#include <algorithm>

namespace agg::net {

ReconnectPolicy::ReconnectPolicy(agg::config::ReconnectConfig config)
    : config_(std::move(config))
{
}

std::chrono::milliseconds ReconnectPolicy::base_delay_for_attempt(unsigned attempt) const
{
    std::uint64_t delay_ms = config_.initial_backoff_ms;

    for (unsigned i = 0; i < attempt && delay_ms < config_.max_backoff_ms; ++i) {
        delay_ms *= 2;
    }

    delay_ms = std::min(delay_ms, config_.max_backoff_ms);

    return std::chrono::milliseconds(delay_ms);
}

std::chrono::milliseconds ReconnectPolicy::apply_jitter(std::chrono::milliseconds base)
{
    if (config_.jitter_ratio <= 0.0) {
        return base;
    }

    const auto base_ms = static_cast<double>(base.count());
    const auto spread = base_ms * config_.jitter_ratio;

    std::uniform_real_distribution<double> distribution(base_ms - spread, base_ms + spread);
    const auto jittered_ms = distribution(rng_);

    return std::chrono::milliseconds(static_cast<std::int64_t>(std::max(0.0, jittered_ms)));
}

bool ReconnectPolicy::wait_for_next_attempt()
{
    const auto delay = apply_jitter(base_delay_for_attempt(attempt_));

    std::unique_lock<std::mutex> lock(mutex_);
    const bool interrupted = cv_.wait_for(lock, delay, [this] { return stop_requested_; });

    if (interrupted) {
        return false;
    }

    ++attempt_;
    return true;
}

void ReconnectPolicy::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
    }
    cv_.notify_all();
}

void ReconnectPolicy::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    attempt_ = 0;
    stop_requested_ = false;
}

unsigned ReconnectPolicy::current_attempt() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return attempt_;
}

} // namespace agg::net
