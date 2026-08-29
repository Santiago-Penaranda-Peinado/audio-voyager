#pragma once

#include <atomic>
#include <array>
#include <cstddef>

namespace audio_voyager::core {

/**
 * @brief Lock-Free, Wait-Free Triple Buffer.
 * 
 * Enables the Producer (DSP worker / Audio Engine) to publish complete 
 * simulation state snapshots at its own cadence, while the Consumer 
 * (GPU Render / Telemetry loop) can read the most recent snapshot at 60/144/240+ FPS
 * with absolute zero blocking, zero mutexes, and zero data races.
 */
template <typename T>
class TripleBuffer {
public:
    TripleBuffer()
        : shared_index_(0)
        , write_index_(1)
        , read_index_(2)
        , has_new_(false) {
    }

    explicit TripleBuffer(const T& initial_value)
        : shared_index_(0)
        , write_index_(1)
        , read_index_(2)
        , has_new_(false) {
        slots_[0] = initial_value;
        slots_[1] = initial_value;
        slots_[2] = initial_value;
    }

    ~TripleBuffer() = default;

    // Non-copyable and non-movable
    TripleBuffer(const TripleBuffer&) = delete;
    TripleBuffer& operator=(const TripleBuffer&) = delete;
    TripleBuffer(TripleBuffer&&) = delete;
    TripleBuffer& operator=(TripleBuffer&&) = delete;

    /**
     * @brief Producer writes a new state and commits it atomically.
     * @param state State snapshot to write.
     */
    void write(const T& state) noexcept {
        slots_[write_index_] = state;
        // Atomically exchange write index with the shared clean slot
        write_index_ = shared_index_.exchange(write_index_, std::memory_order_acq_rel);
        has_new_.store(true, std::memory_order_release);
    }

    /**
     * @brief Consumer checks and retrieves the latest state.
     * @param out_state Reference to store the latest snapshot.
     * @return true if new state was available, false if consuming previous clean state.
     */
    bool read(T& out_state) noexcept {
        if (has_new_.exchange(false, std::memory_order_acq_rel)) {
            // Atomically exchange read index with the most recent shared clean slot
            read_index_ = shared_index_.exchange(read_index_, std::memory_order_acq_rel);
            out_state = slots_[read_index_];
            return true;
        }

        // Return latest available data without state swap
        out_state = slots_[read_index_];
        return false;
    }

    /**
     * @brief Direct pointer access for zero-copy read.
     */
    const T& get_latest() const noexcept {
        return slots_[read_index_];
    }

private:
    std::array<T, 3> slots_{};
    std::atomic<size_t> shared_index_{0};
    size_t write_index_{1};
    size_t read_index_{2};
    std::atomic<bool> has_new_{false};
};

} // namespace audio_voyager::core
