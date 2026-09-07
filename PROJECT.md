# Project: Audio-Voyager

## Architecture
Audio-Voyager is a real-time synesthetic 3D audio visualizer built in C++20 and modern OpenGL (4.3+ Core Profile) targeting 140+ FPS performance on modern GPUs.
- **Audio DSP Engine**: Dual-stream pipeline at 144 Hz ("El Músculo" Stream A fast path & "El Cerebro" Stream B psychoacoustics) in `src/audio/`.
- **Semantic Brain**: Continuous temporal smoothing, mood classification, and state mapping in `src/brain/`.
- **Autonomous Art Director & Camera**: 6DoF camera kinematics with 2nd-order critically damped mass-spring-damper physics, aerodynamic banking, and continuous spatial navigation in `src/director/`.
- **Render Engine & Shaders**: Raymarched SDF TPMS (gyroid) procedural topology, Beer-Lambert volumetric lighting, thresholded bloom, quadratic light attenuation, and calibrated ACES filmic tonemapping in `src/graphics/` and `shaders/`.
- **Compute Particle Simulation**: 1,000,000 GPU particles simulated via compute shaders in `shaders/particle_physics.comp`.

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | F1.1 Mass-Spring-Damper Filter | 2nd-order critically damped filter ($\zeta = 1.0$) for position, orientation, pitch, roll, and FOV | M1 | ORIGINAL_REQUEST §R1 |
| 2 | F1.2 100% Audio-Driven Camera | Purely acoustic modulation; eliminate hardcoded shakes, stroboscopic jumps, and scripted animations | M1 | ORIGINAL_REQUEST §R1 |
| 3 | F1.3 Forward Velocity Clamping | Strict forward cruise speed clamped to $[1.2, 4.5]\text{ m/s}$ across quiet and high-energy passages | M1 | ORIGINAL_REQUEST §R1 |
| 4 | F1.4 Spline Continuity | Stationary 3D flight spline $S(z)$ eliminating modulo-wrapping jumps and roll twitches | M1 | ORIGINAL_REQUEST §R1 |
| 5 | F1.5 Aerodynamic Banking | Curvature-based roll banking and gimbal-safe look-at matrix generation | M1 | ORIGINAL_REQUEST §R1 |
| 6 | F2.1 Beer-Lambert Volumetric Fog | Exponential optical extinction $\Delta t$ and bounded God rays to prevent additive runaway | M2 | ORIGINAL_REQUEST §R2 |
| 7 | F2.2 Thresholded Bloom Filter | Bright-pass threshold isolating peak emissives from ambient fog and mid-tones | M2 | ORIGINAL_REQUEST §R2 |
| 8 | F2.3 Global Auto-Exposure HDR | Frame-averaged temporal luminance adaptation with extended clamp preventing whiteout | M2 | ORIGINAL_REQUEST §R2 |
| 9 | F2.4 Cavity AO & Light Attenuation | Quadratic falloff on entity lights and strict ambient occlusion in deep crevices | M2 | ORIGINAL_REQUEST §R2 |
| 10 | F2.5 ACES Filmic Tonemapping | Fine-tuned curve preserving highlight roll-off and dark geometric definition | M2 | ORIGINAL_REQUEST §R2 |
| 11 | F3.1 144 Hz DSP & Uniform Pipeline | Low-latency audio feature transfer from DSP engine to UBO and render pipelines | M3 | ORIGINAL_REQUEST §R3 |
| 12 | F3.2 Physical Space Deformation | Sub-bass cavity dilation, transient shockwaves, harmonic resonance, and treble sparkles | M3 | ORIGINAL_REQUEST §R3 |
| 13 | F3.3 Continuous Morphological Blending | Barycentric smooth blending between liquid/lofi/jazz and crystalline/fractal/metal geometries | M3 | ORIGINAL_REQUEST §R3 |
| 14 | F3.4 Tunnel Clearance & Particles | Guaranteed flight clearance along spline and reactive 1M particle compute simulation | M3 | ORIGINAL_REQUEST §R3 |
| 15 | F4.1 Docker Build Verification | Clean build via `docker compose run --rm audio-voyager bash /workspace/scripts/build_windows_exe.sh` | M4 | ORIGINAL_REQUEST §Acceptance |
| 16 | F4.2 E2E & Adversarial Hardening | Comprehensive test coverage across silence, jazz/lofi, and metal/speedcore audio profiles | M4 | ORIGINAL_REQUEST §Acceptance |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | Camera Kinematics & 6DoF Inertia (R1) | `AutonomousArtDirector`, `SemanticBrain` speed/roll, `CriticallyDampedSpring` | Survey Complete | DONE |
| M2 | HDR Exposure, Volumetric Fog & Contrast (R2) | `shaders/raymarching.frag`, `shaders/bloom_blur.frag`, `shaders/postprocess.frag`, `postprocess.cpp` | Survey Complete | DONE |
| M3 | Audio Reactivity & Procedural Topology (R3) | `shaders/raymarching.frag` (SDFs & morphing), `src/brain/`, `src/audio/`, `renderer.cpp` | M1, M2 | DONE |
| M4 | Final Milestone: E2E Verification & Docker Build | E2E test suite pass (Tiers 1-4) + Adversarial hardening (Tier 5) + Docker MinGW build | M1, M2, M3 | IN_PROGRESS |

## Interface Contracts
### Audio DSP ↔ Semantic Brain (`src/audio/audio_types.hpp`, `src/brain/semantic_brain.hpp`)
- Input: `AudioAnalysisState` containing `stream_a` (144 Hz fast RMS, sub-bass, kick, snare, energy, spectral flux) and `stream_b` (psychoacoustic brightness, complexity, harmonic chroma, tempo, beat phase).
- Output: `AudioSemanticVector` with smooth normalized parameters (`sub_bass_energy`, `kick_transient`, `melodic_mids`, `treble_sparkle`, `speed_forward`, `target_fov`, `target_pitch`, `target_roll`, `morph_weights`).

### Semantic Brain ↔ Autonomous Art Director (`src/director/autonomous_art_director.hpp`)
- Input: `AudioSemanticVector`, `dt` (frame delta time).
- State: 2nd-order critically damped springs for position offsets $(x, y)$, forward speed $v_z \in [1.2, 4.5]$, pitch $\theta$, roll $\phi$, and FOV.
- Output: `glm::vec3 camera_pos`, `glm::vec3 camera_dir`, `glm::vec3 camera_up`, `float fov_degrees`, `glm::mat4 view_matrix`, `glm::mat4 proj_matrix`.

### Autonomous Art Director & Brain ↔ Renderer UBO (`src/graphics/renderer.cpp`, `shaders/raymarching.frag`, `shaders/postprocess.frag`)
- UBO `CameraBlock`: `vec3 u_camera_pos`, `vec3 u_camera_dir`, `vec3 u_camera_up`, `mat4 u_inv_view_proj`.
- UBO `AudioBlock`: `float u_sub_bass`, `float u_kick`, `float u_melodic_mids`, `float u_treble`, `float u_shockwave_radius`, `float u_morph_smooth`, `float u_morph_fractal`, `float u_morph_crystal`, `float u_space_density`.
- Postprocess Uniforms: `u_bloom_threshold`, `u_bloom_intensity`, `u_adapted_luminance`.

## Code Layout
- `src/audio/`: 144 Hz DSP engine, FFT, transient detection, dual-stream psychoacoustics.
- `src/brain/`: Semantic brain, temporal smoothing, mood classification.
- `src/director/`: Autonomous art director, 6DoF camera kinematics, spline navigation.
- `src/graphics/`: OpenGL renderer, shader compilation, UBO management, postprocessing pipeline.
- `shaders/`:
  - `raymarching.frag`: SDF raymarching, TPMS gyroids, fractal/crystal morphing, Beer-Lambert fog, lighting.
  - `bloom_blur.frag`: Thresholded Gaussian bloom downsampling & ping-pong blur.
  - `postprocess.frag`: ACES filmic tonemapping, global temporal auto-exposure, chromatic aberration, vignette.
  - `particle_physics.comp`: 1M particle compute simulation.
- `scripts/build_windows_exe.sh`: MinGW-w64 cross-compilation in Docker.
