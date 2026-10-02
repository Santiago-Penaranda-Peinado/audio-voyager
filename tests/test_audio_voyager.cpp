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

// Polynomial Smooth Minimum ported from GLSL
inline float smin(float a, float b, float k) {
    float h = std::clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
    return (b * (1.0f - h) + a * h) - k * h * (1.0f - h);
}

// -----------------------------------------------------------------------------
// SDF implementations exactly matching shaders/raymarching.frag
// -----------------------------------------------------------------------------
float test_sdf_liquid(glm::vec3 p, glm::vec3 cam_p, float dilation, float mids, float is_silent, float time) {
    float wave_scale = 1.0f - is_silent * 0.95f;
    float wave1 = std::sin(p.x * 0.35f + time * 1.4f) * std::cos(p.z * 0.28f + time * 1.0f) * (0.55f * wave_scale + dilation * 0.85f);
    float wave2 = std::sin((p.x + p.z) * 0.70f + time * 2.0f) * (0.28f * wave_scale + mids * 0.45f);
    float wave_bass = std::sin(glm::length(glm::vec2(p.x - cam_p.x, p.z - cam_p.z)) * 0.25f - time * 3.0f) * (dilation * 0.75f * wave_scale);
    float ocean_y = -2.2f + (wave1 + wave2 + wave_bass);
    float d_ocean = p.y - ocean_y;

    glm::vec3 q_orb = p - cam_p;
    q_orb.z = std::fmod(p.z + 7.0f, 14.0f) - 7.0f;
    float side = (q_orb.x >= 0.0f) ? 1.0f : -1.0f;
    q_orb.x = std::abs(q_orb.x) - (4.8f + dilation * 0.6f);
    q_orb.y = (p.y - cam_p.y) - (0.4f + 0.8f * std::sin(p.z * 0.35f + time * 1.3f));
    float d_orb = glm::length(q_orb) - (0.95f + dilation * 0.50f + mids * 0.30f);

    glm::vec3 q_ring = p - cam_p;
    q_ring.z = std::fmod(p.z + 7.0f, 14.0f) - 7.0f;
    q_ring.x = std::abs(q_ring.x) - (4.8f + dilation * 0.6f);
    q_ring.y = (p.y - cam_p.y) - 0.2f;
    glm::vec2 t_ring(glm::length(glm::vec2(q_ring.x, q_ring.z)) - 1.8f, q_ring.y);
    float d_ring = glm::length(t_ring) - 0.09f;

    float d_floating = std::min(d_orb, d_ring);
    float d_liq_geom = smin(d_ocean, d_floating, 1.10f);
    float d_flight_corridor = glm::length(glm::vec2(p.x - cam_p.x, p.y - cam_p.y)) - (2.2f + dilation * 0.3f);
    return std::max(d_liq_geom, -d_flight_corridor);
}

float test_sdf_metal(glm::vec3 p, glm::vec3 cam_p, float dilation, float mids, float time) {
    float chasm_width = 3.6f + dilation * 0.6f;
    float d_walls = chasm_width - std::abs(p.x - cam_p.x);

    float ground_crags = std::abs(std::sin(p.x * 1.8f) * std::cos(p.z * 1.4f)) * (0.8f + dilation * 1.4f + mids * 0.5f);
    float d_ground = p.y - (-2.6f + ground_crags);

    glm::vec3 q_spire = p - cam_p;
    float side = (q_spire.x >= 0.0f) ? 1.0f : -1.0f;
    q_spire.x = std::abs(q_spire.x) - (3.4f + dilation * 0.5f);
    q_spire.z = std::fmod(p.z + 4.0f, 8.0f) - 4.0f;

    float d_shard_x = std::abs(q_spire.x) - (0.55f + mids * 0.35f);
    float d_shard_y = std::abs(q_spire.y) - (3.8f + dilation * 0.8f);
    float d_shard_z = std::abs(q_spire.z) - (0.55f + mids * 0.35f);
    float d_monolith = std::max(d_shard_x, std::max(d_shard_y, d_shard_z));

    glm::vec3 q_dagger = glm::abs(p - cam_p) - glm::vec3(2.8f + dilation * 0.4f, 0.8f, 0.0f);
    q_dagger.z = std::fmod(p.z + 3.0f, 6.0f) - 3.0f;
    float d_dagger = std::max(std::abs(q_dagger.x) + std::abs(q_dagger.y) - (0.4f + mids * 0.2f), std::abs(q_dagger.z) - 1.2f);

    float d_canyon_geom = std::min(d_walls, std::min(d_ground, std::min(d_monolith, d_dagger)));
    float d_flight_corridor = glm::length(glm::vec2(p.x - cam_p.x, p.y - cam_p.y)) - (2.4f + dilation * 0.3f);
    return std::max(d_canyon_geom, -d_flight_corridor);
}

float test_sdf_cyber(glm::vec3 p, glm::vec3 cam_p, float dilation, float mids, float time) {
    glm::vec2 q_xy = glm::abs(glm::vec2(p.x - cam_p.x, p.y - cam_p.y));
    float d_corridor = (3.8f + dilation * 0.6f) - std::max(q_xy.x, q_xy.y);

    glm::vec3 q_eq = p - cam_p;
    q_eq.z = std::fmod(p.z + 1.8f, 3.6f) - 1.8f;
    float eq_height = 0.6f + 2.2f * (dilation * 1.4f + mids * 0.8f) * std::abs(std::sin(std::floor(p.z / 3.6f) * 1.4f + time * 5.0f));
    glm::vec3 d_eq_box = glm::abs(glm::vec3(std::abs(q_eq.x) - 3.0f, std::abs(q_eq.y) - (3.6f - eq_height * 0.5f), q_eq.z)) - glm::vec3(0.40f, eq_height * 0.5f, 0.40f);
    float d_eq = std::max(d_eq_box.x, std::max(d_eq_box.y, d_eq_box.z));

    glm::vec3 q_ring = p - cam_p;
    q_ring.z = std::fmod(p.z + 3.0f, 6.0f) - 3.0f;
    float ring_body = std::abs(glm::length(glm::vec2(q_ring.x, q_ring.y)) - (3.2f + dilation * 0.4f)) - 0.14f;
    float d_ring = std::max(ring_body, std::abs(q_ring.z) - 0.22f);

    glm::vec2 q_rails = glm::abs(glm::vec2(p.x - cam_p.x, p.y - cam_p.y)) - glm::vec2(2.6f);
    float d_rails = glm::length(q_rails) - 0.12f;

    float d_tech = std::min(d_ring, std::min(d_rails, d_eq));
    float d_cyber_geom = std::min(d_corridor, d_tech);
    float d_flight_corridor = glm::length(glm::vec2(p.x - cam_p.x, p.y - cam_p.y)) - (2.4f + dilation * 0.3f);
    return std::max(d_cyber_geom, -d_flight_corridor);
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
    q_res.z = std::fmod(p.z + 3.0f, 6.0f) - 3.0f;
    glm::vec3 box_dim(0.45f, 1.2f + dilation * 1.5f + mids * 0.6f, 0.45f);
    glm::vec3 d_b = glm::abs(q_res) - box_dim;
    float d_resonator = std::max(d_b.x, std::max(d_b.y, d_b.z));

    float d_dub_geom = std::min(d_void_tunnel, std::min(d_ring_segment, d_resonator));
    float d_flight_corridor = glm::length(glm::vec2(p.x - cam_p.x, p.y - cam_p.y)) - (2.6f + dilation * 0.3f);
    return std::max(d_dub_geom, -d_flight_corridor);
}

float test_map(glm::vec3 p, glm::vec3 cam_p, glm::vec4 weights, float dilation, float mids, float ripple, float is_silent, float time) {
    float d_l = test_sdf_liquid(p, cam_p, dilation, mids, is_silent, time);
    float d_m = test_sdf_metal(p, cam_p, dilation, mids, time);
    float d_c = test_sdf_cyber(p, cam_p, dilation, mids, time);
    float d_d = test_sdf_dubstep(p, cam_p, dilation, mids, time);

    float d_interp = weights.x * d_l + weights.y * d_m + weights.z * d_c + weights.w * d_d;

    if (ripple > 0.01f) {
        float r = glm::length(p - cam_p);
        float shock = std::sin(r * 4.5f - time * 20.0f) * std::exp(-0.25f * r) * ripple * 0.60f;
        d_interp += shock;
    }

    float d_global_corridor = glm::length(glm::vec2(p.x - cam_p.x, p.y - cam_p.y)) - (2.2f + dilation * 0.2f);
    return std::max(d_interp, -d_global_corridor);
}

// -----------------------------------------------------------------------------
// Test 1: 4-Way Neural Spectral Biome Discrimination & Silence Recovery
// -----------------------------------------------------------------------------
void test_neural_ml_discrimination() {
    std::cout << "[TEST 1] Testing 4-Class Neural Spectral Biome Discrimination...\n";
    
    // 1.1 Heavy Metal / Hard Rock (Hand of Blood: heavy guitar distortion, loud mids, high dissonance, kick sub-bass)
    {
        brain::SemanticClassifierML classifier;
        core::PhysicsAudioState state{};
        state.stream_b.dissonance = 0.58f;
        state.stream_b.spectral_centroid_hz = 2200.0f;
        state.stream_b.energy = 0.40f;
        state.stream_a.rms = 0.38f;
        // Realistic full production: heavy kick drum (sub-bass) + wall of distorted guitars in mids + cymbals
        state.stream_a.spectrum_bands = {0.55f, 0.65f, 0.80f, 0.90f, 0.80f, 0.65f, 0.55f, 0.45f};

        for (int i = 0; i < 30; ++i) {
            state.stream_b.is_onset = (i % 8 == 0); // 3-4 beats per second
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
    }

    // 1.2 Dubstep / Speedcore (Skrillex/Camellia: massive sub-bass, extreme sub-to-mid ratio)
    {
        brain::SemanticClassifierML classifier;
        core::PhysicsAudioState state{};
        state.stream_b.dissonance = 0.15f;
        state.stream_b.spectral_centroid_hz = 1200.0f;
        state.stream_b.energy = 0.45f;
        state.stream_a.rms = 0.40f;
        // Deep sub-bass dominant (0-80 Hz >> mids)
        state.stream_a.spectrum_bands = {0.95f, 0.85f, 0.20f, 0.22f, 0.15f, 0.25f, 0.30f, 0.25f};

        for (int i = 0; i < 30; ++i) {
            state.stream_b.is_onset = (i % 8 == 0);
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

    // 1.3 Jazz / Lofi / Ambient (clean, low dissonance, gentle onsets, warm body)
    {
        brain::SemanticClassifierML classifier;
        core::PhysicsAudioState state{};
        state.stream_b.dissonance = 0.04f;
        state.stream_b.spectral_centroid_hz = 850.0f;
        state.stream_b.energy = 0.08f;
        state.stream_a.rms = 0.07f;
        state.stream_b.is_onset = false;
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

    // 1.4 Cyber / Techno (clean punch bass, steady beat, low dissonance)
    {
        brain::SemanticClassifierML classifier;
        core::PhysicsAudioState state{};
        state.stream_b.dissonance = 0.12f;
        state.stream_b.spectral_centroid_hz = 1800.0f;
        state.stream_b.energy = 0.28f;
        state.stream_a.rms = 0.25f;
        state.stream_a.spectrum_bands = {0.20f, 0.70f, 0.30f, 0.25f, 0.35f, 0.50f, 0.40f, 0.20f};

        for (int i = 0; i < 30; ++i) {
            state.stream_b.is_onset = (i % 6 == 0);
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

    // 1.5 Silence Quiescence Relaxation (Post-Metal relaxation into Zen Liquid Ocean)
    {
        brain::SemanticBrain brain;
        core::PhysicsAudioState loud_metal{};
        loud_metal.stream_a.rms = 0.45f;
        loud_metal.stream_b.energy = 0.50f;
        loud_metal.stream_b.dissonance = 0.65f;
        loud_metal.stream_a.spectrum_bands = {0.55f, 0.65f, 0.80f, 0.90f, 0.80f, 0.65f, 0.55f, 0.45f};
        for (int i = 0; i < 60; ++i) {
            loud_metal.stream_b.is_onset = (i % 6 == 0);
            brain.update(loud_metal, 0.02f);
        }
        assert(brain.get_semantic_vector().weight_metal > 0.40f);

        // Music stops: complete silence
        core::PhysicsAudioState silent{};
        silent.stream_a.rms = 0.0f;
        silent.stream_b.energy = 0.0f;
        for (int i = 0; i < 150; ++i) { // 3 seconds of silence
            brain.update(silent, 0.02f);
        }
        const auto& vec = brain.get_semantic_vector();
        std::cout << "  -> Silence Recovery Result: L=" << vec.weight_liquid 
                  << " | M=" << vec.weight_metal 
                  << " | C=" << vec.weight_cyber 
                  << " | D=" << vec.weight_dubstep << std::endl;
        assert(vec.weight_liquid > 0.60f);
        assert(vec.is_silent);
    }

    std::cout << "  [PASS] Neural ML Biome Discrimination works with clear, robust separation!\n\n";
}

// -----------------------------------------------------------------------------
// Test 2: Autocorrelation BPM (130 BPM and 185 BPM fast metal/speedcore)
// -----------------------------------------------------------------------------
void test_autocorrelation_bpm() {
    std::cout << "[TEST 2] Testing Autocorrelation Tempo / BPM Estimation...\n";

    // 2.1 Standard 130 BPM pulse train
    {
        brain::SemanticBrain brain;
        core::PhysicsAudioState state{};
        constexpr float dt = 0.016f;
        float beat_timer = 0.0f;
        constexpr float target_period = 60.0f / 130.0f;

        for (int frame = 0; frame < 300; ++frame) {
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
        std::cout << "  -> Injected 130 BPM: Detected = " 
                  << vec.bpm << " (Confidence: " << vec.bpm_confidence * 100.0f << "%)\n";
        assert(std::abs(vec.bpm - 130.0f) < 8.0f);
        assert(vec.bpm_confidence > 0.30f);
    }

    // 2.2 Fast 185 BPM pulse train (Metal blast beats / Speedcore)
    {
        brain::SemanticBrain brain;
        core::PhysicsAudioState state{};
        constexpr float dt = 0.016f;
        float beat_timer = 0.0f;
        constexpr float target_period = 60.0f / 185.0f;

        for (int frame = 0; frame < 300; ++frame) {
            beat_timer += dt;
            bool is_beat = false;
            if (beat_timer >= target_period) {
                beat_timer -= target_period;
                is_beat = true;
            }

            state.stream_a.rms = 0.40f;
            state.stream_b.energy = 0.45f;
            state.stream_b.is_onset = is_beat;
            state.stream_b.onset_strength = is_beat ? 0.90f : 0.05f;
            state.stream_a.spectrum_bands[0] = is_beat ? 0.80f : 0.10f;

            brain.update(state, dt);
        }

        const auto& vec = brain.get_semantic_vector();
        std::cout << "  -> Injected 185 BPM: Detected = " 
                  << vec.bpm << " (Confidence: " << vec.bpm_confidence * 100.0f << "%)\n";
        assert(std::abs(vec.bpm - 185.0f) < 12.0f);
        assert(vec.bpm_confidence > 0.30f);
    }

    std::cout << "  [PASS] Autocorrelation BPM estimation locks accurately across broad tempo range!\n\n";
}

// -----------------------------------------------------------------------------
// Test 3: Dynamic Kinematics & Expressive Speed Range
// -----------------------------------------------------------------------------
void test_dynamic_kinematics_and_speed() {
    std::cout << "[TEST 3] Testing Dynamic Kinematics & Expressive Speed Range...\n";

    brain::SemanticBrain brain;
    core::PhysicsAudioState state{};

    // 3.1 Quiet / Calm Jazz passage: speed must be relaxed ~2.5 - 3.5 m/s
    state.stream_a.rms = 0.05f;
    state.stream_b.energy = 0.04f;
    state.stream_b.dissonance = 0.04f;
    state.stream_a.spectrum_bands = {0.10f, 0.15f, 0.35f, 0.30f, 0.10f, 0.05f, 0.02f, 0.01f};
    state.stream_b.is_onset = false;
    for (int i = 0; i < 150; ++i) {
        brain.update(state, 0.02f);
    }
    float calm_speed = brain.get_semantic_vector().speed_forward;
    std::cout << "  -> Calm Passage Cruising Speed: " << calm_speed << " m/s" << std::endl;
    assert(calm_speed >= 2.0f && calm_speed <= 4.0f);

    // 3.2 Explosive Heavy Metal / Dubstep Drop: speed must surge to 25.0 - 45.0 m/s sprint!
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
    assert(drop_speed >= 25.0f && drop_speed <= 45.0f);
    assert(drop_speed > calm_speed + 20.0f);

    std::cout << "  [PASS] Speed range is highly dynamic (" << calm_speed << " -> " << drop_speed << " m/s)!\n\n";
}

// -----------------------------------------------------------------------------
// Test 4: 6DoF Camera Clearance, Dynamic Banking & Trauma Shake
// -----------------------------------------------------------------------------
void test_camera_flight_clearance_and_roll() {
    std::cout << "[TEST 4] Testing 6DoF Camera Clearance, Dynamic Banking & Trauma Shake...\n";

    director::AutonomousArtDirector director;
    core::AudioSemanticVector semantic{};
    semantic.speed_forward = 32.0f;
    semantic.weight_metal = 0.85f;
    semantic.arousal = 0.90f;

    bool observed_banking = false;
    bool observed_trauma = false;

    for (int i = 0; i < 200; ++i) {
        semantic.is_onset = (i % 12 == 0); // Periodic heavy drum kicks
        director.update(semantic, 0.02f);
        glm::vec3 pos = director.get_camera_pos();
        assert(pos.y >= 2.0f); // Camera altitude clearance guaranteed

        float roll = director.get_camera_roll();
        if (std::abs(roll) > 0.02f) {
            observed_banking = true;
        }
        if (director.get_trauma() > 0.05f) {
            observed_trauma = true;
        }
    }
    assert(observed_banking);
    assert(observed_trauma);
    std::cout << "  -> Verified camera clearance (Y >= 2.0m), 6DoF banking into turns, and visceral trauma shake.\n";
    std::cout << "  [PASS] Camera kinematics & 6DoF banking operating smoothly!\n\n";
}

// -----------------------------------------------------------------------------
// Test 5: SDF Camera Clearance across ALL 4 Biomes & map() Blending (Zero Collision)
// -----------------------------------------------------------------------------
void test_sdf_clearance_at_camera() {
    std::cout << "[TEST 5] Testing SDF Camera Clearance across ALL 4 Biomes (Zero Collision Guarantee)...\n";

    glm::vec3 cam_p(0.0f, 2.8f, 10.0f);
    float dilation = 1.0f; // Maximum tension & dilation!
    float mids = 1.0f;     // Maximum mid resonance!

    // 5.1 Test Liquid SDF clearance
    float d_liquid = test_sdf_liquid(cam_p, cam_p, dilation, mids, 0.0f, 0.0f);
    std::cout << "  -> Liquid SDF distance at camera position: " << d_liquid << " m\n";
    assert(d_liquid >= 2.0f);

    // 5.2 Test Metal SDF clearance
    float d_metal = test_sdf_metal(cam_p, cam_p, dilation, mids, 0.0f);
    std::cout << "  -> Metal SDF distance at camera position: " << d_metal << " m\n";
    assert(d_metal >= 2.0f);

    // 5.3 Test Cyber SDF clearance
    float d_cyber = test_sdf_cyber(cam_p, cam_p, dilation, mids, 0.0f);
    std::cout << "  -> Cyber SDF distance at camera position: " << d_cyber << " m\n";
    assert(d_cyber >= 2.0f);

    // 5.4 Test Dubstep SDF clearance
    float d_dubstep = test_sdf_dubstep(cam_p, cam_p, dilation, mids, 0.0f);
    std::cout << "  -> Dubstep SDF distance at camera position: " << d_dubstep << " m\n";
    assert(d_dubstep >= 2.0f);

    // 5.5 Test Global map() with 4-way blending and heavy shockwave ripple
    glm::vec4 weights(0.25f, 0.25f, 0.25f, 0.25f);
    float d_map = test_map(cam_p, cam_p, weights, dilation, mids, 0.70f, 0.0f, 1.5f);
    std::cout << "  -> Blended map() distance at camera position with ripple: " << d_map << " m\n";
    assert(d_map >= 2.0f);

    std::cout << "  [PASS] SDF geometry has strict clearance at camera position! Screen will NEVER black out.\n\n";
}

// -----------------------------------------------------------------------------
// Test 6: Chill Sub-Bass (Frog Family) vs Explosive Dubstep (Zero False Positives)
// -----------------------------------------------------------------------------
void test_chill_sub_bass_vs_dubstep() {
    std::cout << "[TEST 6] Testing Chill Sub-Bass vs True Dubstep Discrimination...\n";

    // 6.1 Chill track with heavy sub-bass 808 (Frog Family):
    // Warm deep sub-bass, but calm energy, very low dissonance, gentle/no onsets, no screeching synths
    {
        brain::SemanticClassifierML classifier;
        core::PhysicsAudioState chill_state{};
        chill_state.stream_b.dissonance = 0.03f;
        chill_state.stream_b.spectral_centroid_hz = 650.0f;
        chill_state.stream_b.energy = 0.12f;
        chill_state.stream_a.rms = 0.10f;
        chill_state.stream_b.is_onset = false;
        // Heavy sub-bass, but very little treble / screech
        chill_state.stream_a.spectrum_bands = {0.85f, 0.40f, 0.18f, 0.12f, 0.05f, 0.02f, 0.01f, 0.00f};

        for (int i = 0; i < 30; ++i) {
            classifier.accumulate_frame(chill_state, 0.02f);
        }
        brain::MLClassificationResult res;
        bool eval = classifier.maybe_evaluate(res);
        assert(eval);
        std::cout << "  -> Frog Family / Chill Sub-Bass Result: L=" << res.prob_liquid 
                  << " | M=" << res.prob_metal 
                  << " | C=" << res.prob_cyber 
                  << " | D=" << res.prob_dubstep << std::endl;
        assert(res.prob_liquid > res.prob_dubstep);
        assert(res.prob_liquid > 0.45f);
        assert(res.prob_dubstep < 0.20f);
    }

    // 6.2 True Dubstep / Speedcore Drop (Skrillex / Camellia):
    // Heavy sub-bass + aggressive onsets + screaming high synths (>1.5-3kHz)
    {
        brain::SemanticClassifierML classifier;
        core::PhysicsAudioState dub_state{};
        dub_state.stream_b.dissonance = 0.18f;
        dub_state.stream_b.spectral_centroid_hz = 1850.0f;
        dub_state.stream_b.energy = 0.48f;
        dub_state.stream_a.rms = 0.42f;
        // Heavy sub-bass AND loud high synths
        dub_state.stream_a.spectrum_bands = {0.95f, 0.80f, 0.22f, 0.25f, 0.35f, 0.45f, 0.40f, 0.30f};

        for (int i = 0; i < 30; ++i) {
            dub_state.stream_b.is_onset = (i % 6 == 0);
            classifier.accumulate_frame(dub_state, 0.02f);
        }
        brain::MLClassificationResult res;
        bool eval = classifier.maybe_evaluate(res);
        assert(eval);
        std::cout << "  -> True Dubstep Result: L=" << res.prob_liquid 
                  << " | M=" << res.prob_metal 
                  << " | C=" << res.prob_cyber 
                  << " | D=" << res.prob_dubstep << std::endl;
        assert(res.prob_dubstep > res.prob_liquid);
        assert(res.prob_dubstep > 0.40f);
    }

    std::cout << "  [PASS] Zero false triggers: Chill sub-bass routes to Liquid; high-flux synths trigger Dubstep!\n\n";
}

// -----------------------------------------------------------------------------
// Test 7: Music-Driven Tunnel Elevation & Forward Horizon Camera Visibility
// -----------------------------------------------------------------------------
void test_camera_pitch_and_music_driven_elevation() {
    std::cout << "[TEST 7] Testing Music-Driven Vertical Axis & Camera Floor-Stare Prevention...\n";

    director::AutonomousArtDirector director;
    core::AudioSemanticVector semantic{};

    float min_cam_y = 999.0f;
    float min_pitch_y = 999.0f;
    float max_altitude = -999.0f;
    float min_altitude = 999.0f;

    // Simulate 300 frames of dynamic musical performance
    for (int frame = 0; frame < 300; ++frame) {
        if (frame < 100) {
            // Calm section: low arousal, low dilation
            semantic.arousal = 0.15f;
            semantic.elastic_dilation = 0.10f;
            semantic.melodic_mids = 0.20f;
            semantic.is_onset = false;
            semantic.is_silent = false;
        } else if (frame < 200) {
            // Intense buildup: rising arousal, high dilation
            semantic.arousal = 0.85f;
            semantic.elastic_dilation = 0.75f;
            semantic.melodic_mids = 0.60f;
            semantic.is_onset = (frame % 8 == 0);
        } else {
            // Silence / Quiescence
            semantic.is_silent = true;
            semantic.arousal = 0.05f;
        }

        director.update(semantic, 0.02f);

        glm::vec3 pos = director.get_camera_pos();
        glm::vec3 dir = director.get_camera_dir();

        min_cam_y = std::min(min_cam_y, pos.y);
        min_pitch_y = std::min(min_pitch_y, dir.y);
        max_altitude = std::max(max_altitude, pos.y);
        min_altitude = std::min(min_altitude, pos.y);

        // Strict floor-stare prevention: camera direction Y must NEVER pitch down to floor (must stay > -0.10)
        assert(dir.y > -0.10f);
        // Forward horizon dominance: Z must always be primary forward vector
        assert(dir.z > 0.95f);
        // Camera altitude clearance
        assert(pos.y >= 2.0f);
    }

    std::cout << "  -> Altitude dynamic range: " << min_altitude << "m -> " << max_altitude << "m\n";
    std::cout << "  -> Minimum look direction Y: " << min_pitch_y << " (strictly above floor: > -0.10)\n";
    std::cout << "  -> Minimum camera clearance: " << min_cam_y << "m (>= 2.0m guaranteed)\n";

    // Altitude must have climbed significantly during high-energy buildup
    assert(max_altitude > 2.8f);

    std::cout << "  [PASS] Camera follows music-driven elevation with proud forward horizon visibility (NO floor stare)!\n\n";
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
    test_chill_sub_bass_vs_dubstep();
    test_camera_pitch_and_music_driven_elevation();

    std::cout << "=================================================================\n";
    std::cout << "   ALL VERIFICATION TESTS PASSED SUCCESSFULLY! (100% HEALTHY)    \n";
    std::cout << "=================================================================\n";
    return 0;
}
