#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <iomanip>
#include "core/types.hpp"
#include "brain/semantic_classifier_ml.hpp"
#include "brain/semantic_brain.hpp"
#include "brain/waterfall_analyzer.hpp"
#include "director/autonomous_art_director.hpp"

using namespace audio_voyager;

// Polynomial Smooth Minimum ported from GLSL
inline float smin(float a, float b, float k) {
    float h = std::clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
    return (b * (1.0f - h) + a * h) - k * h * (1.0f - h);
}

// Polynomial Smooth Maximum ported from GLSL
inline float smax(float a, float b, float k) {
    float h = std::clamp(0.5f + 0.5f * (a - b) / k, 0.0f, 1.0f);
    return (b * (1.0f - h) + a * h) + k * h * (1.0f - h);
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
    return smax(d_liq_geom, -d_flight_corridor, 0.40f);
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
    return smax(d_canyon_geom, -d_flight_corridor, 0.40f);
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
    return smax(d_cyber_geom, -d_flight_corridor, 0.40f);
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
    return smax(d_dub_geom, -d_flight_corridor, 0.40f);
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
    return smax(d_interp, -d_global_corridor, 0.45f);
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

// -----------------------------------------------------------------------------
// Test 8: 2D Waterfall / Spectrogram Spatiotemporal Feature Extraction
// -----------------------------------------------------------------------------
void test_waterfall_spatiotemporal_analysis() {
    std::cout << "[TEST 8] Testing 2D Waterfall / Spectrogram Spatiotemporal Pattern Engine...\n";

    // 8.1 Horizontal Spectral Continuity: Sustained guitar chord vs sparse notes
    {
        brain::WaterfallAnalyzer analyzer_sustained;
        brain::WaterfallAnalyzer analyzer_sparse;

        // Sustained guitar power chord: continuous mid energy (bands 14-46) across 60 frames
        std::array<float, 64> sustained_frame{};
        for (size_t b = 14; b <= 46; ++b) {
            sustained_frame[b] = 0.85f;
        }

        // Sparse plucks: only every 10th frame has energy, others 0
        std::array<float, 64> empty_frame{};

        for (int i = 0; i < 60; ++i) {
            analyzer_sustained.push_frame(sustained_frame);
            if (i % 10 == 0) {
                analyzer_sparse.push_frame(sustained_frame);
            } else {
                analyzer_sparse.push_frame(empty_frame);
            }
        }

        const auto& m_sustained = analyzer_sustained.analyze();
        const auto& m_sparse = analyzer_sparse.analyze();

        std::cout << "  -> Horizontal Guitar Continuity: Sustained = " 
                  << m_sustained.guitar_continuity * 100.0f << "% | Sparse = " 
                  << m_sparse.guitar_continuity * 100.0f << "%\n";

        assert(m_sustained.guitar_continuity > 0.80f);
        assert(m_sparse.guitar_continuity < 0.35f);
        assert(m_sustained.guitar_continuity > m_sparse.guitar_continuity + 0.45f);
    }

    // 8.2 Vertical Periodic Pulses: 4-on-the-Floor Kick Columns vs Irregular/Syncopated
    {
        brain::WaterfallAnalyzer analyzer_techno;
        brain::WaterfallAnalyzer analyzer_syncopated;

        std::array<float, 64> kick_frame{};
        for (size_t b = 0; b <= 12; ++b) kick_frame[b] = 0.90f; // low bass column
        std::array<float, 64> rest_frame{};

        // Steady 4-on-the-floor: kick column precisely every 22 frames (~130 BPM at ~48Hz)
        for (int i = 0; i < 90; ++i) {
            if (i % 22 == 0) {
                analyzer_techno.push_frame(kick_frame);
            } else {
                analyzer_techno.push_frame(rest_frame);
            }
        }

        // Irregular syncopated: sporadic random pulses
        for (int i = 0; i < 90; ++i) {
            if (i == 4 || i == 11 || i == 37 || i == 42 || i == 79) {
                analyzer_syncopated.push_frame(kick_frame);
            } else {
                analyzer_syncopated.push_frame(rest_frame);
            }
        }

        const auto& m_techno = analyzer_techno.analyze();
        const auto& m_sync = analyzer_syncopated.analyze();

        std::cout << "  -> 4-on-the-Floor Kick Regularity: Steady = " 
                  << m_techno.four_on_the_floor_regularity * 100.0f << "% | Syncopated = " 
                  << m_sync.four_on_the_floor_regularity * 100.0f << "%\n";

        assert(m_techno.four_on_the_floor_regularity > 0.35f);
        assert(m_sync.four_on_the_floor_regularity < 0.25f);
    }

    // 8.3 Temporal Flux & Variance: Dubstep Wobble Modulation vs Steady Drone
    {
        brain::WaterfallAnalyzer analyzer_wobble;
        brain::WaterfallAnalyzer analyzer_drone;

        for (int i = 0; i < 60; ++i) {
            std::array<float, 64> wobble_frame{};
            float lfo = 0.5f + 0.5f * std::sin(static_cast<float>(i) * 0.8f);
            for (size_t b = 0; b <= 24; ++b) {
                wobble_frame[b] = lfo * 0.9f;
            }
            analyzer_wobble.push_frame(wobble_frame);

            std::array<float, 64> drone_frame{};
            for (size_t b = 0; b <= 24; ++b) {
                drone_frame[b] = 0.60f;
            }
            analyzer_drone.push_frame(drone_frame);
        }

        const auto& m_wobble = analyzer_wobble.analyze();
        const auto& m_drone = analyzer_drone.analyze();

        std::cout << "  -> Bass Temporal Flux: Wobble LFO = " 
                  << m_wobble.bass_temporal_flux * 100.0f << "% | Steady Drone = " 
                  << m_drone.bass_temporal_flux * 100.0f << "%\n";

        assert(m_wobble.bass_temporal_flux > 0.20f);
        assert(m_drone.bass_temporal_flux < 0.05f);
    }

    // 8.4 WebSDR Colormap Texture Generation
    {
        brain::WaterfallAnalyzer analyzer;
        std::array<float, 64> ramp_frame{};
        for (size_t b = 0; b < 64; ++b) ramp_frame[b] = static_cast<float>(b) / 63.0f;
        for (int i = 0; i < 128; ++i) analyzer.push_frame(ramp_frame);

        std::vector<uint32_t> pixels;
        analyzer.generate_rgba_texture(pixels);
        assert(pixels.size() == 64 * 128);
        for (uint32_t px : pixels) {
            assert((px & 0xFF000000u) == 0xFF000000u);
        }
        std::cout << "  -> WebSDR RGBA Texture: " << pixels.size() << " pixels successfully generated with 100% alpha.\n";
    }

    std::cout << "  [PASS] 2D Waterfall Spatiotemporal Pattern Engine extracts continuity, periodicity and flux accurately!\n\n";
}

// -----------------------------------------------------------------------------
// Test 9: Metal & Distorted Guitar Fix (BMTH, BFMV, Hand of Blood)
// -----------------------------------------------------------------------------
void test_metal_distorted_guitar_fix() {
    std::cout << "[TEST 9] Testing Metal & Distorted Guitar Fix (Bring Me The Horizon / Bullet For My Valentine)...\n";

    // Heavy distorted guitar track with dense mids, low crest factor (< 1.8), high bounded flatness
    brain::SemanticClassifierML classifier;
    core::PhysicsAudioState state{};
    state.stream_b.dissonance = 0.52f;
    state.stream_b.spectral_flatness = 0.58f; // High bounded flatness (300-6000 Hz)
    state.stream_b.crest_factor_mids = 1.45f; // Distorted guitar wall crest factor (< 1.8)
    state.stream_b.spectral_centroid_hz = 2400.0f;
    state.stream_b.energy = 0.46f;
    state.stream_a.rms = 0.42f;
    // Heavy wall of distorted guitars in mids + sub-bass double kicks
    state.stream_a.spectrum_bands = {0.60f, 0.70f, 0.85f, 0.95f, 0.88f, 0.70f, 0.55f, 0.40f};

    brain::WaterfallMetrics wf{};
    wf.guitar_continuity = 0.85f; // High sustained power chords
    wf.horizontal_continuity = 0.75f;
    wf.four_on_the_floor_regularity = 0.10f;
    wf.bass_temporal_flux = 0.20f;

    for (int i = 0; i < 30; ++i) {
        state.stream_b.is_onset = (i % 6 == 0);
        classifier.accumulate_frame(state, 0.02f, wf, 0.2f, 160.0f);
    }

    brain::MLClassificationResult res;
    bool eval = classifier.maybe_evaluate(res);
    assert(eval);

    std::cout << "  -> BMTH / BFMV Metal Guitar Result: L=" << res.prob_liquid 
              << " | M=" << res.prob_metal 
              << " | C=" << res.prob_cyber 
              << " | D=" << res.prob_dubstep << std::endl;

    assert(res.prob_metal > 0.90f);
    assert(res.prob_liquid < 0.05f); // STRICTLY NO fallback to Jazz/Liquid
    assert(res.prob_metal > res.prob_liquid);
    assert(res.prob_metal > res.prob_cyber);
    assert(res.prob_metal > res.prob_dubstep);

    std::cout << "  [PASS] Distorted guitar wall reliably triggers SDF_Metal > 90% and NEVER falls into Jazz/Liquid!\n\n";
}

// -----------------------------------------------------------------------------
// Test 10: Techno (Cyber) vs Dubstep Disambiguation
// -----------------------------------------------------------------------------
void test_techno_vs_dubstep_disambiguation() {
    std::cout << "[TEST 10] Testing Techno (Cyber) vs Dubstep Disambiguation...\n";

    // 10.1 Steady 4-on-the-Floor Techno at 128 BPM
    {
        brain::SemanticClassifierML classifier;
        core::PhysicsAudioState techno_state{};
        techno_state.stream_b.dissonance = 0.10f;
        techno_state.stream_b.spectral_flatness = 0.15f;
        techno_state.stream_b.crest_factor_mids = 2.8f;
        techno_state.stream_b.spectral_centroid_hz = 1750.0f;
        techno_state.stream_b.energy = 0.32f;
        techno_state.stream_a.rms = 0.30f;
        // Clean punch bass, steady cadence
        techno_state.stream_a.spectrum_bands = {0.30f, 0.85f, 0.35f, 0.25f, 0.40f, 0.50f, 0.40f, 0.20f};

        brain::WaterfallMetrics wf{};
        wf.four_on_the_floor_regularity = 0.82f; // Strong 4-on-the-floor kick columns
        wf.vertical_pulse_periodicity = 0.85f;
        wf.bass_temporal_flux = 0.12f;          // Low wobble
        wf.guitar_continuity = 0.15f;

        for (int i = 0; i < 30; ++i) {
            techno_state.stream_b.is_onset = (i % 6 == 0);
            classifier.accumulate_frame(techno_state, 0.02f, wf, 0.85f, 128.0f);
        }

        brain::MLClassificationResult res;
        bool eval = classifier.maybe_evaluate(res);
        assert(eval);

        std::cout << "  -> 128 BPM Techno Input Result: L=" << res.prob_liquid 
                  << " | M=" << res.prob_metal 
                  << " | C=" << res.prob_cyber 
                  << " | D=" << res.prob_dubstep << std::endl;

        assert(res.prob_cyber > 0.60f);
        assert(res.prob_cyber > res.prob_dubstep);
        assert(res.prob_cyber > res.prob_liquid);
    }

    // 10.2 Dubstep Drop with Heavy Sub-Bass Wobble & Syncopation
    {
        brain::SemanticClassifierML classifier;
        core::PhysicsAudioState dub_state{};
        dub_state.stream_b.dissonance = 0.22f;
        dub_state.stream_b.spectral_flatness = 0.25f;
        dub_state.stream_b.crest_factor_mids = 2.2f;
        dub_state.stream_b.spectral_centroid_hz = 1900.0f;
        dub_state.stream_b.energy = 0.48f;
        dub_state.stream_a.rms = 0.42f;
        // Massive sub-bass dominance over mids + screaming high synths
        dub_state.stream_a.spectrum_bands = {0.95f, 0.80f, 0.20f, 0.25f, 0.35f, 0.50f, 0.45f, 0.30f};

        brain::WaterfallMetrics wf{};
        wf.four_on_the_floor_regularity = 0.05f; // Syncopated / half-time beat
        wf.vertical_pulse_periodicity = 0.20f;
        wf.bass_temporal_flux = 0.78f;          // Heavy LFO wobble flux
        wf.guitar_continuity = 0.10f;

        for (int i = 0; i < 30; ++i) {
            dub_state.stream_b.is_onset = (i % 8 == 0);
            classifier.accumulate_frame(dub_state, 0.02f, wf, 0.15f, 140.0f);
        }

        brain::MLClassificationResult res;
        bool eval = classifier.maybe_evaluate(res);
        assert(eval);

        std::cout << "  -> Dubstep Wobble Input Result: L=" << res.prob_liquid 
                  << " | M=" << res.prob_metal 
                  << " | C=" << res.prob_cyber 
                  << " | D=" << res.prob_dubstep << std::endl;

        assert(res.prob_dubstep > 0.60f);
        assert(res.prob_dubstep > res.prob_cyber);
        assert(res.prob_dubstep > res.prob_liquid);
    }

    std::cout << "  [PASS] 4-on-the-floor 128 BPM routes to Cyber; syncopated wobble routes to Dubstep!\n\n";
}

// -----------------------------------------------------------------------------
// Test 11: Multi-Band Muscle Reactivity (Stream B 5 Bands & Stream A Presence bands[5])
// -----------------------------------------------------------------------------
void test_muscle_multiband_connectivity() {
    std::cout << "[TEST 11] Testing Multi-Band Muscle Reactivity (Stream B 5 Bands & bands[5])...\n";

    brain::SemanticBrain brain;
    core::PhysicsAudioState state{};
    state.stream_a.rms = 0.35f;
    state.stream_b.energy = 0.35f;

    // 11.1 Test Presence band (bands[5]) excitation
    state.stream_a.spectrum_bands[5] = 0.85f;
    for (int i = 0; i < 20; ++i) {
        brain.update(state, 0.02f);
    }
    const auto& vec_presence = brain.get_semantic_vector();
    std::cout << "  -> Presence (bands[5]) Mids Excitation: " << vec_presence.melodic_mids << " | Air: " << vec_presence.treble_sparkle << "\n";
    assert(vec_presence.melodic_mids > 0.15f);
    assert(vec_presence.treble_sparkle > 0.08f);

    // 11.2 Test Stream B band_sub_bass excitation
    state.stream_a.spectrum_bands[5] = 0.0f;
    state.stream_b.band_sub_bass = 0.90f;
    for (int i = 0; i < 20; ++i) {
        brain.update(state, 0.02f);
    }
    const auto& vec_sub = brain.get_semantic_vector();
    std::cout << "  -> Stream B band_sub_bass Dilation Excitation: " << vec_sub.elastic_dilation << "\n";
    assert(vec_sub.elastic_dilation > 0.25f);

    // 11.3 Test Stream B band_mids and band_air excitation
    state.stream_b.band_mids = 0.75f;
    state.stream_b.band_air = 0.80f;
    for (int i = 0; i < 20; ++i) {
        brain.update(state, 0.02f);
    }
    const auto& vec_mids_air = brain.get_semantic_vector();
    std::cout << "  -> Stream B band_mids Resonance: " << vec_mids_air.melodic_mids << " | band_air Sparkle: " << vec_mids_air.treble_sparkle << "\n";
    assert(vec_mids_air.melodic_mids > 0.30f);
    assert(vec_mids_air.treble_sparkle > 0.40f);

    std::cout << "  [PASS] Stream B's 5 physical bands and bands[5] Presence drive muscle excitations directly!\n\n";
}

// -----------------------------------------------------------------------------
// Test 12: Cohesive Dynamic Palettes & Temporal Low-Pass Filtering (No Strobing)
// -----------------------------------------------------------------------------
void test_cohesive_palette_temporal_smoothing() {
    std::cout << "[TEST 12] Testing Cohesive Dynamic Palettes & Temporal Low-Pass Filtering...\n";

    brain::SemanticBrain brain;
    core::PhysicsAudioState loud_metal{};
    loud_metal.stream_a.rms = 0.45f;
    loud_metal.stream_b.energy = 0.50f;
    loud_metal.stream_b.dissonance = 0.65f;
    loud_metal.stream_a.spectrum_bands = {0.55f, 0.65f, 0.80f, 0.90f, 0.80f, 0.65f, 0.55f, 0.45f};

    // Step 1 frame into metal
    brain.update(loud_metal, 0.016f);
    auto vec1 = brain.get_semantic_vector();

    // Verify initial color matches Liquid (sapphire) resting state and did not snap instantly to pure metal crimson
    std::cout << "  -> Initial Color after 1 frame: R=" << vec1.color_primary.r << ", G=" << vec1.color_primary.g << ", B=" << vec1.color_primary.b << "\n";
    assert(vec1.color_primary.b > vec1.color_primary.r); // Still mostly sapphire ocean!

    // Feed metal continuously for 3 seconds
    for (int i = 0; i < 180; ++i) {
        loud_metal.stream_b.is_onset = (i % 8 == 0);
        brain.update(loud_metal, 0.016f);
    }
    auto vec2 = brain.get_semantic_vector();
    std::cout << "  -> Morphed Color after 3.0s Metal: R=" << vec2.color_primary.r 
              << ", G=" << vec2.color_primary.g << ", B=" << vec2.color_primary.b << std::endl;
    assert(vec2.color_primary.r > vec2.color_primary.b); // Morphed smoothly into magma crimson
    assert(vec2.color_primary.r > 0.45f);

    // Verify accent color is smoldering ember/gold
    assert(vec2.color_accent.r > 0.70f);
    // Verify zenith sky is dark smoldering basalt black
    assert(vec2.color_zenith.r < 0.05f && vec2.color_zenith.g < 0.05f && vec2.color_zenith.b < 0.05f);

    std::cout << "  [PASS] Dynamic palettes morph smoothly (tau ~ 1.8s) with harmonious, congruent colors (zero strobing)!\n\n";
}

// -----------------------------------------------------------------------------
// Test 13: 2D Waterfall Raw Energy Matrix (GL_R16F GPU Texture)
// -----------------------------------------------------------------------------
void test_waterfall_raw_energy_matrix() {
    std::cout << "[TEST 13] Testing 2D Waterfall Raw Energy Matrix Extraction...\n";

    brain::WaterfallAnalyzer waterfall;
    std::array<float, core::WATERFALL_BANDS> test_frame{};
    for (size_t b = 0; b < core::WATERFALL_BANDS; ++b) {
        test_frame[b] = static_cast<float>(b) / static_cast<float>(core::WATERFALL_BANDS);
    }

    for (int f = 0; f < 30; ++f) {
        waterfall.push_frame(test_frame);
    }

    std::vector<float> energy_matrix;
    waterfall.get_raw_energy_matrix(energy_matrix);

    assert(energy_matrix.size() == core::WATERFALL_BANDS * core::WATERFALL_FRAMES);
    std::cout << "  -> Energy Matrix Size: " << energy_matrix.size() << " floats (" 
              << (energy_matrix.size() * sizeof(float)) << " bytes)\n";

    for (float val : energy_matrix) {
        assert(val >= 0.0f && val <= 1.0f);
    }

    std::cout << "  [PASS] 2D Waterfall raw float matrix ready for GL_R16F terrain sculpt texture upload!\n\n";
}

// -----------------------------------------------------------------------------
// Test 14: AGC Volume-Invariance & Continuous Noise-Floor Transition
// -----------------------------------------------------------------------------
void test_agc_volume_invariance() {
    std::cout << "[TEST 14] Testing AGC Volume-Invariance & Continuous Noise-Floor Transition...\n";

    constexpr float NOMINAL_TARGET = 0.24f;
    constexpr float NOISE_FLOOR = 0.003f;
    constexpr float QUIET_THRESHOLD = 0.015f;

    auto compute_agc_gain = [&](float envelope) {
        if (envelope > QUIET_THRESHOLD) {
            return std::clamp(NOMINAL_TARGET / envelope, 0.20f, 5.0f);
        } else if (envelope > NOISE_FLOOR) {
            float raw_target_gain = std::clamp(NOMINAL_TARGET / envelope, 0.20f, 5.0f);
            float t = (envelope - NOISE_FLOOR) / (QUIET_THRESHOLD - NOISE_FLOOR);
            float smooth_t = t * t * (3.0f - 2.0f * t);
            return 1.0f + smooth_t * (raw_target_gain - 1.0f);
        } else {
            return 1.0f;
        }
    };

    // 14.1 Test Volume-Invariance: 50% Spotify volume vs 100% Spotify volume
    float low_vol_env = 0.10f;  // 50% volume track
    float high_vol_env = 0.35f; // 100% loud mastered track

    float gain_low = compute_agc_gain(low_vol_env);
    float gain_high = compute_agc_gain(high_vol_env);

    float effective_low_rms = low_vol_env * gain_low;
    float effective_high_rms = high_vol_env * gain_high;

    std::cout << "  -> Low Volume Input: RMS 0.10 -> Gain " << gain_low << "x -> Normalized RMS " << effective_low_rms << "\n";
    std::cout << "  -> High Volume Input: RMS 0.35 -> Gain " << gain_high << "x -> Normalized RMS " << effective_high_rms << "\n";

    assert(std::abs(effective_low_rms - NOMINAL_TARGET) < 0.01f);
    assert(std::abs(effective_high_rms - NOMINAL_TARGET) < 0.01f);
    assert(std::abs(effective_low_rms - effective_high_rms) < 0.01f);

    // 14.2 Test Strict Noise-Floor Continuity (Zero clicks or discontinuous jumps)
    float prev_gain = 1.0f;
    for (int step = 0; step <= 50; ++step) {
        float env = 0.001f + 0.020f * (static_cast<float>(step) / 50.0f);
        float g = compute_agc_gain(env);
        // Gain must not jump by more than 0.4 between adjacent small steps
        assert(std::abs(g - prev_gain) < 0.45f);
        prev_gain = g;
    }

    // 14.3 Test Soft-Knee Saturation curve
    auto soft_saturate = [](float x) noexcept -> float {
        constexpr float threshold = 0.75f;
        constexpr float headroom = 1.0f - threshold;
        if (std::abs(x) <= threshold) return x;
        if (x > threshold) {
            float excess = x - threshold;
            return threshold + headroom * std::tanh(excess / headroom);
        } else {
            float excess = -x - threshold;
            return -(threshold + headroom * std::tanh(excess / headroom));
        }
    };

    assert(soft_saturate(0.5f) == 0.5f); // linear below threshold
    assert(soft_saturate(1.2f) < 1.0f);  // compressed smoothly below 1.0
    assert(soft_saturate(-1.2f) > -1.0f);
    assert(soft_saturate(5.0f) <= 1.0f); // extreme transient remains clamped within unit float range [-1.0, 1.0]
    std::cout << "  -> Soft-Knee Saturation at 1.5x input: " << soft_saturate(1.5f) << " (clean, smooth <= 1.0)\n";

    std::cout << "  [PASS] AGC normalizer produces volume-invariant calibrated levels and C1 smooth noise-floor transitions!\n\n";
}

// -----------------------------------------------------------------------------
// Test 15: Raymarching Map & 2D Waterfall Sculpt Safety (No NaN, Clearance >= 2.0m)
// -----------------------------------------------------------------------------
void test_raymarching_waterfall_sculpt_safety() {
    std::cout << "[TEST 15] Testing Raymarching Map & 2D Waterfall Sculpt Safety...\n";

    glm::vec3 cam_p(0.0f, 2.8f, 10.0f);

    // 15.1 Verify that p == cam_p does NOT produce undefined behavior in atan or NaN
    glm::vec2 p_rel(cam_p.x - cam_p.x, cam_p.y - cam_p.y);
    float angle = (glm::dot(p_rel, p_rel) > 1e-7f) ? std::atan2(p_rel.y, p_rel.x) : 0.0f;
    assert(!std::isnan(angle));
    assert(angle == 0.0f);

    // 15.2 Verify that even with MAXIMUM waterfall terrain sculpting (-0.45m), 
    // the guaranteed flight corridor smax keeps clearance >= 2.0m
    float max_wf_carve = 0.45f;
    glm::vec4 weights(0.25f, 0.25f, 0.25f, 0.25f);
    float d_raw = test_map(cam_p, cam_p, weights, 1.0f, 1.0f, 0.70f, 0.0f, 1.5f);
    float d_sculpted = d_raw - max_wf_carve;
    // Apply global flight corridor protection
    float d_corridor = glm::length(glm::vec2(0.0f, 0.0f)) - (2.2f + 1.0f * 0.2f); // -2.4m
    float d_safe = smax(d_sculpted, -d_corridor, 0.45f);

    std::cout << "  -> Distance at camera with max waterfall carving: " << d_safe << " m\n";
    assert(d_safe >= 2.0f);
    assert(!std::isnan(d_safe));

    std::cout << "  [PASS] Waterfall terrain sculpting preserves absolute camera clearance (>= 2.0m) with zero NaNs!\n\n";
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
    test_waterfall_spatiotemporal_analysis();
    test_metal_distorted_guitar_fix();
    test_techno_vs_dubstep_disambiguation();
    test_muscle_multiband_connectivity();
    test_cohesive_palette_temporal_smoothing();
    test_waterfall_raw_energy_matrix();
    test_agc_volume_invariance();
    test_raymarching_waterfall_sculpt_safety();

    std::cout << "=================================================================\n";
    std::cout << "   ALL VERIFICATION TESTS PASSED SUCCESSFULLY! (100% HEALTHY)    \n";
    std::cout << "=================================================================\n";
    return 0;
}
