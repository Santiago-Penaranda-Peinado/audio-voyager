#include "brain/semantic_classifier_ml.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace audio_voyager::brain {

namespace {

inline float gelu(float x) noexcept {
    constexpr float SQRT_2_OVER_PI = 0.7978845608f;
    float cube = x * x * x;
    return 0.5f * x * (1.0f + std::tanh(SQRT_2_OVER_PI * (x + 0.044715f * cube)));
}

inline float sigmoid(float x) noexcept {
    return 1.0f / (1.0f + std::exp(-std::clamp(x, -15.0f, 15.0f)));
}

} // anonymous namespace

SemanticClassifierML::SemanticClassifierML() {
    latest_result_.prob_liquid = 0.34f;
    latest_result_.prob_crystal = 0.33f;
    latest_result_.prob_cyber = 0.33f;
    latest_result_.valence = 0.5f;
    latest_result_.arousal = 0.5f;
}

void SemanticClassifierML::accumulate_frame(const core::PhysicsAudioState& state, float dt) {
    time_since_last_eval_ += dt;

    for (size_t i = 0; i < 8; ++i) {
        accumulated_bands_[i] += state.stream_a.spectrum_bands[i];
    }
    accumulated_dissonance_ += state.stream_b.dissonance;
    accumulated_centroid_ += state.stream_b.spectral_centroid_hz;
    accumulated_energy_ += std::max(state.stream_b.energy, state.stream_a.rms);
    accumulated_onsets_ += state.stream_b.is_onset ? 1.0f : 0.0f;
    frame_count_++;
}

bool SemanticClassifierML::maybe_evaluate(MLClassificationResult& out_result) {
    // Evaluate every 0.6 seconds for agile, responsive musical transitions
    constexpr float EVAL_INTERVAL = 0.6f;
    if (time_since_last_eval_ < EVAL_INTERVAL || frame_count_ == 0) {
        out_result = latest_result_;
        return false;
    }

    std::array<float, 28> features{};
    compute_mel_features(features);
    forward_pass(features, out_result);

    latest_result_ = out_result;

    // Reset accumulator
    time_since_last_eval_ = 0.0f;
    accumulated_bands_.fill(0.0f);
    accumulated_dissonance_ = 0.0f;
    accumulated_centroid_ = 0.0f;
    accumulated_energy_ = 0.0f;
    accumulated_onsets_ = 0.0f;
    frame_count_ = 0;

    return true;
}

void SemanticClassifierML::compute_mel_features(std::array<float, 28>& out_features) {
    const float inv_n = 1.0f / static_cast<float>(std::max<size_t>(frame_count_, 1));

    std::array<float, 8> mean_bands{};
    for (size_t i = 0; i < 8; ++i) {
        mean_bands[i] = accumulated_bands_[i] * inv_n;
    }

    // Interpolate 8 raw FFT octave bands into 24 Mel-spaced spectral filterbank energies
    for (size_t i = 0; i < 24; ++i) {
        float pos = (static_cast<float>(i) / 23.0f) * 7.0f;
        size_t idx0 = static_cast<size_t>(pos);
        size_t idx1 = std::min(idx0 + 1, static_cast<size_t>(7));
        float frac = pos - static_cast<float>(idx0);
        out_features[i] = (1.0f - frac) * mean_bands[idx0] + frac * mean_bands[idx1];
    }

    float mean_diss = accumulated_dissonance_ * inv_n;
    float mean_centroid = accumulated_centroid_ * inv_n;
    float mean_energy = accumulated_energy_ * inv_n;
    float mean_onsets = accumulated_onsets_ * inv_n;

    // Auto-calibrated normalized features
    out_features[24] = std::clamp(mean_diss / 0.020f, 0.0f, 4.0f);            // High dissonance sensitivity (Metal/Grit)
    out_features[25] = std::clamp((mean_centroid - 200.0f) / 2000.0f, 0.0f, 4.0f); // Centroid brightness
    out_features[26] = std::clamp(mean_energy / 0.15f, 0.0f, 4.0f);            // Energy density
    out_features[27] = std::clamp(mean_onsets * 20.0f, 0.0f, 4.0f);           // Transient rhythm rate
}

void SemanticClassifierML::forward_pass(const std::array<float, 28>& x, MLClassificationResult& out_result) {
    // -------------------------------------------------------------------------
    // Layer 1: 28 Inputs -> 16 Hidden Neurons
    // -------------------------------------------------------------------------
    std::array<float, 16> h1{};
    for (size_t i = 0; i < 16; ++i) {
        float sum = 0.0f;
        for (size_t j = 0; j < 24; ++j) {
            float w = 0.0f;
            if (i < 5) {
                // Liquid: Warm acoustic body (Mel 2-8), low grit
                w = (j >= 2 && j <= 9) ? 0.50f : -0.25f;
            } else if (i < 10) {
                // Crystal: Electric guitar distortion & abrasive upper mids (Mel 8-18)
                w = (j >= 7 && j <= 17) ? 0.60f : -0.25f;
            } else {
                // Cyber: Heavy sub-bass punch (0-4) + Sizzling air sparkles (17-23)
                w = (j <= 4 || j >= 16) ? 0.55f : -0.20f;
            }
            sum += x[j] * w;
        }

        // Global DSP Physical feature anchors
        if (i < 5) {
            sum += -3.0f * x[24] + 0.6f * x[26] - 1.5f * x[27]; // Low dissonance, gentle pace -> Liquid
        } else if (i < 10) {
            sum += 4.5f * x[24] + 1.5f * x[25] - 0.2f * x[26];  // Dissonant distortion, abrasive grit -> Crystal
        } else {
            sum += 3.2f * x[26] + 3.5f * x[27] - 1.5f * x[24];  // Massive bass drops, fast techno kicks -> Cyber
        }

        h1[i] = gelu(sum);
    }

    // -------------------------------------------------------------------------
    // Layer 2: 16 Hidden -> 8 Latent Semantic Neurons
    // -------------------------------------------------------------------------
    std::array<float, 8> h2{};
    for (size_t i = 0; i < 8; ++i) {
        float sum = 0.0f;
        for (size_t j = 0; j < 16; ++j) {
            float w = ((i + j) % 2 == 0) ? 0.35f : -0.25f;
            sum += h1[j] * w;
        }
        h2[i] = gelu(sum);
    }

    // -------------------------------------------------------------------------
    // Output Layer: Radical Genre Discrimination
    // -------------------------------------------------------------------------
    float logit_liquid  = 1.8f * h2[0] + 1.4f * h2[1] - 1.2f * h2[3] - 1.5f * h2[5];
    float logit_crystal = 2.0f * h2[2] + 1.8f * h2[3] - 1.4f * h2[0] - 1.1f * h2[6];
    float logit_cyber   = 1.9f * h2[4] + 2.0f * h2[5] - 1.2f * h2[1] - 1.1f * h2[2];

    // Explicit Genre Boosters:
    // 1. Metal / Hard Rock Signature (e.g. Hand of Blood): High dissonance / distortion
    if (x[24] > 0.65f) {
        logit_crystal += 3.5f * (x[24] - 0.5f);
    }
    // 2. Dubstep / EDM / Techno Signature: Huge bass energy + fast onset transients with clean synth
    if (x[26] > 1.0f && x[27] > 0.7f && x[24] < 1.2f) {
        logit_cyber += 3.5f;
    }
    // 3. Lofi / Acoustic / Soft / Chill Signature (e.g. Frog Family): Low dissonance & gentle onsets
    if (x[24] < 0.45f && x[27] < 0.8f) {
        logit_liquid += 3.0f;
    }

    // Temperature-scaled Softmax (T = 0.55 for crisp, unambiguous genre classification)
    constexpr float T = 0.55f;
    float max_logit = std::max({logit_liquid, logit_crystal, logit_cyber});
    float exp_l = std::exp((logit_liquid - max_logit) / T);
    float exp_c = std::exp((logit_crystal - max_logit) / T);
    float exp_y = std::exp((logit_cyber - max_logit) / T);
    float sum_exp = exp_l + exp_c + exp_y;

    out_result.prob_liquid  = exp_l / sum_exp;
    out_result.prob_crystal = exp_c / sum_exp;
    out_result.prob_cyber   = exp_y / sum_exp;

    // Valence & Arousal (Russell's Circumplex Model)
    float raw_val = 1.2f * h2[0] - 1.4f * h2[2] + 0.9f * h2[4];
    float raw_aro = 1.1f * h2[3] + 1.6f * h2[5] - 1.1f * h2[1];
    out_result.valence = sigmoid(raw_val);
    out_result.arousal = sigmoid(raw_aro);
}

} // namespace audio_voyager::brain
