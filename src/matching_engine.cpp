#include "matching_engine/matching_engine.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace matching_engine {

std::vector<Trade> MatchingEngine::submit_limit_order(const OrderId order_id,
                                                       const Side side,
                                                       const Price price,
                                                       const Quantity quantity) {
    if (seen_order_ids_.contains(order_id)) {
        throw std::invalid_argument("duplicate order ID");
    }
    ensure_sequence_available();

    Order incoming{
        order_id, side, OrderType::limit, price, quantity, next_sequence_};

    seen_order_ids_.insert(order_id);
    ++next_sequence_;

    std::vector<Trade> trades;
    trades.reserve(book_.order_count());

    const Side resting_side = side == Side::buy ? Side::sell : Side::buy;
    while (!incoming.is_filled()) {
        Order* const resting = book_.best_order(resting_side);
        if (resting == nullptr || !crosses(incoming, *resting)) {
            break;
        }

        ensure_sequence_available();
        ensure_trade_id_available();

        const Quantity executed_quantity =
            std::min(incoming.remaining_quantity(),
                     resting->remaining_quantity());
        const OrderId resting_id = resting->id();
        const Price execution_price = resting->price();
        const OrderId buy_order_id =
            side == Side::buy ? incoming.id() : resting_id;
        const OrderId sell_order_id =
            side == Side::sell ? incoming.id() : resting_id;

        Trade trade{next_trade_id_,
                    buy_order_id,
                    sell_order_id,
                    execution_price,
                    executed_quantity,
                    next_sequence_};

        incoming.apply_fill(executed_quantity);
        book_.fill(resting_id, executed_quantity);
        trades.push_back(std::move(trade));
        ++next_trade_id_;
        ++next_sequence_;
    }

    if (!incoming.is_filled()) {
        book_.add(std::move(incoming));
    }

    return trades;
}

bool MatchingEngine::cancel(const OrderId order_id) {
    return book_.cancel(order_id);
}

const OrderBook& MatchingEngine::book() const noexcept {
    return book_;
}

bool MatchingEngine::crosses(const Order& incoming, const Order& resting) {
    switch (incoming.side()) {
        case Side::buy:
            return incoming.price() >= resting.price();
        case Side::sell:
            return incoming.price() <= resting.price();
    }

    throw std::logic_error("incoming order contains an invalid side");
}

void MatchingEngine::ensure_sequence_available() const {
    if (next_sequence_ == std::numeric_limits<SequenceNumber>::max()) {
        throw std::overflow_error("event sequence exhausted");
    }
}

void MatchingEngine::ensure_trade_id_available() const {
    if (next_trade_id_ == std::numeric_limits<TradeId>::max()) {
        throw std::overflow_error("trade ID sequence exhausted");
    }
}

}  // namespace matching_engine
