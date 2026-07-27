#pragma once

#include "matching_engine/types.hpp"

#include <variant>

namespace matching_engine {

struct AddCommand {
    OrderId order_id;
    Side side;
    Price price;
    Quantity quantity;

    bool operator==(const AddCommand&) const = default;
};

struct CancelCommand {
    OrderId order_id;

    bool operator==(const CancelCommand&) const = default;
};

using Command = std::variant<AddCommand, CancelCommand>;

}  // namespace matching_engine
