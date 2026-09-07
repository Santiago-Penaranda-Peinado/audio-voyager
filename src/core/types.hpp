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
constexpr size_t FFT_SIZE_STREAM_A = 512;
constexpr size_t FFT_BANDS_COUNT = 8;
constexpr size_t ANALYSIS_FRAME_SIZE_STREAM_B = 1024;
constexpr size_t ANALYSIS_HOP_SIZE_STREAM_B = 256;

inline const std::array<std::string, 8> FFT_BAND_NAMES = {
    "Sub-Bass", "Bass", "Low-Mid", "Mid", "High-Mid", "Presence", "Treble", "Air"
};

// Stream A Snapshot (Ultra low-latency raw audio buffers, < 0.03 ms latency)
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
    float spectral_flatness{0.0f};     // Wiener entropy: 0.0 = pure tone, 1.0 = white noise/distortion
    float onset_strength{0.0f};
    bool is_onset{false};
    float energy{0.0f};
    float rms{0.0f};
    float dynamic_gain{1.0f};
    float compute_time_us{0.0f};
    uint64_t timestamp_us{0};

    // Multi-band frequency decomposition (from 1024-pt FFT, 46.875 Hz/bin)
    float band_sub_bass{0.0f};   // Bins 1-4:   47 - 188 Hz (skipping DC!)
    float band_bass{0.0f};       // Bins 4-11:  188 - 516 Hz
    float band_mids{0.0f};       // Bins 11-64: 516 - 3000 Hz (Vocals / Melody)
    float band_treble{0.0f};     // Bins 64-171: 3000 - 8000 Hz
    float band_air{0.0f};        // Bins 171-427: 8000 - 20000 Hz
};

// Combined Physics Audio State
struct PhysicsAudioState {
    StreamASnapshot stream_a{};
    StreamBSnapshot stream_b{};
    uint64_t frame_index{0};
    uint64_t timestamp_us{0};
};

// Continuous 5-Biome Semantic Vector for Raymarching and Art Direction
struct AudioSemanticVector {
    // 1. THE MIND: Continuous 5-Biome Barycentric Weights (Sum = 1.0)
    float weight_ocean{0.20f};    // Biome 1: Silk Ocean & Bioluminescent Dew (Lofi / Jazz / Ambient)
    float weight_metal{0.20f};    // Biome 2: Obsidian Fracture & Magma Spikes (Metal / Hard Rock)
    float weight_cyber{0.20f};    // Biome 3: Equalizer Skyscraper City & Arches (Dubstep / EDM / Techno)
    float weight_ethereal{0.20f}; // Biome 4: Celestial Golden Rings & Obelisks (Symphonic / Dreamy)
    float weight_funk{0.20f};     // Biome 5: Psychedelic Jelly Floor & Metaballs (Acid Funk / Electro)

    // Affective Coordinates
    float valence{0.5f};          // Emotional warmth / harmonic consonance
    float arousal{0.5f};          // Kinetic intensity / arousal
    float norm_centroid{0.5f};    // Synesthetic chromatic hue [0, 1]

    // 2. THE MUSCLE: Multi-Band Physical Excitations (@ 144 Hz DSP)
    float elastic_dilation{0.0f}; // Sub-Bass cavity & wave dilation (20-80Hz)
    float surface_ripple{0.0f};   // Onset shockwave ripple on surfaces
    float melodic_mids{0.0f};     // Mid-frequency vocal/melody deformation
    float treble_sparkle{0.0f};   // High-frequency sparkle & specular
    float emission_pulse{1.0f};   // Volumetric HDR glow pulse
    bool is_onset{false};
    bool is_silent{false};

    // 3. Autonomous Kinematics (Warp Flight)
    float speed_forward{1.4f};    // Forward cruising speed (1.2 -> 4.2 m/s)
    float camera_roll{0.0f};      // Smooth aerodynamic banking roll
    float bpm{120.0f};
    float bpm_confidence{0.5f};
    float beat_phase{0.0f};       // Accumulated beat phase [0, 2*PI), locked to tempo
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

    float bloom_intensity{0.45f};
    float chromatic_aberration{0.015f};
    float speed_multiplier{1.0f};

    std::array<float, 3> color_base{0.9f, 0.6f, 0.2f};
    std::array<float, 3> color_peak{0.4f, 0.8f, 1.0f};

    bool show_hud{false};
    bool fullscreen{false};
};

} // namespace audio_voyager::core
