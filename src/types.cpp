#include "matching_engine/types.hpp"

#include <ostream>
#include <stdexcept>

namespace matching_engine {

std::string_view to_string(const Side side) {
    switch (side) {
    case Side::buy:
        return "BUY";
    case Side::sell:
        return "SELL";
    }

    throw std::invalid_argument("invalid order side");
}

std::string_view to_string(const OrderType order_type) {
    switch (order_type) {
    case OrderType::limit:
        return "LIMIT";
    }

    throw std::invalid_argument("invalid order type");
}

std::ostream& operator<<(std::ostream& output, const Side side) {
    return output << to_string(side);
}

std::ostream& operator<<(std::ostream& output, const OrderType order_type) {
    return output << to_string(order_type);
}

} // namespace matching_engine
