#pragma once

#include "matching_engine/types.hpp"

#include <iosfwd>

namespace matching_engine {

class Order {
public:
    Order(OrderId id,
          Side side,
          OrderType type,
          Price price,
          Quantity quantity,
          SequenceNumber sequence);

    [[nodiscard]] OrderId id() const noexcept;
    [[nodiscard]] Side side() const noexcept;
    [[nodiscard]] OrderType type() const noexcept;
    [[nodiscard]] Price price() const noexcept;
    [[nodiscard]] Quantity quantity() const noexcept;
    [[nodiscard]] Quantity remaining_quantity() const noexcept;
    [[nodiscard]] SequenceNumber sequence() const noexcept;
    [[nodiscard]] bool is_filled() const noexcept;

    void apply_fill(Quantity fill_quantity);

private:
    OrderId id_;
    Side side_;
    OrderType type_;
    Price price_;
    Quantity quantity_;
    Quantity remaining_quantity_;
    SequenceNumber sequence_;
};

std::ostream& operator<<(std::ostream& output, const Order& order);

}  // namespace matching_engine
