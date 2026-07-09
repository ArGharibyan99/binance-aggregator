#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

namespace agg::runtime {

/// Thread-safe bounded blocking queue used to hand items between runtime
/// stages (e.g. raw messages, parsed trade events, snapshots to write).
/// Capacity is a constructor parameter so callers can size it from
/// configuration once the runtime pipeline wires one up.
///
/// push() blocks while the queue is full; pop() blocks while it is empty.
/// shutdown() unblocks any waiting threads: push() then fails (returns
/// false) instead of blocking forever, and pop() drains any remaining
/// items before returning std::nullopt. This lets a stage's thread be
/// stopped cleanly instead of hanging on a queue operation.
template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity)
        : capacity_(capacity)
    {
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    /// Blocks while the queue is full. Returns false without pushing if
    /// shutdown() is (or becomes) active before space is available;
    /// returns true once `item` has been pushed.
    bool push(T item)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] { return shutdown_ || items_.size() < capacity_; });

        if (shutdown_) {
            return false;
        }

        items_.push_back(std::move(item));
        lock.unlock();
        not_empty_.notify_one();
        return true;
    }

    /// Blocks while the queue is empty. Returns std::nullopt once the
    /// queue has been shut down and fully drained.
    std::optional<T> pop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return shutdown_ || !items_.empty(); });

        if (items_.empty()) {
            return std::nullopt;
        }

        T item = std::move(items_.front());
        items_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return item;
    }

    /// Unblocks any threads currently waiting in push()/pop() and causes
    /// all future push() calls to fail. Items already queued remain
    /// available to pop() until drained. Idempotent.
    void shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        not_full_.notify_all();
        not_empty_.notify_all();
    }

    std::size_t capacity() const noexcept { return capacity_; }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    std::deque<T> items_;
    std::size_t capacity_;
    bool shutdown_ = false;
};

} // namespace agg::runtime
