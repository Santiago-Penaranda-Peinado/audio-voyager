#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <array>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace audio_voyager::core {
constexpr double PI = 3.14159265358979323846;
constexpr float PI_F = 3.14159265358979323846f;

// =============================================================================
// Audio Pipeline Constants
// =============================================================================
constexpr uint32_t DEFAULT_SAMPLE_RATE = 48000;
constexpr uint32_t DEFAULT_CHANNELS = 2;
constexpr size_t RAW_OSCILLOSCOPE_SAMPLES = 256;
constexpr size_t FFT_SIZE_STREAM_A = 1024;
constexpr size_t FFT_BANDS_COUNT = 8;
constexpr size_t ANALYSIS_FRAME_SIZE_STREAM_B = 1024;
constexpr size_t ANALYSIS_HOP_SIZE_STREAM_B = 256;

// =============================================================================
// Stream A: Raw Reactivity Snapshot (Fast Path - Oscilloscope + FFT Bands)
// =============================================================================
struct StreamASnapshot {
    uint64_t timestamp_us{0};
    std::array<float, RAW_OSCILLOSCOPE_SAMPLES> waveform{};
    std::array<float, FFT_BANDS_COUNT> spectrum_bands{};
    float peak_amplitude{0.0f};
    float rms{0.0f};
    float latency_ms{0.0f};
};

// =============================================================================
// Stream B: Soul Physics Snapshot (Essentia C++ DSP - Semantic Descriptors)
// =============================================================================
struct StreamBSnapshot {
    uint64_t timestamp_us{0};
    float spectral_centroid_hz{0.0f};
    float spectral_centroid_norm{0.0f};
    float dissonance{0.0f};
    float onset_strength{0.0f};
    bool is_onset{false};
    float energy{0.0f};
    float rms{0.0f};
    float compute_time_us{0.0f};
};

// =============================================================================
// Complete Telemetry State for Dual-Stream Audio Pipeline
// =============================================================================
struct PhysicsAudioState {
    uint64_t frame_index{0};
    StreamASnapshot stream_a;
    StreamBSnapshot stream_b;
};

// =============================================================================
// Phase 5: Auto-Calibrated & Purely Procedural State Space (AGC Normalized)
// =============================================================================
struct AudioSemanticVector {
    // Dynamic AGC-Normalized Signals [0.0, 1.0] (Auto-Gain Calibrated over 12s)
    float norm_dissonance{0.0f};     // Full 0.0 to 1.0 range regardless of song mastering
    float norm_centroid{0.0f};       // Spectral Center of Mass [0: Sub-bass Warmth -> 1: Air/Treble Brilliance]
    float norm_energy{0.0f};         // Dynamic acoustic mass / RMS volume [0.0 to 1.0]
    float norm_sub_bass{0.0f};       // Sub-bass kick & rumble [0.0 to 1.0]
    float norm_treble{0.0f};         // High-frequency air & screams [0.0 to 1.0]

    // Rhythm & Transients
    float bpm{120.0f};
    float bpm_confidence{0.8f};
    float onset_strength{0.0f};
    bool is_onset{false};

    // Autonomous Navigation & Topology Control
    float speed_forward{3.0f};       // Z-axis flight velocity
    float topology_folding{0.0f};    // KIFS / Gyroid space folding intensity [0 = fluid liquid, 1 = razor crystal]
    float cavity_scale{1.0f};        // Space scale & cavity dilation
    float glitch_intensity{0.0f};    // Drop shockwave & CRT scanline distortion
    float speed_lines{0.0f};         // 2D kinetic anime warp streaks
};

// =============================================================================
// Director's Debug & Override Tuners (Accessed via F12)
// =============================================================================
struct PhysicsTuners {
    bool show_hud{false};
    bool fullscreen{false};
    float speed_multiplier{1.0f};
    float bloom_intensity{1.5f};
    float chromatic_aberration{0.018f};
};

inline const std::array<std::string, FFT_BANDS_COUNT> FFT_BAND_NAMES = {
    "Sub-Bass (20-60Hz) ",
    "Bass (60-250Hz)    ",
    "Low-Mid (250-500Hz)",
    "Mid (500-2kHz)     ",
    "High-Mid (2k-4kHz) ",
    "Presence (4k-6kHz) ",
    "Brilliance (6k-12k)",
    "Air (12k-20kHz)    "
};

} // namespace audio_voyager::core
