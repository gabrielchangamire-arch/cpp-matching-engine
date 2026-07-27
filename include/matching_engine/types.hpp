#pragma once

#include <cstdint>
#include <iosfwd>
#include <string_view>

namespace matching_engine {

using OrderId = std::uint64_t;
using TradeId = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::int64_t;
using SequenceNumber = std::uint64_t;

enum class Side : std::uint8_t {
    buy,
    sell,
};

enum class OrderType : std::uint8_t {
    limit,
};

[[nodiscard]] std::string_view to_string(Side side);
[[nodiscard]] std::string_view to_string(OrderType order_type);

std::ostream& operator<<(std::ostream& output, Side side);
std::ostream& operator<<(std::ostream& output, OrderType order_type);

} // namespace matching_engine
