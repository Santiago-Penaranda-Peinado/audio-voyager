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
    latest_result_.prob_liquid = 0.25f;
    latest_result_.prob_metal = 0.25f;
    latest_result_.prob_crystal = 0.25f;
    latest_result_.prob_cyber = 0.25f;
    latest_result_.prob_dubstep = 0.25f;
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
    // Evaluate every 0.5 seconds for agile, responsive musical transitions across phrases
    constexpr float EVAL_INTERVAL = 0.5f;
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

    // Frame-rate independent rhythmic cadence (events per second / Hz)
    float dt_eval = std::max(time_since_last_eval_, 0.05f);
    float onset_cadence_hz = accumulated_onsets_ / dt_eval;

    // Auto-calibrated normalized features
    out_features[24] = std::clamp(mean_diss * 3.0f, 0.0f, 3.0f);                      // Dissonance & guitar distortion
    out_features[25] = std::clamp((mean_centroid - 200.0f) / 2500.0f, 0.0f, 3.0f);  // Centroid brightness
    out_features[26] = std::clamp(mean_energy / 0.12f, 0.0f, 3.0f);                  // Acoustic energy density
    out_features[27] = std::clamp(onset_cadence_hz / 5.0f, 0.0f, 3.0f);               // Rhythm & onset cadence (1.0 = 5 Hz)
}

void SemanticClassifierML::forward_pass(const std::array<float, 28>& x, MLClassificationResult& out_result) {
    // Spectral summary bands
    float sub_energy   = (x[0] + x[1] + x[2] + x[3]) * 0.25f;
    float punch_energy = (x[4] + x[5] + x[6] + x[7]) * 0.25f;
    float mids_energy  = (x[8] + x[10] + x[12] + x[14] + x[16]) * 0.20f;
    float treble_energy= (x[18] + x[20] + x[22] + x[23]) * 0.25f;
    float diss         = x[24];
    float brightness   = x[25];
    float energy       = x[26];
    float onsets       = x[27];

    // -------------------------------------------------------------------------
    // Layer 1: 28 Inputs -> 16 Specialized Latent Neurons
    // -------------------------------------------------------------------------
    std::array<float, 16> h1{};
    for (size_t i = 0; i < 16; ++i) {
        float sum = 0.0f;
        for (size_t j = 0; j < 24; ++j) {
            float w = 0.0f;
            if (i < 4) {
                // Liquid: Warm acoustic mid-bass, low dissonance
                w = (j >= 2 && j <= 9) ? 0.45f : -0.20f;
            } else if (i < 8) {
                // Metal: Screaming electric guitar mid saturation & raw acoustic bite
                w = (j >= 6 && j <= 16) ? 0.65f : -0.15f;
            } else if (i < 12) {
                // Cyber: Tight punchy kick & clean high synth leads
                w = (j >= 3 && j <= 7) ? 0.50f : ((j >= 16) ? 0.35f : -0.15f);
            } else {
                // Dubstep / Speedcore: Deep sub-bass monster & extreme transient bite
                w = (j <= 3) ? 0.80f : ((j >= 17) ? 0.40f : -0.25f);
            }
            sum += x[j] * w;
        }

        // Feature anchors with clear zero-crossing discrimination
        if (i < 4) {
            // Liquid: High reward for low dissonance, gentle onsets, acoustic warmth
            sum += 2.8f * (0.50f - diss) + 2.0f * (0.60f - onsets) + 1.2f * mids_energy;
        } else if (i < 8) {
            // Metal: Heavy harmonic distortion (high dissonance), loud guitar mids, aggressive pace
            sum += 3.8f * (diss - 0.35f) + 2.6f * (mids_energy - 0.25f) + 1.6f * (onsets - 0.35f);
        } else if (i < 12) {
            // Cyber: Clean punch bass, steady cadence, low distortion
            sum += 2.8f * (punch_energy - 0.25f) + 1.8f * (onsets - 0.30f) + 2.0f * (0.40f - diss);
        } else {
            // Dubstep: Massive sub-bass dominance, explosive drops, speedcore tempo
            sum += 3.6f * (sub_energy - 0.28f) + 2.2f * (onsets - 0.40f) + 2.0f * (sub_energy - mids_energy);
        }

        h1[i] = gelu(sum);
    }

    // -------------------------------------------------------------------------
    // Layer 2: 16 -> 8 Latent Semantic Neurons (Structured Pathway Routing)
    // -------------------------------------------------------------------------
    std::array<float, 8> h2{};
    for (size_t i = 0; i < 8; ++i) {
        size_t pathway = i / 2; // 0: Liquid, 1: Metal, 2: Cyber, 3: Dubstep
        float sum = 0.0f;
        for (size_t j = 0; j < 16; ++j) {
            size_t j_pathway = j / 4;
            float w = (j_pathway == pathway) ? 0.60f : -0.18f;
            sum += h1[j] * w;
        }
        h2[i] = gelu(sum);
    }

    // -------------------------------------------------------------------------
    // Output Layer: 4-Way Sharp Musical Discrimination
    // -------------------------------------------------------------------------
    float logit_liquid  = 1.5f * (h2[0] + h2[1]) - 0.6f * (h2[2] + h2[6]);
    float logit_metal   = 1.5f * (h2[2] + h2[3]) - 0.6f * (h2[0] + h2[4]);
    float logit_cyber   = 1.5f * (h2[4] + h2[5]) - 0.6f * (h2[0] + h2[2]);
    float logit_dubstep = 1.5f * (h2[6] + h2[7]) - 0.6f * (h2[0] + h2[4]);

    // Explicit Physical Genre Signatures (balanced and calibrated, NO single winner):

    // 1. Heavy Metal / Hard Rock Signature (e.g. Hand of Blood):
    //    Continuous harmonic distortion / high Sethares dissonance + loud guitar mids + cymbal wash
    //    Metal songs also have heavy kick drums, so we check for high dissonance & guitar mids
    if (diss > 0.35f && mids_energy > 0.22f) {
        float metal_strength = (diss - 0.30f) * 2.8f + (mids_energy - 0.20f) * 2.0f + treble_energy * 0.8f;
        logit_metal += std::clamp(metal_strength, 0.0f, 3.2f);
    }

    // 2. Dubstep / Speedcore Signature (e.g. Skrillex, Camellia):
    //    Extreme sub-bass dominance over mids OR hyper-speed electronic onsets with bright synths
    float sub_dominance = sub_energy / (mids_energy + 0.05f);
    if (sub_dominance > 1.2f && sub_energy > 0.25f && diss < 0.50f) {
        logit_dubstep += std::clamp((sub_dominance - 1.0f) * 2.0f + sub_energy * 2.0f, 0.0f, 3.2f);
    } else if (onsets > 1.0f && brightness > 0.7f && diss < 0.50f) {
        // Camellia / Speedcore: Ultra-fast BPM synthetic drops
        logit_dubstep += 2.5f;
    }

    // 3. Cyber / Techno / Synthwave Signature:
    //    Steady electronic punch bass, clean consonance (low dissonance), balanced spectrum
    if (punch_energy > 0.22f && diss < 0.35f && onsets > 0.30f && onsets < 1.4f) {
        logit_cyber += 2.2f;
    }

    // 4. Liquid / Jazz / Lofi / Soft Acoustic Signature:
    //    Clean acoustic timbre (very low dissonance), gentle pace, warm harmonic body
    if (diss < 0.25f && (onsets < 0.60f || energy < 0.35f)) {
        float jazz_peace = (0.35f - diss) * 3.0f + std::max(0.0f, 0.65f - onsets) * 2.0f;
        logit_liquid += std::clamp(jazz_peace, 0.0f, 3.2f);
    }

    // Calibrated temperature-scaled 4-way Softmax (T = 1.35 for smooth, expressive continuous blending)
    constexpr float T = 1.35f;
    float max_logit = std::max({logit_liquid, logit_metal, logit_cyber, logit_dubstep});
    float exp_l = std::exp((logit_liquid  - max_logit) / T);
    float exp_m = std::exp((logit_metal   - max_logit) / T);
    float exp_c = std::exp((logit_cyber   - max_logit) / T);
    float exp_d = std::exp((logit_dubstep - max_logit) / T);
    float sum_exp = exp_l + exp_m + exp_c + exp_d;

    out_result.prob_liquid  = exp_l / sum_exp;
    out_result.prob_metal   = exp_m / sum_exp;
    out_result.prob_crystal = out_result.prob_metal; // synced for legacy
    out_result.prob_cyber   = exp_c / sum_exp;
    out_result.prob_dubstep = exp_d / sum_exp;

    // Valence & Arousal (Russell's Circumplex Model)
    float raw_val = 1.3f * h2[0] - 1.6f * h2[2] + 0.8f * h2[4] - 0.4f * h2[6];
    float raw_aro = 1.6f * h2[2] + 1.8f * h2[6] + 0.8f * h2[4] - 1.2f * h2[0];
    out_result.valence = sigmoid(raw_val);
    out_result.arousal = sigmoid(raw_aro);
}

} // namespace audio_voyager::brain
