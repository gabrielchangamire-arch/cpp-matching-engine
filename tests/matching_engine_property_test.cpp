#include "matching_engine/matching_engine.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <gtest/gtest.h>
#include <map>
#include <random>
#include <utility>
#include <vector>

namespace matching_engine {
namespace {

struct ReferenceOrder {
    OrderId id;
    Side side;
    Price price;
    Quantity remaining_quantity;
    SequenceNumber sequence;
};

class ReferenceEngine {
  public:
    std::vector<Trade> submit(const OrderId order_id, const Side side,
                              const Price price, const Quantity quantity) {
        const SequenceNumber order_sequence = next_sequence_++;
        Quantity incoming_quantity = quantity;
        std::vector<Trade> trades;

        while (incoming_quantity > 0) {
            const auto resting = best_opposite_order(side);
            if (resting == orders_.end() || !crosses(side, price, resting->price)) {
                break;
            }

            const Quantity executed =
                std::min(incoming_quantity, resting->remaining_quantity);
            const OrderId buy_order_id = side == Side::buy ? order_id : resting->id;
            const OrderId sell_order_id = side == Side::sell ? order_id : resting->id;
            trades.emplace_back(next_trade_id_++, buy_order_id, sell_order_id,
                                resting->price, executed, next_sequence_++);

            incoming_quantity -= executed;
            resting->remaining_quantity -= executed;
            if (resting->remaining_quantity == 0) {
                orders_.erase(resting);
            }
        }

        if (incoming_quantity > 0) {
            orders_.push_back(ReferenceOrder{order_id, side, price, incoming_quantity,
                                             order_sequence});
        }
        return trades;
    }

    bool cancel(const OrderId order_id) {
        const auto order = std::find_if(orders_.begin(), orders_.end(),
                                        [order_id](const ReferenceOrder& candidate) {
                                            return candidate.id == order_id;
                                        });
        if (order == orders_.end()) {
            return false;
        }
        orders_.erase(order);
        return true;
    }

    [[nodiscard]] std::vector<BookLevel> depth(const Side side) const {
        using AggregatedLevel = std::pair<Quantity, std::size_t>;
        std::map<Price, AggregatedLevel, std::greater<>> bids;
        std::map<Price, AggregatedLevel, std::less<>> asks;

        for (const ReferenceOrder& order : orders_) {
            AggregatedLevel* aggregate = nullptr;
            if (order.side == Side::buy) {
                aggregate = &bids[order.price];
            } else {
                aggregate = &asks[order.price];
            }
            auto& [quantity, count] = *aggregate;
            quantity += order.remaining_quantity;
            ++count;
        }

        std::vector<BookLevel> result;
        if (side == Side::buy) {
            result.reserve(bids.size());
            for (const auto& [price, aggregate] : bids) {
                result.push_back(BookLevel{price, aggregate.first, aggregate.second});
            }
        } else {
            result.reserve(asks.size());
            for (const auto& [price, aggregate] : asks) {
                result.push_back(BookLevel{price, aggregate.first, aggregate.second});
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<OrderId> active_order_ids() const {
        std::vector<OrderId> ids;
        ids.reserve(orders_.size());
        for (const ReferenceOrder& order : orders_) {
            ids.push_back(order.id);
        }
        return ids;
    }

    [[nodiscard]] std::size_t order_count() const noexcept {
        return orders_.size();
    }

  private:
    using OrderIterator = std::vector<ReferenceOrder>::iterator;

    OrderIterator best_opposite_order(const Side incoming_side) {
        OrderIterator best = orders_.end();
        for (auto candidate = orders_.begin(); candidate != orders_.end();
             ++candidate) {
            if (candidate->side == incoming_side) {
                continue;
            }
            if (best == orders_.end() ||
                has_priority(*candidate, *best, incoming_side)) {
                best = candidate;
            }
        }
        return best;
    }

    static bool has_priority(const ReferenceOrder& candidate,
                             const ReferenceOrder& current, const Side incoming_side) {
        if (candidate.price == current.price) {
            return candidate.sequence < current.sequence;
        }
        if (incoming_side == Side::buy) {
            return candidate.price < current.price;
        }
        return candidate.price > current.price;
    }

    static bool crosses(const Side incoming_side, const Price incoming_price,
                        const Price resting_price) {
        if (incoming_side == Side::buy) {
            return incoming_price >= resting_price;
        }
        return incoming_price <= resting_price;
    }

    std::vector<ReferenceOrder> orders_;
    SequenceNumber next_sequence_{1};
    TradeId next_trade_id_{1};
};

void expect_equal_trades(const std::vector<Trade>& actual,
                         const std::vector<Trade>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t index = 0; index < actual.size(); ++index) {
        EXPECT_EQ(actual[index].id(), expected[index].id());
        EXPECT_EQ(actual[index].buy_order_id(), expected[index].buy_order_id());
        EXPECT_EQ(actual[index].sell_order_id(), expected[index].sell_order_id());
        EXPECT_EQ(actual[index].price(), expected[index].price());
        EXPECT_EQ(actual[index].quantity(), expected[index].quantity());
        EXPECT_EQ(actual[index].sequence(), expected[index].sequence());
    }
}

void expect_equal_book(const OrderBook& actual, const ReferenceEngine& expected) {
    const auto expected_bids = expected.depth(Side::buy);
    const auto expected_asks = expected.depth(Side::sell);

    EXPECT_EQ(actual.order_count(), expected.order_count());
    EXPECT_EQ(actual.depth(Side::buy), expected_bids);
    EXPECT_EQ(actual.depth(Side::sell), expected_asks);

    if (expected_bids.empty()) {
        EXPECT_FALSE(actual.best_bid().has_value());
    } else {
        EXPECT_EQ(actual.best_bid(), expected_bids.front());
    }
    if (expected_asks.empty()) {
        EXPECT_FALSE(actual.best_ask().has_value());
    } else {
        EXPECT_EQ(actual.best_ask(), expected_asks.front());
    }

    const auto best_bid = actual.best_bid();
    const auto best_ask = actual.best_ask();
    if (best_bid.has_value() && best_ask.has_value()) {
        EXPECT_LT(best_bid->price, best_ask->price);
    }
}

TEST(MatchingEnginePropertyTest, MatchesReferenceModelAcrossRandomizedStreams) {
    constexpr std::size_t seed_count = 24;
    constexpr std::size_t commands_per_seed = 500;

    for (std::size_t seed = 1; seed <= seed_count; ++seed) {
        std::mt19937_64 random{seed};
        std::uniform_int_distribution<int> operation_distribution{0, 99};
        std::uniform_int_distribution<int> side_distribution{0, 1};
        std::uniform_int_distribution<Price> price_distribution{9'950, 10'050};
        std::uniform_int_distribution<Quantity> quantity_distribution{1, 50};

        MatchingEngine actual;
        ReferenceEngine expected;
        OrderId next_order_id = 1;

        for (std::size_t command_index = 0; command_index < commands_per_seed;
             ++command_index) {
            SCOPED_TRACE(testing::Message{} << "seed=" << seed
                                            << " command=" << command_index);
            const int operation = operation_distribution(random);

            if (operation < 20) {
                const auto active_ids = expected.active_order_ids();
                OrderId order_id = next_order_id + 10'000;
                if (!active_ids.empty() && operation < 15) {
                    std::uniform_int_distribution<std::size_t> id_distribution{
                        0, active_ids.size() - 1};
                    order_id = active_ids[id_distribution(random)];
                }
                EXPECT_EQ(actual.cancel(order_id), expected.cancel(order_id));
            } else {
                const Side side =
                    side_distribution(random) == 0 ? Side::buy : Side::sell;
                const Price price = price_distribution(random);
                const Quantity quantity = quantity_distribution(random);
                const auto actual_trades =
                    actual.submit_limit_order(next_order_id, side, price, quantity);
                const auto expected_trades =
                    expected.submit(next_order_id, side, price, quantity);
                expect_equal_trades(actual_trades, expected_trades);
                ++next_order_id;
            }

            expect_equal_book(actual.book(), expected);
        }
    }
}

} // namespace
} // namespace matching_engine
