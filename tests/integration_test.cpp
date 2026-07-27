#include "matching_engine/matching_engine.hpp"

#include <gtest/gtest.h>

namespace matching_engine {
namespace {

TEST(IntegrationTest, ProducesExactTradesAndFinalBookState) {
    MatchingEngine engine;
    static_cast<void>(engine.submit_limit_order(1, Side::sell, 10'100, 5));
    static_cast<void>(engine.submit_limit_order(2, Side::sell, 10'200, 7));
    static_cast<void>(engine.submit_limit_order(3, Side::buy, 10'000, 4));

    const auto buy_trades = engine.submit_limit_order(4, Side::buy, 10'300, 10);
    ASSERT_EQ(buy_trades.size(), 2U);
    EXPECT_EQ(buy_trades[0].id(), 1U);
    EXPECT_EQ(buy_trades[0].buy_order_id(), 4U);
    EXPECT_EQ(buy_trades[0].sell_order_id(), 1U);
    EXPECT_EQ(buy_trades[0].price(), 10'100);
    EXPECT_EQ(buy_trades[0].quantity(), 5);
    EXPECT_EQ(buy_trades[0].sequence(), 5U);
    EXPECT_EQ(buy_trades[1].id(), 2U);
    EXPECT_EQ(buy_trades[1].buy_order_id(), 4U);
    EXPECT_EQ(buy_trades[1].sell_order_id(), 2U);
    EXPECT_EQ(buy_trades[1].price(), 10'200);
    EXPECT_EQ(buy_trades[1].quantity(), 5);
    EXPECT_EQ(buy_trades[1].sequence(), 6U);

    const auto sell_trades = engine.submit_limit_order(5, Side::sell, 9'900, 6);
    ASSERT_EQ(sell_trades.size(), 1U);
    EXPECT_EQ(sell_trades[0].id(), 3U);
    EXPECT_EQ(sell_trades[0].buy_order_id(), 3U);
    EXPECT_EQ(sell_trades[0].sell_order_id(), 5U);
    EXPECT_EQ(sell_trades[0].price(), 10'000);
    EXPECT_EQ(sell_trades[0].quantity(), 4);
    EXPECT_EQ(sell_trades[0].sequence(), 8U);

    ASSERT_TRUE(engine.cancel(2));
    EXPECT_EQ(engine.book().snapshot(), "Order book (1 order)\n"
                                        "ASKS (best first)\n"
                                        "  9900 | qty 2 | orders 1\n"
                                        "BIDS (best first)\n"
                                        "  (empty)\n");
}

} // namespace
} // namespace matching_engine
