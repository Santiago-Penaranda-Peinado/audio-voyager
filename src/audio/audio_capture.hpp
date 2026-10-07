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

    // AGC & Input Gain Sensitivity Controls
    void set_input_gain(float gain) noexcept { manual_gain_.store(gain, std::memory_order_relaxed); }
    [[nodiscard]] float get_input_gain() const noexcept { return manual_gain_.load(std::memory_order_relaxed); }
    void set_agc_enabled(bool enabled) noexcept { agc_enabled_.store(enabled, std::memory_order_relaxed); }
    [[nodiscard]] bool is_agc_enabled() const noexcept { return agc_enabled_.load(std::memory_order_relaxed); }
    [[nodiscard]] float get_dynamic_gain() const noexcept { return current_gain_.load(std::memory_order_relaxed); }

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

    // Automatic Gain Control (AGC) state
    std::atomic<float> manual_gain_{1.0f};
    std::atomic<bool> agc_enabled_{true};
    std::atomic<float> current_gain_{1.0f};
    float agc_envelope_{0.25f};
};

} // namespace audio_voyager::audio
