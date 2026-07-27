#include "matching_engine/concurrent_matching_engine.hpp"

#include <limits>
#include <map>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace matching_engine {

ConcurrentMatchingEngine::ConcurrentMatchingEngine(
    const std::size_t queue_capacity, const IngestionSequence max_sequence_gap)
    : queue_(queue_capacity), max_sequence_gap_(max_sequence_gap),
      worker_(&ConcurrentMatchingEngine::run, this) {}

ConcurrentMatchingEngine::~ConcurrentMatchingEngine() {
    shutdown();
}

std::future<CommandResult>
ConcurrentMatchingEngine::submit(const IngestionSequence ingestion_sequence,
                                 Command command) {
    if (ingestion_sequence == 0) {
        throw std::invalid_argument("ingestion sequence must be positive");
    }

    {
        std::scoped_lock lock{state_mutex_};
        if (!accepting_) {
            throw std::runtime_error("concurrent matching engine is shut down");
        }
        if (ingestion_sequence < next_ingestion_sequence_) {
            throw std::invalid_argument("stale ingestion sequence");
        }
        if (ingestion_sequence - next_ingestion_sequence_ > max_sequence_gap_) {
            throw std::out_of_range("ingestion sequence exceeds the configured gap");
        }
        if (!submitted_sequences_.insert(ingestion_sequence).second) {
            throw std::invalid_argument("duplicate ingestion sequence");
        }
    }

    Envelope envelope{ingestion_sequence, command, {}};
    std::future<CommandResult> result = envelope.completion.get_future();
    if (!queue_.push(std::move(envelope))) {
        std::scoped_lock lock{state_mutex_};
        submitted_sequences_.erase(ingestion_sequence);
        throw std::runtime_error("concurrent matching engine stopped accepting");
    }
    return result;
}

void ConcurrentMatchingEngine::shutdown() {
    std::scoped_lock shutdown_lock{shutdown_mutex_};
    {
        std::scoped_lock lock{state_mutex_};
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
        std::scoped_lock lock{state_mutex_};
        worker_finished_ = true;
    }
}

std::string ConcurrentMatchingEngine::book_snapshot() const {
    std::scoped_lock lock{state_mutex_};
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
            const IngestionSequence processed_sequence = envelope.ingestion_sequence;
            process(std::move(envelope));

            bool sequence_space_exhausted = false;
            {
                std::scoped_lock lock{state_mutex_};
                submitted_sequences_.erase(processed_sequence);
                if (next_sequence == std::numeric_limits<IngestionSequence>::max()) {
                    accepting_ = false;
                    sequence_space_exhausted = true;
                } else {
                    ++next_sequence;
                    next_ingestion_sequence_ = next_sequence;
                }
            }
            if (sequence_space_exhausted) {
                queue_.close();
            }
            continue;
        }

        auto envelope = queue_.pop();
        if (!envelope.has_value()) {
            break;
        }
        pending.emplace(envelope->ingestion_sequence, std::move(*envelope));
    }

    for (auto& [sequence, envelope] : pending) {
        {
            std::scoped_lock lock{state_mutex_};
            submitted_sequences_.erase(sequence);
        }
        reject(std::move(envelope), "missing an earlier contiguous ingestion sequence");
    }
}

void ConcurrentMatchingEngine::process(Envelope envelope) {
    CommandResult result{envelope.ingestion_sequence, false, {}, {}};
    try {
        std::visit(
            [this, &result](const auto& command) {
                using CommandType = std::decay_t<decltype(command)>;
                if constexpr (std::is_same_v<CommandType, AddCommand>) {
                    result.trades =
                        engine_.submit_limit_order(command.order_id, command.side,
                                                   command.price, command.quantity);
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
    envelope.completion.set_value(
        CommandResult{envelope.ingestion_sequence, false, {}, std::move(error)});
}

} // namespace matching_engine
