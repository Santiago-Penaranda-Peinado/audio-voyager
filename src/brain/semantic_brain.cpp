#include "brain/semantic_brain.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace audio_voyager::brain {

SemanticBrain::SemanticBrain() {
    onset_history_.resize(ONSET_HISTORY_SIZE, 0.0f);
}

void SemanticBrain::update(const core::PhysicsAudioState& audio_state, float dt) {
    dt = std::clamp(dt, 0.001f, 0.05f);

    float raw_rms = std::max(audio_state.stream_a.rms, audio_state.stream_b.rms);
    bool is_silent = (raw_rms < 0.005f);
    vector_.is_silent = is_silent;

    // =========================================================================
    // 1. LA MENTE: Inferencia de Machine Learning Ágil (~0.5s)
    // =========================================================================
    if (!is_silent) {
        ml_classifier_.accumulate_frame(audio_state, dt);
    }
    
    MLClassificationResult ml_result;
    ml_classifier_.maybe_evaluate(ml_result);

    // Filtro Leaky Integrator Ágil (tau ~ 1.2s para transiciones vivas entre partes)
    const float alpha_mind = 1.0f - std::exp(-0.85f * dt);
    smooth_weight_liquid_  += alpha_mind * (ml_result.prob_liquid  - smooth_weight_liquid_);
    smooth_weight_metal_   += alpha_mind * (ml_result.prob_metal   - smooth_weight_metal_);
    smooth_weight_crystal_ = smooth_weight_metal_;
    smooth_weight_cyber_   += alpha_mind * (ml_result.prob_cyber   - smooth_weight_cyber_);
    smooth_weight_dubstep_ += alpha_mind * (ml_result.prob_dubstep - smooth_weight_dubstep_);
    smooth_valence_        += alpha_mind * (ml_result.valence      - smooth_valence_);
    smooth_arousal_        += alpha_mind * (ml_result.arousal      - smooth_arousal_);

    // Normalización baricéntrica 4-dimensional estricta (w_l + w_m + w_c + w_d = 1.0)
    float sum_w = smooth_weight_liquid_ + smooth_weight_metal_ + smooth_weight_cyber_ + smooth_weight_dubstep_;
    if (sum_w > 1e-4f) {
        vector_.weight_liquid  = smooth_weight_liquid_  / sum_w;
        vector_.weight_metal   = smooth_weight_metal_   / sum_w;
        vector_.weight_crystal = vector_.weight_metal;
        vector_.weight_cyber   = smooth_weight_cyber_   / sum_w;
        vector_.weight_dubstep = smooth_weight_dubstep_ / sum_w;
    } else {
        vector_.weight_liquid  = 0.25f;
        vector_.weight_metal   = 0.25f;
        vector_.weight_crystal = 0.25f;
        vector_.weight_cyber   = 0.25f;
        vector_.weight_dubstep = 0.25f;
    }
    vector_.valence = smooth_valence_;
    vector_.arousal = is_silent ? 0.05f : smooth_arousal_;

    // =========================================================================
    // 2. EL MÚSCULO: Reactividad Física Multi-Banda Instantánea (144 Hz DSP)
    // =========================================================================
    const auto& bands = audio_state.stream_a.spectrum_bands;
    float raw_sub_bass = bands[0] * 1.8f + bands[1] * 0.9f;
    float raw_mids = bands[2] * 0.7f + bands[3] * 1.4f + bands[4] * 1.0f;
    float raw_air = bands[6] * 0.9f + bands[7] * 1.8f;
    float raw_energy = std::max(audio_state.stream_b.energy, raw_rms);
    float raw_centroid_hz = audio_state.stream_b.spectral_centroid_hz;

    if (is_silent) {
        // Modo Reposo Zen en Silencio
        smooth_dilation_ += (1.0f - std::exp(-8.0f * dt)) * (0.0f - smooth_dilation_);
        smooth_ripple_   += (1.0f - std::exp(-20.0f * dt)) * (0.0f - smooth_ripple_);
        smooth_mids_     += (1.0f - std::exp(-8.0f * dt)) * (0.0f - smooth_mids_);
        smooth_air_      += (1.0f - std::exp(-8.0f * dt)) * (0.0f - smooth_air_);
        smooth_emission_ += (1.0f - std::exp(-5.0f * dt)) * (0.35f - smooth_emission_);
        smooth_speed_    += (1.0f - std::exp(-4.0f * dt)) * (0.20f - smooth_speed_);
        vector_.is_onset = false;
    } else {
        // 2.1 Dilatación Elástica de Cavidad / Olas (Sub-Bass potente)
        float target_dilation = std::clamp(raw_sub_bass * 0.65f + (audio_state.stream_b.is_onset ? 0.35f : 0.0f), 0.0f, 1.0f);
        smooth_dilation_ += (1.0f - std::exp(-30.0f * dt)) * (target_dilation - smooth_dilation_);

        // 2.2 Ondulación Superficial (Transitorios / Onsets)
        float target_ripple = audio_state.stream_b.is_onset ? (audio_state.stream_b.onset_strength * 0.70f) : 0.0f;
        smooth_ripple_ += (1.0f - std::exp(-45.0f * dt)) * (target_ripple - smooth_ripple_);

        // 2.3 Resonancia de Medios (Voz / Melodía / Guitarras)
        float target_mids = std::clamp(raw_mids * 0.65f, 0.0f, 1.0f);
        smooth_mids_ += (1.0f - std::exp(-22.0f * dt)) * (target_mids - smooth_mids_);

        // 2.4 Destellos de Agudos / Aire (Hi-Hats / Platillos)
        float target_air = std::clamp(raw_air * 0.85f, 0.0f, 1.0f);
        smooth_air_ += (1.0f - std::exp(-35.0f * dt)) * (target_air - smooth_air_);

        // 2.5 Pulso de Emisión Volumétrica & HDR Glow
        float target_emission = 0.7f + raw_energy * 1.5f + (audio_state.stream_b.is_onset ? 0.5f : 0.0f);
        smooth_emission_ += (1.0f - std::exp(-18.0f * dt)) * (target_emission - smooth_emission_);

        // 2.6 Tono HSV Guiado por Centroide
        float target_centroid = std::clamp((raw_centroid_hz - 200.0f) / 4500.0f, 0.0f, 1.0f);
        smooth_centroid_ += (1.0f - std::exp(-10.0f * dt)) * (target_centroid - smooth_centroid_);

        vector_.is_onset = audio_state.stream_b.is_onset;

        // 2.7 Velocidad Cinemática (Amplio rango dinámico 1.0 a 4.8 m/s según género y BPM)
        float bpm_factor = std::clamp((vector_.bpm - 60.0f) / 140.0f, 0.0f, 1.0f);
        float genre_base_speed = 
            vector_.weight_liquid  * 1.25f + 
            vector_.weight_cyber   * 2.35f + 
            vector_.weight_metal   * 3.40f + 
            vector_.weight_dubstep * 3.60f;

        float target_speed = genre_base_speed * (0.80f + 0.35f * bpm_factor) + raw_energy * 0.70f;
        if (vector_.is_onset && raw_energy > 0.35f) {
            target_speed += 0.85f * (vector_.weight_metal * 1.2f + vector_.weight_dubstep * 1.4f + 0.4f);
        }
        target_speed = std::clamp(target_speed, 1.0f, 4.8f);
        smooth_speed_ += (1.0f - std::exp(-6.0f * dt)) * (target_speed - smooth_speed_);
    }

    vector_.elastic_dilation = smooth_dilation_;
    vector_.surface_ripple   = smooth_ripple_;
    vector_.melodic_mids     = smooth_mids_;
    vector_.treble_sparkle   = smooth_air_;
    vector_.emission_pulse   = smooth_emission_;
    vector_.norm_centroid    = smooth_centroid_;
    vector_.speed_forward    = smooth_speed_;

    // =========================================================================
    // 3. TEMPO / BPM DINÁMICO (Autocorrelación normalizada y filtro de resonancia)
    // =========================================================================
    update_bpm(raw_sub_bass, audio_state.stream_b.onset_strength, dt, is_silent);
}

void SemanticBrain::update_bpm(float sub_bass, float onset_val, float dt, bool is_silent) {
    if (is_silent) {
        current_bpm_conf_ *= std::exp(-2.0f * dt);
        vector_.bpm = current_bpm_;
        vector_.bpm_confidence = current_bpm_conf_;
        return;
    }

    // 1. Acumular en buffer circular a frecuencia constante ~45 Hz
    bpm_sample_timer_ += dt;
    if (bpm_sample_timer_ >= BPM_HOP_INTERVAL) {
        bpm_sample_timer_ -= BPM_HOP_INTERVAL;
        float sample_val = onset_val * 1.5f + sub_bass * 0.8f;
        onset_history_.push_back(sample_val);
        if (onset_history_.size() > ONSET_HISTORY_SIZE) {
            onset_history_.pop_front();
        }
    }

    // 2. Evaluar autocorrelación periódicamente cada 0.10s
    bpm_calc_timer_ += dt;
    if (bpm_calc_timer_ < 0.10f || onset_history_.size() < ONSET_HISTORY_SIZE) {
        vector_.bpm = current_bpm_;
        vector_.bpm_confidence = current_bpm_conf_;
        return;
    }
    bpm_calc_timer_ = 0.0f;

    const size_t n = onset_history_.size();
    float mean_val = 0.0f;
    for (float v : onset_history_) mean_val += v;
    mean_val /= static_cast<float>(n);

    float variance = 0.0f;
    for (float v : onset_history_) {
        float d = v - mean_val;
        variance += d * d;
    }
    variance /= static_cast<float>(n);

    if (variance < 1e-4f) {
        current_bpm_conf_ *= 0.95f;
        vector_.bpm = current_bpm_;
        vector_.bpm_confidence = current_bpm_conf_;
        return;
    }

    // Rango de BPM: 65 a 215 BPM
    // Lag min = 60 / (215 * 0.022) = 12 samples
    // Lag max = 60 / (65 * 0.022) = 42 samples
    constexpr int MIN_LAG = 12;
    constexpr int MAX_LAG = 42;

    float best_corr = -1.0f;
    int best_lag = 23; // ~120 BPM default

    std::vector<float> corr_scores(MAX_LAG + 2, 0.0f);

    for (int lag = MIN_LAG; lag <= MAX_LAG; ++lag) {
        float sum = 0.0f;
        int count = 0;
        for (int i = 0; i < static_cast<int>(n) - lag; ++i) {
            sum += (onset_history_[i] - mean_val) * (onset_history_[i + lag] - mean_val);
            count++;
        }
        float r = (count > 0) ? (sum / (static_cast<float>(count) * variance)) : 0.0f;

        // Prior gaussiano centrado en 125 BPM para estabilizar armónicos
        float cand_bpm = 60.0f / (static_cast<float>(lag) * BPM_HOP_INTERVAL);
        float bpm_diff = (cand_bpm - 125.0f) / 50.0f;
        float prior = std::exp(-0.5f * bpm_diff * bpm_diff);

        float score = r * (0.65f + 0.35f * prior);
        corr_scores[lag] = score;

        if (score > best_corr) {
            best_corr = score;
            best_lag = lag;
        }
    }

    if (best_corr > 0.20f && best_lag > MIN_LAG && best_lag < MAX_LAG) {
        // Interpolación parabólica sub-muestra
        float y0 = corr_scores[best_lag - 1];
        float y1 = corr_scores[best_lag];
        float y2 = corr_scores[best_lag + 1];
        float denom = 2.0f * (2.0f * y1 - y0 - y2);
        float delta = (std::abs(denom) > 1e-5f) ? (y2 - y0) / denom : 0.0f;
        delta = std::clamp(delta, -0.5f, 0.5f);

        float fine_lag = static_cast<float>(best_lag) + delta;
        float detected_bpm = 60.0f / (fine_lag * BPM_HOP_INTERVAL);
        detected_bpm = std::clamp(detected_bpm, 60.0f, 220.0f);

        float confidence = std::clamp(best_corr * 1.5f, 0.2f, 0.98f);

        // Actualización inercial suave
        float alpha_bpm = 0.15f * confidence;
        current_bpm_ += alpha_bpm * (detected_bpm - current_bpm_);
        current_bpm_conf_ += 0.20f * (confidence - current_bpm_conf_);
    } else {
        current_bpm_conf_ *= 0.96f;
    }

    vector_.bpm = current_bpm_;
    vector_.bpm_confidence = std::clamp(current_bpm_conf_, 0.1f, 1.0f);
}

} // namespace audio_voyager::brain
