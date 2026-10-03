#pragma once

#include "core/types.hpp"
#include "core/lockfree_ring_buffer.hpp"
#include <vector>
#include <array>
#include <complex>

namespace audio_voyager::audio {

/**
 * @brief Stream A: Raw Reactivity Processor (Ultra Low Latency Fast Path).
 * 
 * Computes in real-time:
 * 1. Instantaneous Oscilloscope Waveform.
 * 2. Hann-windowed Fast Fourier Transform (FFT).
 * 3. 8 Logarithmic Frequency Energy Bands.
 * 4. Peak Amplitude and Signal RMS.
 */
class StreamARaw {
public:
    using SampleRingBuffer = core::LockFreeRingBuffer<float>;

    explicit StreamARaw(SampleRingBuffer& input_ring_buffer, 
                        uint32_t sample_rate = core::DEFAULT_SAMPLE_RATE);
    ~StreamARaw() = default;

    /**
     * @brief Pulls available audio samples and computes a fresh StreamASnapshot.
     * @return StreamASnapshot with current oscilloscope and spectrum state.
     */
    core::StreamASnapshot process();

private:
    void init_hann_window();
    void compute_fft(const std::vector<float>& windowed_signal, std::vector<float>& magnitude_spectrum);
    void extract_frequency_bands(const std::vector<float>& magnitude_spectrum, 
                                 std::array<float, core::FFT_BANDS_COUNT>& out_bands);
    void extract_mel_bands(const std::vector<float>& magnitude_spectrum, 
                           std::array<float, core::WATERFALL_BANDS>& out_mel_bands);

    SampleRingBuffer& input_ring_buffer_;
    uint32_t sample_rate_;

    std::vector<float> time_buffer_;        // Size: FFT_SIZE_STREAM_A (1024)
    std::vector<float> hann_window_;        // Size: FFT_SIZE_STREAM_A (1024)
    std::vector<float> windowed_buffer_;    // Size: FFT_SIZE_STREAM_A (1024)
    std::vector<float> magnitude_spectrum_; // Size: FFT_SIZE_STREAM_A / 2 + 1 (513)
    
    // Internal complex FFT scratch buffer
    std::vector<std::complex<float>> fft_complex_buffer_;
    std::vector<size_t> bit_reverse_indices_;

    // Smoothing filter for visual aesthetics on band bars
    std::array<float, core::FFT_BANDS_COUNT> smooth_bands_{};
};

} // namespace audio_voyager::audio
