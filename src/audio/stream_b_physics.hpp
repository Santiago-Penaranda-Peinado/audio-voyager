#pragma once

#include "core/types.hpp"
#include "core/lockfree_ring_buffer.hpp"
#include "core/triple_buffer.hpp"
#include <thread>
#include <atomic>
#include <vector>

namespace audio_voyager::audio {

/**
 * @brief Stream B: Soul Physics Processor (Essentia C++ DSP Analysis Pipeline).
 * 
 * Runs in a dedicated high-priority worker thread to extract semantic physical forces:
 * 1. Spectral Centroid (Center of Mass in Hz -> Buoyancy / Upward Gravity / Fluid Turbulence).
 * 2. Sethares Sensory Dissonance (Roughness / Tension -> Vector Field Vorticity / Space Warping).
 * 3. Onset Detection (High-Frequency Content / Complex Domain -> Kinetic Shockwaves).
 * 4. Acoustic Energy Envelope.
 */
class StreamBPhysics {
public:
    using SampleRingBuffer = core::LockFreeRingBuffer<float>;

    explicit StreamBPhysics(SampleRingBuffer& input_ring_buffer,
                            uint32_t sample_rate = core::DEFAULT_SAMPLE_RATE);
    ~StreamBPhysics();

    // Non-copyable
    StreamBPhysics(const StreamBPhysics&) = delete;
    StreamBPhysics& operator=(const StreamBPhysics&) = delete;

    void start();
    void stop();
    [[nodiscard]] bool is_running() const noexcept { return is_running_.load(std::memory_order_acquire); }

    /**
     * @brief Retrieves the latest computed physics snapshot via lock-free triple buffering.
     */
    bool get_latest_snapshot(core::StreamBSnapshot& out_snapshot) noexcept;

private:
    void worker_loop();
    void process_frame(const float* frame, size_t frame_size, core::StreamBSnapshot& out_snapshot);

    // Native & Essentia DSP algorithms
    void init_dsp_pipeline();
    float compute_spectral_centroid(const std::vector<float>& spectrum);
    float compute_sethares_dissonance(const std::vector<float>& spectrum);
    float compute_onset_novelty(const std::vector<float>& spectrum);

    SampleRingBuffer& input_ring_buffer_;
    uint32_t sample_rate_;

    std::atomic<bool> is_running_{false};
    std::thread worker_thread_;

    // Triple buffer for lock-free publishing to render/engine thread
    core::TripleBuffer<core::StreamBSnapshot> snapshot_buffer_;

    // Analysis frame buffer
    std::vector<float> analysis_frame_;
    std::vector<float> hann_window_;
    std::vector<float> windowed_frame_;
    std::vector<float> magnitude_spectrum_;
    std::vector<float> prev_magnitude_spectrum_;

    // Onset peak picking state
    float onset_moving_average_{0.0f};
    float onset_moving_std_{0.0f};
    int onset_cooldown_frames_{0};
};

} // namespace audio_voyager::audio
