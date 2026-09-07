#include "brain/semantic_classifier_ml.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace audio_voyager::brain {

namespace {

inline float sigmoid(float x) noexcept {
    return 1.0f / (1.0f + std::exp(-std::clamp(x, -15.0f, 15.0f)));
}

} // anonymous namespace

SemanticClassifierML::SemanticClassifierML() {
    latest_result_.prob_ocean = 0.20f;
    latest_result_.prob_metal = 0.20f;
    latest_result_.prob_cyber = 0.20f;
    latest_result_.prob_ethereal = 0.20f;
    latest_result_.prob_funk = 0.20f;
    latest_result_.valence = 0.5f;
    latest_result_.arousal = 0.5f;
}

void SemanticClassifierML::accumulate_frame(const core::PhysicsAudioState& state, float dt) {
    time_since_last_eval_ += dt;

    for (size_t i = 0; i < 8; ++i) {
        accumulated_bands_[i] += state.stream_a.spectrum_bands[i];
    }
    
    // Combined Roughness: Sethares dissonance + Spectral Flatness (Wiener entropy)
    // Distorted metal guitars create a dense broadband noise floor (high flatness)
    // whereas harmonic/clean signals have low flatness.
    float frame_roughness = std::max(state.stream_b.dissonance, state.stream_b.spectral_flatness * 0.85f);
    accumulated_dissonance_ += frame_roughness;
    
    accumulated_centroid_ += state.stream_b.spectral_centroid_hz;
    accumulated_energy_ += std::max(state.stream_b.energy, state.stream_a.rms);
    accumulated_onsets_ += state.stream_b.is_onset ? 1.0f : 0.0f;
    frame_count_++;
}

bool SemanticClassifierML::maybe_evaluate(MLClassificationResult& out_result) {
    constexpr float EVAL_INTERVAL = 0.35f;
    if (time_since_last_eval_ < EVAL_INTERVAL || frame_count_ == 0) {
        out_result = latest_result_;
        return false;
    }

    std::array<float, 28> features{};
    compute_mel_features(features);
    forward_pass(features, out_result);

    latest_result_ = out_result;

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

    for (size_t i = 0; i < 24; ++i) {
        float pos = (static_cast<float>(i) / 23.0f) * 7.0f;
        size_t idx0 = static_cast<size_t>(pos);
        size_t idx1 = std::min(idx0 + 1, static_cast<size_t>(7));
        float frac = pos - static_cast<float>(idx0);
        out_features[i] = (1.0f - frac) * mean_bands[idx0] + frac * mean_bands[idx1];
    }

    out_features[24] = accumulated_dissonance_ * inv_n;
    out_features[25] = accumulated_centroid_ * inv_n;
    out_features[26] = accumulated_energy_ * inv_n;
    // Framerate-independent pulse density (onsets per second)
    out_features[27] = (time_since_last_eval_ > 0.01f)
        ? (accumulated_onsets_ / time_since_last_eval_)
        : 0.0f;
}

void SemanticClassifierML::forward_pass(const std::array<float, 28>& x, MLClassificationResult& out_result) {
    // 1. Compute Relative Multi-Band Spectral Balances
    float total_energy = 0.0f;
    for (size_t i = 0; i < 24; ++i) {
        total_energy += x[i];
    }
    total_energy = std::max(total_energy, 1e-4f);

    float sub_bass_share = (x[0] + x[1] + x[2] + x[3]) / total_energy;
    float mids_share     = (x[7] + x[8] + x[9] + x[10] + x[11]) / total_energy;
    float air_share      = (x[19] + x[20] + x[21] + x[22] + x[23]) / total_energy;

    // 2. Normalized 6D Acoustic Feature Vector f in [0, 1]^6
    std::array<float, 6> f{};

    // f[0]: Spectral Centroid (Warm <-> Bright)
    f[0] = std::clamp((x[25] - 200.0f) / 3800.0f, 0.0f, 1.0f);

    // f[1]: Energy / Flux (Calm <-> Kinetic)
    f[1] = std::clamp(x[26] / 0.16f, 0.0f, 1.0f);

    // f[2]: Roughness (Smooth Consonance <-> Aggressive Distorted Noise)
    f[2] = std::clamp(x[24] / 0.40f, 0.0f, 1.0f);

    // f[3]: Mid vs Sub-Bass Ratio  
    f[3] = std::clamp(mids_share / (mids_share + sub_bass_share + 1e-4f), 0.0f, 1.0f);

    // f[4]: Air Sibilance / Sparkle
    f[4] = std::clamp(air_share / 0.25f, 0.0f, 1.0f);

    // f[5]: Rhythmic Pulse Density (onsets per second, framerate-independent)
    f[5] = std::clamp(x[27] / 6.0f, 0.0f, 1.0f); // 6 onsets/s = max density

    // 3. Recalibrated Prototype Centroids
    //                           [Centroid, Energy, Roughness, MidVsBass, Air, Pulse/s]
    constexpr std::array<float, 6> PROTO_OCEAN    = {0.30f, 0.18f, 0.08f, 0.55f, 0.12f, 0.10f};
    constexpr std::array<float, 6> PROTO_METAL    = {0.50f, 0.80f, 0.75f, 0.52f, 0.30f, 0.75f};
    constexpr std::array<float, 6> PROTO_CYBER    = {0.55f, 0.82f, 0.12f, 0.30f, 0.70f, 0.78f};
    constexpr std::array<float, 6> PROTO_ETHEREAL = {0.75f, 0.25f, 0.05f, 0.65f, 0.55f, 0.10f};
    constexpr std::array<float, 6> PROTO_FUNK     = {0.45f, 0.55f, 0.12f, 0.50f, 0.35f, 0.60f};

    constexpr std::array<float, 6> WEIGHTS = {1.0f, 1.0f, 1.8f, 1.5f, 1.3f, 0.8f};

    auto compute_weighted_dist_sq = [&](const std::array<float, 6>& proto) {
        float d2 = 0.0f;
        for (size_t i = 0; i < 6; ++i) {
            float diff = f[i] - proto[i];
            d2 += WEIGHTS[i] * diff * diff;
        }
        return d2;
    };

    float d2_ocean    = compute_weighted_dist_sq(PROTO_OCEAN);
    float d2_metal    = compute_weighted_dist_sq(PROTO_METAL);
    float d2_cyber    = compute_weighted_dist_sq(PROTO_CYBER);
    float d2_ethereal = compute_weighted_dist_sq(PROTO_ETHEREAL);
    float d2_funk     = compute_weighted_dist_sq(PROTO_FUNK);

    // 4. Gaussian RBF Similarity & Softmax
    constexpr float SIGMA_SQ_2 = 0.55f;
    float sim_ocean    = std::exp(-d2_ocean / SIGMA_SQ_2);
    float sim_metal    = std::exp(-d2_metal / SIGMA_SQ_2);
    float sim_cyber    = std::exp(-d2_cyber / SIGMA_SQ_2);
    float sim_ethereal = std::exp(-d2_ethereal / SIGMA_SQ_2);
    float sim_funk     = std::exp(-d2_funk / SIGMA_SQ_2);

    float sum_sim = sim_ocean + sim_metal + sim_cyber + sim_ethereal + sim_funk;
    if (sum_sim < 1e-6f) sum_sim = 1.0f;

    out_result.prob_ocean    = sim_ocean / sum_sim;
    out_result.prob_metal    = sim_metal / sum_sim;
    out_result.prob_cyber    = sim_cyber / sum_sim;
    out_result.prob_ethereal = sim_ethereal / sum_sim;
    out_result.prob_funk     = sim_funk / sum_sim;

    // 5. Emotional Circumplex Coordinates
    float raw_val = 1.0f * out_result.prob_ocean + 1.2f * out_result.prob_funk + 1.0f * out_result.prob_ethereal - 1.5f * out_result.prob_metal;
    float raw_aro = 1.5f * out_result.prob_metal + 1.5f * out_result.prob_cyber + 1.2f * out_result.prob_funk - 1.2f * out_result.prob_ocean;
    out_result.valence = sigmoid(raw_val);
    out_result.arousal = sigmoid(raw_aro);
}

} // namespace audio_voyager::brain
