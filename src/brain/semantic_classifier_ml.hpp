#pragma once

#include "core/types.hpp"
#include <array>
#include <vector>

namespace audio_voyager::brain {

struct MLClassificationResult {
    float prob_ocean{0.20f};
    float prob_metal{0.20f};
    float prob_cyber{0.20f};
    float prob_ethereal{0.20f};
    float prob_funk{0.20f};
    float valence{0.5f};
    float arousal{0.5f};
};

class SemanticClassifierML {
public:
    SemanticClassifierML();
    ~SemanticClassifierML() = default;

    void accumulate_frame(const core::PhysicsAudioState& state, float dt);
    bool maybe_evaluate(MLClassificationResult& out_result);

private:
    void compute_mel_features(std::array<float, 28>& out_features);
    void forward_pass(const std::array<float, 28>& features, MLClassificationResult& out_result);

    float time_since_last_eval_{0.0f};
    size_t frame_count_{0};

    std::array<float, 8> accumulated_bands_{};
    float accumulated_dissonance_{0.0f};
    float accumulated_centroid_{0.0f};
    float accumulated_energy_{0.0f};
    float accumulated_onsets_{0.0f};

    MLClassificationResult latest_result_{};
};

} // namespace audio_voyager::brain
