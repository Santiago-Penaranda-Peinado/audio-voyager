#pragma once

#include "core/types.hpp"
#include "core/lockfree_ring_buffer.hpp"
#include "core/triple_buffer.hpp"
#include "audio/audio_capture.hpp"
#include "audio/stream_a_raw.hpp"
#include "audio/stream_b_physics.hpp"
#include <memory>
#include <atomic>

namespace audio_voyager::engine {

/**
 * @brief Master Audio Engine orchestrating the Dual-Stream DSP Pipeline.
 * 
 * Manages:
 * - Loopback audio capture thread.
 * - Stream A (Raw Reactivity / Oscilloscope + FFT fast path).
 * - Stream B (Soul Physics / Essentia C++ DSP worker).
 * - Triple-buffered synchronization with the GPU / Simulation loop.
 */
class AudioEngine {
public:
    explicit AudioEngine(audio::AudioCaptureConfig config = audio::AudioCaptureConfig{});
    ~AudioEngine();

    // Non-copyable
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool start();
    void stop();
    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] bool is_synthetic() const noexcept;

    /**
     * @brief Updates and polls the combined PhysicsAudioState.
     * Can be called at any frame rate from the render/telemetry loop.
     * @param out_state Reference where the latest state will be stored.
     * @return true if new state was updated.
     */
    bool poll_state(core::PhysicsAudioState& out_state);

    // Audio Normalization & Gain Sensitivity Controls
    void set_input_gain(float gain) noexcept;
    void set_agc_enabled(bool enabled) noexcept;
    [[nodiscard]] float get_dynamic_gain() const noexcept;

private:
    audio::AudioCaptureConfig config_;

    // Lock-Free SPSC Ring Buffers (Cache-aligned)
    core::LockFreeRingBuffer<float> ring_buffer_stream_a_{32768};
    core::LockFreeRingBuffer<float> ring_buffer_stream_b_{32768};

    // Subsystems
    std::unique_ptr<audio::AudioCapture> capture_;
    std::unique_ptr<audio::StreamARaw> stream_a_;
    std::unique_ptr<audio::StreamBPhysics> stream_b_;

    // State publishing
    core::TripleBuffer<core::PhysicsAudioState> state_buffer_;
    core::PhysicsAudioState current_state_{};
    std::atomic<bool> is_running_{false};
    uint64_t frame_counter_{0};
};

} // namespace audio_voyager::engine
