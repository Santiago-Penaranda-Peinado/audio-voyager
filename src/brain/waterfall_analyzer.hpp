#pragma once

#include "core/types.hpp"
#include <array>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace audio_voyager::brain {

/**
 * @brief Spatiotemporal Features extracted from 2D Time-Frequency Waterfall Matrix
 */
struct WaterfallMetrics {
    float horizontal_continuity{0.0f};       // [0, 1] Overall sustained spectral continuity
    float guitar_continuity{0.0f};            // [0, 1] Sustained energy in 300Hz-6000Hz (distorted guitar wall)
    float vertical_pulse_periodicity{0.0f};   // [0, 1] Autocorrelation of bass column pulses
    float four_on_the_floor_regularity{0.0f}; // [0, 1] Match for 4-on-the-floor steady kick cadence
    float bass_temporal_flux{0.0f};           // [0, 1] Sub/bass modulation/wobble (Dubstep LFO)
    float mid_temporal_flux{0.0f};            // [0, 1] Melodic mid-frequency flux
    float temporal_variance{0.0f};            // [0, 1] Total temporal variance across matrix
};

namespace detail {
// WebSDR classic waterfall colormap (Navy -> Blue -> Cyan -> Green -> Yellow -> Red -> White)
inline uint32_t websdr_colormap(float val) {
    val = std::clamp(val, 0.0f, 1.0f);

    struct ColorStop { float pos; uint8_t r; uint8_t g; uint8_t b; };
    static constexpr std::array<ColorStop, 7> STOPS = {{
        {0.00f,   4,   8,  28},  // Deep Midnight Navy
        {0.18f,   0,  48, 160},  // Deep Marine Blue
        {0.38f,   0, 190, 220},  // Vibrant Cyan
        {0.58f,  30, 215,  75},  // Neon Green
        {0.78f, 255, 215,   0},  // Pure Yellow
        {0.92f, 240,  55,  20},  // Fiery Red
        {1.00f, 255, 255, 255}   // Blazing White
    }};

    uint8_t r = 255, g = 255, b = 255;
    for (size_t i = 0; i < STOPS.size() - 1; ++i) {
        if (val >= STOPS[i].pos && val <= STOPS[i + 1].pos) {
            float frac = (val - STOPS[i].pos) / (STOPS[i + 1].pos - STOPS[i].pos);
            r = static_cast<uint8_t>(STOPS[i].r + frac * (STOPS[i + 1].r - STOPS[i].r));
            g = static_cast<uint8_t>(STOPS[i].g + frac * (STOPS[i + 1].g - STOPS[i].g));
            b = static_cast<uint8_t>(STOPS[i].b + frac * (STOPS[i + 1].b - STOPS[i].b));
            break;
        }
    }

    // RGBA 32-bit little-endian: 0xAABBGGRR
    return (0xFF000000u) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
}
} // namespace detail

/**
 * @brief 2D Time-Frequency Waterfall Matrix & Spatiotemporal Pattern Analysis Engine.
 * 
 * Maintains a 64 Mel/Bark bands x 128 frames (~2.5s) rolling spectral history.
 * Performs real-time feature extraction:
 * 1. Horizontal spectral continuity (sustained guitar chords/distorted riffs vs sparse acoustic notes).
 * 2. Vertical periodic pulses (4-on-the-floor kick columns vs syncopated beats).
 * 3. Temporal variance & flux across bands (dubstep wobble vs steady techno vs ambient).
 * 4. WebSDR colormap texture generation for live ImGui HUD monitoring.
 */
class WaterfallAnalyzer {
public:
    static constexpr size_t BANDS = core::WATERFALL_BANDS;   // 64 Mel bands
    static constexpr size_t FRAMES = core::WATERFALL_FRAMES; // 128 frames (~2.5s)

    WaterfallAnalyzer() {
        for (auto& row : matrix_) {
            row.fill(0.0f);
        }
    }
    ~WaterfallAnalyzer() = default;

    // Push new frame of 64 Mel bands
    void push_frame(const std::array<float, BANDS>& mel_bands) {
        matrix_[write_idx_] = mel_bands;
        write_idx_ = (write_idx_ + 1) % FRAMES;
        total_frames_++;
    }

    // Push frame interpolated from 8 octave bands (for synthetic or test mode)
    void push_frame_octaves(const std::array<float, core::FFT_BANDS_COUNT>& octave_bands) {
        std::array<float, BANDS> mel_interpolated{};
        for (size_t b = 0; b < BANDS; ++b) {
            float pos = (static_cast<float>(b) / static_cast<float>(BANDS - 1)) * 7.0f;
            size_t idx0 = static_cast<size_t>(pos);
            size_t idx1 = std::min(idx0 + 1, size_t{7});
            float frac = pos - static_cast<float>(idx0);
            mel_interpolated[b] = (1.0f - frac) * octave_bands[idx0] + frac * octave_bands[idx1];
        }
        push_frame(mel_interpolated);
    }

    // Perform spatiotemporal pattern analysis
    const WaterfallMetrics& analyze() {
        const size_t n_frames = std::min(total_frames_, FRAMES);
        if (n_frames < 8) {
            metrics_ = WaterfallMetrics{};
            return metrics_;
        }

        // 1. Horizontal Spectral Continuity
        // Distorted guitars / power chords maintain sustained energy across frames in mids (300Hz-6000Hz, bands 14-46)
        float sum_guitar_energy = 0.0f;
        float sum_guitar_overlap = 0.0f;
        float sum_guitar_union = 0.0f;

        float sum_total_overlap = 0.0f;
        float sum_total_union = 0.0f;

        for (size_t b = 0; b < BANDS; ++b) {
            float band_overlap = 0.0f;
            float band_union = 0.0f;
            float band_energy = 0.0f;

            for (size_t t = 0; t < n_frames - 1; ++t) {
                float v0 = get_frame_chronological(t)[b];
                float v1 = get_frame_chronological(t + 1)[b];
                band_overlap += std::min(v0, v1);
                band_union += std::max(v0, v1);
                band_energy += v0;
            }

            sum_total_overlap += band_overlap;
            sum_total_union += (band_union + 1e-4f);

            // Active musical guitar / melodic range (approx 300 Hz - 6000 Hz)
            if (b >= 14 && b <= 46) {
                sum_guitar_energy += band_energy;
                sum_guitar_overlap += band_overlap;
                sum_guitar_union += (band_union + 1e-4f);
            }
        }

        metrics_.horizontal_continuity = std::clamp(sum_total_overlap / sum_total_union, 0.0f, 1.0f);
        metrics_.guitar_continuity = std::clamp(sum_guitar_overlap / sum_guitar_union, 0.0f, 1.0f);

        // 2. Vertical Column Pulses & 4-on-the-Floor Cadence
        // Sum sub-bass and low-bass bands (0 to 12) for each frame to track vertical kick energy
        std::vector<float> bass_column(n_frames, 0.0f);
        float sum_bass = 0.0f;
        for (size_t t = 0; t < n_frames; ++t) {
            const auto& frame = get_frame_chronological(t);
            float col_energy = 0.0f;
            for (size_t b = 0; b <= 12; ++b) {
                col_energy += frame[b];
            }
            bass_column[t] = col_energy;
            sum_bass += col_energy;
        }

        float mean_bass = sum_bass / static_cast<float>(n_frames);
        float var_bass = 0.0f;
        for (float v : bass_column) {
            float d = v - mean_bass;
            var_bass += d * d;
        }
        var_bass /= static_cast<float>(n_frames);

        if (var_bass > 1e-5f) {
            // Autocorrelation over lags 12 to 36 (approx 65 to 220 BPM at ~45-50 Hz sampling)
            float best_r = 0.0f;
            int best_lag = 0;

            for (int lag = 12; lag <= 36 && lag < static_cast<int>(n_frames / 2); ++lag) {
                float sum_corr = 0.0f;
                int count = 0;
                for (size_t t = 0; t + lag < n_frames; ++t) {
                    sum_corr += (bass_column[t] - mean_bass) * (bass_column[t + lag] - mean_bass);
                    count++;
                }
                float r = (count > 0) ? (sum_corr / (static_cast<float>(count) * var_bass)) : 0.0f;
                if (r > best_r) {
                    best_r = r;
                    best_lag = lag;
                }
            }

            metrics_.vertical_pulse_periodicity = std::clamp(best_r, 0.0f, 1.0f);

            // 4-on-the-floor steady kick cadence: steady periodic kick columns at 120-140 BPM (lag 18 to 26)
            if (best_lag >= 17 && best_lag <= 27 && best_r > 0.25f) {
                metrics_.four_on_the_floor_regularity = std::clamp((best_r - 0.20f) * 1.4f, 0.0f, 1.0f);
            } else {
                metrics_.four_on_the_floor_regularity = 0.0f;
            }
        } else {
            metrics_.vertical_pulse_periodicity = 0.0f;
            metrics_.four_on_the_floor_regularity = 0.0f;
        }

        // 3. Temporal Flux & Variance Across Bands
        // Sub/bass flux (bands 0-24) detects aggressive Dubstep wobble / filter modulation
        float sum_bass_flux = 0.0f;
        float sum_mid_flux = 0.0f;
        float sum_variance = 0.0f;

        for (size_t b = 0; b < BANDS; ++b) {
            float band_sum = 0.0f;
            float band_flux = 0.0f;

            for (size_t t = 0; t < n_frames - 1; ++t) {
                float v0 = get_frame_chronological(t)[b];
                float v1 = get_frame_chronological(t + 1)[b];
                band_sum += v0;
                band_flux += std::max(0.0f, v0 - v1);
            }
            band_sum += get_frame_chronological(n_frames - 1)[b];

            float mean = band_sum / static_cast<float>(n_frames);
            float var = 0.0f;
            for (size_t t = 0; t < n_frames; ++t) {
                float diff = get_frame_chronological(t)[b] - mean;
                var += diff * diff;
            }
            var /= static_cast<float>(n_frames);
            sum_variance += var;

            float norm_flux = band_flux / static_cast<float>(n_frames - 1);
            if (b <= 24) {
                sum_bass_flux += norm_flux;
            } else if (b <= 48) {
                sum_mid_flux += norm_flux;
            }
        }

        metrics_.bass_temporal_flux = std::clamp((sum_bass_flux / 25.0f) * 8.0f, 0.0f, 1.0f);
        metrics_.mid_temporal_flux = std::clamp((sum_mid_flux / 24.0f) * 8.0f, 0.0f, 1.0f);
        metrics_.temporal_variance = std::clamp((sum_variance / static_cast<float>(BANDS)) * 12.0f, 0.0f, 1.0f);

        return metrics_;
    }

    [[nodiscard]] const WaterfallMetrics& get_metrics() const noexcept { return metrics_; }

    // Generates a 64 x 128 RGBA image buffer using WebSDR colormap
    void generate_rgba_texture(std::vector<uint32_t>& out_pixels) const {
        out_pixels.resize(BANDS * FRAMES);

        for (size_t y = 0; y < FRAMES; ++y) {
            // Chronological order: Row 0 is newest frame (top), Row FRAMES-1 is oldest frame (bottom)
            const auto& frame = get_frame_chronological(y);
            for (size_t x = 0; x < BANDS; ++x) {
                float val = frame[x];
                out_pixels[y * BANDS + x] = detail::websdr_colormap(val);
            }
        }
    }

    // Access to chronological frame (0 = newest, FRAMES-1 = oldest)
    [[nodiscard]] const std::array<float, BANDS>& get_frame_chronological(size_t age_index) const {
        size_t clamped_age = std::min(age_index, FRAMES - 1);
        size_t idx = (write_idx_ + FRAMES - 1 - clamped_age) % FRAMES;
        return matrix_[idx];
    }

    [[nodiscard]] size_t get_total_frames() const noexcept { return total_frames_; }

private:
    std::array<std::array<float, BANDS>, FRAMES> matrix_{};
    size_t write_idx_{0};
    size_t total_frames_{0};
    WaterfallMetrics metrics_{};
};

} // namespace audio_voyager::brain
