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
    [[nodiscard]] const WaterfallAnalyzer& get_waterfall_analyzer() const noexcept {
        return waterfall_analyzer_;
    }
    [[nodiscard]] const WaterfallMetrics& get_waterfall_metrics() const noexcept {
        return waterfall_analyzer_.get_metrics();
    }

private:
    void update_bpm(float sub_bass, float onset_val, float dt, bool is_silent);

    core::AudioSemanticVector vector_{};
    WaterfallAnalyzer waterfall_analyzer_{};
    SemanticClassifierML ml_classifier_{};
    MLClassificationResult last_ml_result_{};
    float last_best_corr_{0.0f};

    // Slow temporal integrator for Mind / ML biome weights (tau ~ 3.5s)
    float smooth_weight_liquid_{0.25f};
    float smooth_weight_metal_{0.25f};
    float smooth_weight_crystal_{0.25f};
    float smooth_weight_cyber_{0.25f};
    float smooth_weight_dubstep_{0.25f};
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

    // Fast transient rhythm and tempo gear-shift reactivity
    float prev_energy_{0.0f};
    float prev_flux_{0.0f};
    float gear_shift_impulse_{0.0f};

    // Continuous Temporal Low-Pass Filtering for Cohesive Biome Colors (tau ~ 1.8s)
    glm::vec3 smooth_color_primary_{0.08f, 0.42f, 0.88f};
    glm::vec3 smooth_color_accent_{1.00f, 0.72f, 0.22f};
    glm::vec3 smooth_color_zenith_{0.005f, 0.012f, 0.028f};
    glm::vec3 smooth_part_base_{0.06f, 0.38f, 0.85f};
    glm::vec3 smooth_part_peak_{1.00f, 0.75f, 0.25f};

    // BPM autocorrelation tracking
    static constexpr size_t ONSET_HISTORY_SIZE = 360;
    static constexpr float BPM_HOP_INTERVAL = 0.022f; // ~45 Hz sampling
    std::deque<float> onset_history_{};
    float bpm_sample_timer_{0.0f};
    float bpm_calc_timer_{0.0f};
    float current_bpm_{120.0f};
    float current_bpm_conf_{0.5f};
    float smooth_sub_baseline_{0.0f};
};

} // namespace audio_voyager::brain
