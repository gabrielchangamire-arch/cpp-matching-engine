#pragma once

#include "matching_engine/order.hpp"
#include "matching_engine/types.hpp"

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace matching_engine {

class MatchingEngine;

struct BookLevel {
    Price price;
    Quantity quantity;
    std::size_t order_count;

    bool operator==(const BookLevel&) const = default;
};

class OrderBook {
public:
    OrderBook() = default;
    ~OrderBook() = default;

    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = delete;
    OrderBook& operator=(OrderBook&&) = delete;

    void add(Order order);
    [[nodiscard]] bool cancel(OrderId order_id);

    [[nodiscard]] bool contains(OrderId order_id) const;
    [[nodiscard]] std::size_t order_count() const noexcept;
    [[nodiscard]] std::optional<BookLevel> best_bid() const;
    [[nodiscard]] std::optional<BookLevel> best_ask() const;
    [[nodiscard]] std::vector<BookLevel> depth(Side side,
                                               std::size_t max_levels = 0) const;
    [[nodiscard]] std::string snapshot() const;

private:
    struct PriceLevel {
        std::list<Order> orders;
        Quantity total_quantity{0};
    };

    using BidLevels = std::map<Price, PriceLevel, std::greater<Price>>;
    using AskLevels = std::map<Price, PriceLevel, std::less<Price>>;
    using OrderIterator = std::list<Order>::iterator;

    struct OrderLocation {
        Side side;
        Price price;
        OrderIterator order;
    };

    friend class MatchingEngine;

    [[nodiscard]] Order* best_order(Side side);
    void fill(OrderId order_id, Quantity quantity);

    BidLevels bids_;
    AskLevels asks_;
    std::unordered_map<OrderId, OrderLocation> locations_;
};

std::ostream& operator<<(std::ostream& output, const BookLevel& level);
std::ostream& operator<<(std::ostream& output, const OrderBook& book);

}  // namespace matching_engine
