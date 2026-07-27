#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

namespace matching_engine {

template <typename Value> class ThreadSafeQueue {
  public:
    enum class TimedPopStatus : std::uint8_t { value, closed, timeout };

    struct TimedPopResult {
        TimedPopStatus status;
        std::optional<Value> value;
    };

    explicit ThreadSafeQueue(const std::size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) {
            throw std::invalid_argument("queue capacity must be positive");
        }
    }

    ~ThreadSafeQueue() = default;

    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue(ThreadSafeQueue&&) = delete;
    ThreadSafeQueue& operator=(ThreadSafeQueue&&) = delete;

    [[nodiscard]] bool push(Value value) {
        std::unique_lock lock{mutex_};
        not_full_.wait(lock, [this] { return closed_ || values_.size() < capacity_; });
        if (closed_) {
            return false;
        }

        values_.push_back(std::move(value));
        lock.unlock();
        not_empty_.notify_one();
        return true;
    }

    [[nodiscard]] std::optional<Value> pop() {
        std::unique_lock lock{mutex_};
        not_empty_.wait(lock, [this] { return closed_ || !values_.empty(); });
        if (values_.empty()) {
            return std::nullopt;
        }

        Value value = std::move(values_.front());
        values_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return value;
    }

    template <typename Clock, typename Duration>
    [[nodiscard]] TimedPopResult
    pop_until(const std::chrono::time_point<Clock, Duration>& deadline) {
        std::unique_lock lock{mutex_};
        if (!not_empty_.wait_until(lock, deadline,
                                   [this] { return closed_ || !values_.empty(); })) {
            return {TimedPopStatus::timeout, std::nullopt};
        }
        if (values_.empty()) {
            return {TimedPopStatus::closed, std::nullopt};
        }

        Value value = std::move(values_.front());
        values_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return {TimedPopStatus::value, std::move(value)};
    }

    void close() {
        {
            std::scoped_lock lock{mutex_};
            closed_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    [[nodiscard]] bool is_closed() const {
        std::scoped_lock lock{mutex_};
        return closed_;
    }

    [[nodiscard]] std::size_t size() const {
        std::scoped_lock lock{mutex_};
        return values_.size();
    }

  private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::deque<Value> values_;
    bool closed_{false};
};

} // namespace matching_engine
