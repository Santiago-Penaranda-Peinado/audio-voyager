#pragma once

#include "core/types.hpp"
#include <array>
#include <vector>
#include <cmath>
#include <cstddef>

namespace audio_voyager::brain {

struct MLClassificationResult {
    float prob_liquid{1.0f};   // P(Acoustic, Organic, Harmonious)
    float prob_crystal{0.0f};  // P(Tonal Tension, High Dissonance, Metallic)
    float prob_cyber{0.0f};    // P(Electronic, High-Energy Synth, Steady Beat)
    float valence{0.5f};       // Emotional Valence [0.0 = dark/tense, 1.0 = bright/euphoric]
    float arousal{0.5f};       // Physiological Arousal [0.0 = calm, 1.0 = intense]
};

class SemanticClassifierML {
public:
    SemanticClassifierML();
    ~SemanticClassifierML() = default;

    // Accumulates short-term DSP observations into analysis window
    void accumulate_frame(const core::PhysicsAudioState& state, float dt);

    // Evaluates the embedded neural perception model if interval has elapsed
    bool maybe_evaluate(MLClassificationResult& out_result);

    [[nodiscard]] const MLClassificationResult& get_latest_result() const noexcept {
        return latest_result_;
    }

private:
    void compute_mel_features(std::array<float, 28>& out_features);
    void forward_pass(const std::array<float, 28>& input, MLClassificationResult& out_result);

    // Analysis window accumulation
    float time_since_last_eval_{0.0f};
    static constexpr float EVALUATION_INTERVAL_S = 1.0f; // 1.0 Hz evaluation frequency

    std::array<float, 8> accumulated_bands_{};
    float accumulated_dissonance_{0.0f};
    float accumulated_centroid_{0.0f};
    float accumulated_energy_{0.0f};
    float accumulated_onsets_{0.0f};
    size_t frame_count_{0};

    MLClassificationResult latest_result_{};
};

} // namespace audio_voyager::brain
