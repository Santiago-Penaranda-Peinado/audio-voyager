#pragma once

#include "core/types.hpp"
#include "core/lockfree_ring_buffer.hpp"
#include <memory>
#include <atomic>
#include <functional>
#include <thread>

// Forward declaration of miniaudio types
struct ma_context;
struct ma_device;

namespace audio_voyager::audio {

struct AudioCaptureConfig {
    uint32_t sample_rate{core::DEFAULT_SAMPLE_RATE};
    uint32_t channels{core::DEFAULT_CHANNELS};
    uint32_t period_size_in_frames{256}; // ~5.33ms at 48kHz
    bool synthetic_test_mode{false};     // Generates rich harmonic + transient signals for offline testing
};

class AudioCapture {
public:
    using SampleRingBuffer = core::LockFreeRingBuffer<float>;

    AudioCapture(SampleRingBuffer& stream_a_buffer, 
                 SampleRingBuffer& stream_b_buffer, 
                 AudioCaptureConfig config = AudioCaptureConfig{});
    ~AudioCapture();

    // Non-copyable
    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    bool start();
    void stop();
    [[nodiscard]] bool is_running() const noexcept { return is_running_.load(std::memory_order_acquire); }
    [[nodiscard]] bool is_synthetic() const noexcept { return is_synthetic_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint32_t get_sample_rate() const noexcept { return config_.sample_rate; }

    // Internal real-time audio callback called by miniaudio
    void process_audio_frames(const float* input_frames, size_t frame_count) noexcept;

private:
    bool init_miniaudio_device();
    void start_synthetic_generator();

    AudioCaptureConfig config_;
    SampleRingBuffer& stream_a_buffer_;
    SampleRingBuffer& stream_b_buffer_;

    std::atomic<bool> is_running_{false};
    std::atomic<bool> is_synthetic_{false};

    // Miniaudio device and context handles
    std::unique_ptr<ma_context> ma_context_;
    std::unique_ptr<ma_device> ma_device_;

    // Synthetic generator thread if in test mode or if loopback hardware is unavailable
    std::thread synthetic_thread_;
    std::atomic<bool> stop_synthetic_{false};
};

} // namespace audio_voyager::audio
