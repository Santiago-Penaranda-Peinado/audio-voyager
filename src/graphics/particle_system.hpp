#pragma once

#include "core/types.hpp"
#include "graphics/shader.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <cmath>

namespace audio_voyager::graphics {

struct alignas(16) GpuParticle {
    float pos_life[4]; // x, y, z, life (0.0 -> 1.0)
    float vel_mass[4]; // vx, vy, vz, mass
};

struct alignas(16) AudioPhysicsUbo {
    float audio_physics[4];  // x: centroid_norm, y: dissonance, z: onset_strength, w: is_onset
    float audio_energy[4];   // x: rms, y: peak, z: sub_bass, w: high_treble
    float sim_params[4];     // x: dt, y: total_time, z: particle_count, w: damping
    float physics_scales[4]; // x: gravity_scale, y: vorticity_scale, z: shockwave_scale, w: attraction_scale
    float color_base[4];     // rgb: base resting color, w: point_size_scale
    float color_peak[4];     // rgb: peak high energy color, w: opacity_scale
    float cam_pos[4];        // xyz: camera pos, w: melodic_mids
    float laser_pos[4];      // xyz: laser entity pos, w: treble_sparkle
    float cam_dir[4];        // xyz: camera forward dir, w: spare
};

struct LeakyIntegrator {
    float value{0.0f};
    float attack_rate{40.0f};
    float decay_rate{4.5f};

    float update(float target, float dt, float current_decay_rate) noexcept {
        float rate = (target > value) ? attack_rate : current_decay_rate;
        float alpha = 1.0f - std::exp(-rate * dt);
        value += alpha * (target - value);
        return value;
    }
};

class ParticleSystem {
public:
    explicit ParticleSystem(size_t particle_count = 32768);
    ~ParticleSystem();

    // Non-copyable
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    bool init();
    void update(float dt, float total_time, 
                const core::PhysicsAudioState& audio_state, 
                const core::AudioSemanticVector& semantic, 
                const glm::vec3& cam_pos,
                const glm::vec3& cam_dir,
                const glm::vec3& laser_pos,
                float particle_size = 1.0f,
                float particle_opacity = 0.70f);
    void render(const glm::mat4& view_proj);

    [[nodiscard]] size_t get_particle_count() const noexcept { return particle_count_; }

private:
    void init_particle_buffers();
    void init_shaders();

    size_t particle_count_{32768};
    uint32_t ssbo_{0};
    uint32_t vao_{0};
    uint32_t ubo_{0};

    Shader compute_shader_;
    Shader render_shader_;

    LeakyIntegrator smooth_centroid_{0.0f, 30.0f, 3.5f};
    LeakyIntegrator smooth_dissonance_{0.0f, 45.0f, 5.0f};
    LeakyIntegrator smooth_onset_{0.0f, 80.0f, 12.0f};
    LeakyIntegrator smooth_rms_{0.0f, 35.0f, 4.0f};
    LeakyIntegrator smooth_sub_bass_{0.0f, 35.0f, 4.0f};
};

} // namespace audio_voyager::graphics
