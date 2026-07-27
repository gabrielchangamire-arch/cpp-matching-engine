#include "matching_engine/order.hpp"

#include <iostream>

int main() {
    const matching_engine::Order example_order{
        1,
        matching_engine::Side::buy,
        matching_engine::OrderType::limit,
        10'000,
        100,
        1,
    };

    std::cout << "C++ Matching Engine\n" << example_order << '\n';
    return 0;
}
