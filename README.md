# AUDIO-VOYAGER

## Autonomous Continuous SDF Raymarching & Real-Time Spectral Machine Learning Engine

`audio-voyager` is a native, high-performance C++20 audio-reactive generative engine designed for real-time visualization through Signed Distance Field (SDF) raymarching, embedded neural spectral inference, and modern OpenGL 4.5 Core Profile graphics.

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
           |  Embedded Neural MLP      |                       |    Real-Time DSP Worker   |
           +-------------+-------------+                       +-------------+-------------+
                         |                                                   |
                         |  Barycentric Biome Weights                        |  Kinetic Excitation
                         |  (Liquid / Crystal / Cyber)                       |  (Dilation, Shockwaves,
                         |  Affective Coordinates                            |   Volumetric HDR Pulse)
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
                               |      HDR Dual-Pass Optical Bloom      |
                               |    ACES Filmic Tone Mapping (1080p)   |
                               +---------------------------------------+
```

---

## 2. Core Subsystems

### 2.1 The Mind: Embedded Neural Semantic Classifier (`SemanticClassifierML`)
- **Feature Extraction**: 28-dimensional spectral vector comprising a 24-band Mel-frequency filterbank energy distribution, spectral centroid normalized ratio, inharmonicity/dissonance index, and transient onset novelty density.
- **Inference Model**: Multi-Layer Perceptron (MLP) with Gaussian Error Linear Unit (GELU) non-linearities and numerically stabilized Softmax normalization:
  $$\mathbf{w}_{\text{target}} = \text{Softmax}\left(\mathbf{W}_3 \cdot \text{GELU}(\mathbf{W}_2 \cdot \text{GELU}(\mathbf{W}_1 \mathbf{x} + \mathbf{b}_1) + \mathbf{b}_2) + \mathbf{b}_3\right)$$
- **Temporal Integration**: Evaluated at $1.0\,\text{Hz}$ and integrated through a higher-order leaky integrator ($\tau \approx 4.0\,\text{s}$), ensuring structural biome transitions occur strictly across musical phrase boundaries rather than frame-by-frame fluctuations.

### 2.2 The Muscle: High-Frequency Physical DSP Pipeline
- **Stream A (Fast-Path Reactivity)**: Dual-stream ring buffer processing 256 PCM oscilloscope samples and an 8-band Fast-FFT filterbank with latency $< 0.03\,\text{ms}$.
- **Stream B (Physical Forces)**: Essentia C++ and native C++20 DSP worker evaluating Sethares dissonance, High-Frequency Content (HFC) onset novelty, and RMS acoustic energy.
- **Physical Excitation Mapping**:
  - **Sub-Bass Energy ($20-120\,\text{Hz}$)**: Modulates the cavern boundary radius through an elastic dilation function $\delta(r) = r_0 + \kappa \cdot E_{\text{bass}}$.
  - **Onset Transients**: Generates a decaying sinusoidal shockwave perturbation over surface distance evaluations.
  - **RMS Kinetic Mass**: Directly drives volumetric emission density and specular highlight intensity.

### 2.3 GPU SDF Raymarching Architecture (`shaders/raymarching.frag`)
The geometry is computed entirely in screen-space via three structurally distinct signed distance functions:

1. **`SDF_Liquid` (Organic / Harmonious State)**:
   - Undulating minimal surface cavity combined with viscous metaball configurations and broad polynomial smooth minimum operations:
     $$\text{smin}(a, b, k) = \text{mix}(b, a, h) - k \cdot h \cdot (1 - h), \quad h = \text{clamp}\left(0.5 + 0.5 \frac{b - a}{k}, 0.0, 1.0\right)$$
2. **`SDF_Crystal` (Tension / Distortion State)**:
   - Faceted octagonal prism corridors, quartz monoliths, and Kaleidoscopic Iterated Function Systems (KIFS) utilizing planar clipping operations (`max`, `abs`).
3. **`SDF_Cyber` (Electronic / Rhythmic State)**:
   - Chamfered rectangular conduits, periodic gate ring superstructures ($z \pmod{5.0}$), and longitudinal neon energy rails.

- **Barycentric Trilateral Blending**:
  $$d_{\text{world}}(p) = w_{\text{liquid}} \cdot d_{\text{liquid}}(p) + w_{\text{crystal}} \cdot d_{\text{crystal}}(p) + w_{\text{cyber}} \cdot d_{\text{cyber}}(p)$$

---

## 3. Building and Execution

### 3.1 Precompiled Standalone Distribution (Windows 64-bit)
The portable package is located in `dist/audio-voyager-windows/` and includes static dependencies.

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
| **`F12`** | Toggle Real-Time Semantic Telemetry Overlay (Dear ImGui) |
| **`ESC`** | Graceful engine shutdown |

---

## 5. Technical Specifications

- **Language Standard**: ISO C++20 (`-std=c++20`).
- **Graphics API**: OpenGL 4.5 Core Profile (`GLSL 450 core`).
- **Framebuffer Architecture**: Multi-target HDR (`GL_RGBA16F`), dual-pass Gaussian Bloom downsampling, ACES Filmic Tone Mapping.
- **Audio Thread Safety**: Wait-free, lock-free SPSC circular ring buffers.
- **Target Performance**: $140+\,\text{FPS}$ at $1920 \times 1080$ on modern GPU hardware.
