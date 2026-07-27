#include "matching_engine/order_book.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace matching_engine {

void OrderBook::add(Order order) {
    if (contains(order.id())) {
        throw std::invalid_argument("duplicate active order ID");
    }

    const auto add_to_levels = [this, &order](auto& levels) {
        auto [level, level_was_inserted] = levels.try_emplace(order.price());
        const Quantity remaining = order.remaining_quantity();
        if (remaining >
            std::numeric_limits<Quantity>::max() - level->second.total_quantity) {
            if (level_was_inserted) {
                levels.erase(level);
            }
            throw std::overflow_error("price-level quantity overflow");
        }

        level->second.orders.push_back(order);
        const auto inserted_order = std::prev(level->second.orders.end());

        try {
            locations_.emplace(inserted_order->id(),
                               OrderLocation{inserted_order->side(),
                                             inserted_order->price(), inserted_order});
        } catch (...) {
            level->second.orders.pop_back();
            if (level->second.orders.empty()) {
                levels.erase(level);
            }
            throw;
        }

        level->second.total_quantity += remaining;
    };

    switch (order.side()) {
    case Side::buy:
        add_to_levels(bids_);
        return;
    case Side::sell:
        add_to_levels(asks_);
        return;
    }

    throw std::invalid_argument("invalid order side");
}

bool OrderBook::cancel(const OrderId order_id) {
    const auto location = locations_.find(order_id);
    if (location == locations_.end()) {
        return false;
    }

    const auto erase_from_levels = [&location](auto& levels) {
        const auto level = levels.find(location->second.price);
        if (level == levels.end()) {
            throw std::logic_error("order location references a missing level");
        }

        level->second.total_quantity -= location->second.order->remaining_quantity();
        level->second.orders.erase(location->second.order);
        if (level->second.orders.empty()) {
            levels.erase(level);
        }
    };

    switch (location->second.side) {
    case Side::buy:
        erase_from_levels(bids_);
        break;
    case Side::sell:
        erase_from_levels(asks_);
        break;
    }

    locations_.erase(location);
    return true;
}

bool OrderBook::contains(const OrderId order_id) const {
    return locations_.contains(order_id);
}

std::size_t OrderBook::order_count() const noexcept {
    return locations_.size();
}

std::optional<BookLevel> OrderBook::best_bid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }

    const auto& [price, level] = *bids_.begin();
    return BookLevel{price, level.total_quantity, level.orders.size()};
}

std::optional<BookLevel> OrderBook::best_ask() const {
    if (asks_.empty()) {
        return std::nullopt;
    }

    const auto& [price, level] = *asks_.begin();
    return BookLevel{price, level.total_quantity, level.orders.size()};
}

std::vector<BookLevel> OrderBook::depth(const Side side,
                                        const std::size_t max_levels) const {
    std::vector<BookLevel> result;

    const auto append_levels = [&result, max_levels](const auto& levels) {
        const std::size_t count =
            max_levels == 0 ? levels.size() : std::min(max_levels, levels.size());
        result.reserve(count);

        for (const auto& [price, level] : levels) {
            if (max_levels != 0 && result.size() == max_levels) {
                break;
            }
            result.push_back(
                BookLevel{price, level.total_quantity, level.orders.size()});
        }
    };

    switch (side) {
    case Side::buy:
        append_levels(bids_);
        return result;
    case Side::sell:
        append_levels(asks_);
        return result;
    }

    throw std::invalid_argument("invalid order side");
}

std::string OrderBook::snapshot() const {
    std::ostringstream output;
    output << "Order book (" << order_count()
           << (order_count() == 1 ? " order)\n" : " orders)\n");

    output << "ASKS (best first)\n";
    const auto asks = depth(Side::sell);
    if (asks.empty()) {
        output << "  (empty)\n";
    } else {
        for (const BookLevel& level : asks) {
            output << "  " << level << '\n';
        }
    }

    output << "BIDS (best first)\n";
    const auto bids = depth(Side::buy);
    if (bids.empty()) {
        output << "  (empty)\n";
    } else {
        for (const BookLevel& level : bids) {
            output << "  " << level << '\n';
        }
    }

    return output.str();
}

Order* OrderBook::best_order(const Side side) {
    switch (side) {
    case Side::buy:
        return bids_.empty() ? nullptr : &bids_.begin()->second.orders.front();
    case Side::sell:
        return asks_.empty() ? nullptr : &asks_.begin()->second.orders.front();
    }

    throw std::invalid_argument("invalid order side");
}

void OrderBook::fill(const OrderId order_id, const Quantity quantity) {
    const auto location = locations_.find(order_id);
    if (location == locations_.end()) {
        throw std::logic_error("cannot fill an order that is not active");
    }

    const auto fill_in_levels = [this, &location, quantity](auto& levels) {
        const auto level = levels.find(location->second.price);
        if (level == levels.end()) {
            throw std::logic_error("order location references a missing level");
        }

        location->second.order->apply_fill(quantity);
        level->second.total_quantity -= quantity;

        if (location->second.order->is_filled()) {
            level->second.orders.erase(location->second.order);
            locations_.erase(location);
            if (level->second.orders.empty()) {
                levels.erase(level);
            }
        }
    };

    switch (location->second.side) {
    case Side::buy:
        fill_in_levels(bids_);
        return;
    case Side::sell:
        fill_in_levels(asks_);
        return;
    }

    throw std::logic_error("order location contains an invalid side");
}

std::ostream& operator<<(std::ostream& output, const BookLevel& level) {
    return output << level.price << " | qty " << level.quantity << " | orders "
                  << level.order_count;
}

std::ostream& operator<<(std::ostream& output, const OrderBook& book) {
    return output << book.snapshot();
}

} // namespace matching_engine
