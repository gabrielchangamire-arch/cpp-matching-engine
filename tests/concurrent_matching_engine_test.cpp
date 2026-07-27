#include "matching_engine/concurrent_matching_engine.hpp"

#include <array>
#include <future>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <thread>

namespace matching_engine {
namespace {

TEST(ConcurrentMatchingEngineTest, ProcessesConcurrentProducersByExplicitSequence) {
    ConcurrentMatchingEngine engine{2};
    std::array<std::optional<std::future<CommandResult>>, 6> futures;

    std::thread producer_one{[&] {
        futures[3] = engine.submit(4, AddCommand{4, Side::buy, 10'300, 10});
        futures[0] = engine.submit(1, AddCommand{1, Side::sell, 10'100, 5});
    }};
    std::thread producer_two{[&] {
        futures[1] = engine.submit(2, AddCommand{2, Side::sell, 10'200, 7});
        futures[4] = engine.submit(5, AddCommand{5, Side::sell, 9'900, 6});
    }};
    std::thread producer_three{[&] {
        futures[2] = engine.submit(3, AddCommand{3, Side::buy, 10'000, 4});
        futures[5] = engine.submit(6, CancelCommand{2});
    }};

    producer_one.join();
    producer_two.join();
    producer_three.join();

    // All producers have joined, so each fixed slot has been populated.
    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    std::array<CommandResult, 6> results{
        futures[0].value().get(), futures[1].value().get(), futures[2].value().get(),
        futures[3].value().get(), futures[4].value().get(), futures[5].value().get(),
    };
    // NOLINTEND(bugprone-unchecked-optional-access)
    engine.shutdown();

    for (std::size_t index = 0; index < results.size(); ++index) {
        EXPECT_EQ(results[index].ingestion_sequence, index + 1);
        EXPECT_TRUE(results[index].accepted) << results[index].error;
    }
    ASSERT_EQ(results[3].trades.size(), 2U);
    EXPECT_EQ(results[3].trades[0].sell_order_id(), 1U);
    EXPECT_EQ(results[3].trades[1].sell_order_id(), 2U);
    ASSERT_EQ(results[4].trades.size(), 1U);
    EXPECT_EQ(results[4].trades[0].buy_order_id(), 3U);
    EXPECT_EQ(engine.book_snapshot(), "Order book (1 order)\n"
                                      "ASKS (best first)\n"
                                      "  9900 | qty 2 | orders 1\n"
                                      "BIDS (best first)\n"
                                      "  (empty)\n");
}

TEST(ConcurrentMatchingEngineTest, ReturnsCommandValidationErrorsThroughFuture) {
    ConcurrentMatchingEngine engine{2};
    auto invalid_add = engine.submit(1, AddCommand{1, Side::buy, 0, 1});
    auto unknown_cancel = engine.submit(2, CancelCommand{999});

    const auto add_result = invalid_add.get();
    const auto cancel_result = unknown_cancel.get();
    engine.shutdown();

    EXPECT_FALSE(add_result.accepted);
    EXPECT_NE(add_result.error.find("price"), std::string::npos);
    EXPECT_FALSE(cancel_result.accepted);
    EXPECT_NE(cancel_result.error.find("unknown"), std::string::npos);
}

TEST(ConcurrentMatchingEngineTest, RejectsInvalidAndDuplicateIngestionSequences) {
    ConcurrentMatchingEngine engine{2};
    auto first = engine.submit(1, AddCommand{1, Side::buy, 100, 1});

    EXPECT_THROW(static_cast<void>(engine.submit(0, AddCommand{2, Side::buy, 100, 1})),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(engine.submit(1, AddCommand{2, Side::buy, 100, 1})),
                 std::invalid_argument);
    EXPECT_TRUE(first.get().accepted);
    engine.shutdown();
}

TEST(ConcurrentMatchingEngineTest, RejectsPendingCommandsWhenSequenceHasGap) {
    ConcurrentMatchingEngine engine{1};
    auto future = engine.submit(2, AddCommand{2, Side::buy, 100, 1});

    engine.shutdown();
    const auto result = future.get();

    EXPECT_FALSE(result.accepted);
    EXPECT_NE(result.error.find("missing"), std::string::npos);
}

TEST(ConcurrentMatchingEngineTest, RejectsSubmissionAndAllowsSnapshotAfterShutdown) {
    ConcurrentMatchingEngine engine{1};
    engine.shutdown();

    EXPECT_THROW(static_cast<void>(engine.submit(1, AddCommand{1, Side::buy, 100, 1})),
                 std::runtime_error);
    EXPECT_NE(engine.book_snapshot().find("Order book (0 orders)"), std::string::npos);
}

TEST(ConcurrentMatchingEngineTest, SnapshotBeforeShutdownIsRejected) {
    ConcurrentMatchingEngine engine{1};

    EXPECT_THROW(static_cast<void>(engine.book_snapshot()), std::logic_error);
    engine.shutdown();
}

} // namespace
} // namespace matching_engine
