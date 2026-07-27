#pragma once

#include "matching_engine/types.hpp"

#include <iosfwd>

namespace matching_engine {

class Trade {
public:
    Trade(TradeId id,
          OrderId buy_order_id,
          OrderId sell_order_id,
          Price price,
          Quantity quantity,
          SequenceNumber sequence);

    [[nodiscard]] TradeId id() const noexcept;
    [[nodiscard]] OrderId buy_order_id() const noexcept;
    [[nodiscard]] OrderId sell_order_id() const noexcept;
    [[nodiscard]] Price price() const noexcept;
    [[nodiscard]] Quantity quantity() const noexcept;
    [[nodiscard]] SequenceNumber sequence() const noexcept;

private:
    TradeId id_;
    OrderId buy_order_id_;
    OrderId sell_order_id_;
    Price price_;
    Quantity quantity_;
    SequenceNumber sequence_;
};

std::ostream& operator<<(std::ostream& output, const Trade& trade);

}  // namespace matching_engine
