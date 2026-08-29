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
    // 1. LA MENTE: Inferencia de Machine Learning Ágil (~0.6s)
    // =========================================================================
    if (!is_silent) {
        ml_classifier_.accumulate_frame(audio_state, dt);
    }
    
    MLClassificationResult ml_result;
    ml_classifier_.maybe_evaluate(ml_result);

    // Filtro Leaky Integrator Ágil (tau ~ 1.2s para transiciones vivas entre partes)
    const float alpha_mind = 1.0f - std::exp(-0.85f * dt);
    smooth_weight_liquid_  += alpha_mind * (ml_result.prob_liquid - smooth_weight_liquid_);
    smooth_weight_crystal_ += alpha_mind * (ml_result.prob_crystal - smooth_weight_crystal_);
    smooth_weight_cyber_   += alpha_mind * (ml_result.prob_cyber - smooth_weight_cyber_);
    smooth_valence_        += alpha_mind * (ml_result.valence - smooth_valence_);
    smooth_arousal_        += alpha_mind * (ml_result.arousal - smooth_arousal_);

    // Normalización baricéntrica estricta (w_l + w_c + w_y = 1.0)
    float sum_w = smooth_weight_liquid_ + smooth_weight_crystal_ + smooth_weight_cyber_;
    if (sum_w > 1e-4f) {
        vector_.weight_liquid  = smooth_weight_liquid_ / sum_w;
        vector_.weight_crystal = smooth_weight_crystal_ / sum_w;
        vector_.weight_cyber   = smooth_weight_cyber_ / sum_w;
    } else {
        vector_.weight_liquid  = 0.34f;
        vector_.weight_crystal = 0.33f;
        vector_.weight_cyber   = 0.33f;
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
        smooth_speed_    += (1.0f - std::exp(-4.0f * dt)) * (0.18f - smooth_speed_);
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
        float target_emission = 0.7f + raw_energy * 1.6f + (audio_state.stream_b.is_onset ? 0.6f : 0.0f);
        smooth_emission_ += (1.0f - std::exp(-18.0f * dt)) * (target_emission - smooth_emission_);

        // 2.6 Tono HSV Guiado por Centroide
        float target_centroid = std::clamp((raw_centroid_hz - 200.0f) / 4500.0f, 0.0f, 1.0f);
        smooth_centroid_ += (1.0f - std::exp(-10.0f * dt)) * (target_centroid - smooth_centroid_);

        vector_.is_onset = audio_state.stream_b.is_onset;

        // 2.7 Velocidad Cinemática
        float target_speed = 1.1f + vector_.arousal * 0.8f + raw_energy * 0.6f;
        if (vector_.is_onset && raw_energy > 0.4f) {
            target_speed += 0.65f;
        }
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
    // 3. TEMPO / BPM DINÁMICO
    // =========================================================================
    update_bpm(raw_sub_bass, audio_state.stream_b.onset_strength, dt, is_silent);
}

void SemanticBrain::update_bpm(float sub_bass, float onset_val, float dt, bool is_silent) {
    if (is_silent) {
        current_bpm_ *= std::exp(-1.8f * dt);
        current_bpm_conf_ *= std::exp(-3.0f * dt);
        vector_.bpm = current_bpm_;
        vector_.bpm_confidence = current_bpm_conf_;
        return;
    }

    time_since_last_beat_ += dt;
    onset_history_.push_back(sub_bass * 0.7f + onset_val * 0.3f);
    if (onset_history_.size() > ONSET_HISTORY_SIZE) {
        onset_history_.pop_front();
    }

    if (onset_val > 0.45f && time_since_last_beat_ > 0.20f) {
        float measured_interval = time_since_last_beat_;
        time_since_last_beat_ = 0.0f;

        if (measured_interval >= 0.25f && measured_interval <= 1.0f) {
            beat_interval_estimate_ += 0.25f * (measured_interval - beat_interval_estimate_);
        }
    }

    float estimated_bpm = 60.0f / std::clamp(beat_interval_estimate_, 0.25f, 1.0f);
    current_bpm_ = std::clamp(estimated_bpm, 60.0f, 220.0f);

    float mean_onset = std::accumulate(onset_history_.begin(), onset_history_.end(), 0.0f) / static_cast<float>(onset_history_.size());
    float variance = 0.0f;
    for (float val : onset_history_) {
        float diff = val - mean_onset;
        variance += diff * diff;
    }
    variance /= static_cast<float>(onset_history_.size());

    float pulse_clarity = std::clamp(std::sqrt(variance) * 4.0f, 0.1f, 1.0f);
    current_bpm_conf_ = std::clamp(pulse_clarity * 1.3f, 0.3f, 0.98f);

    vector_.bpm = current_bpm_;
    vector_.bpm_confidence = current_bpm_conf_;
}

} // namespace audio_voyager::brain
