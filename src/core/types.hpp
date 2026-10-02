#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <numbers>
#include <glm/glm.hpp>

namespace audio_voyager::core {

// Mathematical constants
inline constexpr float PI = 3.14159265358979323846f;
inline constexpr float TWO_PI = 6.28318530717958647692f;

// Audio Engine Constants
constexpr uint32_t DEFAULT_SAMPLE_RATE = 48000;
constexpr uint32_t DEFAULT_CHANNELS = 2;
constexpr size_t RAW_OSCILLOSCOPE_SAMPLES = 256;
constexpr size_t FFT_SIZE_STREAM_A = 1024;
constexpr size_t FFT_BANDS_COUNT = 8;
constexpr size_t ANALYSIS_FRAME_SIZE_STREAM_B = 1024;
constexpr size_t ANALYSIS_HOP_SIZE_STREAM_B = 256;

inline const std::array<std::string, 8> FFT_BAND_NAMES = {
    "Sub-Bass", "Bass", "Low-Mid", "Mid", "High-Mid", "Presence", "Treble", "Air"
};

// Stream A Snapshot (Ultra low-latency raw audio buffers, < 0.05 ms latency)
struct StreamASnapshot {
    std::array<float, RAW_OSCILLOSCOPE_SAMPLES> waveform{};
    std::array<float, FFT_BANDS_COUNT> spectrum_bands{};
    float rms{0.0f};
    float peak_amplitude{0.0f};
    float latency_ms{0.0f};
    uint64_t timestamp_us{0};
    uint64_t frame_index{0};
};

using RawAudioFrame = StreamASnapshot;

// Stream B Snapshot (High-precision physical features from DSP engine)
struct StreamBSnapshot {
    float spectral_centroid_hz{0.0f};
    float spectral_centroid_norm{0.0f};
    float dissonance{0.0f};
    float spectral_flatness{0.0f};
    float onset_strength{0.0f};
    bool is_onset{false};
    float energy{0.0f};
    float rms{0.0f};
    float dynamic_gain{1.0f};
    float compute_time_us{0.0f};
    uint64_t timestamp_us{0};

    // Multi-band frequency decomposition
    float band_sub_bass{0.0f};   // 20 - 80 Hz
    float band_bass{0.0f};       // 80 - 250 Hz
    float band_mids{0.0f};       // 250 - 2500 Hz (Vocals / Melody)
    float band_treble{0.0f};     // 2500 - 8000 Hz (Percussion / Snares)
    float band_air{0.0f};        // 8000 - 20000 Hz (Sparkle / Sibilance)
};

// Combined Physics Audio State
struct PhysicsAudioState {
    StreamASnapshot stream_a{};
    StreamBSnapshot stream_b{};
    uint64_t frame_index{0};
    uint64_t timestamp_us{0};
};

// Continuous Semantic Vector for Raymarching Biomes and Autonomous Direction
struct AudioSemanticVector {
    // 1. THE MIND: Continuous Biome Weights (Inferred via ML, Sum = 1.0)
    float weight_liquid{0.25f};   // P(Jazz, Lofi, Ambient, Silk Ocean)
    float weight_metal{0.25f};    // P(Metal, Heavy Rock, Hand of Blood, Obsidian Spire Chasm)
    float weight_crystal{0.25f};  // Alias/Tension: synced with weight_metal
    float weight_cyber{0.25f};    // P(Electronic, Techno, Synthwave, Cyber Highway)
    float weight_dubstep{0.25f};  // P(Dubstep, Speedcore, Skrillex, Camellia, Quantum Bass Void)
    float valence{0.5f};          // Emotional Valence [0.0 = dark/tense, 1.0 = bright/euphoric]
    float arousal{0.5f};          // Physiological Arousal [0.0 = calm, 1.0 = intense]

    // 2. THE MUSCLE: Multi-Band Physical Excitations (Evaluated at 144 Hz)
    float elastic_dilation{0.0f};  // Sub-Bass cavity & wave dilation
    float surface_ripple{0.0f};    // Onset shockwave ripple on surfaces
    float melodic_mids{0.0f};      // Mid-frequency vocal/melody deformation
    float treble_sparkle{0.0f};    // High-frequency particle sparkle & specular
    float emission_pulse{1.0f};    // Volumetric HDR glow pulse
    float norm_centroid{0.5f};     // Synesthetic HSV base color
    bool is_onset{false};          // Transient beat drop
    bool is_silent{false};         // Quiescence / Silence state

    // 3. Autonomous Kinematics
    float speed_forward{1.3f};     // Camera cruising speed (m/s)
    float camera_roll{0.0f};       // Camera bank / roll angle (radians)
    float bpm{120.0f};             // Detected Tempo
    float bpm_confidence{0.5f};    // Tempo Confidence (decays to 0 on silence)
};

// Runtime GUI tuners (F12 Secret HUD)
struct PhysicsTuners {
    float damping{0.985f};
    float gravity_scale{1.0f};
    float vorticity_scale{1.0f};
    float shockwave_scale{1.0f};
    float attraction_scale{1.0f};
    float point_size_scale{1.0f};
    float decay_rate{4.5f};

    float bloom_intensity{0.30f};
    float chromatic_aberration{0.015f};
    float speed_multiplier{1.0f};

    std::array<float, 3> color_base{0.9f, 0.6f, 0.2f};
    std::array<float, 3> color_peak{0.4f, 0.8f, 1.0f};

    bool show_hud{false};
    bool fullscreen{false};
};

} // namespace audio_voyager::core
