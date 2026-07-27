#pragma once

#include "matching_engine/order_book.hpp"
#include "matching_engine/trade.hpp"
#include "matching_engine/types.hpp"

#include <unordered_set>
#include <vector>

namespace matching_engine {

class MatchingEngine {
public:
    [[nodiscard]] std::vector<Trade> submit_limit_order(OrderId order_id,
                                                        Side side,
                                                        Price price,
                                                        Quantity quantity);
    [[nodiscard]] bool cancel(OrderId order_id);

    [[nodiscard]] const OrderBook& book() const noexcept;

private:
    [[nodiscard]] static bool crosses(const Order& incoming,
                                      const Order& resting);
    void ensure_sequence_available() const;
    void ensure_trade_id_available() const;

    OrderBook book_;
    std::unordered_set<OrderId> seen_order_ids_;
    SequenceNumber next_sequence_{1};
    TradeId next_trade_id_{1};
};

}  // namespace matching_engine
