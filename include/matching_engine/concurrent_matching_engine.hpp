#pragma once

#include "matching_engine/command.hpp"
#include "matching_engine/matching_engine.hpp"
#include "matching_engine/thread_safe_queue.hpp"
#include "matching_engine/trade.hpp"

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
    explicit ConcurrentMatchingEngine(std::size_t queue_capacity);
    ~ConcurrentMatchingEngine();

    ConcurrentMatchingEngine(const ConcurrentMatchingEngine&) = delete;
    ConcurrentMatchingEngine& operator=(const ConcurrentMatchingEngine&) = delete;
    ConcurrentMatchingEngine(ConcurrentMatchingEngine&&) = delete;
    ConcurrentMatchingEngine& operator=(ConcurrentMatchingEngine&&) = delete;

    [[nodiscard]] std::future<CommandResult> submit(
        IngestionSequence ingestion_sequence,
        Command command);

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

    mutable std::mutex shutdown_mutex_;
    mutable std::mutex state_mutex_;
    std::unordered_set<IngestionSequence> submitted_sequences_;
    bool accepting_{true};
    bool worker_finished_{false};
    std::thread worker_;
};

}  // namespace matching_engine
