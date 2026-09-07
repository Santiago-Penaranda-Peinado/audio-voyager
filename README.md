# AUDIO-VOYAGER

## Autonomous Continuous SDF Raymarching & Real-Time Spectral Machine Learning Engine

`audio-voyager` is a native, ultra-high performance C++20 audio-reactive generative engine designed for real-time visualization through Signed Distance Field (SDF) raymarching, embedded neural spectral inference, and modern OpenGL 4.5 Core Profile graphics.

---

## 1. Architectural Overview

The engine operates on a strict temporal and structural decoupling principle, dividing analysis and rendering into two complementary domains:

```
                               +---------------------------------------+
                               |          Audio Input Stream           |
                               |    (WASAPI Loopback / ALSA / PCM)     |
                               +-------------------+-------------------+
                                                   |
                                                   v
                         +-------------------------+-------------------------+
                         |                                                   |
                         v                                                   v
           +---------------------------+                       +---------------------------+
           |         THE MIND          |                       |        THE MUSCLE         |
           | Low-Frequency Domain (~2.5Hz) |                   | High-Frequency Domain (144Hz) |
           |  Embedded Neural MLP      |                       |    Real-Time DSP Worker   |
           +-------------+-------------+                       +-------------+-------------+
                         |                                                   |
                         |  Continuous Barycentric Weights                   |  Kinetic Multi-Band Forces
                         |  (Liquid / Crystal / Cyber)                       |  (Sub-Bass Dilation, Mids,
                         |  Affective Coordinates                            |   Treble Sparkle, Onsets)
                         |                                                   |
                         +-------------------------+-------------------------+
                                                   |
                                                   v
                               +---------------------------------------+
                               |     Uniform Buffer Object (std140)    |
                               +-------------------+-------------------+
                                                   |
                                                   v
                               +---------------------------------------+
                               |       GPU SDF Raymarching Core        |
                               |   Trilateral Barycentric Biomes       |
                               |    Volumetric Light God Rays (3D)     |
                               |     HDR Dual-Pass Optical Bloom       |
                               |    ACES Filmic Tone Mapping (1080p)   |
                               +-------------------+-------------------+
                                                   |
                                                   v
                               +---------------------------------------+
                               |       Autonomous Art Director         |
                               |      Kinetic Warp Drive (6.5 m/s)     |
                               |     Musical Head-Bobbing Tracking     |
                               +---------------------------------------+
```

---

## 2. Core Subsystems

### 2.1 The Mind: Embedded Neural Semantic Classifier (`SemanticClassifierML`)
- **Relative Spectral Balance**: Rather than arbitrary static thresholds, the engine computes normalized energy proportions across the Mel spectrum:
  $$\text{Mid-to-Bass Ratio} = \frac{E_{\text{mids}} (300-3000\,\text{Hz})}{\max(E_{\text{sub-bass}} (20-150\,\text{Hz}), 0.08)}$$
  $$\text{Air Share} = \frac{E_{\text{air}} (8-20\,\text{kHz})}{E_{\text{total}} + 10^{-4}}$$
- **Softmax Barycentric Normalization**: Continuous probability distribution ($T = 0.85$) ensuring that all 3 biomes blend harmoniously:
  $$w_i = \frac{e^{\text{logit}_i / T}}{\sum_{j} e^{\text{logit}_j / T}}, \quad \sum_{i} w_i = 1.0$$
- **Affective Space**: Computes Valence (pleasantness) and Arousal (physiological energy) using Russell's Circumplex Model.

### 2.2 The Muscle: High-Frequency Multi-Band DSP Pipeline
- **Stream A (Fast-Path Reactivity)**: Lock-free ring buffer processing 256 oscilloscope samples and an 8-band FFT filterbank with latency $< 0.03\,\text{ms}$.
- **Stream B (Physical DSP Forces)**: High-performance C++20 worker thread executing zero-heap-allocation FFTs, Sethares sensory dissonance, and High-Frequency Content (HFC) onset detection.
- **Physical Excitation Mapping**:
  - **Sub-Bass Energy ($20-80\,\text{Hz}$)**: Powers large oceanic wave crests and cavity dilation.
  - **Melodic Vocal Mids ($300-3000\,\text{Hz}$)**: Modulates floating resonant pearls and volumetric atmospheric God Rays.
  - **Treble & Air ($8-20\,\text{kHz}$)**: Drives specular highlights and stellar sparkles.
  - **Transient Onsets**: Injects radial shockwaves into surface distance evaluations:
    $$d_{\text{perturbed}}(p) = d(p) + \sin(4 r - 18 t) \cdot e^{-0.25 r} \cdot \sigma_{\text{ripple}}$$

### 2.3 GPU SDF Raymarching & Volumetric Lighting (`shaders/raymarching.frag`)
The geometry is computed in screen-space via three structurally distinct signed distance functions:

1. **`SDF_Liquid` (Organic / Harmonious Ocean)**:
   - Gerstner-style harmonic multi-octave water surface combined with floating bioluminescent dew spheres:
     $$\text{smin}(a, b, k) = \text{mix}(b, a, h) - k \cdot h \cdot (1 - h), \quad h = \text{clamp}\left(0.5 + 0.5 \frac{b - a}{k}, 0.0, 1.0\right)$$
2. **`SDF_Crystal` (Obsidian Spires & Shattered Tension)**:
   - Lateral colonnade of towering obsidian monoliths ($|x| \ge 6.0\,\text{m}$) and razor quartz octahedrons, ensuring a guaranteed $12.4\,\text{m}$ clear flight corridor.
3. **`SDF_Cyber` (Quantum Matrix Highway)**:
   - Towering audio-reactive equalizer skyscrapers, periodic accelerator gate rings ($z \pmod{8.0}$), and neon energy conduit rails.

- **Volumetric Atmospheric Scattering (God Rays)**:
  $$I_{\text{scatter}}(p) = \frac{1.0}{1.0 + \|p - p_{\text{laser}}\|^2 \cdot 0.08} \cdot (0.8 + 1.2 \cdot E_{\text{mids}})$$

### 2.4 Autonomous Art Director & Kinetic Warp Drive (`AutonomousArtDirector`)
- **Musical Head-Bobbing**: Subtle vertical breathing ($\pm 1.2^\circ$ pitch) locked to the beat.
- **Kinetic Warp Drive**: Dynamic camera velocity scaling from **$0.2\,\text{m/s}$** in silence/breakdowns up to **$6.5\,\text{m/s}$** during explosive drops, with dynamic FOV expansion ($66^\circ \to 88^\circ$).
- **Anti-Submersion Clearance**: Strict height clamp ($y_{\text{cam}} \ge 2.0\,\text{m}$), guaranteeing the camera never clips through terrain or water.

---

## 3. Building and Execution

### 3.1 Precompiled Standalone Distribution (Windows 64-bit)
The portable package is located in `dist/audio-voyager-windows/` and includes all static dependencies and shaders.

To launch:
```powershell
.\audio_voyager.exe
```

For synthetic laboratory test signals:
```powershell
.\audio_voyager.exe --synthetic
```

### 3.2 Compilation from Source (Docker / MinGW Toolchain)
```powershell
docker compose run --rm audio-voyager bash /workspace/scripts/build_windows_exe.sh
```

---

## 4. Keyboard Controls

| Key | Description |
| :--- | :--- |
| **`F11`** | Toggle Borderless Fullscreen mode |
| **`F12`** | Toggle Real-Time Semantic Telemetry Overlay & Optical Post-Process Tuners (Dear ImGui) |
| **`ESC`** | Graceful engine shutdown |

---

## 5. Technical Specifications

- **Language Standard**: ISO C++20 (`-std=c++20`).
- **Graphics API**: OpenGL 4.5 Core Profile (`GLSL 450 core`).
- **Framebuffer Architecture**: Multi-target HDR (`GL_RGBA16F`), dual-pass Gaussian Bloom downsampling, ACES Filmic Tone Mapping.
- **Audio Thread Safety**: Wait-free, lock-free SPSC circular ring buffers with preallocated FFT scratch buffers (zero heap allocations in audio loops).
- **Target Performance**: $145 - 152\,\text{FPS}$ at $1920 \times 1080$ on NVIDIA GeForce RTX 5060.
