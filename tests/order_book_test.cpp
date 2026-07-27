#include "matching_engine/order_book.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace matching_engine {
namespace {

Order make_order(const OrderId id, const Side side, const Price price,
                 const Quantity quantity, const SequenceNumber sequence = 1) {
    return Order{id, side, OrderType::limit, price, quantity, sequence};
}

TEST(OrderBookTest, StartsEmpty) {
    const OrderBook book;

    EXPECT_EQ(book.order_count(), 0U);
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_TRUE(book.depth(Side::buy).empty());
    EXPECT_TRUE(book.depth(Side::sell).empty());
}

TEST(OrderBookTest, SelectsHighestPriceAsBestBid) {
    OrderBook book;
    book.add(make_order(1, Side::buy, 100, 3));
    book.add(make_order(2, Side::buy, 102, 4));
    book.add(make_order(3, Side::buy, 101, 5));

    EXPECT_EQ(book.best_bid(), (BookLevel{102, 4, 1}));
}

TEST(OrderBookTest, SelectsLowestPriceAsBestAsk) {
    OrderBook book;
    book.add(make_order(1, Side::sell, 102, 3));
    book.add(make_order(2, Side::sell, 100, 4));
    book.add(make_order(3, Side::sell, 101, 5));

    EXPECT_EQ(book.best_ask(), (BookLevel{100, 4, 1}));
}

TEST(OrderBookTest, AggregatesOrdersAtSamePrice) {
    OrderBook book;
    book.add(make_order(1, Side::buy, 100, 3));
    book.add(make_order(2, Side::buy, 100, 4, 2));

    EXPECT_EQ(book.best_bid(), (BookLevel{100, 7, 2}));
    EXPECT_EQ(book.order_count(), 2U);
}

TEST(OrderBookTest, ReturnsDepthInBestPriceOrder) {
    OrderBook book;
    book.add(make_order(1, Side::buy, 99, 1));
    book.add(make_order(2, Side::buy, 101, 2));
    book.add(make_order(3, Side::buy, 100, 3));
    book.add(make_order(4, Side::sell, 104, 4));
    book.add(make_order(5, Side::sell, 102, 5));
    book.add(make_order(6, Side::sell, 103, 6));

    EXPECT_EQ(book.depth(Side::buy),
              (std::vector<BookLevel>{{101, 2, 1}, {100, 3, 1}, {99, 1, 1}}));
    EXPECT_EQ(book.depth(Side::sell),
              (std::vector<BookLevel>{{102, 5, 1}, {103, 6, 1}, {104, 4, 1}}));
}

TEST(OrderBookTest, LimitsReturnedDepth) {
    OrderBook book;
    book.add(make_order(1, Side::buy, 99, 1));
    book.add(make_order(2, Side::buy, 101, 2));
    book.add(make_order(3, Side::buy, 100, 3));

    EXPECT_EQ(book.depth(Side::buy, 2),
              (std::vector<BookLevel>{{101, 2, 1}, {100, 3, 1}}));
}

TEST(OrderBookTest, DetectsDuplicateActiveIdWithoutChangingBook) {
    OrderBook book;
    book.add(make_order(1, Side::buy, 100, 3));

    EXPECT_THROW(book.add(make_order(1, Side::sell, 101, 9)), std::invalid_argument);
    EXPECT_EQ(book.order_count(), 1U);
    EXPECT_EQ(book.best_bid(), (BookLevel{100, 3, 1}));
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookTest, RejectsUnknownCancellationWithoutChangingBook) {
    OrderBook book;
    book.add(make_order(1, Side::buy, 100, 3));

    EXPECT_FALSE(book.cancel(999));
    EXPECT_EQ(book.order_count(), 1U);
}

TEST(OrderBookTest, CancelsOrderAndUpdatesAggregatedQuantity) {
    OrderBook book;
    book.add(make_order(1, Side::buy, 100, 3));
    book.add(make_order(2, Side::buy, 100, 4, 2));

    EXPECT_TRUE(book.cancel(1));
    EXPECT_FALSE(book.contains(1));
    EXPECT_TRUE(book.contains(2));
    EXPECT_EQ(book.best_bid(), (BookLevel{100, 4, 1}));
}

TEST(OrderBookTest, RemovesEmptyPriceLevelAfterCancellation) {
    OrderBook book;
    book.add(make_order(1, Side::sell, 100, 3));
    book.add(make_order(2, Side::sell, 101, 4, 2));

    EXPECT_TRUE(book.cancel(1));
    EXPECT_EQ(book.best_ask(), (BookLevel{101, 4, 1}));
}

TEST(OrderBookTest, KeepsBidAndAskSidesIndependent) {
    OrderBook book;
    book.add(make_order(1, Side::buy, 100, 3));
    book.add(make_order(2, Side::sell, 100, 4, 2));

    EXPECT_EQ(book.best_bid(), (BookLevel{100, 3, 1}));
    EXPECT_EQ(book.best_ask(), (BookLevel{100, 4, 1}));
}

TEST(OrderBookTest, AggregatesOnlyRemainingQuantity) {
    Order partially_filled = make_order(1, Side::buy, 100, 10);
    partially_filled.apply_fill(4);
    OrderBook book;

    book.add(partially_filled);

    EXPECT_EQ(book.best_bid(), (BookLevel{100, 6, 1}));
}

TEST(OrderBookTest, RejectsPriceLevelQuantityOverflow) {
    OrderBook book;
    book.add(make_order(1, Side::buy, 100, std::numeric_limits<Quantity>::max()));

    EXPECT_THROW(book.add(make_order(2, Side::buy, 100, 1, 2)), std::overflow_error);
    EXPECT_EQ(book.order_count(), 1U);
    EXPECT_FALSE(book.contains(2));
}

TEST(OrderBookTest, RejectsInvalidDepthSide) {
    const OrderBook book;

    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    EXPECT_THROW(static_cast<void>(book.depth(static_cast<Side>(255))),
                 std::invalid_argument);
}

TEST(OrderBookTest, PrintsReadableSnapshot) {
    OrderBook book;
    book.add(make_order(1, Side::buy, 10'000, 5));
    book.add(make_order(2, Side::sell, 10'100, 7, 2));

    EXPECT_EQ(book.snapshot(), "Order book (2 orders)\n"
                               "ASKS (best first)\n"
                               "  10100 | qty 7 | orders 1\n"
                               "BIDS (best first)\n"
                               "  10000 | qty 5 | orders 1\n");
}

} // namespace
} // namespace matching_engine
