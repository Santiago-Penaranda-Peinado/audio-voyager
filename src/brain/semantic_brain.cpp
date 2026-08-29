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

    // 1. Raw DSP Input Extraction
    float raw_diss = audio_state.stream_b.dissonance;
    float raw_centroid_hz = audio_state.stream_b.spectral_centroid_hz;
    float raw_energy = std::max(audio_state.stream_b.energy, audio_state.stream_a.rms);
    float raw_sub_bass = audio_state.stream_a.spectrum_bands[0] * 1.5f + audio_state.stream_a.spectrum_bands[1] * 0.8f;
    float raw_treble = (audio_state.stream_a.spectrum_bands[5] + 
                        audio_state.stream_a.spectrum_bands[6] + 
                        audio_state.stream_a.spectrum_bands[7]) * 0.6f;

    // 2. Dynamic Auto-Gain Control (AGC) Calibration (0.0 to 1.0)
    float agc_norm_diss = agc_dissonance_.update(raw_diss, dt);
    float agc_norm_centroid = agc_centroid_.update(raw_centroid_hz, dt);
    float agc_norm_energy = agc_energy_.update(raw_energy, dt);
    float agc_norm_sub = agc_sub_bass_.update(raw_sub_bass, dt);
    float agc_norm_treble = agc_treble_.update(raw_treble, dt);

    // 3. Smooth through Asymmetric Leaky Integrators
    vector_.norm_dissonance = smooth_dissonance_.update(agc_norm_diss, dt);
    vector_.norm_centroid = smooth_centroid_.update(agc_norm_centroid, dt);
    vector_.norm_energy = smooth_energy_.update(agc_norm_energy, dt);
    vector_.norm_sub_bass = smooth_sub_bass_.update(agc_norm_sub, dt);
    vector_.norm_treble = smooth_treble_.update(agc_norm_treble, dt);

    vector_.onset_strength = audio_state.stream_b.onset_strength;
    vector_.is_onset = audio_state.stream_b.is_onset;

    // 4. BPM & Rhythm Tracking
    update_bpm(raw_sub_bass, vector_.onset_strength, dt);

    // 5. Topology & Navigation Mapping (100% Procedural)
    // Dissonance directly drives space folding / fractalization
    vector_.topology_folding = smooth_folding_.update(vector_.norm_dissonance, dt);

    // Energy directly drives space scale & pulsation
    vector_.cavity_scale = 1.0f + vector_.norm_energy * 0.6f + (vector_.is_onset ? 0.35f : 0.0f);

    // Speed is dynamically driven by energy, BPM, and sub-bass
    float target_speed = 2.2f + (vector_.bpm / 120.0f) * 1.5f + vector_.norm_energy * 3.2f;
    if (vector_.is_onset) target_speed += 2.0f;
    vector_.speed_forward = smooth_speed_.update(target_speed, dt);

    // Glitch & shockwave on drop / high treble
    float target_glitch = (vector_.is_onset ? 1.0f : 0.0f) + (vector_.norm_treble > 0.6f ? (vector_.norm_treble - 0.6f) * 2.0f : 0.0f);
    vector_.glitch_intensity = smooth_glitch_.update(std::clamp(target_glitch, 0.0f, 1.0f), dt);

    // 2D Speed lines intensity
    vector_.speed_lines = std::clamp((vector_.speed_forward - 3.5f) / 4.0f + (vector_.is_onset ? 0.6f : 0.0f), 0.0f, 1.0f);
}

void SemanticBrain::update_bpm(float sub_bass, float onset_val, float dt) {
    time_since_last_beat_ += dt;
    
    onset_history_.push_back(sub_bass * 0.7f + onset_val * 0.3f);
    if (onset_history_.size() > ONSET_HISTORY_SIZE) {
        onset_history_.pop_front();
    }

    if (onset_val > 0.45f && time_since_last_beat_ > 0.22f) {
        float measured_interval = time_since_last_beat_;
        time_since_last_beat_ = 0.0f;

        if (measured_interval >= 0.25f && measured_interval <= 1.0f) {
            beat_interval_estimate_ += 0.25f * (measured_interval - beat_interval_estimate_);
        }
    }

    float estimated_bpm = 60.0f / std::clamp(beat_interval_estimate_, 0.25f, 1.0f);
    vector_.bpm = std::clamp(estimated_bpm, 60.0f, 220.0f);

    float mean_onset = std::accumulate(onset_history_.begin(), onset_history_.end(), 0.0f) / static_cast<float>(onset_history_.size());
    float variance = 0.0f;
    for (float val : onset_history_) {
        float diff = val - mean_onset;
        variance += diff * diff;
    }
    variance /= static_cast<float>(onset_history_.size());

    float pulse_clarity = std::clamp(std::sqrt(variance) * 4.0f, 0.1f, 1.0f);
    vector_.bpm_confidence = std::clamp(pulse_clarity * 1.3f, 0.4f, 0.98f);
}

} // namespace audio_voyager::brain
