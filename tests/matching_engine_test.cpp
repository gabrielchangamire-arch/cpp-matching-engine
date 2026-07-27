#include "matching_engine/matching_engine.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

namespace matching_engine {
namespace {

TEST(MatchingEngineTest, BuyWithNoAskRestsOnBook) {
    MatchingEngine engine;

    const auto trades = engine.submit_limit_order(1, Side::buy, 100, 5);

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(engine.book().best_bid(), (BookLevel{100, 5, 1}));
    EXPECT_FALSE(engine.book().best_ask().has_value());
}

TEST(MatchingEngineTest, SellWithNoBidRestsOnBook) {
    MatchingEngine engine;

    const auto trades = engine.submit_limit_order(1, Side::sell, 100, 5);

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(engine.book().best_ask(), (BookLevel{100, 5, 1}));
    EXPECT_FALSE(engine.book().best_bid().has_value());
}

TEST(MatchingEngineTest, FullyFillsEqualQuantityOrders) {
    MatchingEngine engine;
    static_cast<void>(engine.submit_limit_order(1, Side::sell, 100, 5));

    const auto trades = engine.submit_limit_order(2, Side::buy, 100, 5);

    ASSERT_EQ(trades.size(), 1U);
    EXPECT_EQ(trades[0].id(), 1U);
    EXPECT_EQ(trades[0].buy_order_id(), 2U);
    EXPECT_EQ(trades[0].sell_order_id(), 1U);
    EXPECT_EQ(trades[0].price(), 100);
    EXPECT_EQ(trades[0].quantity(), 5);
    EXPECT_EQ(engine.book().order_count(), 0U);
}

TEST(MatchingEngineTest, PartiallyFillsRestingOrder) {
    MatchingEngine engine;
    static_cast<void>(engine.submit_limit_order(1, Side::sell, 100, 10));

    const auto trades = engine.submit_limit_order(2, Side::buy, 100, 4);

    ASSERT_EQ(trades.size(), 1U);
    EXPECT_EQ(trades[0].quantity(), 4);
    EXPECT_EQ(engine.book().best_ask(), (BookLevel{100, 6, 1}));
    EXPECT_FALSE(engine.book().best_bid().has_value());
}

TEST(MatchingEngineTest, RestsIncomingRemainderAfterPartialFill) {
    MatchingEngine engine;
    static_cast<void>(engine.submit_limit_order(1, Side::sell, 100, 4));

    const auto trades = engine.submit_limit_order(2, Side::buy, 100, 10);

    ASSERT_EQ(trades.size(), 1U);
    EXPECT_EQ(trades[0].quantity(), 4);
    EXPECT_EQ(engine.book().best_bid(), (BookLevel{100, 6, 1}));
    EXPECT_FALSE(engine.book().best_ask().has_value());
}

TEST(MatchingEngineTest, MatchesOneIncomingOrderAgainstMultipleOrders) {
    MatchingEngine engine;
    static_cast<void>(engine.submit_limit_order(1, Side::sell, 100, 3));
    static_cast<void>(engine.submit_limit_order(2, Side::sell, 100, 4));

    const auto trades = engine.submit_limit_order(3, Side::buy, 100, 6);

    ASSERT_EQ(trades.size(), 2U);
    EXPECT_EQ(trades[0].sell_order_id(), 1U);
    EXPECT_EQ(trades[0].quantity(), 3);
    EXPECT_EQ(trades[1].sell_order_id(), 2U);
    EXPECT_EQ(trades[1].quantity(), 3);
    EXPECT_EQ(engine.book().best_ask(), (BookLevel{100, 1, 1}));
}

TEST(MatchingEngineTest, HonorsAskPricePriority) {
    MatchingEngine engine;
    static_cast<void>(engine.submit_limit_order(1, Side::sell, 102, 2));
    static_cast<void>(engine.submit_limit_order(2, Side::sell, 101, 2));

    const auto trades = engine.submit_limit_order(3, Side::buy, 102, 2);

    ASSERT_EQ(trades.size(), 1U);
    EXPECT_EQ(trades[0].sell_order_id(), 2U);
    EXPECT_EQ(trades[0].price(), 101);
    EXPECT_EQ(engine.book().best_ask(), (BookLevel{102, 2, 1}));
}

TEST(MatchingEngineTest, HonorsBidPricePriority) {
    MatchingEngine engine;
    static_cast<void>(engine.submit_limit_order(1, Side::buy, 100, 2));
    static_cast<void>(engine.submit_limit_order(2, Side::buy, 101, 2));

    const auto trades = engine.submit_limit_order(3, Side::sell, 100, 2);

    ASSERT_EQ(trades.size(), 1U);
    EXPECT_EQ(trades[0].buy_order_id(), 2U);
    EXPECT_EQ(trades[0].price(), 101);
    EXPECT_EQ(engine.book().best_bid(), (BookLevel{100, 2, 1}));
}

TEST(MatchingEngineTest, HonorsTimePriorityAtSamePrice) {
    MatchingEngine engine;
    static_cast<void>(engine.submit_limit_order(10, Side::sell, 100, 2));
    static_cast<void>(engine.submit_limit_order(11, Side::sell, 100, 2));

    const auto trades = engine.submit_limit_order(12, Side::buy, 100, 3);

    ASSERT_EQ(trades.size(), 2U);
    EXPECT_EQ(trades[0].sell_order_id(), 10U);
    EXPECT_EQ(trades[1].sell_order_id(), 11U);
    EXPECT_EQ(engine.book().best_ask(), (BookLevel{100, 1, 1}));
}

TEST(MatchingEngineTest, DoesNotMatchPricesThatDoNotCross) {
    MatchingEngine engine;
    static_cast<void>(engine.submit_limit_order(1, Side::sell, 101, 5));

    const auto trades = engine.submit_limit_order(2, Side::buy, 100, 5);

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(engine.book().best_bid(), (BookLevel{100, 5, 1}));
    EXPECT_EQ(engine.book().best_ask(), (BookLevel{101, 5, 1}));
}

TEST(MatchingEngineTest, CancellationPreventsFutureMatch) {
    MatchingEngine engine;
    static_cast<void>(engine.submit_limit_order(1, Side::sell, 100, 5));
    ASSERT_TRUE(engine.cancel(1));

    const auto trades = engine.submit_limit_order(2, Side::buy, 100, 5);

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(engine.book().best_bid(), (BookLevel{100, 5, 1}));
}

TEST(MatchingEngineTest, CancelsRemainderAfterPartialFill) {
    MatchingEngine engine;
    static_cast<void>(engine.submit_limit_order(1, Side::sell, 100, 10));
    static_cast<void>(engine.submit_limit_order(2, Side::buy, 100, 4));

    EXPECT_TRUE(engine.cancel(1));
    EXPECT_FALSE(engine.book().best_ask().has_value());
}

TEST(MatchingEngineTest, RejectsDuplicateActiveOrderId) {
    MatchingEngine engine;
    static_cast<void>(engine.submit_limit_order(1, Side::buy, 100, 1));

    EXPECT_THROW(static_cast<void>(engine.submit_limit_order(1, Side::sell, 101, 1)),
                 std::invalid_argument);
}

TEST(MatchingEngineTest, RejectsReusedIdAfterCancellation) {
    MatchingEngine engine;
    static_cast<void>(engine.submit_limit_order(1, Side::buy, 100, 1));
    ASSERT_TRUE(engine.cancel(1));

    EXPECT_THROW(static_cast<void>(engine.submit_limit_order(1, Side::buy, 100, 1)),
                 std::invalid_argument);
}

TEST(MatchingEngineTest, RejectsReusedIdAfterCompleteFill) {
    MatchingEngine engine;
    static_cast<void>(engine.submit_limit_order(1, Side::sell, 100, 1));
    static_cast<void>(engine.submit_limit_order(2, Side::buy, 100, 1));

    EXPECT_THROW(static_cast<void>(engine.submit_limit_order(1, Side::sell, 100, 1)),
                 std::invalid_argument);
}

TEST(MatchingEngineTest, InvalidSubmissionDoesNotReserveOrderId) {
    MatchingEngine engine;

    EXPECT_THROW(static_cast<void>(engine.submit_limit_order(1, Side::buy, 0, 1)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(engine.submit_limit_order(1, Side::buy, 100, 0)),
                 std::invalid_argument);
    EXPECT_NO_THROW(static_cast<void>(engine.submit_limit_order(1, Side::buy, 100, 1)));
}

TEST(MatchingEngineTest, HandlesMaximumValidQuantity) {
    MatchingEngine engine;
    constexpr Quantity maximum = std::numeric_limits<Quantity>::max();
    static_cast<void>(engine.submit_limit_order(1, Side::sell, 100, maximum));

    const auto trades = engine.submit_limit_order(2, Side::buy, 100, maximum);

    ASSERT_EQ(trades.size(), 1U);
    EXPECT_EQ(trades[0].quantity(), maximum);
    EXPECT_EQ(engine.book().order_count(), 0U);
}

TEST(MatchingEngineTest, UnknownCancellationReturnsFalseOnEmptyBook) {
    MatchingEngine engine;

    EXPECT_FALSE(engine.cancel(999));
}

} // namespace
} // namespace matching_engine
