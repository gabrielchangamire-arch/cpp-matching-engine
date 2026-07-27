#include "matching_engine/matching_engine.hpp"

#include <iostream>

int main() {
    matching_engine::MatchingEngine engine;
    static_cast<void>(
        engine.submit_limit_order(1, matching_engine::Side::sell, 10'100, 5));
    const auto trades =
        engine.submit_limit_order(2, matching_engine::Side::buy, 10'100, 3);

    std::cout << "C++ Matching Engine\n";
    for (const auto& trade : trades) {
        std::cout << trade << '\n';
    }
    std::cout << engine.book();
    return 0;
}
