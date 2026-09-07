#pragma once

#include "core/types.hpp"
#include "brain/semantic_classifier_ml.hpp"
#include <deque>

namespace audio_voyager::brain {

class SemanticBrain {
public:
    SemanticBrain();
    ~SemanticBrain() = default;

    void update(const core::PhysicsAudioState& audio_state, float dt);
    [[nodiscard]] const core::AudioSemanticVector& get_semantic_vector() const noexcept { return vector_; }

private:
    void update_bpm(float sub_bass, float onset_val, float dt, bool is_silent);

    SemanticClassifierML ml_classifier_;
    core::AudioSemanticVector vector_{};

    // Smooth Leaky Integrators for 5 Biomes (The Mind)
    float smooth_weight_ocean_{0.20f};
    float smooth_weight_metal_{0.20f};
    float smooth_weight_cyber_{0.20f};
    float smooth_weight_ethereal_{0.20f};
    float smooth_weight_funk_{0.20f};
    float smooth_valence_{0.5f};
    float smooth_arousal_{0.5f};

    // Real-Time 144Hz Physical Excitations (The Muscle)
    float smooth_dilation_{0.0f};
    float smooth_ripple_{0.0f};
    float smooth_mids_{0.0f};
    float smooth_air_{0.0f};
    float smooth_emission_{1.0f};
    float smooth_centroid_{0.5f};
    float smooth_speed_{1.4f};
    float smooth_roll_{0.0f};

    // BPM state tracker & beat phase accumulator
    float current_bpm_{120.0f};
    float current_bpm_conf_{0.5f};
    float time_since_last_beat_{0.0f};
    float beat_interval_estimate_{0.5f}; // 120 BPM default
    float beat_phase_{0.0f};

    std::deque<float> onset_history_;
    static constexpr size_t ONSET_HISTORY_SIZE = 64;
};

} // namespace audio_voyager::brain
