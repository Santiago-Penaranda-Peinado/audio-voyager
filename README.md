# AUDIO-VOYAGER

## Autonomous Continuous SDF Raymarching & Real-Time Spectral Machine Learning Engine

`audio-voyager` is a native, high-performance C++20 audio-reactive generative engine designed for real-time visualization through Signed Distance Field (SDF) raymarching, embedded neural spectral inference, and modern OpenGL 4.5 Core Profile graphics.

> [!IMPORTANT]
> **Complete Technical Documentation**: For the exhaustive engineering manual detailing all mathematical formulations, DSP algorithms, 2nd-order physics equations, and guidelines for extending the engine, refer to:
> 📖 [docs/ARCHITECTURE_AND_SYSTEMS_MANUAL.md](docs/ARCHITECTURE_AND_SYSTEMS_MANUAL.md)

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
          | Low-Frequency Domain (~1Hz) |                     | High-Frequency Domain (144Hz) |
          |  Waterfall 2D & Neural ML |                       |    Real-Time DSP Worker   |
          +-------------+-------------+                       +-------------+-------------+
                        |                                                   |
                        |  Barycentric Biome Weights                        |  Kinetic Excitation
                        |  (5 Biomes + Affective Space)                     |  (Dilation, Onset Impulse,
                        |  Autocorrelation BPM & Tempo                      |   Harmonic Veins, Shocks)
                        |                                                   |
                        +-------------------------+-------------------------+
                                                  |
                                                  v
                              +---------------------------------------+
                              |     Autonomous Art Director Engine    |
                              |   2nd-Order Spring-Mass-Damper Model  |
                              |   World-Space Spline Track C(z)       |
                              +-------------------+-------------------+
                                                  |
                                                  v
                              +---------------------------------------+
                              |     Uniform Buffer Object (std140)    |
                              +-------------------+-------------------+
                                                  |
                                                  v
                              +---------------------------------------+
                              |       GPU SDF Raymarching Core        |
                              |      5 Branching Procedural Biomes    |
                              |     Procedural 3D Neon Architecture   |
                              |    HDR Dual-Pass Gaussian Bloom & ACES|
                              +---------------------------------------+
```

---

## 2. Core Subsystems

### 2.1 The Mind: Embedded Neural Semantic Classifier & Waterfall Engine
- **2D Waterfall Spatiotemporal Engine (`WaterfallAnalyzer`)**: Rolling matrix of 64 Mel bands $\times$ 128 temporal frames (~2.7s history) extracting horizontal guitar distortion continuity, 4-on-the-floor kick pulse cadence, and temporal bass flux (LFO wobble). Generates a dynamic `GL_R16F` texture sculpting the 3D raymarching terrain.
- **Embedded Neural Classifier (`SemanticClassifierML`)**: 5-way softmax computing continuous barycentric weights across 5 distinct biomes (Liquid Silk Ocean, Volcanic Obsidian Chasm, Cyber Highway, Quantum Bass Abyss, and Celestial Nebula Stargate) plus Russell's circumplex affective coordinates (Arousal / Valence).
- **Tempo Autocorrelation (`SemanticBrain`)**: 360-sample buffer (~8.0s window) with parabolic sub-sample peak interpolation locking onto musical BPM and triggering "gear-shift" kinetic surges on abrupt tempo drops.

### 2.2 The Muscle: Dual-Stream High-Frequency DSP Pipeline
- **Stream A (Fast-Path Reactivity)**: Ultra-low latency ($< 0.05\,\text{ms}$) circular ring buffer processing 256 PCM oscilloscope samples, 1024-point Fast-FFT into 8 standard musical bands, and Automatic Gain Control (AGC) with soft-knee limiter.
- **Stream B (Physical Forces)**: Native C++20 worker evaluating normalized Sethares dissonance, bounded spectral flatness (300 – 6000 Hz for high-gain guitar discrimination), mid-band crest factor, spectral flux, and High-Frequency Content (HFC) onsets.

### 2.3 Cinematic Director & 2nd-Order Spring-Mass-Damper Physics Engine
- **Harmonic Spring-Mass-Damper Suspension**: 2nd-order differential equation ($\ddot{x} + 2\zeta\omega_n\dot{x} + \omega_n^2(x - x_{\text{target}}) = F$) solved with a 4-substep symplectic Euler integrator. Provides physical suspension bounce on drum kicks, inelastic bump stops, lateral centrifugal sway on turns ($f_{\text{sway}} = +a_{\text{centrifugal}} \cdot 0.45$), and 6DoF chassis banking.
- **World-Space Spline Track $\mathbf{C}(z)$**: Curvature is 100% audio reactive (dead-straight $X=0.00\,\text{m}$ in silence; dynamically curves and dives with musical energy).

### 2.4 GPU SDF Raymarching & 3D Procedural Architecture (`shaders/raymarching.frag`)
- **112-Step Adaptive Sphere Tracing**: Branching biome optimization evaluating only active biomes ($w_i > 0.02$) achieving 145+ FPS on modern GPUs.
- **5 Procedural Biomes**:
  1. *Liquid Silk Ocean*: Harmonious waves and floating mercury dew pearls.
  2. *Volcanic Obsidian Chasm*: Basalt spires and molten magma fissures.
  3. *Cyber Matrix & Neon Highway*: Quantized equalizer towers and neon conduits.
  4. *Quantum Bass Abyss*: Undulating cylindrical chamber with LFO wobble and gravity rings.
  5. *Celestial Nebula Stargate*: Open-air floating crystalline causeway with zero ceiling.
- **Dynamic 3D Procedural Architecture**: Neon gate portals, floating equalizer monoliths, and extruded 3D energy guardrails that emerge dynamically during rhythmic peaks and speed sprints.
- **Safe Spherical Frustum Bubble**: Local spherical clearance eliminating dual-hole / black-hole artifacts on turns.

---

## 3. Building and Execution

### 3.1 Precompiled Standalone Distribution (Windows 64-bit)
The portable package is located in `dist/audio-voyager-windows/` and includes all required runtime DLLs and GLSL shaders.

To launch:
```powershell
.\audio_voyager.exe
```

Or using the batch launcher:
```powershell
.\run_audio_voyager.bat
```

### 3.2 Compilation from Source (Docker / MinGW Toolchain)
```powershell
docker compose run --rm audio-voyager bash /workspace/scripts/build_windows_exe.sh
```

### 3.3 Running Automated Test Suite (22 Unit Tests)
```powershell
docker run --rm -v "${PWD}:/workspace" audio-voyager:latest bash -c \
  "g++ -std=c++20 -O2 -I/workspace/src /workspace/tests/test_audio_voyager.cpp \
   /workspace/src/brain/waterfall_analyzer.cpp /workspace/src/brain/semantic_classifier_ml.cpp \
   /workspace/src/brain/semantic_brain.cpp /workspace/src/director/autonomous_art_director.cpp \
   -o /tmp/test_runner && /tmp/test_runner"
```

---

## 4. Keyboard Controls

| Key | Description |
| :--- | :--- |
| **`F11`** | Toggle Borderless Fullscreen mode |
| **`F12`** | Toggle Real-Time Semantic Telemetry Overlay (Dear ImGui) & WebSDR Waterfall |
| **`ESC`** | Graceful engine shutdown |
| **Mouse Scroll** | Live input sensitivity gain adjustment ($0.1\times - 5.0\times$) |

---

## 5. Technical Specifications

- **Language Standard**: ISO C++20 (`-std=c++20`).
- **Graphics API**: OpenGL 4.5 Core Profile (`GLSL 450 core`).
- **Audio Capture**: Windows WASAPI Loopback via miniaudio (48 kHz, Stereo, non-blocking lock-free).
- **Framebuffer Architecture**: Multi-target HDR (`GL_RGBA16F`), dual-pass Gaussian Bloom downsampling, directional screen-space god rays, ACES Filmic Tone Mapping.
- **Physics Engine**: 2nd-order harmonic spring-mass-damper with symplectic Euler integration.
- **Target Performance**: $140+\,\text{FPS}$ at $1920 \times 1080$ on NVIDIA GeForce RTX hardware.
