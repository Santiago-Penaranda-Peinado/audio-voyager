#define _USE_MATH_DEFINES
#include "audio/stream_b_physics.hpp"
#include "core/types.hpp"
#include <cmath>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <complex>
#include <iostream>

#ifdef USE_ESSENTIA
#include <essentia/essentia.h>
#include <essentia/algorithmfactory.h>
#include <essentia/pool.h>
#endif

namespace audio_voyager::audio {

namespace {
// Sethares Dissonance Model constants
constexpr float SETHARES_A = 3.5f;
constexpr float SETHARES_B = 5.75f;
constexpr float SETHARES_X_STAR = 0.24f;
constexpr float SETHARES_S1 = 0.0207f;
constexpr float SETHARES_S2 = 18.96f;
} // anonymous namespace

StreamBPhysics::StreamBPhysics(SampleRingBuffer& input_ring_buffer, uint32_t sample_rate)
    : input_ring_buffer_(input_ring_buffer)
    , sample_rate_(sample_rate) {

    analysis_frame_.assign(core::ANALYSIS_FRAME_SIZE_STREAM_B, 0.0f);
    hann_window_.assign(core::ANALYSIS_FRAME_SIZE_STREAM_B, 0.0f);
    windowed_frame_.assign(core::ANALYSIS_FRAME_SIZE_STREAM_B, 0.0f);
    magnitude_spectrum_.assign(core::ANALYSIS_FRAME_SIZE_STREAM_B / 2 + 1, 0.0f);
    prev_magnitude_spectrum_.assign(core::ANALYSIS_FRAME_SIZE_STREAM_B / 2 + 1, 0.0f);

    init_dsp_pipeline();
}

StreamBPhysics::~StreamBPhysics() {
    stop();
#ifdef USE_ESSENTIA
    essentia::shutdown();
#endif
}

void StreamBPhysics::init_dsp_pipeline() {
#ifdef USE_ESSENTIA
    essentia::init();
    std::cout << "[StreamBPhysics] Essentia C++ DSP Framework successfully initialized.\n";
#else
    std::cout << "[StreamBPhysics] High-Performance Native C++20 DSP Engine active (Sethares Dissonance + Spectral Centroid + HFC Onsets).\n";
#endif

    const size_t n = core::ANALYSIS_FRAME_SIZE_STREAM_B;
    for (size_t i = 0; i < n; ++i) {
        hann_window_[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(n - 1)));
    }
}

float StreamBPhysics::compute_spectral_centroid(const std::vector<float>& spectrum) {
    const float bin_hz = static_cast<float>(sample_rate_) / static_cast<float>(core::ANALYSIS_FRAME_SIZE_STREAM_B);
    float numerator = 0.0f;
    float denominator = 0.0f;

    for (size_t k = 0; k < spectrum.size(); ++k) {
        const float freq = static_cast<float>(k) * bin_hz;
        const float mag = spectrum[k];
        numerator += freq * mag;
        denominator += mag;
    }

    if (denominator < 1e-6f) {
        return 0.0f;
    }
    return numerator / denominator;
}

float StreamBPhysics::compute_sethares_dissonance(const std::vector<float>& spectrum) {
    // 1. Peak picking: Extract prominent spectral peaks
    struct Peak { float freq; float mag; };
    constexpr size_t MAX_PEAKS = 20;
    std::vector<Peak> peaks;
    peaks.reserve(MAX_PEAKS);

    const float bin_hz = static_cast<float>(sample_rate_) / static_cast<float>(core::ANALYSIS_FRAME_SIZE_STREAM_B);
    for (size_t k = 2; k < spectrum.size() - 2; ++k) {
        if (spectrum[k] > spectrum[k - 1] && spectrum[k] > spectrum[k + 1] && spectrum[k] > 0.005f) {
            peaks.push_back({ static_cast<float>(k) * bin_hz, spectrum[k] });
        }
    }

    // Keep top N loudest peaks
    if (peaks.size() > MAX_PEAKS) {
        std::partial_sort(peaks.begin(), peaks.begin() + MAX_PEAKS, peaks.end(),
            [](const Peak& a, const Peak& b) { return a.mag > b.mag; });
        peaks.resize(MAX_PEAKS);
    }

    if (peaks.size() < 2) {
        return 0.0f;
    }

    float max_mag = 0.0f;
    for (const auto& p : peaks) {
        max_mag = std::max(max_mag, p.mag);
    }
    if (max_mag < 1e-6f) {
        return 0.0f;
    }

    // 2. Compute pairwise sensory dissonance (Sethares model)
    // Partial amplitudes normalized relative to maximum peak: a_i = m_i / max_mag in [0, 1]
    float total_dissonance = 0.0f;
    for (size_t i = 0; i < peaks.size(); ++i) {
        const float a1 = peaks[i].mag / max_mag;
        for (size_t j = i + 1; j < peaks.size(); ++j) {
            const float a2 = peaks[j].mag / max_mag;
            const float f_min = std::min(peaks[i].freq, peaks[j].freq);
            const float f_diff = std::abs(peaks[i].freq - peaks[j].freq);
            const float a_prod = a1 * a2;

            const float s = SETHARES_X_STAR / (SETHARES_S1 * f_min + SETHARES_S2);
            const float arg = s * f_diff;
            const float diss = a_prod * (std::exp(-SETHARES_A * arg) - std::exp(-SETHARES_B * arg));
            total_dissonance += diss;
        }
    }

    // Normalize dissonance into [0, 1] range (with normalized partials, dense dissonant chords reach 0.6-0.8)
    return std::clamp(total_dissonance * 0.25f, 0.0f, 1.0f);
}

float StreamBPhysics::compute_spectral_flatness(const std::vector<float>& spectrum) {
    if (spectrum.size() <= 2) return 0.0f;
    constexpr float EPSILON = 1e-9f;
    float sum_log = 0.0f;
    float sum_power = 0.0f;
    size_t count = 0;

    // Power spectrum Wiener entropy (geometric mean / arithmetic mean), skipping DC bin 0
    for (size_t k = 1; k < spectrum.size(); ++k) {
        float power = spectrum[k] * spectrum[k];
        sum_power += power;
        sum_log += std::log(power + EPSILON);
        count++;
    }

    if (count == 0) return 0.0f;
    float arithmetic_mean = sum_power / static_cast<float>(count);
    if (arithmetic_mean < EPSILON) return 0.0f;

    float geometric_mean = std::exp(sum_log / static_cast<float>(count));
    float flatness = geometric_mean / arithmetic_mean;
    return std::clamp(flatness, 0.0f, 1.0f);
}

float StreamBPhysics::compute_onset_novelty(const std::vector<float>& spectrum) {
    // High-Frequency Content (HFC) + Spectral Flux
    float flux = 0.0f;
    float hfc = 0.0f;
    const size_t num_bins = spectrum.size();

    for (size_t k = 0; k < num_bins; ++k) {
        const float diff = spectrum[k] - prev_magnitude_spectrum_[k];
        if (diff > 0.0f) {
            flux += diff;
        }
        hfc += static_cast<float>(k) * spectrum[k];
    }

    const float novelty = (flux * 1.5f + (hfc / static_cast<float>(num_bins)) * 0.5f);
    return novelty;
}

void StreamBPhysics::process_frame(const float* frame, size_t frame_size, core::StreamBSnapshot& out_snapshot) {
    const auto t_start = std::chrono::steady_clock::now();

    // 1. Windowing
    for (size_t i = 0; i < frame_size; ++i) {
        windowed_frame_[i] = frame[i] * hann_window_[i];
    }

    // 2. High-Performance Radix-2 Cooley-Tukey FFT for Stream B
    const size_t n = frame_size;
    std::vector<std::complex<float>> fft_scratch(n);
    for (size_t i = 0; i < n; ++i) {
        // Bit-reversal index
        size_t rev = 0;
        size_t temp = i;
        for (size_t bit = 1; bit < n; bit <<= 1) {
            rev = (rev << 1) | (temp & 1);
            temp >>= 1;
        }
        fft_scratch[rev] = std::complex<float>(windowed_frame_[i], 0.0f);
    }

    for (size_t len = 2; len <= n; len <<= 1) {
        const float angle = -2.0f * static_cast<float>(M_PI) / static_cast<float>(len);
        const std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t j = 0; j < len / 2; ++j) {
                const std::complex<float> u = fft_scratch[i + j];
                const std::complex<float> v = fft_scratch[i + j + len / 2] * w;
                fft_scratch[i + j] = u + v;
                fft_scratch[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    const size_t num_bins = n / 2 + 1;
    const float norm = 2.0f / static_cast<float>(n);
    for (size_t k = 0; k < num_bins; ++k) {
        magnitude_spectrum_[k] = std::abs(fft_scratch[k]) * norm;
    }

    // 3. Compute Soul Physics Descriptors
    // A. Spectral Centroid (Hz & Normalized [0, 1])
    const float centroid_hz = compute_spectral_centroid(magnitude_spectrum_);
    out_snapshot.spectral_centroid_hz = centroid_hz;
    out_snapshot.spectral_centroid_norm = std::clamp(centroid_hz / (sample_rate_ * 0.45f), 0.0f, 1.0f);

    // B. Sethares Dissonance (Roughness / Vorticity)
    out_snapshot.dissonance = compute_sethares_dissonance(magnitude_spectrum_);

    // B2. Spectral Flatness (Wiener Entropy: Heavy distortion / saturation vs clean harmonic)
    out_snapshot.spectral_flatness = compute_spectral_flatness(magnitude_spectrum_);

    // C. Onset Novelty & Peak Trigger (Kinetic Shockwaves)
    const float novelty = compute_onset_novelty(magnitude_spectrum_);
    out_snapshot.onset_strength = std::clamp(novelty * 4.0f, 0.0f, 1.0f);

    // Dynamic adaptive thresholding for Onset detection
    onset_moving_average_ = onset_moving_average_ * 0.92f + novelty * 0.08f;
    const float threshold = onset_moving_average_ * 1.8f + 0.08f;

    if (onset_cooldown_frames_ > 0) {
        --onset_cooldown_frames_;
        out_snapshot.is_onset = false;
    } else if (novelty > threshold && novelty > 0.05f) {
        out_snapshot.is_onset = true;
        onset_cooldown_frames_ = 4; // ~21ms cooldown between shockwaves
    } else {
        out_snapshot.is_onset = false;
    }

    // D. Global Energy & RMS
    float sum_energy = 0.0f;
    for (size_t i = 0; i < frame_size; ++i) {
        sum_energy += frame[i] * frame[i];
    }
    out_snapshot.energy = std::clamp(sum_energy / static_cast<float>(frame_size) * 10.0f, 0.0f, 1.0f);
    out_snapshot.rms = std::clamp(std::sqrt(sum_energy / static_cast<float>(frame_size)), 0.0f, 1.0f);

    // E. Multi-band frequency decomposition for physical reaction
    const float bin_hz_dec = static_cast<float>(sample_rate_) / static_cast<float>(frame_size);
    float sum_sub = 0.0f, sum_bass = 0.0f, sum_mids = 0.0f, sum_treble = 0.0f, sum_air = 0.0f;
    for (size_t k = 1; k < num_bins; ++k) {
        float f = static_cast<float>(k) * bin_hz_dec;
        float mag = magnitude_spectrum_[k];
        if (f < 80.0f) sum_sub += mag;
        else if (f < 250.0f) sum_bass += mag;
        else if (f < 2500.0f) sum_mids += mag;
        else if (f < 8000.0f) sum_treble += mag;
        else if (f < 20000.0f) sum_air += mag;
    }
    out_snapshot.band_sub_bass = std::clamp(sum_sub * 0.4f, 0.0f, 1.0f);
    out_snapshot.band_bass     = std::clamp(sum_bass * 0.35f, 0.0f, 1.0f);
    out_snapshot.band_mids     = std::clamp(sum_mids * 0.25f, 0.0f, 1.0f);
    out_snapshot.band_treble   = std::clamp(sum_treble * 0.20f, 0.0f, 1.0f);
    out_snapshot.band_air      = std::clamp(sum_air * 0.30f, 0.0f, 1.0f);

    // Save previous spectrum for next flux calculation
    prev_magnitude_spectrum_ = magnitude_spectrum_;

    const auto t_end = std::chrono::steady_clock::now();
    out_snapshot.compute_time_us = std::chrono::duration<float, std::micro>(t_end - t_start).count();
    out_snapshot.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        t_start.time_since_epoch()).count();
}

void StreamBPhysics::worker_loop() {
    constexpr size_t HOP_SIZE = core::ANALYSIS_HOP_SIZE_STREAM_B;
    constexpr size_t FRAME_SIZE = core::ANALYSIS_FRAME_SIZE_STREAM_B;
    float hop_scratch[HOP_SIZE];
    core::StreamBSnapshot current_snapshot;

    while (is_running_.load(std::memory_order_relaxed)) {
        if (input_ring_buffer_.available_read() >= HOP_SIZE) {
            const size_t pulled = input_ring_buffer_.pop_n(hop_scratch, HOP_SIZE);
            if (pulled == HOP_SIZE) {
                // Slide analysis frame and append new hop
                std::move(analysis_frame_.begin() + HOP_SIZE, analysis_frame_.end(), analysis_frame_.begin());
                std::copy_n(hop_scratch, HOP_SIZE, analysis_frame_.data() + (FRAME_SIZE - HOP_SIZE));

                // Process DSP frame and extract physical forces
                process_frame(analysis_frame_.data(), FRAME_SIZE, current_snapshot);

                // Publish atomically to triple buffer
                snapshot_buffer_.write(current_snapshot);
            }
        } else {
            // Adaptive sleep to avoid busy waiting while maintaining ultra-low latency (~1ms)
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }
}

void StreamBPhysics::start() {
    if (is_running_.load(std::memory_order_acquire)) {
        return;
    }
    is_running_.store(true, std::memory_order_release);
    worker_thread_ = std::thread(&StreamBPhysics::worker_loop, this);
}

void StreamBPhysics::stop() {
    if (!is_running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

bool StreamBPhysics::get_latest_snapshot(core::StreamBSnapshot& out_snapshot) noexcept {
    return snapshot_buffer_.read(out_snapshot);
}

} // namespace audio_voyager::audio
