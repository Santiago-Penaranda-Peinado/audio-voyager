#define _USE_MATH_DEFINES
#include "audio/stream_b_physics.hpp"
#include "core/types.hpp"
#include <cmath>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <complex>
#include <iostream>

namespace audio_voyager::audio {

namespace {
constexpr float SETHARES_A = 3.5f;
constexpr float SETHARES_B = 5.75f;
constexpr float SETHARES_X_STAR = 0.24f;
constexpr float SETHARES_S1 = 0.0207f;
constexpr float SETHARES_S2 = 18.96f;
} // anonymous namespace

StreamBPhysics::StreamBPhysics(SampleRingBuffer& input_ring_buffer, uint32_t sample_rate)
    : input_ring_buffer_(input_ring_buffer)
    , sample_rate_(sample_rate) {

    const size_t n = core::ANALYSIS_FRAME_SIZE_STREAM_B;
    analysis_frame_.assign(n, 0.0f);
    hann_window_.assign(n, 0.0f);
    windowed_frame_.assign(n, 0.0f);
    magnitude_spectrum_.assign(n / 2 + 1, 0.0f);
    prev_magnitude_spectrum_.assign(n / 2 + 1, 0.0f);
    fft_scratch_.assign(n, {0.0f, 0.0f});

    bit_reverse_indices_.resize(n);
    for (size_t i = 0; i < n; ++i) {
        size_t rev = 0;
        size_t temp = i;
        for (size_t bit = 1; bit < n; bit <<= 1) {
            rev = (rev << 1) | (temp & 1);
            temp >>= 1;
        }
        bit_reverse_indices_[i] = rev;
    }

    init_dsp_pipeline();
}

StreamBPhysics::~StreamBPhysics() {
    stop();
}

void StreamBPhysics::init_dsp_pipeline() {
    std::cout << "[StreamBPhysics] High-Performance Native C++20 DSP Engine active "
              << "(Sethares Dissonance + Spectral Flatness + Multi-Band + HFC Onsets).\n";

    const size_t n = core::ANALYSIS_FRAME_SIZE_STREAM_B;
    for (size_t i = 0; i < n; ++i) {
        hann_window_[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(n - 1)));
    }
}

float StreamBPhysics::compute_spectral_centroid(const std::vector<float>& spectrum) {
    const float bin_hz = static_cast<float>(sample_rate_) / static_cast<float>(core::ANALYSIS_FRAME_SIZE_STREAM_B);
    float numerator = 0.0f;
    float denominator = 0.0f;

    for (size_t k = 1; k < spectrum.size(); ++k) { // Skip bin 0 (DC)
        const float freq = static_cast<float>(k) * bin_hz;
        const float mag = spectrum[k];
        numerator += freq * mag;
        denominator += mag;
    }

    if (denominator < 1e-6f) return 0.0f;
    return numerator / denominator;
}

float StreamBPhysics::compute_sethares_dissonance(const std::vector<float>& spectrum) {
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

    if (peaks.size() > MAX_PEAKS) {
        std::partial_sort(peaks.begin(), peaks.begin() + MAX_PEAKS, peaks.end(),
            [](const Peak& a, const Peak& b) { return a.mag > b.mag; });
        peaks.resize(MAX_PEAKS);
    }

    if (peaks.size() < 2) return 0.0f;

    float total_dissonance = 0.0f;
    for (size_t i = 0; i < peaks.size(); ++i) {
        for (size_t j = i + 1; j < peaks.size(); ++j) {
            const float f_min = std::min(peaks[i].freq, peaks[j].freq);
            const float f_diff = std::abs(peaks[i].freq - peaks[j].freq);
            const float a_prod = peaks[i].mag * peaks[j].mag;

            const float s = SETHARES_X_STAR / (SETHARES_S1 * f_min + SETHARES_S2);
            const float arg = s * f_diff;
            const float diss = a_prod * (std::exp(-SETHARES_A * arg) - std::exp(-SETHARES_B * arg));
            total_dissonance += diss;
        }
    }

    return std::clamp(total_dissonance * 5.0f, 0.0f, 1.0f);
}

float StreamBPhysics::compute_spectral_flatness(const std::vector<float>& spectrum) {
    // Wiener Entropy: geometric_mean / arithmetic_mean
    // 0.0 = pure tone (all energy in one bin), 1.0 = white noise (flat spectrum)
    // Distorted guitars produce high flatness (dense broadband energy)
    const size_t start_bin = 2;   // Skip DC and very low bins
    const size_t end_bin = std::min(spectrum.size(), static_cast<size_t>(256)); // Up to ~12 kHz

    if (end_bin <= start_bin) return 0.0f;

    float sum_log = 0.0f;
    float sum_linear = 0.0f;
    size_t count = 0;

    for (size_t k = start_bin; k < end_bin; ++k) {
        float mag = std::max(spectrum[k], 1e-10f);
        sum_log += std::log(mag);
        sum_linear += mag;
        count++;
    }

    if (count == 0 || sum_linear < 1e-8f) return 0.0f;

    float geo_mean = std::exp(sum_log / static_cast<float>(count));
    float arith_mean = sum_linear / static_cast<float>(count);

    return std::clamp(geo_mean / arith_mean, 0.0f, 1.0f);
}

float StreamBPhysics::compute_onset_novelty(const std::vector<float>& spectrum) {
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

void StreamBPhysics::extract_multi_band_energy(const std::vector<float>& spectrum, core::StreamBSnapshot& snap) {
    // 1024-pt FFT at 48kHz = 46.875 Hz/bin
    // CRITICAL: Skip Bin 0 (DC offset) — this was the sub-bass blindness bug!
    auto sum_bins = [&](size_t start, size_t end) {
        float sum = 0.0f;
        end = std::min(end, spectrum.size());
        for (size_t k = start; k < end; ++k) {
            sum += spectrum[k];
        }
        size_t count = (end > start) ? (end - start) : 1;
        return sum / static_cast<float>(count);
    };

    snap.band_sub_bass = std::clamp(sum_bins(1, 5) * 12.0f, 0.0f, 1.0f);    // 47 - 234 Hz (SKIP DC!)
    snap.band_bass     = std::clamp(sum_bins(5, 11) * 10.0f, 0.0f, 1.0f);    // 234 - 516 Hz
    snap.band_mids     = std::clamp(sum_bins(11, 64) * 8.0f, 0.0f, 1.0f);    // 516 - 3000 Hz
    snap.band_treble   = std::clamp(sum_bins(64, 171) * 14.0f, 0.0f, 1.0f);  // 3000 - 8000 Hz
    snap.band_air      = std::clamp(sum_bins(171, 427) * 20.0f, 0.0f, 1.0f); // 8000 - 20000 Hz
}

void StreamBPhysics::process_frame(const float* frame, size_t frame_size, core::StreamBSnapshot& out_snapshot) {
    const auto t_start = std::chrono::steady_clock::now();

    // 1. Windowing
    for (size_t i = 0; i < frame_size; ++i) {
        windowed_frame_[i] = frame[i] * hann_window_[i];
    }

    // 2. Zero-Allocation Radix-2 Cooley-Tukey FFT
    const size_t n = frame_size;
    for (size_t i = 0; i < n; ++i) {
        fft_scratch_[bit_reverse_indices_[i]] = std::complex<float>(windowed_frame_[i], 0.0f);
    }

    for (size_t len = 2; len <= n; len <<= 1) {
        const float angle = -2.0f * static_cast<float>(M_PI) / static_cast<float>(len);
        const std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t j = 0; j < len / 2; ++j) {
                const std::complex<float> u = fft_scratch_[i + j];
                const std::complex<float> v = fft_scratch_[i + j + len / 2] * w;
                fft_scratch_[i + j] = u + v;
                fft_scratch_[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    const size_t num_bins = n / 2 + 1;
    const float norm = 2.0f / static_cast<float>(n);
    for (size_t k = 0; k < num_bins; ++k) {
        magnitude_spectrum_[k] = std::abs(fft_scratch_[k]) * norm;
    }

    // 3. Extract ALL acoustic features
    // A. Spectral Centroid (skip DC)
    const float centroid_hz = compute_spectral_centroid(magnitude_spectrum_);
    out_snapshot.spectral_centroid_hz = centroid_hz;
    out_snapshot.spectral_centroid_norm = std::clamp(centroid_hz / (sample_rate_ * 0.45f), 0.0f, 1.0f);

    // B. Sethares Dissonance
    out_snapshot.dissonance = compute_sethares_dissonance(magnitude_spectrum_);

    // C. Spectral Flatness (Wiener Entropy) — captures broadband distortion
    out_snapshot.spectral_flatness = compute_spectral_flatness(magnitude_spectrum_);

    // D. Multi-Band Energy Decomposition (from 1024-pt FFT, NOT the broken 512-pt Stream A)
    extract_multi_band_energy(magnitude_spectrum_, out_snapshot);

    // E. Onset Novelty
    const float novelty = compute_onset_novelty(magnitude_spectrum_);
    out_snapshot.onset_strength = std::clamp(novelty * 4.0f, 0.0f, 1.0f);

    onset_moving_average_ = onset_moving_average_ * 0.92f + novelty * 0.08f;
    const float threshold = onset_moving_average_ * 1.8f + 0.08f;

    if (onset_cooldown_frames_ > 0) {
        --onset_cooldown_frames_;
        out_snapshot.is_onset = false;
    } else if (novelty > threshold && novelty > 0.05f) {
        out_snapshot.is_onset = true;
        onset_cooldown_frames_ = 4;
    } else {
        out_snapshot.is_onset = false;
    }

    // F. Global Energy & RMS
    float sum_energy = 0.0f;
    for (size_t i = 0; i < frame_size; ++i) {
        sum_energy += frame[i] * frame[i];
    }
    out_snapshot.energy = std::clamp(sum_energy / static_cast<float>(frame_size) * 10.0f, 0.0f, 1.0f);
    out_snapshot.rms = std::clamp(std::sqrt(sum_energy / static_cast<float>(frame_size)), 0.0f, 1.0f);

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
                std::move(analysis_frame_.begin() + HOP_SIZE, analysis_frame_.end(), analysis_frame_.begin());
                std::copy_n(hop_scratch, HOP_SIZE, analysis_frame_.data() + (FRAME_SIZE - HOP_SIZE));
                process_frame(analysis_frame_.data(), FRAME_SIZE, current_snapshot);
                snapshot_buffer_.write(current_snapshot);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }
}

void StreamBPhysics::start() {
    if (is_running_.load(std::memory_order_acquire)) return;
    is_running_.store(true, std::memory_order_release);
    worker_thread_ = std::thread(&StreamBPhysics::worker_loop, this);
}

void StreamBPhysics::stop() {
    if (!is_running_.load(std::memory_order_acquire)) return;
    is_running_.store(false, std::memory_order_release);
    if (worker_thread_.joinable()) worker_thread_.join();
}

bool StreamBPhysics::get_latest_snapshot(core::StreamBSnapshot& out_snapshot) noexcept {
    return snapshot_buffer_.read(out_snapshot);
}

} // namespace audio_voyager::audio
