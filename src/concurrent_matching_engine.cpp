#include "matching_engine/concurrent_matching_engine.hpp"

#include <map>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace matching_engine {

ConcurrentMatchingEngine::ConcurrentMatchingEngine(const std::size_t queue_capacity)
    : queue_(queue_capacity), worker_(&ConcurrentMatchingEngine::run, this) {}

ConcurrentMatchingEngine::~ConcurrentMatchingEngine() {
    shutdown();
}

std::future<CommandResult> ConcurrentMatchingEngine::submit(
    const IngestionSequence ingestion_sequence,
    Command command) {
    if (ingestion_sequence == 0) {
        throw std::invalid_argument("ingestion sequence must be positive");
    }

    {
        std::lock_guard lock{state_mutex_};
        if (!accepting_) {
            throw std::runtime_error("concurrent matching engine is shut down");
        }
        if (!submitted_sequences_.insert(ingestion_sequence).second) {
            throw std::invalid_argument("duplicate ingestion sequence");
        }
    }

    Envelope envelope{ingestion_sequence, std::move(command), {}};
    std::future<CommandResult> result = envelope.completion.get_future();
    if (!queue_.push(std::move(envelope))) {
        std::lock_guard lock{state_mutex_};
        submitted_sequences_.erase(ingestion_sequence);
        throw std::runtime_error("concurrent matching engine stopped accepting");
    }
    return result;
}

void ConcurrentMatchingEngine::shutdown() {
    std::lock_guard shutdown_lock{shutdown_mutex_};
    {
        std::lock_guard lock{state_mutex_};
        if (!accepting_ && worker_finished_) {
            return;
        }
        accepting_ = false;
    }

    queue_.close();
    if (worker_.joinable()) {
        worker_.join();
    }

    {
        std::lock_guard lock{state_mutex_};
        worker_finished_ = true;
    }
}

std::string ConcurrentMatchingEngine::book_snapshot() const {
    std::lock_guard lock{state_mutex_};
    if (!worker_finished_) {
        throw std::logic_error("book snapshot requires completed shutdown");
    }
    return engine_.book().snapshot();
}

void ConcurrentMatchingEngine::run() {
    IngestionSequence next_sequence = 1;
    std::map<IngestionSequence, Envelope> pending;

    while (true) {
        auto ready = pending.find(next_sequence);
        if (ready != pending.end()) {
            Envelope envelope = std::move(ready->second);
            pending.erase(ready);
            process(std::move(envelope));
            ++next_sequence;
            continue;
        }

        auto envelope = queue_.pop();
        if (!envelope.has_value()) {
            break;
        }
        pending.emplace(envelope->ingestion_sequence, std::move(*envelope));
    }

    for (auto& [sequence, envelope] : pending) {
        static_cast<void>(sequence);
        reject(std::move(envelope),
               "missing an earlier contiguous ingestion sequence");
    }
}

void ConcurrentMatchingEngine::process(Envelope envelope) {
    CommandResult result{envelope.ingestion_sequence, false, {}, {}};
    try {
        std::visit(
            [this, &result](const auto& command) {
                using CommandType = std::decay_t<decltype(command)>;
                if constexpr (std::is_same_v<CommandType, AddCommand>) {
                    result.trades = engine_.submit_limit_order(command.order_id,
                                                                command.side,
                                                                command.price,
                                                                command.quantity);
                } else {
                    if (!engine_.cancel(command.order_id)) {
                        throw std::invalid_argument(
                            "cannot cancel unknown active order ID");
                    }
                }
            },
            envelope.command);
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    envelope.completion.set_value(std::move(result));
}

void ConcurrentMatchingEngine::reject(Envelope envelope, std::string error) {
    envelope.completion.set_value(CommandResult{envelope.ingestion_sequence,
                                                 false,
                                                 {},
                                                 std::move(error)});
}

}  // namespace matching_engine
