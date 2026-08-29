#pragma once

#include "core/types.hpp"
#include "brain/semantic_classifier_ml.hpp"
#include <deque>
#include <vector>
#include <cmath>
#include <algorithm>

namespace audio_voyager::brain {

class SemanticBrain {
public:
    SemanticBrain();
    ~SemanticBrain() = default;

    void update(const core::PhysicsAudioState& audio_state, float dt);
    [[nodiscard]] const core::AudioSemanticVector& get_semantic_vector() const noexcept {
        return vector_;
    }

private:
    void update_bpm(float sub_bass, float onset_val, float dt, bool is_silent);

    core::AudioSemanticVector vector_{};
    SemanticClassifierML ml_classifier_{};

    // Slow temporal integrator for Mind / ML biome weights (tau ~ 3.5s)
    float smooth_weight_liquid_{0.34f};
    float smooth_weight_crystal_{0.33f};
    float smooth_weight_cyber_{0.33f};
    float smooth_valence_{0.5f};
    float smooth_arousal_{0.5f};

    // Fast-path Leaky Integrators for Muscle / DSP excitation
    float smooth_dilation_{0.0f};
    float smooth_ripple_{0.0f};
    float smooth_mids_{0.0f};
    float smooth_air_{0.0f};
    float smooth_emission_{1.0f};
    float smooth_centroid_{0.5f};
    float smooth_speed_{1.3f};

    // BPM tracking
    static constexpr size_t ONSET_HISTORY_SIZE = 128;
    std::deque<float> onset_history_{};
    float time_since_last_beat_{0.0f};
    float beat_interval_estimate_{0.5f};
    float current_bpm_{120.0f};
    float current_bpm_conf_{0.5f};
};

} // namespace audio_voyager::brain
