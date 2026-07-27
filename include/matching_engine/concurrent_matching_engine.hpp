#pragma once

#include "matching_engine/command.hpp"
#include "matching_engine/matching_engine.hpp"
#include "matching_engine/thread_safe_queue.hpp"
#include "matching_engine/trade.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace matching_engine {

using IngestionSequence = std::uint64_t;

struct CommandResult {
    IngestionSequence ingestion_sequence;
    bool accepted;
    std::vector<Trade> trades;
    std::string error;
};

class ConcurrentMatchingEngine {
  public:
    static constexpr IngestionSequence default_max_sequence_gap{1'024};
    static constexpr std::chrono::milliseconds default_sequence_gap_timeout{1'000};

    explicit ConcurrentMatchingEngine(
        std::size_t queue_capacity,
        IngestionSequence max_sequence_gap = default_max_sequence_gap,
        std::chrono::milliseconds sequence_gap_timeout = default_sequence_gap_timeout);
    ~ConcurrentMatchingEngine();

    ConcurrentMatchingEngine(const ConcurrentMatchingEngine&) = delete;
    ConcurrentMatchingEngine& operator=(const ConcurrentMatchingEngine&) = delete;
    ConcurrentMatchingEngine(ConcurrentMatchingEngine&&) = delete;
    ConcurrentMatchingEngine& operator=(ConcurrentMatchingEngine&&) = delete;

    [[nodiscard]] std::future<CommandResult>
    submit(IngestionSequence ingestion_sequence, Command command);

    void shutdown();
    [[nodiscard]] std::string book_snapshot() const;

  private:
    struct Envelope {
        IngestionSequence ingestion_sequence;
        Command command;
        std::promise<CommandResult> completion;
    };

    void run();
    void process(Envelope envelope);
    static void reject(Envelope envelope, std::string error);

    ThreadSafeQueue<Envelope> queue_;
    MatchingEngine engine_;
    const IngestionSequence max_sequence_gap_;
    const std::chrono::milliseconds sequence_gap_timeout_;

    mutable std::mutex shutdown_mutex_;
    mutable std::mutex state_mutex_;
    std::unordered_set<IngestionSequence> submitted_sequences_;
    IngestionSequence next_ingestion_sequence_{1};
    std::string terminal_error_;
    bool accepting_{true};
    bool worker_finished_{false};
    std::thread worker_;
};

} // namespace matching_engine
