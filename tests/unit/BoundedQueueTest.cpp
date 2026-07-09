#include <agg/runtime/BoundedQueue.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <thread>

namespace {
using agg::runtime::BoundedQueue;
}

TEST(BoundedQueueTest, PushAndPopPreserveFifoOrder)
{
    BoundedQueue<int> queue(8);

    ASSERT_TRUE(queue.push(1));
    ASSERT_TRUE(queue.push(2));
    ASSERT_TRUE(queue.push(3));

    EXPECT_EQ(queue.pop(), std::optional<int>(1));
    EXPECT_EQ(queue.pop(), std::optional<int>(2));
    EXPECT_EQ(queue.pop(), std::optional<int>(3));
}

TEST(BoundedQueueTest, PushBlocksUntilCapacityAvailable)
{
    BoundedQueue<int> queue(1);
    ASSERT_TRUE(queue.push(1));

    std::atomic<bool> second_push_started{ false };
    std::atomic<bool> second_push_completed{ false };

    std::thread pusher([&] {
        second_push_started = true;
        queue.push(2);
        second_push_completed = true;
    });

    while (!second_push_started.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(second_push_completed.load());

    const auto popped = queue.pop();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(*popped, 1);

    pusher.join();
    EXPECT_TRUE(second_push_completed.load());
    EXPECT_EQ(queue.size(), 1U);
}

TEST(BoundedQueueTest, PopBlocksUntilItemIsPushed)
{
    BoundedQueue<int> queue(4);

    std::optional<int> popped;
    std::thread popper([&] {
        popped = queue.pop();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    queue.push(42);
    popper.join();

    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(*popped, 42);
}

TEST(BoundedQueueTest, PopReturnsNulloptWhenEmptyAndShutDown)
{
    BoundedQueue<int> queue(4);
    queue.shutdown();

    EXPECT_FALSE(queue.pop().has_value());
}

TEST(BoundedQueueTest, PopDrainsRemainingItemsAfterShutdownBeforeReturningNullopt)
{
    BoundedQueue<int> queue(4);
    ASSERT_TRUE(queue.push(1));
    ASSERT_TRUE(queue.push(2));

    queue.shutdown();

    EXPECT_EQ(queue.pop(), std::optional<int>(1));
    EXPECT_EQ(queue.pop(), std::optional<int>(2));
    EXPECT_FALSE(queue.pop().has_value());
}

TEST(BoundedQueueTest, PushFailsImmediatelyAfterShutdownEvenIfNotFull)
{
    BoundedQueue<int> queue(4);
    queue.shutdown();

    EXPECT_FALSE(queue.push(1));
}

TEST(BoundedQueueTest, ShutdownUnblocksWaitingPushAndReturnsFalse)
{
    BoundedQueue<int> queue(1);
    ASSERT_TRUE(queue.push(1));

    bool push_result = true;
    std::thread pusher([&] {
        push_result = queue.push(2);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    queue.shutdown();
    pusher.join();

    EXPECT_FALSE(push_result);
}

TEST(BoundedQueueTest, ShutdownUnblocksWaitingPopWithNullopt)
{
    BoundedQueue<int> queue(4);

    std::optional<int> popped = 0;
    std::thread popper([&] {
        popped = queue.pop();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    queue.shutdown();
    popper.join();

    EXPECT_FALSE(popped.has_value());
}
