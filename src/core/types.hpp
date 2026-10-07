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
constexpr size_t WATERFALL_BANDS = 64;
constexpr size_t WATERFALL_FRAMES = 128;

inline const std::array<std::string, 8> FFT_BAND_NAMES = {
    "Sub-Bass", "Bass", "Low-Mid", "Mid", "High-Mid", "Presence", "Treble", "Air"
};

// Stream A Snapshot (Ultra low-latency raw audio buffers, < 0.05 ms latency)
struct StreamASnapshot {
    std::array<float, RAW_OSCILLOSCOPE_SAMPLES> waveform{};
    std::array<float, FFT_BANDS_COUNT> spectrum_bands{};
    std::array<float, WATERFALL_BANDS> mel_bands{};
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
    float spectral_flatness{0.0f}; // Bounded Spectral Flatness (300 Hz - 6000 Hz active musical guitar range)
    float crest_factor_mids{0.0f}; // Mid-band Crest Factor (Peak vs RMS: distorted guitar < 1.8, jazz > 3.5)
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
    float gear_shift_pulse{0.0f};  // Dynamic kinetic gear shift impulse on sudden rhythm/tempo transitions [0.0 - 1.2]

    // 4. 2D Waterfall Spatiotemporal Pattern Insights
    float waterfall_guitar_continuity{0.0f}; // Horizontal continuity in guitar mids [0, 1]
    float waterfall_kick_regularity{0.0f};   // Vertical 4-on-the-floor kick pulse regularity [0, 1]
    float waterfall_temporal_flux{0.0f};     // Bass temporal modulation/wobble [0, 1]

    // 5. Cohesive Smooth Harmonized Colors
    glm::vec3 color_primary{0.08f, 0.42f, 0.88f};   // Main terrain/geometry body color
    glm::vec3 color_accent{1.00f, 0.72f, 0.22f};    // Secondary/highlights/sparks/laser reflection
    glm::vec3 color_zenith{0.005f, 0.012f, 0.028f}; // Deep atmosphere/sky zenith color
    glm::vec3 color_part_base{0.06f, 0.38f, 0.85f}; // Smooth particle base color
    glm::vec3 color_part_peak{1.00f, 0.75f, 0.25f}; // Smooth particle peak color
};

// Harmonically Tuned Biome Palettes
struct BiomePalette {
    glm::vec3 primary;
    glm::vec3 accent;
    glm::vec3 zenith;
    glm::vec3 particle_base;
    glm::vec3 particle_peak;
};

// Liquid: deep bioluminescent sapphire ocean with warm amber/gold accents
inline const BiomePalette PALETTE_LIQUID = {
    .primary       = glm::vec3(0.08f, 0.42f, 0.88f),
    .accent        = glm::vec3(1.00f, 0.72f, 0.22f),
    .zenith        = glm::vec3(0.012f, 0.025f, 0.048f),
    .particle_base = glm::vec3(0.06f, 0.38f, 0.85f),
    .particle_peak = glm::vec3(1.00f, 0.75f, 0.25f)
};

// Metal: smoldering basalt black and volcanic magma crimson with ember sparks
inline const BiomePalette PALETTE_METAL = {
    .primary       = glm::vec3(0.92f, 0.08f, 0.05f),
    .accent        = glm::vec3(1.00f, 0.45f, 0.05f),
    .zenith        = glm::vec3(0.035f, 0.008f, 0.012f),
    .particle_base = glm::vec3(0.90f, 0.10f, 0.04f),
    .particle_peak = glm::vec3(1.00f, 0.55f, 0.10f)
};

// Cyber: neon indigo/cyan highway with electric magenta accents
inline const BiomePalette PALETTE_CYBER = {
    .primary       = glm::vec3(0.02f, 0.82f, 0.95f),
    .accent        = glm::vec3(1.00f, 0.08f, 0.75f),
    .zenith        = glm::vec3(0.015f, 0.028f, 0.048f),
    .particle_base = glm::vec3(0.02f, 0.80f, 0.95f),
    .particle_peak = glm::vec3(1.00f, 0.06f, 0.80f)
};

// Dubstep: deep ultraviolet void with toxic emerald acid flashes
inline const BiomePalette PALETTE_DUBSTEP = {
    .primary       = glm::vec3(0.55f, 0.05f, 0.95f),
    .accent        = glm::vec3(0.15f, 1.00f, 0.30f),
    .zenith        = glm::vec3(0.025f, 0.010f, 0.048f),
    .particle_base = glm::vec3(0.15f, 1.00f, 0.32f),
    .particle_peak = glm::vec3(0.70f, 0.10f, 1.00f)
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
    float input_gain{1.0f};       // Manual input gain / sensitivity multiplier [0.10x - 5.00x]
    bool agc_enabled{true};       // Automatic Gain Control (Volume normalizer)

    // GPU Compute Particle System Controls
    bool enable_particles{true};  // Master ON/OFF toggle for compute & rendering
    float particle_size{1.0f};    // Particle point sprite size multiplier [0.2x - 3.0x]
    float particle_opacity{0.70f};// Particle alpha opacity multiplier [0.1x - 2.5x]

    std::array<float, 3> color_base{0.9f, 0.6f, 0.2f};
    std::array<float, 3> color_peak{0.4f, 0.8f, 1.0f};

    bool show_hud{false};
    bool fullscreen{false};
};

} // namespace audio_voyager::core
