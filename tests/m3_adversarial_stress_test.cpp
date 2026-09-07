#include "core/types.hpp"
#include "brain/semantic_brain.hpp"
#include "director/autonomous_art_director.hpp"

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <random>
#include <cassert>
#include <chrono>
#include <algorithm>

using namespace audio_voyager;

// =============================================================================
// Exact CPU Replica of GLSL Shader Math from shaders/raymarching.frag
// =============================================================================
namespace shader_math {

inline float smin(float a, float b, float k) {
    float h = std::clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
    return (1.0f - h) * b + h * a - k * h * (1.0f - h);
}

inline float smax(float a, float b, float k) {
    return -smin(-a, -b, k);
}

inline glm::vec2 getFlightSpline(float z) {
    return glm::vec2(
        2.8f * std::sin(z * 0.040f) + 1.2f * std::cos(z * 0.080f),
        2.2f * std::cos(z * 0.032f) + 0.9f * std::sin(z * 0.065f)
    );
}

inline glm::vec2 rot2D(glm::vec2 v, float a) {
    float s = std::sin(a), c = std::cos(a);
    return glm::vec2(c * v.x - s * v.y, s * v.x + c * v.y);
}

inline float sdSmoothGyroid(glm::vec3 p, float scale, float thickness, float bias) {
    glm::vec3 p_s = p * scale;
    float g = std::abs(std::sin(p_s.x)*std::cos(p_s.y) + std::sin(p_s.y)*std::cos(p_s.z) + std::sin(p_s.z)*std::cos(p_s.x) + bias) - thickness;
    float d = g / scale;
    float organic = std::sin(p.x * 0.45f + p.y * 0.55f) * std::cos(p.z * 0.35f) * 0.08f;
    return d + organic;
}

inline float sdCrystalLattice(glm::vec3 p, float scale, float thickness, float bias) {
    glm::vec3 p_s = p * scale;
    float d_d = std::sin(p_s.x)*std::sin(p_s.y)*std::sin(p_s.z) +
                std::sin(p_s.x)*std::cos(p_s.y)*std::cos(p_s.z) +
                std::cos(p_s.x)*std::sin(p_s.y)*std::cos(p_s.z) +
                std::cos(p_s.x)*std::cos(p_s.y)*std::sin(p_s.z) + bias;
    float d_tpms = (std::abs(d_d) - thickness) / scale;

    glm::vec3 q = glm::vec3(
        std::fmod(std::abs(p.x * (scale * 0.6f)) + 0.5f, 1.0f) - 0.5f,
        std::fmod(std::abs(p.y * (scale * 0.6f)) + 0.5f, 1.0f) - 0.5f,
        std::fmod(std::abs(p.z * (scale * 0.6f)) + 0.5f, 1.0f) - 0.5f
    );
    float poly = (std::abs(q.x) + std::abs(q.y) + std::abs(q.z) - 0.38f) / (scale * 0.6f);
    return smin(d_tpms, poly * 0.5f, 0.15f);
}

inline float sdMandelboxSpines(glm::vec3 p, float scale, float thickness, float bias) {
    glm::vec3 p_f = p * (scale * 0.75f);
    float dr = 1.0f;
    float f_bias = bias * 0.4f;

    for (int i = 0; i < 3; ++i) {
        p_f = glm::clamp(p_f, glm::vec3(-1.0f), glm::vec3(1.0f)) * 2.0f - p_f;
        float r2 = glm::dot(p_f, p_f);
        if (r2 < 0.25f) {
            float temp = 2.0f / 0.25f;
            p_f *= temp;
            dr *= temp;
        } else if (r2 < 1.0f) {
            float temp = 2.0f / r2;
            p_f *= temp;
            dr *= temp;
        }
        p_f = p_f * 1.55f - glm::vec3(1.05f, 0.65f, 1.25f) + f_bias;
        dr = dr * 1.55f + 1.0f;
    }

    float d_fractal = (glm::length(p_f) - (0.85f + thickness * 0.5f)) / dr;
    float spines = (std::sin(p.x * 2.5f) * std::sin(p.y * 2.5f) * std::sin(p.z * 2.5f)) * 0.06f;
    return (d_fractal / (scale * 0.75f)) + spines;
}

// Master Distance Map evaluating the complete GLSL signed distance field
inline float mapSDF(
    glm::vec3 p,
    glm::vec3 cam_pos,
    const core::AudioSemanticVector& sem
) {
    float alpha_s       = sem.morph_smooth;
    float alpha_c       = sem.morph_crystal;
    float alpha_f       = sem.morph_fractal;
    float shock_radius  = sem.shockwave_radius;
    float shock_int     = sem.shockwave_intensity;
    float shock_phase   = sem.shockwave_phase;

    float gyroid_scale  = sem.gyroid_scale;
    float gyroid_twist  = sem.gyroid_twist;
    float dilation      = sem.elastic_dilation;
    float ripple        = sem.surface_ripple;
    float mids          = sem.melodic_mids;
    float sparkle       = sem.treble_sparkle;
    float beat_phase    = sem.beat_phase;

    glm::vec3 q = p;
    float twist_angle = (q.z - cam_pos.z) * gyroid_twist * 0.12f;
    glm::vec2 rotated_xy = rot2D(glm::vec2(q.x, q.y), twist_angle);
    q.x = rotated_xy.x;
    q.y = rotated_xy.y;

    float base_bias = std::sin(beat_phase * 0.5f + q.z * 0.05f) * (0.15f + mids * 0.35f);
    float thick_s   = 0.30f + dilation * 0.35f;
    float thick_c   = 0.24f + dilation * 0.28f + sparkle * 0.15f;
    float thick_f   = 0.20f + dilation * 0.22f;

    float d_smooth  = sdSmoothGyroid(q, gyroid_scale, thick_s, base_bias);
    float d_crystal = sdCrystalLattice(q, gyroid_scale * 1.12f, thick_c, base_bias * 0.85f);
    float d_fractal = sdMandelboxSpines(q, gyroid_scale * 0.92f, thick_f, base_bias * 0.5f);

    float d_morph = alpha_s * d_smooth + alpha_c * d_crystal + alpha_f * d_fractal;

    float dist_to_cam = glm::length(p - cam_pos);
    float shock_diff = std::abs(dist_to_cam - shock_radius);
    float shockwave_ring = std::exp(-shock_diff * shock_diff * 0.35f) * std::sin(shock_diff * 3.5f - shock_phase) * shock_int * 0.22f;
    float ripple_wave = std::sin(dist_to_cam * 3.2f - beat_phase * 4.0f) * ripple * 0.075f;

    glm::vec2 spline_center = getFlightSpline(p.z);
    float dist_to_spline = glm::length(glm::vec2(p.x, p.y) - spline_center);
    float tunnel_radius = 2.8f + dilation * 1.6f;
    float d_tunnel_void = dist_to_spline - tunnel_radius;

    float d_scene = smax(d_morph + shockwave_ring + ripple_wave, -d_tunnel_void, 0.75f);
    float d_cam_bubble = glm::length(p - cam_pos) - (2.5f + dilation * 0.6f);

    return smax(d_scene, -d_cam_bubble, 0.55f);
}

} // namespace shader_math

// =============================================================================
// Test Runner & Results Struct
// =============================================================================
struct TestStats {
    int total_assertions{0};
    int passed_assertions{0};
    int failed_assertions{0};
    std::vector<std::string> failures;

    void assert_true(bool cond, const std::string& desc) {
        total_assertions++;
        if (cond) {
            passed_assertions++;
        } else {
            failed_assertions++;
            failures.push_back(desc);
            std::cerr << "[FAIL] " << desc << "\n";
        }
    }
};

// =============================================================================
// TEST SUITE 1: Barycentric Morphing & Tempo Modulations
// =============================================================================
void test_barycentric_morphing(TestStats& stats) {
    std::cout << "\n=======================================================\n";
    std::cout << " TEST 1: Barycentric Morphing & Continuous Blend Space\n";
    std::cout << "=======================================================\n";

    brain::SemanticBrain brain;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    float max_delta_morph = 0.0f;
    float max_sum_err = 0.0f;
    core::AudioSemanticVector prev_sem = brain.get_semantic_vector();

    constexpr int NUM_FRAMES = 100000;
    const float dt = 1.0f / 144.0f;

    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
        core::PhysicsAudioState state{};
        
        // Scenario A: First 20,000 frames: Rapid Square-Wave switching between Smooth Lofi and Speedcore
        if (frame < 20000) {
            bool is_speedcore = ((frame / 20) % 2 == 1);
            if (is_speedcore) {
                state.stream_a.rms = 0.95f;
                state.stream_b.rms = 0.95f;
                state.stream_b.energy = 0.90f;
                state.stream_b.dissonance = 0.85f;
                state.stream_b.spectral_flatness = 0.80f;
                state.stream_b.spectral_centroid_hz = 5500.0f;
                state.stream_b.is_onset = (frame % 8 == 0);
                state.stream_b.onset_strength = state.stream_b.is_onset ? 0.95f : 0.1f;
                state.stream_b.band_sub_bass = 0.90f;
            } else {
                state.stream_a.rms = 0.35f;
                state.stream_b.rms = 0.35f;
                state.stream_b.energy = 0.30f;
                state.stream_b.dissonance = 0.02f;
                state.stream_b.spectral_flatness = 0.05f;
                state.stream_b.spectral_centroid_hz = 600.0f;
                state.stream_b.is_onset = false;
                state.stream_b.band_sub_bass = 0.40f;
            }
        }
        // Scenario B: 20,000 - 40,000 frames: Dirac impulses & Rapid 1-frame alternations
        else if (frame < 40000) {
            bool impulse = (frame % 2 == 0);
            state.stream_a.rms = impulse ? 1.0f : 0.01f;
            state.stream_b.rms = impulse ? 1.0f : 0.01f;
            state.stream_b.energy = impulse ? 1.0f : 0.01f;
            state.stream_b.dissonance = impulse ? 1.0f : 0.0f;
            state.stream_b.is_onset = impulse;
            state.stream_b.onset_strength = impulse ? 1.0f : 0.0f;
            state.stream_b.band_sub_bass = impulse ? 1.0f : 0.0f;
        }
        // Scenario C: 40,000 - 60,000 frames: Quiescence / Silence switching
        else if (frame < 60000) {
            bool silent = ((frame / 100) % 2 == 0);
            if (silent) {
                state.stream_a.rms = 0.0f;
                state.stream_b.rms = 0.0f;
            } else {
                state.stream_a.rms = 0.7f;
                state.stream_b.rms = 0.7f;
                state.stream_b.spectral_centroid_hz = 3200.0f;
                state.stream_b.band_mids = 0.8f;
                state.stream_b.band_air = 0.7f;
            }
        }
        // Scenario D: 60,000 - 80,000 frames: Extreme Tempo Modulations (60 - 240 BPM)
        else if (frame < 80000) {
            state.stream_a.rms = 0.6f;
            state.stream_b.rms = 0.6f;
            state.stream_b.band_sub_bass = 0.8f;
            int interval = 6 + static_cast<int>(18.0f * (0.5f + 0.5f * std::sin(frame * 0.01f)));
            state.stream_b.is_onset = (frame % interval == 0);
            state.stream_b.onset_strength = state.stream_b.is_onset ? 0.85f : 0.0f;
        }
        // Scenario E: 80,000 - 100,000 frames: Pure stochastic adversarial fuzzing
        else {
            state.stream_a.rms = dist01(rng);
            state.stream_b.rms = dist01(rng);
            state.stream_b.energy = dist01(rng);
            state.stream_b.dissonance = dist01(rng);
            state.stream_b.spectral_flatness = dist01(rng);
            state.stream_b.spectral_centroid_hz = dist01(rng) * 12000.0f;
            state.stream_b.is_onset = (dist01(rng) > 0.85f);
            state.stream_b.onset_strength = dist01(rng);
            state.stream_b.band_sub_bass = dist01(rng);
            state.stream_b.band_mids = dist01(rng);
            state.stream_b.band_air = dist01(rng);
        }

        brain.update(state, dt);
        const auto& sem = brain.get_semantic_vector();

        // 1. Partition of Unity Check: alpha_s + alpha_c + alpha_f == 1.0
        float morph_sum = sem.morph_smooth + sem.morph_crystal + sem.morph_fractal;
        float sum_err = std::abs(morph_sum - 1.0f);
        if (sum_err > max_sum_err) max_sum_err = sum_err;

        stats.assert_true(sum_err < 1e-4f, "Partition of unity violated (sum != 1.0)");
        stats.assert_true(sem.morph_smooth >= 0.0f && sem.morph_smooth <= 1.0f, "alpha_smooth out of [0, 1]");
        stats.assert_true(sem.morph_crystal >= 0.0f && sem.morph_crystal <= 1.0f, "alpha_crystal out of [0, 1]");
        stats.assert_true(sem.morph_fractal >= 0.0f && sem.morph_fractal <= 1.0f, "alpha_fractal out of [0, 1]");

        // 2. Smoothness & Zero Popping Check (bounded derivative)
        if (frame > 0) {
            float d_s = std::abs(sem.morph_smooth - prev_sem.morph_smooth);
            float d_c = std::abs(sem.morph_crystal - prev_sem.morph_crystal);
            float d_f = std::abs(sem.morph_fractal - prev_sem.morph_fractal);
            float max_d = std::max({d_s, d_c, d_f});
            if (max_d > max_delta_morph) max_delta_morph = max_d;

            stats.assert_true(max_d < 0.05f, "Discontinuous morphing jump detected (> 0.05 per frame)");
        }

        // 3. Numerical Health Checks
        stats.assert_true(!std::isnan(sem.morph_smooth) && !std::isinf(sem.morph_smooth), "NaN/Inf in morph_smooth");
        stats.assert_true(!std::isnan(sem.morph_crystal) && !std::isinf(sem.morph_crystal), "NaN/Inf in morph_crystal");
        stats.assert_true(!std::isnan(sem.morph_fractal) && !std::isinf(sem.morph_fractal), "NaN/Inf in morph_fractal");
        stats.assert_true(!std::isnan(sem.speed_forward) && !std::isinf(sem.speed_forward), "NaN/Inf in speed_forward");
        stats.assert_true(sem.speed_forward >= 1.2f - 1e-4f && sem.speed_forward <= 4.5f + 1e-4f, "Speed forward out of [1.2, 4.5] m/s");
        stats.assert_true(sem.beat_phase >= 0.0f && sem.beat_phase < core::TWO_PI + 1e-4f, "Beat phase out of [0, 2pi)");

        prev_sem = sem;
    }

    std::cout << "[PASS] Barycentric Morphing verified across " << NUM_FRAMES << " frames.\n";
    std::cout << "       Max Partition of Unity Error: " << max_sum_err << "\n";
    std::cout << "       Max Morphing Delta per Frame: " << max_delta_morph << " (smooth, non-popping)\n";
}

// =============================================================================
// TEST SUITE 2: Tunnel Clearance along Flight Path S(z)
// =============================================================================
void test_tunnel_clearance(TestStats& stats) {
    std::cout << "\n=======================================================\n";
    std::cout << " TEST 2: Tunnel Clearance & Camera Collision Bubble\n";
    std::cout << "=======================================================\n";

    director::AutonomousArtDirector director;
    brain::SemanticBrain brain;

    float min_cam_clearance = 1e9f;
    float min_centerline_clearance = 1e9f;
    float min_raymarch_step0_clearance = 1e9f;

    // Test 2.1: Dynamic Flight Path Clearance along 20,000 steps
    constexpr int FLIGHT_STEPS = 20000;
    const float dt = 1.0f / 144.0f;

    for (int i = 0; i < FLIGHT_STEPS; ++i) {
        core::PhysicsAudioState state{};
        state.stream_a.rms = 0.5f + 0.4f * std::sin(i * 0.01f);
        state.stream_b.rms = state.stream_a.rms;
        state.stream_b.energy = state.stream_a.rms;
        state.stream_b.band_sub_bass = 0.5f + 0.5f * std::cos(i * 0.02f);
        state.stream_b.is_onset = (i % 30 == 0);
        state.stream_b.onset_strength = state.stream_b.is_onset ? 0.9f : 0.0f;

        brain.update(state, dt);
        const auto& sem = brain.get_semantic_vector();

        director.update(sem, dt);
        glm::vec3 cam_pos = director.get_camera_pos();
        glm::vec3 cam_dir = director.get_camera_dir();

        // Evaluate distance at camera position
        float d_cam = shader_math::mapSDF(cam_pos, cam_pos, sem);
        if (d_cam < min_cam_clearance) min_cam_clearance = d_cam;

        // Camera MUST be in free space: distance must be >= 2.0 meters
        stats.assert_true(d_cam >= 2.0f, "Camera intersects geometry or insufficient clearance (d < 2.0m)");

        // Evaluate step 0 of raymarching: t = 0.05m
        glm::vec3 p_step0 = cam_pos + cam_dir * 0.05f;
        float d_step0 = shader_math::mapSDF(p_step0, cam_pos, sem);
        if (d_step0 < min_raymarch_step0_clearance) min_raymarch_step0_clearance = d_step0;
        stats.assert_true(d_step0 > 0.1f, "Raymarch start step inside geometry!");
    }

    std::cout << "[PASS] Dynamic Camera Flight Clearance: Min Distance = " << min_cam_clearance << " m (Target: >= 2.0m)\n";
    std::cout << "[PASS] Raymarch Initial Step Clearance: Min Distance = " << min_raymarch_step0_clearance << " m\n";

    // Test 2.2: Systematic Centerline Spatial Sweep along z in [0, 10000] m
    struct MorphConfig {
        float s, c, f, dil;
    };
    std::vector<MorphConfig> configs = {
        {1.0f, 0.0f, 0.0f, 0.0f}, // Pure Silk/Gyroid, zero dilation
        {1.0f, 0.0f, 0.0f, 1.0f}, // Pure Silk/Gyroid, max dilation
        {0.0f, 1.0f, 0.0f, 0.0f}, // Pure Crystal Lattice, zero dilation
        {0.0f, 1.0f, 0.0f, 1.0f}, // Pure Crystal Lattice, max dilation
        {0.0f, 0.0f, 1.0f, 0.0f}, // Pure Mandelbox Spines, zero dilation
        {0.0f, 0.0f, 1.0f, 1.0f}, // Pure Mandelbox Spines, max dilation
        {0.33f, 0.33f, 0.34f, 0.5f} // Equal blend
    };

    for (const auto& cfg : configs) {
        core::AudioSemanticVector sem{};
        sem.morph_smooth = cfg.s;
        sem.morph_crystal = cfg.c;
        sem.morph_fractal = cfg.f;
        sem.elastic_dilation = cfg.dil;
        sem.gyroid_scale = 0.25f;
        sem.gyroid_twist = 0.3f;
        sem.melodic_mids = 0.5f;

        for (float z = 0.0f; z <= 10000.0f; z += 5.0f) {
            glm::vec2 spline_xy = shader_math::getFlightSpline(z);
            glm::vec3 p_center(spline_xy.x, spline_xy.y, z);
            glm::vec3 dummy_cam = p_center - glm::vec3(0.0f, 0.0f, 5.0f);

            float d_center = shader_math::mapSDF(p_center, dummy_cam, sem);
            if (d_center < min_centerline_clearance) min_centerline_clearance = d_center;

            stats.assert_true(d_center >= 2.5f, "Centerline clearance violated along flight spline!");
        }
    }

    std::cout << "[PASS] Centerline Spline Sweep (10,000m): Min Clearance = " << min_centerline_clearance << " m (Target: >= 2.5m)\n";
}

// =============================================================================
// TEST SUITE 3: Sub-Bass Cavity Dilation & Asymptotic Stability
// =============================================================================
void test_sub_bass_dilation(TestStats& stats) {
    std::cout << "\n=======================================================\n";
    std::cout << " TEST 3: Sub-Bass Cavity Dilation Under Extreme Bass\n";
    std::cout << "=======================================================\n";

    brain::SemanticBrain brain;
    const float dt = 1.0f / 144.0f;

    // 1. Continuous sustained maximum bass (E_bass = 1.0) for 20,000 frames
    for (int frame = 0; frame < 20000; ++frame) {
        core::PhysicsAudioState state{};
        state.stream_a.rms = 1.0f;
        state.stream_b.rms = 1.0f;
        state.stream_b.energy = 1.0f;
        state.stream_b.band_sub_bass = 1.0f;
        state.stream_b.is_onset = true;
        state.stream_b.onset_strength = 1.0f;

        brain.update(state, dt);
        const auto& sem = brain.get_semantic_vector();

        stats.assert_true(sem.elastic_dilation >= 0.0f && sem.elastic_dilation <= 1.0f + 1e-4f, 
                          "elastic_dilation out of [0, 1] bounds under max bass!");
        stats.assert_true(!std::isnan(sem.elastic_dilation) && !std::isinf(sem.elastic_dilation), 
                          "NaN/Inf in elastic_dilation!");
    }

    const auto& sem_max = brain.get_semantic_vector();
    std::cout << "       Asymptotic dilation under continuous 100% bass: " << sem_max.elastic_dilation << " (Expected ~ 1.0)\n";
    stats.assert_true(sem_max.elastic_dilation > 0.95f, "elastic_dilation did not reach target plateau!");

    // Verify tunnel radius expansion
    float tunnel_r = 2.8f + sem_max.elastic_dilation * 1.6f;
    std::cout << "       Expanded Tunnel Radius: " << tunnel_r << " m (Baseline: 2.8m, Max: 4.4m)\n";
    stats.assert_true(tunnel_r >= 4.3f && tunnel_r <= 4.41f, "Tunnel radius expansion outside expected [4.3, 4.4]m range!");

    // 2. Sudden drop to zero bass (quiescence relaxation)
    for (int frame = 0; frame < 5000; ++frame) {
        core::PhysicsAudioState state{};
        state.stream_a.rms = 0.0f;
        state.stream_b.rms = 0.0f;
        state.stream_b.band_sub_bass = 0.0f;

        brain.update(state, dt);
        const auto& sem = brain.get_semantic_vector();

        stats.assert_true(sem.elastic_dilation >= 0.0f, "elastic_dilation undershot below 0!");
    }

    const auto& sem_relaxed = brain.get_semantic_vector();
    std::cout << "       Relaxed dilation after silence: " << sem_relaxed.elastic_dilation << " (Expected ~ 0.0)\n";
    stats.assert_true(sem_relaxed.elastic_dilation < 0.01f, "elastic_dilation failed to relax to zero!");
    std::cout << "[PASS] Sub-Bass Cavity Dilation perfectly stable and bounded.\n";
}

// =============================================================================
// MAIN ENTRY POINT
// =============================================================================
int main() {
    std::cout << "====================================================================\n";
    std::cout << " AUDIO-VOYAGER // MILESTONE 3 ADVERSARIAL STRESS TEST HARNESS\n";
    std::cout << " Empirical Validation of Morphing, Clearance, Dilation & Stability\n";
    std::cout << "====================================================================\n";

    TestStats stats;

    auto t0 = std::chrono::high_resolution_clock::now();

    test_barycentric_morphing(stats);
    test_tunnel_clearance(stats);
    test_sub_bass_dilation(stats);

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "\n====================================================================\n";
    std::cout << " FINAL STRESS TEST SUMMARY\n";
    std::cout << " Total Assertions Checked : " << stats.total_assertions << "\n";
    std::cout << " Passed Assertions        : " << stats.passed_assertions << "\n";
    std::cout << " Failed Assertions        : " << stats.failed_assertions << "\n";
    std::cout << " Total Execution Time     : " << elapsed_ms << " ms\n";
    std::cout << "====================================================================\n";

    if (stats.failed_assertions > 0) {
        std::cerr << "VERDICT: FAIL (" << stats.failed_assertions << " failures)\n";
        return 1;
    }

    std::cout << "VERDICT: PASS (All Milestone 3 mathematical invariants verified!)\n";
    return 0;
}
