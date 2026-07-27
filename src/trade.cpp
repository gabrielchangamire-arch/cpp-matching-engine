#include "matching_engine/trade.hpp"

#include <ostream>
#include <stdexcept>

namespace matching_engine {

Trade::Trade(const TradeId id, const OrderId buy_order_id, const OrderId sell_order_id,
             const Price price, const Quantity quantity, const SequenceNumber sequence)
    : id_(id), buy_order_id_(buy_order_id), sell_order_id_(sell_order_id),
      price_(price), quantity_(quantity), sequence_(sequence) {
    if (id_ == 0) {
        throw std::invalid_argument("trade ID must be positive");
    }
    if (buy_order_id_ == 0 || sell_order_id_ == 0) {
        throw std::invalid_argument("trade order IDs must be positive");
    }
    if (buy_order_id_ == sell_order_id_) {
        throw std::invalid_argument("buy and sell order IDs must differ");
    }
    if (price_ <= 0) {
        throw std::invalid_argument("trade price must be positive");
    }
    if (quantity_ <= 0) {
        throw std::invalid_argument("trade quantity must be positive");
    }
    if (sequence_ == 0) {
        throw std::invalid_argument("trade sequence must be positive");
    }
}

TradeId Trade::id() const noexcept {
    return id_;
}

OrderId Trade::buy_order_id() const noexcept {
    return buy_order_id_;
}

OrderId Trade::sell_order_id() const noexcept {
    return sell_order_id_;
}

Price Trade::price() const noexcept {
    return price_;
}

Quantity Trade::quantity() const noexcept {
    return quantity_;
}

SequenceNumber Trade::sequence() const noexcept {
    return sequence_;
}

std::ostream& operator<<(std::ostream& output, const Trade& trade) {
    return output << "Trade{id=" << trade.id()
                  << ", buy_order_id=" << trade.buy_order_id()
                  << ", sell_order_id=" << trade.sell_order_id()
                  << ", price=" << trade.price() << ", quantity=" << trade.quantity()
                  << ", sequence=" << trade.sequence() << '}';
}

} // namespace matching_engine
