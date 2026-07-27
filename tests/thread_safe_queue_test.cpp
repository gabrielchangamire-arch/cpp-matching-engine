#include "matching_engine/thread_safe_queue.hpp"

#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <stdexcept>

namespace matching_engine {
namespace {

using namespace std::chrono_literals;

TEST(ThreadSafeQueueTest, RejectsZeroCapacity) {
    EXPECT_THROW(static_cast<void>(ThreadSafeQueue<int>{0}), std::invalid_argument);
}

TEST(ThreadSafeQueueTest, PreservesFifoOrder) {
    ThreadSafeQueue<int> queue{2};

    ASSERT_TRUE(queue.push(10));
    ASSERT_TRUE(queue.push(20));

    EXPECT_EQ(queue.pop(), 10);
    EXPECT_EQ(queue.pop(), 20);
}

TEST(ThreadSafeQueueTest, CloseWakesWaitingConsumer) {
    ThreadSafeQueue<int> queue{1};
    auto consumer = std::async(std::launch::async, [&queue] { return queue.pop(); });

    EXPECT_EQ(consumer.wait_for(50ms), std::future_status::timeout);
    queue.close();

    EXPECT_EQ(consumer.wait_for(1s), std::future_status::ready);
    EXPECT_FALSE(consumer.get().has_value());
}

TEST(ThreadSafeQueueTest, CapacityAppliesBackpressureToProducer) {
    ThreadSafeQueue<int> queue{1};
    ASSERT_TRUE(queue.push(10));
    auto producer = std::async(std::launch::async, [&queue] { return queue.push(20); });

    EXPECT_EQ(producer.wait_for(50ms), std::future_status::timeout);
    EXPECT_EQ(queue.pop(), 10);

    EXPECT_EQ(producer.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(producer.get());
    EXPECT_EQ(queue.pop(), 20);
}

TEST(ThreadSafeQueueTest, ClosedQueueRejectsNewValuesButDrainsExistingValues) {
    ThreadSafeQueue<int> queue{2};
    ASSERT_TRUE(queue.push(10));

    queue.close();

    EXPECT_TRUE(queue.is_closed());
    EXPECT_FALSE(queue.push(20));
    EXPECT_EQ(queue.pop(), 10);
    EXPECT_FALSE(queue.pop().has_value());
}

} // namespace
} // namespace matching_engine
