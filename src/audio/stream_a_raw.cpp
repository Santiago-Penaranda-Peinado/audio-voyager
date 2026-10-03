#define _USE_MATH_DEFINES
#include "audio/stream_a_raw.hpp"
#include <cmath>
#include <chrono>
#include <algorithm>
#include <numeric>

namespace audio_voyager::audio {

namespace {
// Band frequency limits in Hz: [low, high]
constexpr std::array<std::pair<float, float>, core::FFT_BANDS_COUNT> BAND_LIMITS = {{
    {20.0f,    60.0f},   // Sub-Bass
    {60.0f,    250.0f},  // Bass
    {250.0f,   500.0f},  // Low-Mid
    {500.0f,   2000.0f}, // Mid
    {2000.0f,  4000.0f}, // High-Mid
    {4000.0f,  6000.0f}, // Presence
    {6000.0f,  12000.0f},// Brilliance
    {12000.0f, 20000.0f} // Air
}};
} // anonymous namespace

StreamARaw::StreamARaw(SampleRingBuffer& input_ring_buffer, uint32_t sample_rate)
    : input_ring_buffer_(input_ring_buffer)
    , sample_rate_(sample_rate) {
    
    time_buffer_.assign(core::FFT_SIZE_STREAM_A, 0.0f);
    hann_window_.assign(core::FFT_SIZE_STREAM_A, 0.0f);
    windowed_buffer_.assign(core::FFT_SIZE_STREAM_A, 0.0f);
    magnitude_spectrum_.assign(core::FFT_SIZE_STREAM_A / 2 + 1, 0.0f);
    fft_complex_buffer_.assign(core::FFT_SIZE_STREAM_A, {0.0f, 0.0f});

    init_hann_window();

    // Precalculate bit-reversal indices for high-performance in-place FFT
    bit_reverse_indices_.resize(core::FFT_SIZE_STREAM_A);
    const size_t n = core::FFT_SIZE_STREAM_A;
    for (size_t i = 0; i < n; ++i) {
        size_t rev = 0;
        size_t temp = i;
        for (size_t bit = 1; bit < n; bit <<= 1) {
            rev = (rev << 1) | (temp & 1);
            temp >>= 1;
        }
        bit_reverse_indices_[i] = rev;
    }
}

void StreamARaw::init_hann_window() {
    const size_t n = core::FFT_SIZE_STREAM_A;
    for (size_t i = 0; i < n; ++i) {
        hann_window_[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(n - 1)));
    }
}

void StreamARaw::compute_fft(const std::vector<float>& windowed_signal, std::vector<float>& magnitude_spectrum) {
    const size_t n = core::FFT_SIZE_STREAM_A;

    // 1. Bit-reversal permutation into complex scratch buffer
    for (size_t i = 0; i < n; ++i) {
        fft_complex_buffer_[bit_reverse_indices_[i]] = std::complex<float>(windowed_signal[i], 0.0f);
    }

    // 2. In-place Cooley-Tukey Radix-2 FFT
    for (size_t len = 2; len <= n; len <<= 1) {
        const float angle = -2.0f * static_cast<float>(M_PI) / static_cast<float>(len);
        const std::complex<float> wlen(std::cos(angle), std::sin(angle));
        
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t j = 0; j < len / 2; ++j) {
                const std::complex<float> u = fft_complex_buffer_[i + j];
                const std::complex<float> v = fft_complex_buffer_[i + j + len / 2] * w;
                fft_complex_buffer_[i + j] = u + v;
                fft_complex_buffer_[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    // 3. Magnitude spectrum extraction (normalized)
    const size_t num_bins = n / 2 + 1;
    const float norm_factor = 2.0f / static_cast<float>(n);
    for (size_t i = 0; i < num_bins; ++i) {
        magnitude_spectrum[i] = std::abs(fft_complex_buffer_[i]) * norm_factor;
    }
}

void StreamARaw::extract_frequency_bands(const std::vector<float>& magnitude_spectrum, 
                                        std::array<float, core::FFT_BANDS_COUNT>& out_bands) {
    const float bin_resolution = static_cast<float>(sample_rate_) / static_cast<float>(core::FFT_SIZE_STREAM_A);
    const size_t num_bins = magnitude_spectrum.size();

    for (size_t b = 0; b < core::FFT_BANDS_COUNT; ++b) {
        const float f_low = BAND_LIMITS[b].first;
        const float f_high = BAND_LIMITS[b].second;

        size_t bin_start = std::clamp(static_cast<size_t>(f_low / bin_resolution), size_t{1}, num_bins - 1);
        size_t bin_end = std::clamp(static_cast<size_t>(std::ceil(f_high / bin_resolution)), bin_start + 1, num_bins);

        float sum = 0.0f;
        for (size_t bin = bin_start; bin < bin_end; ++bin) {
            sum += magnitude_spectrum[bin];
        }

        float band_energy = sum / static_cast<float>(std::max(size_t{1}, bin_end - bin_start));
        
        // Logarithmic scaling for visual perceptual response
        band_energy = std::clamp(std::log10(1.0f + 9.0f * band_energy * 10.0f), 0.0f, 1.0f);

        // Attack & Decay smoothing filter (instant attack, smooth decay)
        if (band_energy > smooth_bands_[b]) {
            smooth_bands_[b] = band_energy;
        } else {
            smooth_bands_[b] = smooth_bands_[b] * 0.82f + band_energy * 0.18f;
        }

        out_bands[b] = smooth_bands_[b];
    }
}

void StreamARaw::extract_mel_bands(const std::vector<float>& magnitude_spectrum, 
                                   std::array<float, core::WATERFALL_BANDS>& out_mel_bands) {
    const float bin_resolution = static_cast<float>(sample_rate_) / static_cast<float>(core::FFT_SIZE_STREAM_A);
    const size_t num_bins = magnitude_spectrum.size();

    // 64 Mel bands covering 20 Hz to 20000 Hz
    constexpr float m_min = 31.75f;    // 2595 * log10(1 + 20/700)
    constexpr float m_max = 3816.92f;  // 2595 * log10(1 + 20000/700)
    constexpr float m_step = (m_max - m_min) / static_cast<float>(core::WATERFALL_BANDS);

    for (size_t b = 0; b < core::WATERFALL_BANDS; ++b) {
        float m_low = m_min + static_cast<float>(b) * m_step;
        float m_high = m_low + m_step;

        float f_low = 700.0f * (std::pow(10.0f, m_low / 2595.0f) - 1.0f);
        float f_high = 700.0f * (std::pow(10.0f, m_high / 2595.0f) - 1.0f);

        size_t bin_start = std::clamp(static_cast<size_t>(f_low / bin_resolution), size_t{1}, num_bins - 1);
        size_t bin_end = std::clamp(static_cast<size_t>(std::ceil(f_high / bin_resolution)), bin_start + 1, num_bins);

        float sum = 0.0f;
        for (size_t bin = bin_start; bin < bin_end; ++bin) {
            sum += magnitude_spectrum[bin];
        }

        float band_energy = sum / static_cast<float>(std::max(size_t{1}, bin_end - bin_start));
        // Perceptual logarithmic compression
        band_energy = std::clamp(std::log10(1.0f + 9.0f * band_energy * 10.0f), 0.0f, 1.0f);
        out_mel_bands[b] = band_energy;
    }
}

core::StreamASnapshot StreamARaw::process() {
    const auto t_start = std::chrono::steady_clock::now();
    core::StreamASnapshot snapshot;
    snapshot.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        t_start.time_since_epoch()).count();

    // Pull available samples from ring buffer to fill/slide the time buffer
    constexpr size_t SCRATCH_SIZE = 2048;
    float scratch[SCRATCH_SIZE];
    const size_t pulled = input_ring_buffer_.pop_n(scratch, SCRATCH_SIZE);

    if (pulled > 0) {
        if (pulled >= core::FFT_SIZE_STREAM_A) {
            // New data completely fills time buffer
            std::copy_n(scratch + (pulled - core::FFT_SIZE_STREAM_A), core::FFT_SIZE_STREAM_A, time_buffer_.data());
        } else {
            // Slide existing samples to the left and append new ones
            std::move(time_buffer_.begin() + pulled, time_buffer_.end(), time_buffer_.begin());
            std::copy_n(scratch, pulled, time_buffer_.data() + (core::FFT_SIZE_STREAM_A - pulled));
        }
    }

    // 1. Oscilloscope waveform (latest RAW_OSCILLOSCOPE_SAMPLES samples)
    const size_t osc_offset = core::FFT_SIZE_STREAM_A - core::RAW_OSCILLOSCOPE_SAMPLES;
    std::copy_n(time_buffer_.data() + osc_offset, core::RAW_OSCILLOSCOPE_SAMPLES, snapshot.waveform.data());

    // 2. Windowing
    for (size_t i = 0; i < core::FFT_SIZE_STREAM_A; ++i) {
        windowed_buffer_[i] = time_buffer_[i] * hann_window_[i];
    }

    // 3. FFT computation
    compute_fft(windowed_buffer_, magnitude_spectrum_);

    // 4. Frequency band extraction (8 octave bands & 64 Mel waterfall bands)
    extract_frequency_bands(magnitude_spectrum_, snapshot.spectrum_bands);
    extract_mel_bands(magnitude_spectrum_, snapshot.mel_bands);

    // 5. Peak & RMS amplitude
    float peak = 0.0f;
    float sum_sq = 0.0f;
    for (size_t i = 0; i < core::RAW_OSCILLOSCOPE_SAMPLES; ++i) {
        const float val = snapshot.waveform[i];
        peak = std::max(peak, std::abs(val));
        sum_sq += val * val;
    }
    snapshot.peak_amplitude = std::clamp(peak, 0.0f, 1.0f);
    snapshot.rms = std::clamp(std::sqrt(sum_sq / static_cast<float>(core::RAW_OSCILLOSCOPE_SAMPLES)), 0.0f, 1.0f);

    const auto t_end = std::chrono::steady_clock::now();
    snapshot.latency_ms = std::chrono::duration<float, std::milli>(t_end - t_start).count();

    return snapshot;
}

} // namespace audio_voyager::audio
