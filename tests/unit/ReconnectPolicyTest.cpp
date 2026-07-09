#include <agg/net/ReconnectPolicy.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace {

agg::config::ReconnectConfig make_reconnect_config(
    std::uint64_t initial_backoff_ms, std::uint64_t max_backoff_ms, double jitter_ratio)
{
    agg::config::ReconnectConfig config;
    config.initial_backoff_ms = initial_backoff_ms;
    config.max_backoff_ms = max_backoff_ms;
    config.jitter_ratio = jitter_ratio;
    return config;
}

} // namespace

TEST(ReconnectPolicyTest, BaseDelayStartsAtInitialBackoff)
{
    agg::net::ReconnectPolicy policy(make_reconnect_config(500, 30000, 0.0));

    EXPECT_EQ(policy.base_delay_for_attempt(0), std::chrono::milliseconds(500));
}

TEST(ReconnectPolicyTest, BaseDelayDoublesEachAttempt)
{
    agg::net::ReconnectPolicy policy(make_reconnect_config(500, 30000, 0.0));

    EXPECT_EQ(policy.base_delay_for_attempt(1), std::chrono::milliseconds(1000));
    EXPECT_EQ(policy.base_delay_for_attempt(2), std::chrono::milliseconds(2000));
    EXPECT_EQ(policy.base_delay_for_attempt(3), std::chrono::milliseconds(4000));
}

TEST(ReconnectPolicyTest, BaseDelayCapsAtMaxBackoff)
{
    agg::net::ReconnectPolicy policy(make_reconnect_config(1000, 3000, 0.0));

    EXPECT_EQ(policy.base_delay_for_attempt(0), std::chrono::milliseconds(1000));
    EXPECT_EQ(policy.base_delay_for_attempt(1), std::chrono::milliseconds(2000));
    EXPECT_EQ(policy.base_delay_for_attempt(2), std::chrono::milliseconds(3000));
    EXPECT_EQ(policy.base_delay_for_attempt(3), std::chrono::milliseconds(3000));
    EXPECT_EQ(policy.base_delay_for_attempt(10), std::chrono::milliseconds(3000));
}

TEST(ReconnectPolicyTest, ApplyJitterStaysWithinConfiguredBounds)
{
    agg::net::ReconnectPolicy policy(make_reconnect_config(1000, 30000, 0.2));

    for (int i = 0; i < 200; ++i) {
        const auto jittered = policy.apply_jitter(std::chrono::milliseconds(1000));
        EXPECT_GE(jittered.count(), 800);
        EXPECT_LE(jittered.count(), 1200);
    }
}

TEST(ReconnectPolicyTest, ApplyJitterWithZeroRatioReturnsBaseUnchanged)
{
    agg::net::ReconnectPolicy policy(make_reconnect_config(1000, 30000, 0.0));

    EXPECT_EQ(policy.apply_jitter(std::chrono::milliseconds(1234)), std::chrono::milliseconds(1234));
}

TEST(ReconnectPolicyTest, WaitForNextAttemptAdvancesAttemptCountOnNaturalTimeout)
{
    agg::net::ReconnectPolicy policy(make_reconnect_config(5, 5, 0.0));

    EXPECT_EQ(policy.current_attempt(), 0U);
    EXPECT_TRUE(policy.wait_for_next_attempt());
    EXPECT_EQ(policy.current_attempt(), 1U);
    EXPECT_TRUE(policy.wait_for_next_attempt());
    EXPECT_EQ(policy.current_attempt(), 2U);
}

TEST(ReconnectPolicyTest, ResetClearsAttemptCount)
{
    agg::net::ReconnectPolicy policy(make_reconnect_config(5, 5, 0.0));

    ASSERT_TRUE(policy.wait_for_next_attempt());
    ASSERT_EQ(policy.current_attempt(), 1U);

    policy.reset();

    EXPECT_EQ(policy.current_attempt(), 0U);
}

TEST(ReconnectPolicyTest, StopInterruptsWaitPromptlyAndReturnsFalse)
{
    agg::net::ReconnectPolicy policy(make_reconnect_config(10000, 10000, 0.0));

    std::atomic<bool> result_ready{ false };
    bool result = true;

    std::thread waiter([&] {
        result = policy.wait_for_next_attempt();
        result_ready = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    policy.stop();
    waiter.join();

    EXPECT_TRUE(result_ready.load());
    EXPECT_FALSE(result);
    EXPECT_EQ(policy.current_attempt(), 0U);
}

TEST(ReconnectPolicyTest, ResetAfterStopAllowsPolicyToBeReusedForANewLoop)
{
    agg::net::ReconnectPolicy policy(make_reconnect_config(5, 5, 0.0));

    policy.stop();
    EXPECT_FALSE(policy.wait_for_next_attempt());

    policy.reset();
    EXPECT_TRUE(policy.wait_for_next_attempt());
    EXPECT_EQ(policy.current_attempt(), 1U);
}
