#include "matching_engine/concurrent_matching_engine.hpp"
#include "matching_engine/matching_engine.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace matching_engine {
namespace {

constexpr std::int64_t operations_per_batch = 256;

void BM_OrderInsertion(benchmark::State& state) {
    const auto initial_book_size = static_cast<OrderId>(state.range(0));

    for (auto iteration : state) {
        static_cast<void>(iteration);
        state.PauseTiming();
        auto engine = std::make_unique<MatchingEngine>();
        for (OrderId id = 1; id <= initial_book_size; ++id) {
            benchmark::DoNotOptimize(
                engine->submit_limit_order(id, Side::buy, 10'000, 1));
        }
        OrderId next_id = initial_book_size + 1;
        state.ResumeTiming();

        for (std::int64_t index = 0; index < operations_per_batch; ++index) {
            benchmark::DoNotOptimize(
                engine->submit_limit_order(next_id++, Side::buy, 10'000, 1));
        }

        state.PauseTiming();
        benchmark::ClobberMemory();
        engine.reset();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * operations_per_batch);
}

void BM_CrossingMatch(benchmark::State& state) {
    const auto resting_orders = static_cast<OrderId>(state.range(0));

    for (auto iteration : state) {
        static_cast<void>(iteration);
        state.PauseTiming();
        auto engine = std::make_unique<MatchingEngine>();
        for (OrderId id = 1; id <= resting_orders; ++id) {
            benchmark::DoNotOptimize(
                engine->submit_limit_order(id, Side::sell, 10'000, 1));
        }
        state.ResumeTiming();

        benchmark::DoNotOptimize(engine->submit_limit_order(
            resting_orders + 1,
            Side::buy,
            10'000,
            static_cast<Quantity>(resting_orders)));

        state.PauseTiming();
        benchmark::ClobberMemory();
        engine.reset();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

void BM_Cancellation(benchmark::State& state) {
    const auto initial_book_size = static_cast<OrderId>(state.range(0));

    for (auto iteration : state) {
        static_cast<void>(iteration);
        state.PauseTiming();
        auto engine = std::make_unique<MatchingEngine>();
        for (OrderId id = 1; id <= initial_book_size; ++id) {
            benchmark::DoNotOptimize(
                engine->submit_limit_order(id, Side::buy, 10'000, 1));
        }
        state.ResumeTiming();

        benchmark::DoNotOptimize(engine->cancel(initial_book_size));

        state.PauseTiming();
        benchmark::ClobberMemory();
        engine.reset();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations());
}

void BM_BestBidAskLookup(benchmark::State& state) {
    MatchingEngine engine;
    const auto initial_book_size = static_cast<OrderId>(state.range(0));
    for (OrderId id = 1; id <= initial_book_size; ++id) {
        const Price price = 10'000 - static_cast<Price>(id % 100);
        benchmark::DoNotOptimize(
            engine.submit_limit_order(id, Side::buy, price, 1));
    }
    benchmark::DoNotOptimize(engine.submit_limit_order(
        initial_book_size + 1, Side::sell, 10'100, 1));

    for (auto iteration : state) {
        static_cast<void>(iteration);
        benchmark::DoNotOptimize(engine.book().best_bid());
        benchmark::DoNotOptimize(engine.book().best_ask());
    }

    state.SetItemsProcessed(state.iterations() * 2);
}

void BM_MixedWorkload(benchmark::State& state) {
    constexpr OrderId orders_per_group = 250;
    constexpr std::int64_t total_operations = orders_per_group * 4;

    for (auto iteration : state) {
        static_cast<void>(iteration);
        state.PauseTiming();
        auto engine = std::make_unique<MatchingEngine>();
        state.ResumeTiming();

        for (OrderId id = 1; id <= orders_per_group * 2; ++id) {
            benchmark::DoNotOptimize(
                engine->submit_limit_order(id, Side::buy, 10'000, 1));
        }
        for (OrderId id = orders_per_group * 2 + 1;
             id <= orders_per_group * 3;
             ++id) {
            benchmark::DoNotOptimize(
                engine->submit_limit_order(id, Side::sell, 10'000, 1));
        }
        for (OrderId id = orders_per_group + 1;
             id <= orders_per_group * 2;
             ++id) {
            benchmark::DoNotOptimize(engine->cancel(id));
        }

        state.PauseTiming();
        benchmark::ClobberMemory();
        engine.reset();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * total_operations);
}

void BM_SingleProducerIngestion(benchmark::State& state) {
    const auto command_count = static_cast<std::size_t>(state.range(0));

    for (auto iteration : state) {
        static_cast<void>(iteration);
        state.PauseTiming();
        auto engine = std::make_unique<ConcurrentMatchingEngine>(command_count);
        std::vector<std::future<CommandResult>> futures;
        futures.reserve(command_count);
        state.ResumeTiming();

        for (std::size_t index = 0; index < command_count; ++index) {
            const auto sequence = static_cast<IngestionSequence>(index + 1);
            futures.push_back(engine->submit(
                sequence,
                AddCommand{sequence, Side::buy, 10'000, 1}));
        }
        for (auto& future : futures) {
            benchmark::DoNotOptimize(future.get());
        }
        engine->shutdown();

        state.PauseTiming();
        benchmark::ClobberMemory();
        engine.reset();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

void BM_MultipleProducerIngestion(benchmark::State& state) {
    const auto command_count = static_cast<std::size_t>(state.range(0));
    constexpr std::size_t producer_count = 4;

    for (auto iteration : state) {
        static_cast<void>(iteration);
        state.PauseTiming();
        auto engine = std::make_unique<ConcurrentMatchingEngine>(command_count);
        std::vector<std::optional<std::future<CommandResult>>> futures(
            command_count);
        std::vector<std::thread> producers;
        producers.reserve(producer_count);
        state.ResumeTiming();

        for (std::size_t producer = 0; producer < producer_count; ++producer) {
            producers.emplace_back([&, producer] {
                for (std::size_t index = producer; index < command_count;
                     index += producer_count) {
                    const auto sequence =
                        static_cast<IngestionSequence>(index + 1);
                    futures[index] = engine->submit(
                        sequence,
                        AddCommand{sequence, Side::buy, 10'000, 1});
                }
            });
        }
        for (auto& producer : producers) {
            producer.join();
        }
        for (auto& future : futures) {
            benchmark::DoNotOptimize(future->get());
        }
        engine->shutdown();

        state.PauseTiming();
        benchmark::ClobberMemory();
        engine.reset();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(BM_OrderInsertion)->Arg(100)->Arg(1'000)->Iterations(100);
BENCHMARK(BM_CrossingMatch)->Arg(1)->Arg(100)->Arg(1'000)->Iterations(100);
BENCHMARK(BM_Cancellation)->Arg(100)->Arg(1'000)->Iterations(1'000);
BENCHMARK(BM_BestBidAskLookup)->Arg(100)->Arg(1'000);
BENCHMARK(BM_MixedWorkload);
BENCHMARK(BM_SingleProducerIngestion)->Arg(100)->Arg(1'000);
BENCHMARK(BM_MultipleProducerIngestion)->Arg(100)->Arg(1'000);

}  // namespace
}  // namespace matching_engine

BENCHMARK_MAIN();
