#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <new>

namespace audio_voyager::core {

// Standard cache line size for modern x86_64 / ARM64 architectures
constexpr size_t CACHE_LINE_SIZE = 64;

/**
 * @brief Ultra-low-latency Single-Producer Single-Consumer (SPSC) Lock-Free Ring Buffer.
 * 
 * Guarantees:
 * - Real-Time Safe: Zero dynamic allocations on push/pop.
 * - Wait-Free: Lock-free atomic indices with acquire/release memory semantics.
 * - Cache Friendly: Eliminates False Sharing via 64-byte cache line padding.
 */
template <typename T>
class LockFreeRingBuffer {
public:
    explicit LockFreeRingBuffer(size_t capacity = 16384) {
        // Ensure capacity is a power of two for fast bitwise wrapping
        capacity_ = 1;
        while (capacity_ < capacity) {
            capacity_ <<= 1;
        }
        mask_ = capacity_ - 1;
        buffer_.resize(capacity_);
        write_index_.store(0, std::memory_order_relaxed);
        read_index_.store(0, std::memory_order_relaxed);
    }

    ~LockFreeRingBuffer() = default;

    // Non-copyable and non-movable for safety
    LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer(LockFreeRingBuffer&&) = delete;
    LockFreeRingBuffer& operator=(LockFreeRingBuffer&&) = delete;

    /**
     * @brief Pushes a single item into the ring buffer.
     * @return true if pushed, false if buffer is full.
     */
    bool push(const T& item) noexcept {
        const size_t write_idx = write_index_.load(std::memory_order_relaxed);
        const size_t read_idx = read_index_.load(std::memory_order_acquire);

        if ((write_idx - read_idx) >= capacity_) {
            return false; // Buffer Full
        }

        buffer_[write_idx & mask_] = item;
        write_index_.store(write_idx + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pushes an array of items into the ring buffer.
     * @return Number of items actually pushed.
     */
    size_t push_n(const T* items, size_t count) noexcept {
        if (!items || count == 0) return 0;

        const size_t write_idx = write_index_.load(std::memory_order_relaxed);
        const size_t read_idx = read_index_.load(std::memory_order_acquire);

        const size_t available = capacity_ - (write_idx - read_idx);
        const size_t to_write = std::min(count, available);

        if (to_write == 0) return 0;

        const size_t start_idx = write_idx & mask_;
        const size_t first_chunk = std::min(to_write, capacity_ - start_idx);
        const size_t second_chunk = to_write - first_chunk;

        std::copy_n(items, first_chunk, &buffer_[start_idx]);
        if (second_chunk > 0) {
            std::copy_n(items + first_chunk, second_chunk, &buffer_[0]);
        }

        write_index_.store(write_idx + to_write, std::memory_order_release);
        return to_write;
    }

    /**
     * @brief Pops a single item from the ring buffer.
     * @return true if popped, false if buffer is empty.
     */
    bool pop(T& item) noexcept {
        const size_t read_idx = read_index_.load(std::memory_order_relaxed);
        const size_t write_idx = write_index_.load(std::memory_order_acquire);

        if (read_idx == write_idx) {
            return false; // Buffer Empty
        }

        item = buffer_[read_idx & mask_];
        read_index_.store(read_idx + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pops an array of items from the ring buffer.
     * @return Number of items actually popped.
     */
    size_t pop_n(T* out_items, size_t count) noexcept {
        if (!out_items || count == 0) return 0;

        const size_t read_idx = read_index_.load(std::memory_order_relaxed);
        const size_t write_idx = write_index_.load(std::memory_order_acquire);

        const size_t available = write_idx - read_idx;
        const size_t to_read = std::min(count, available);

        if (to_read == 0) return 0;

        const size_t start_idx = read_idx & mask_;
        const size_t first_chunk = std::min(to_read, capacity_ - start_idx);
        const size_t second_chunk = to_read - first_chunk;

        std::copy_n(&buffer_[start_idx], first_chunk, out_items);
        if (second_chunk > 0) {
            std::copy_n(&buffer_[0], second_chunk, out_items + first_chunk);
        }

        read_index_.store(read_idx + to_read, std::memory_order_release);
        return to_read;
    }

    [[nodiscard]] size_t available_read() const noexcept {
        const size_t write_idx = write_index_.load(std::memory_order_acquire);
        const size_t read_idx = read_index_.load(std::memory_order_relaxed);
        return write_idx - read_idx;
    }

    [[nodiscard]] size_t available_write() const noexcept {
        return capacity_ - available_read();
    }

    [[nodiscard]] size_t capacity() const noexcept {
        return capacity_;
    }

    void reset() noexcept {
        write_index_.store(0, std::memory_order_relaxed);
        read_index_.store(0, std::memory_order_relaxed);
    }

private:
    size_t capacity_{0};
    size_t mask_{0};
    std::vector<T> buffer_;

    // Align indices to separate cache lines (64 bytes) to eliminate False Sharing
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> write_index_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> read_index_{0};
};

} // namespace audio_voyager::core
