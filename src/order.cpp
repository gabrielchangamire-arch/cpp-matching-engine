#include "matching_engine/order.hpp"

#include <ostream>
#include <stdexcept>

namespace matching_engine {

Order::Order(const OrderId id, const Side side, const OrderType type, const Price price,
             const Quantity quantity, const SequenceNumber sequence)
    : id_(id), side_(side), type_(type), price_(price), quantity_(quantity),
      remaining_quantity_(quantity), sequence_(sequence) {
    if (id_ == 0) {
        throw std::invalid_argument("order ID must be positive");
    }
    static_cast<void>(to_string(side_));
    static_cast<void>(to_string(type_));
    if (price_ <= 0) {
        throw std::invalid_argument("order price must be positive");
    }
    if (quantity_ <= 0) {
        throw std::invalid_argument("order quantity must be positive");
    }
    if (sequence_ == 0) {
        throw std::invalid_argument("order sequence must be positive");
    }
}

OrderId Order::id() const noexcept {
    return id_;
}

Side Order::side() const noexcept {
    return side_;
}

OrderType Order::type() const noexcept {
    return type_;
}

Price Order::price() const noexcept {
    return price_;
}

Quantity Order::quantity() const noexcept {
    return quantity_;
}

Quantity Order::remaining_quantity() const noexcept {
    return remaining_quantity_;
}

SequenceNumber Order::sequence() const noexcept {
    return sequence_;
}

bool Order::is_filled() const noexcept {
    return remaining_quantity_ == 0;
}

void Order::apply_fill(const Quantity fill_quantity) {
    if (fill_quantity <= 0) {
        throw std::invalid_argument("fill quantity must be positive");
    }
    if (fill_quantity > remaining_quantity_) {
        throw std::invalid_argument("fill quantity exceeds remaining quantity");
    }

    remaining_quantity_ -= fill_quantity;
}

std::ostream& operator<<(std::ostream& output, const Order& order) {
    return output << "Order{id=" << order.id() << ", side=" << order.side()
                  << ", type=" << order.type() << ", price=" << order.price()
                  << ", quantity=" << order.quantity()
                  << ", remaining=" << order.remaining_quantity()
                  << ", sequence=" << order.sequence() << '}';
}

} // namespace matching_engine
