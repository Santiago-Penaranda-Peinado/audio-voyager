#pragma once

#include "core/types.hpp"
#include "core/lockfree_ring_buffer.hpp"
#include "core/triple_buffer.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <complex>

namespace audio_voyager::audio {

class StreamBPhysics {
public:
    using SampleRingBuffer = core::LockFreeRingBuffer<float>;

    explicit StreamBPhysics(SampleRingBuffer& input_ring_buffer,
                            uint32_t sample_rate = core::DEFAULT_SAMPLE_RATE);
    ~StreamBPhysics();

    StreamBPhysics(const StreamBPhysics&) = delete;
    StreamBPhysics& operator=(const StreamBPhysics&) = delete;

    void start();
    void stop();
    [[nodiscard]] bool is_running() const noexcept { return is_running_.load(std::memory_order_acquire); }

    bool get_latest_snapshot(core::StreamBSnapshot& out_snapshot) noexcept;

private:
    void worker_loop();
    void process_frame(const float* frame, size_t frame_size, core::StreamBSnapshot& out_snapshot);

    void init_dsp_pipeline();
    float compute_spectral_centroid(const std::vector<float>& spectrum);
    float compute_sethares_dissonance(const std::vector<float>& spectrum);
    float compute_spectral_flatness(const std::vector<float>& spectrum);
    float compute_onset_novelty(const std::vector<float>& spectrum);
    void extract_multi_band_energy(const std::vector<float>& spectrum, core::StreamBSnapshot& snap);

    SampleRingBuffer& input_ring_buffer_;
    uint32_t sample_rate_;

    std::atomic<bool> is_running_{false};
    std::thread worker_thread_;

    core::TripleBuffer<core::StreamBSnapshot> snapshot_buffer_;

    std::vector<float> analysis_frame_;
    std::vector<float> hann_window_;
    std::vector<float> windowed_frame_;
    std::vector<float> magnitude_spectrum_;
    std::vector<float> prev_magnitude_spectrum_;

    // Preallocated zero-allocation FFT scratch buffers
    std::vector<std::complex<float>> fft_scratch_;
    std::vector<size_t> bit_reverse_indices_;

    float onset_moving_average_{0.0f};
    float onset_moving_std_{0.0f};
    int onset_cooldown_frames_{0};
};

} // namespace audio_voyager::audio
