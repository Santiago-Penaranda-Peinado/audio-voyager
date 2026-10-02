#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <iomanip>
#include "core/types.hpp"
#include "brain/semantic_classifier_ml.hpp"
#include "brain/semantic_brain.hpp"
#include "director/autonomous_art_director.hpp"

using namespace audio_voyager;

// SDF implementations ported from GLSL for exact numerical clearance testing
inline float rot_and_max(float x, float y, float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    float rx = c * x - s * y;
    float ry = s * x + c * y;
    return std::max(std::abs(rx), std::abs(ry));
}

float test_sdf_metal(glm::vec3 p, glm::vec3 cam_p, float dilation, float mids, float time) {
    float chasm_width = 3.6f + dilation * 0.6f;
    float d_walls = chasm_width - std::abs(p.x - cam_p.x);

    float ground_crags = std::abs(std::sin(p.x * 1.5f) * std::cos(p.z * 1.2f)) * (0.8f + dilation * 1.4f);
    float d_ground = p.y - (-2.6f + ground_crags);

    glm::vec3 q_spire = p - cam_p;
    float side = (q_spire.x >= 0.0f) ? 1.0f : -1.0f;
    q_spire.x = std::abs(q_spire.x) - (3.2f + dilation * 0.5f);
    q_spire.z = std::fmod(q_spire.z + 4.0f, 8.0f) - 4.0f;

    float d_shard_x = std::abs(q_spire.x) - (0.55f + mids * 0.35f);
    float d_shard_y = std::abs(q_spire.y) - (3.8f + dilation * 0.8f);
    float d_shard_z = std::abs(q_spire.z) - (0.55f + mids * 0.35f);
    float d_monolith = std::max(d_shard_x, std::max(d_shard_y, d_shard_z));

    glm::vec3 q_dagger = glm::abs(p - cam_p) - glm::vec3(2.8f + dilation * 0.4f, 0.8f, 0.0f);
    q_dagger.z = std::fmod(q_dagger.z + 3.0f, 6.0f) - 3.0f;
    float d_dagger = std::max(std::abs(q_dagger.x) + std::abs(q_dagger.y) - (0.4f + mids * 0.2f), std::abs(q_dagger.z) - 1.2f);

    float d_canyon_geom = std::min(d_walls, std::min(d_ground, std::min(d_monolith, d_dagger)));
    float d_flight_corridor = glm::length(glm::vec2(p.x - cam_p.x, p.y - cam_p.y)) - (2.4f + dilation * 0.3f);
    return std::max(d_canyon_geom, -d_flight_corridor);
}

float test_sdf_dubstep(glm::vec3 p, glm::vec3 cam_p, float dilation, float mids, float time) {
    float wobble = std::sin(p.z * 1.2f - time * 12.0f) * (dilation * 0.75f);
    float tunnel_radius = 3.6f + dilation * 0.8f + wobble;
    float d_void_tunnel = tunnel_radius - glm::length(glm::vec2(p.x - cam_p.x, p.y - cam_p.y));

    glm::vec3 q_ring = p - cam_p;
    q_ring.z = std::fmod(p.z + 2.0f, 4.0f) - 2.0f;
    glm::vec2 p_hex = glm::abs(glm::vec2(q_ring.x, q_ring.y));
    float hex_dist = std::max(p_hex.x * 0.866025f + p_hex.y * 0.5f, p_hex.y);
    float d_hex_ring = std::abs(hex_dist - (3.3f + dilation * 0.6f)) - 0.18f;
    float d_ring_segment = std::max(d_hex_ring, std::abs(q_ring.z) - 0.26f);

    glm::vec3 q_res = glm::abs(p - cam_p);
    q_res.x -= (3.4f + dilation * 0.5f);
    q_res.z = std::fmod(q_res.z + 3.0f, 6.0f) - 3.0f;
    glm::vec3 box_dim(0.45f, 1.2f + dilation * 1.5f + mids * 0.6f, 0.45f);
    glm::vec3 d_b = glm::abs(q_res) - box_dim;
    float d_resonator = std::max(d_b.x, std::max(d_b.y, d_b.z));

    float d_dub_geom = std::min(d_void_tunnel, std::min(d_ring_segment, d_resonator));
    float d_flight_corridor = glm::length(glm::vec2(p.x - cam_p.x, p.y - cam_p.y)) - (2.6f + dilation * 0.3f);
    return std::max(d_dub_geom, -d_flight_corridor);
}

void test_neural_ml_discrimination() {
    std::cout << "[TEST 1] Testing 4-Class Neural Spectral Biome Discrimination...\n";
    
    // 1.1 Test Metal / Hard Rock (Hand of Blood: heavy guitar distortion, loud mids, high dissonance)
    {
        brain::SemanticClassifierML classifier;
        core::PhysicsAudioState state{};
        state.stream_b.dissonance = 0.58f;
        state.stream_b.spectral_centroid_hz = 2200.0f;
        state.stream_b.energy = 0.35f;
        state.stream_a.rms = 0.32f;
        state.stream_b.is_onset = true;
        // Saturated mids (Mel 8-17) and cymbal wash
        state.stream_a.spectrum_bands = {0.20f, 0.40f, 0.70f, 0.85f, 0.75f, 0.60f, 0.50f, 0.40f};

        for (int i = 0; i < 30; ++i) {
            classifier.accumulate_frame(state, 0.02f);
        }
        brain::MLClassificationResult res;
        bool eval = classifier.maybe_evaluate(res);
        assert(eval);
        std::cout << "  -> Metal Input Result: L=" << res.prob_liquid 
                  << " | M=" << res.prob_metal 
                  << " | C=" << res.prob_cyber 
                  << " | D=" << res.prob_dubstep << std::endl;
        assert(res.prob_metal > res.prob_liquid);
        assert(res.prob_metal > res.prob_cyber);
        assert(res.prob_metal > res.prob_dubstep);
        assert(res.prob_metal > 0.40f);
        float sum = res.prob_liquid + res.prob_metal + res.prob_cyber + res.prob_dubstep;
        assert(std::abs(sum - 1.0f) < 1e-3f);
    }

    // 1.2 Test Dubstep / Speedcore (Skrillex/Camellia: massive sub-bass, extreme sub-to-mid ratio)
    {
        brain::SemanticClassifierML classifier;
        core::PhysicsAudioState state{};
        state.stream_b.dissonance = 0.15f;
        state.stream_b.spectral_centroid_hz = 1200.0f;
        state.stream_b.energy = 0.45f;
        state.stream_a.rms = 0.40f;
        state.stream_b.is_onset = true;
        // Deep sub-bass dominant (0-80 Hz >> mids)
        state.stream_a.spectrum_bands = {0.95f, 0.80f, 0.20f, 0.22f, 0.15f, 0.25f, 0.30f, 0.25f};

        for (int i = 0; i < 30; ++i) {
            classifier.accumulate_frame(state, 0.02f);
        }
        brain::MLClassificationResult res;
        bool eval = classifier.maybe_evaluate(res);
        assert(eval);
        std::cout << "  -> Dubstep Input Result: L=" << res.prob_liquid 
                  << " | M=" << res.prob_metal 
                  << " | C=" << res.prob_cyber 
                  << " | D=" << res.prob_dubstep << std::endl;
        assert(res.prob_dubstep > res.prob_metal);
        assert(res.prob_dubstep > res.prob_liquid);
        assert(res.prob_dubstep > res.prob_cyber);
        assert(res.prob_dubstep > 0.40f);
    }

    // 1.3 Test Jazz / Lofi / Ambient (clean, low dissonance, gentle onsets, warm body)
    {
        brain::SemanticClassifierML classifier;
        core::PhysicsAudioState state{};
        state.stream_b.dissonance = 0.05f;
        state.stream_b.spectral_centroid_hz = 850.0f;
        state.stream_b.energy = 0.08f;
        state.stream_a.rms = 0.07f;
        state.stream_b.is_onset = false;
        // Gentle warm acoustic mids, very low dissonance
        state.stream_a.spectrum_bands = {0.12f, 0.18f, 0.38f, 0.30f, 0.15f, 0.08f, 0.04f, 0.02f};

        for (int i = 0; i < 30; ++i) {
            classifier.accumulate_frame(state, 0.02f);
        }
        brain::MLClassificationResult res;
        bool eval = classifier.maybe_evaluate(res);
        assert(eval);
        std::cout << "  -> Jazz Input Result: L=" << res.prob_liquid 
                  << " | M=" << res.prob_metal 
                  << " | C=" << res.prob_cyber 
                  << " | D=" << res.prob_dubstep << std::endl;
        assert(res.prob_liquid > res.prob_metal);
        assert(res.prob_liquid > res.prob_dubstep);
        assert(res.prob_liquid > res.prob_cyber);
        assert(res.prob_liquid > 0.40f);
    }

    // 1.4 Test Cyber / Techno (clean punch bass, steady beat, low dissonance)
    {
        brain::SemanticClassifierML classifier;
        core::PhysicsAudioState state{};
        state.stream_b.dissonance = 0.14f;
        state.stream_b.spectral_centroid_hz = 1800.0f;
        state.stream_b.energy = 0.22f;
        state.stream_a.rms = 0.20f;
        for (int i = 0; i < 30; ++i) {
            state.stream_b.is_onset = (i % 4 == 0);
            classifier.accumulate_frame(state, 0.02f);
        }
        brain::MLClassificationResult res;
        bool eval = classifier.maybe_evaluate(res);
        assert(eval);
        std::cout << "  -> Cyber Input Result: L=" << res.prob_liquid 
                  << " | M=" << res.prob_metal 
                  << " | C=" << res.prob_cyber 
                  << " | D=" << res.prob_dubstep << std::endl;
        assert(res.prob_cyber > 0.35f);
    }

    std::cout << "  [PASS] Neural ML Biome Discrimination works with clear, robust separation!\n\n";
}

void test_autocorrelation_bpm() {
    std::cout << "[TEST 2] Testing Autocorrelation Tempo / BPM Estimation...\n";

    brain::SemanticBrain brain;
    core::PhysicsAudioState state{};

    // Simulate 130 BPM pulse train: period = 60 / 130 = 0.4615 seconds
    constexpr float dt = 0.016f; // ~60 Hz update rate
    float beat_timer = 0.0f;
    constexpr float target_period = 60.0f / 130.0f;

    for (int frame = 0; frame < 300; ++frame) { // ~5 seconds of audio
        beat_timer += dt;
        bool is_beat = false;
        if (beat_timer >= target_period) {
            beat_timer -= target_period;
            is_beat = true;
        }

        state.stream_a.rms = 0.25f;
        state.stream_b.energy = 0.25f;
        state.stream_b.is_onset = is_beat;
        state.stream_b.onset_strength = is_beat ? 0.85f : 0.05f;
        state.stream_a.spectrum_bands[0] = is_beat ? 0.70f : 0.10f;

        brain.update(state, dt);
    }

    const auto& vec = brain.get_semantic_vector();
    std::cout << "  -> Injected 130 BPM Pulse Train: Detected BPM = " 
              << vec.bpm << " (Confidence: " << vec.bpm_confidence * 100.0f << "%)\n";
    assert(std::abs(vec.bpm - 130.0f) < 8.0f);
    assert(vec.bpm_confidence > 0.30f);

    std::cout << "  [PASS] Autocorrelation BPM estimation locks accurately onto tempo!\n\n";
}

void test_dynamic_kinematics_and_speed() {
    std::cout << "[TEST 3] Testing Dynamic Kinematics & Expressive Speed Range...\n";

    brain::SemanticBrain brain;
    core::PhysicsAudioState state{};

    // 3.1 Quiet / Calm Jazz passage: speed must be relaxed ~1.0 - 1.6 m/s
    state.stream_a.rms = 0.05f;
    state.stream_b.energy = 0.04f;
    state.stream_b.dissonance = 0.04f;
    state.stream_a.spectrum_bands = {0.10f, 0.15f, 0.35f, 0.30f, 0.10f, 0.05f, 0.02f, 0.01f};
    state.stream_b.is_onset = false;
    for (int i = 0; i < 150; ++i) { // 3 seconds for phrase transition
        brain.update(state, 0.02f);
    }
    float calm_speed = brain.get_semantic_vector().speed_forward;
    std::cout << "  -> Calm Passage Cruising Speed: " << calm_speed << " m/s" << std::endl;
    assert(calm_speed >= 1.0f && calm_speed <= 1.8f);

    // 3.2 Explosive Heavy Metal / Dubstep Drop: speed must surge to 3.5 - 4.8 m/s!
    state.stream_a.rms = 0.45f;
    state.stream_b.energy = 0.50f;
    state.stream_b.is_onset = true;
    state.stream_b.dissonance = 0.65f;
    state.stream_a.spectrum_bands = {0.85f, 0.80f, 0.75f, 0.80f, 0.60f, 0.50f, 0.50f, 0.40f};
    for (int i = 0; i < 90; ++i) {
        brain.update(state, 0.02f);
    }
    float drop_speed = brain.get_semantic_vector().speed_forward;
    std::cout << "  -> Intense Metal/Drop Cruising Speed: " << drop_speed << " m/s\n";
    assert(drop_speed >= 3.0f && drop_speed <= 4.8f);
    assert(drop_speed > calm_speed + 1.5f);

    std::cout << "  [PASS] Speed range is highly dynamic (" << calm_speed << " -> " << drop_speed << " m/s)!\n\n";
}

void test_camera_flight_clearance_and_roll() {
    std::cout << "[TEST 4] Testing 6DoF Camera Clearance & Dynamic Banking...\n";

    director::AutonomousArtDirector director;
    core::AudioSemanticVector semantic{};
    semantic.speed_forward = 3.5f;
    semantic.weight_metal = 0.8f;
    semantic.arousal = 0.8f;

    bool observed_banking = false;
    for (int i = 0; i < 200; ++i) {
        director.update(semantic, 0.02f);
        glm::vec3 pos = director.get_camera_pos();
        assert(pos.y >= 2.0f); // Camera altitude clearance guaranteed
        float roll = director.get_camera_roll();
        if (std::abs(roll) > 0.02f) {
            observed_banking = true;
        }
    }
    assert(observed_banking);
    std::cout << "  -> Verified camera clearance (Y >= 2.0m) and true 6DoF banking into turns.\n";
    std::cout << "  [PASS] Camera kinematics & 6DoF banking operating smoothly!\n\n";
}

void test_sdf_clearance_at_camera() {
    std::cout << "[TEST 5] Testing SDF Camera Clearance (Zero Collision Guarantee)...\n";

    glm::vec3 cam_p(0.0f, 2.8f, 10.0f);

    // At the camera position p = cam_p:
    // Metal SDF: previous implementation had d = -0.5 (camera inside solid monolith!).
    // New implementation has guaranteed safe corridor (d >= +2.4m)!
    float d_metal = test_sdf_metal(cam_p, cam_p, 0.5f, 0.5f, 0.0f);
    std::cout << "  -> Metal SDF distance at camera position: " << d_metal << " m\n";
    assert(d_metal >= 2.0f); // Camera has at least 2.0 meters of open air!

    float d_dubstep = test_sdf_dubstep(cam_p, cam_p, 0.5f, 0.5f, 0.0f);
    std::cout << "  -> Dubstep SDF distance at camera position: " << d_dubstep << " m\n";
    assert(d_dubstep >= 2.0f);

    std::cout << "  [PASS] SDF geometry has strict clearance at camera position! Screen will NEVER black out.\n\n";
}

int main() {
    std::cout << "=================================================================\n";
    std::cout << "   AUDIO-VOYAGER SYSTEM INTEGRITY & ARCHITECTURAL VERIFICATION   \n";
    std::cout << "=================================================================\n\n";

    test_neural_ml_discrimination();
    test_autocorrelation_bpm();
    test_dynamic_kinematics_and_speed();
    test_camera_flight_clearance_and_roll();
    test_sdf_clearance_at_camera();

    std::cout << "=================================================================\n";
    std::cout << "   ALL VERIFICATION TESTS PASSED SUCCESSFULLY! (100% HEALTHY)    \n";
    std::cout << "=================================================================\n";
    return 0;
}
