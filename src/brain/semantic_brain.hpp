#pragma once

#include "core/types.hpp"
#include <deque>
#include <vector>
#include <cmath>
#include <algorithm>

namespace audio_voyager::brain {

// Dynamic Auto-Gain Calibrator with sliding peak window
class AutoGainControl {
public:
    explicit AutoGainControl(float default_max = 1.0f, float default_min = 0.0f, float decay_time_s = 12.0f)
        : peak_(default_max), floor_(default_min), decay_rate_(1.0f / std::max(decay_time_s, 1.0f)) {}

    float update(float raw_val, float dt) noexcept {
        // Fast attack for peaks
        if (raw_val > peak_) {
            peak_ = raw_val;
        } else {
            // Slow exponential decay to adapt to quieter passages
            peak_ -= peak_ * (1.0f - std::exp(-decay_rate_ * dt * 0.5f));
            peak_ = std::max(peak_, floor_ + 1e-4f);
        }

        // Noise floor adaptation
        if (raw_val < floor_) {
            floor_ = raw_val;
        } else {
            floor_ += (raw_val - floor_) * (1.0f - std::exp(-decay_rate_ * dt * 0.1f));
        }

        float range = peak_ - floor_;
        if (range <= 1e-5f) return 0.5f;

        float normalized = (raw_val - floor_) / range;
        return std::clamp(normalized, 0.0f, 1.0f);
    }

    [[nodiscard]] float get_peak() const noexcept { return peak_; }
    [[nodiscard]] float get_floor() const noexcept { return floor_; }

private:
    float peak_{1.0f};
    float floor_{0.0f};
    float decay_rate_{0.083f}; // ~12s time constant
};

struct AsymmetricFilter {
    float current{0.0f};
    float attack_rate{45.0f};
    float decay_rate{4.0f};

    float update(float target, float dt) noexcept {
        float rate = (target > current) ? attack_rate : decay_rate;
        float alpha = 1.0f - std::exp(-rate * dt);
        current += alpha * (target - current);
        return current;
    }
};

class SemanticBrain {
public:
    SemanticBrain();
    ~SemanticBrain() = default;

    void update(const core::PhysicsAudioState& audio_state, float dt);
    [[nodiscard]] const core::AudioSemanticVector& get_semantic_vector() const noexcept {
        return vector_;
    }

private:
    void update_bpm(float sub_bass, float onset_val, float dt);

    core::AudioSemanticVector vector_{};

    // Auto-Gain Calibrators (12-second rolling calibration)
    AutoGainControl agc_dissonance_{0.005f, 0.0001f, 10.0f};
    AutoGainControl agc_centroid_{4000.0f, 200.0f, 12.0f};
    AutoGainControl agc_energy_{0.3f, 0.01f, 10.0f};
    AutoGainControl agc_sub_bass_{0.4f, 0.01f, 8.0f};
    AutoGainControl agc_treble_{0.3f, 0.01f, 8.0f};

    // Smooth filters
    AsymmetricFilter smooth_dissonance_{0.0f, 50.0f, 3.5f};
    AsymmetricFilter smooth_centroid_{0.5f, 35.0f, 4.0f};
    AsymmetricFilter smooth_energy_{0.0f, 40.0f, 4.0f};
    AsymmetricFilter smooth_sub_bass_{0.0f, 45.0f, 4.0f};
    AsymmetricFilter smooth_treble_{0.0f, 45.0f, 4.0f};
    AsymmetricFilter smooth_speed_{3.0f, 30.0f, 2.5f};
    AsymmetricFilter smooth_folding_{0.0f, 20.0f, 2.5f};
    AsymmetricFilter smooth_glitch_{0.0f, 100.0f, 18.0f};

    // BPM tracking
    static constexpr size_t ONSET_HISTORY_SIZE = 128;
    std::deque<float> onset_history_{};
    float time_since_last_beat_{0.0f};
    float beat_interval_estimate_{0.5f};
};

} // namespace audio_voyager::brain
