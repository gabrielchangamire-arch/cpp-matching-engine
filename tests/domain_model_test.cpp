#include "matching_engine/order.hpp"
#include "matching_engine/trade.hpp"
#include "matching_engine/types.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <sstream>
#include <stdexcept>

namespace matching_engine {
namespace {

TEST(TypesTest, FormatsSides) {
    EXPECT_EQ(to_string(Side::buy), "BUY");
    EXPECT_EQ(to_string(Side::sell), "SELL");
}

TEST(TypesTest, FormatsLimitOrderType) {
    EXPECT_EQ(to_string(OrderType::limit), "LIMIT");
}

TEST(TypesTest, RejectsUnknownSide) {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    EXPECT_THROW(static_cast<void>(to_string(static_cast<Side>(255))),
                 std::invalid_argument);
}

TEST(TypesTest, RejectsUnknownOrderType) {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    EXPECT_THROW(static_cast<void>(to_string(static_cast<OrderType>(255))),
                 std::invalid_argument);
}

TEST(OrderTest, ConstructsValidOrder) {
    const Order order{42, Side::buy, OrderType::limit, 10'125, 50, 7};

    EXPECT_EQ(order.id(), 42U);
    EXPECT_EQ(order.side(), Side::buy);
    EXPECT_EQ(order.type(), OrderType::limit);
    EXPECT_EQ(order.price(), 10'125);
    EXPECT_EQ(order.quantity(), 50);
    EXPECT_EQ(order.remaining_quantity(), 50);
    EXPECT_EQ(order.sequence(), 7U);
    EXPECT_FALSE(order.is_filled());
}

TEST(OrderTest, RejectsZeroId) {
    EXPECT_THROW((Order{0, Side::buy, OrderType::limit, 100, 1, 1}),
                 std::invalid_argument);
}

TEST(OrderTest, RejectsUnknownSide) {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    EXPECT_THROW((Order{1, static_cast<Side>(255), OrderType::limit, 100, 1, 1}),
                 std::invalid_argument);
}

TEST(OrderTest, RejectsUnknownOrderType) {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    EXPECT_THROW((Order{1, Side::buy, static_cast<OrderType>(255), 100, 1, 1}),
                 std::invalid_argument);
}

TEST(OrderTest, RejectsNonPositivePrice) {
    EXPECT_THROW((Order{1, Side::buy, OrderType::limit, 0, 1, 1}),
                 std::invalid_argument);
    EXPECT_THROW((Order{1, Side::buy, OrderType::limit, -1, 1, 1}),
                 std::invalid_argument);
}

TEST(OrderTest, RejectsNonPositiveQuantity) {
    EXPECT_THROW((Order{1, Side::buy, OrderType::limit, 100, 0, 1}),
                 std::invalid_argument);
    EXPECT_THROW((Order{1, Side::buy, OrderType::limit, 100, -1, 1}),
                 std::invalid_argument);
}

TEST(OrderTest, RejectsZeroSequence) {
    EXPECT_THROW((Order{1, Side::buy, OrderType::limit, 100, 1, 0}),
                 std::invalid_argument);
}

TEST(OrderTest, AppliesPartialFill) {
    Order order{1, Side::sell, OrderType::limit, 100, 10, 1};

    order.apply_fill(4);

    EXPECT_EQ(order.remaining_quantity(), 6);
    EXPECT_FALSE(order.is_filled());
}

TEST(OrderTest, AppliesCompleteFill) {
    Order order{1, Side::sell, OrderType::limit, 100, 10, 1};

    order.apply_fill(10);

    EXPECT_EQ(order.remaining_quantity(), 0);
    EXPECT_TRUE(order.is_filled());
}

TEST(OrderTest, RejectsNonPositiveFill) {
    Order order{1, Side::sell, OrderType::limit, 100, 10, 1};

    EXPECT_THROW(order.apply_fill(0), std::invalid_argument);
    EXPECT_THROW(order.apply_fill(-1), std::invalid_argument);
    EXPECT_EQ(order.remaining_quantity(), 10);
}

TEST(OrderTest, RejectsFillAboveRemainingQuantity) {
    Order order{1, Side::sell, OrderType::limit, 100, 10, 1};

    EXPECT_THROW(order.apply_fill(11), std::invalid_argument);
    EXPECT_EQ(order.remaining_quantity(), 10);
}

TEST(OrderTest, FormatsHumanReadableOutput) {
    const Order order{42, Side::buy, OrderType::limit, 10'125, 50, 7};
    std::ostringstream output;

    output << order;

    EXPECT_EQ(output.str(),
              "Order{id=42, side=BUY, type=LIMIT, price=10125, quantity=50, "
              "remaining=50, sequence=7}");
}

TEST(TradeTest, ConstructsValidTrade) {
    const Trade trade{8, 42, 43, 10'125, 25, 9};

    EXPECT_EQ(trade.id(), 8U);
    EXPECT_EQ(trade.buy_order_id(), 42U);
    EXPECT_EQ(trade.sell_order_id(), 43U);
    EXPECT_EQ(trade.price(), 10'125);
    EXPECT_EQ(trade.quantity(), 25);
    EXPECT_EQ(trade.sequence(), 9U);
}

TEST(TradeTest, RejectsZeroId) {
    EXPECT_THROW((Trade{0, 1, 2, 100, 1, 1}), std::invalid_argument);
}

TEST(TradeTest, RejectsZeroBuyOrderId) {
    EXPECT_THROW((Trade{1, 0, 2, 100, 1, 1}), std::invalid_argument);
}

TEST(TradeTest, RejectsZeroSellOrderId) {
    EXPECT_THROW((Trade{1, 2, 0, 100, 1, 1}), std::invalid_argument);
}

TEST(TradeTest, RejectsIdenticalOrderIds) {
    EXPECT_THROW((Trade{1, 2, 2, 100, 1, 1}), std::invalid_argument);
}

TEST(TradeTest, RejectsNonPositivePrice) {
    EXPECT_THROW((Trade{1, 2, 3, 0, 1, 1}), std::invalid_argument);
    EXPECT_THROW((Trade{1, 2, 3, -1, 1, 1}), std::invalid_argument);
}

TEST(TradeTest, RejectsNonPositiveQuantity) {
    EXPECT_THROW((Trade{1, 2, 3, 100, 0, 1}), std::invalid_argument);
    EXPECT_THROW((Trade{1, 2, 3, 100, -1, 1}), std::invalid_argument);
}

TEST(TradeTest, RejectsZeroSequence) {
    EXPECT_THROW((Trade{1, 2, 3, 100, 1, 0}), std::invalid_argument);
}

TEST(TradeTest, FormatsHumanReadableOutput) {
    const Trade trade{8, 42, 43, 10'125, 25, 9};
    std::ostringstream output;

    output << trade;

    EXPECT_EQ(output.str(),
              "Trade{id=8, buy_order_id=42, sell_order_id=43, price=10125, "
              "quantity=25, sequence=9}");
}

} // namespace
} // namespace matching_engine
