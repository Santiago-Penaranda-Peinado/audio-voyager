#include "graphics/particle_system.hpp"
#include <glad/glad.h>
#include <iostream>
#include <random>
#include <algorithm>

namespace audio_voyager::graphics {

ParticleSystem::ParticleSystem(size_t particle_count)
    : particle_count_(particle_count) {
}

ParticleSystem::~ParticleSystem() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (ssbo_) glDeleteBuffers(1, &ssbo_);
    if (ubo_) glDeleteBuffers(1, &ubo_);
}

bool ParticleSystem::init() {
    init_particle_buffers();
    init_shaders();
    return compute_shader_.is_valid() && render_shader_.is_valid();
}

void ParticleSystem::init_particle_buffers() {
    std::cout << "[ParticleSystem] Allocating GPU SSBO for " << particle_count_ 
              << " particles (" << (particle_count_ * sizeof(GpuParticle) / (1024 * 1024)) << " MB VRAM)...\n";

    std::vector<GpuParticle> initial_particles(particle_count_);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist_theta(0.0f, 2.0f * static_cast<float>(M_PI));
    std::uniform_real_distribution<float> dist_phi(-0.5f * static_cast<float>(M_PI), 0.5f * static_cast<float>(M_PI));
    std::uniform_real_distribution<float> dist_r(0.5f, 6.0f);
    std::uniform_real_distribution<float> dist_life(0.2f, 1.0f);
    std::uniform_real_distribution<float> dist_mass(0.5f, 1.2f);

    for (size_t i = 0; i < particle_count_; ++i) {
        float theta = dist_theta(rng);
        float phi = dist_phi(rng);
        float r = dist_r(rng);

        float x = r * std::cos(theta) * std::cos(phi);
        float y = r * std::sin(phi) * 0.4f;
        float z = r * std::sin(theta) * std::cos(phi);

        initial_particles[i].pos_life[0] = x;
        initial_particles[i].pos_life[1] = y;
        initial_particles[i].pos_life[2] = z;
        initial_particles[i].pos_life[3] = dist_life(rng);

        glm::vec3 pos(x, y, z);
        glm::vec3 vel = glm::cross(glm::normalize(pos), glm::vec3(0.0f, 1.0f, 0.0f)) * 2.5f;

        initial_particles[i].vel_mass[0] = vel.x;
        initial_particles[i].vel_mass[1] = vel.y;
        initial_particles[i].vel_mass[2] = vel.z;
        initial_particles[i].vel_mass[3] = dist_mass(rng);
    }

    glGenBuffers(1, &ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 
                 static_cast<GLsizeiptr>(particle_count_ * sizeof(GpuParticle)), 
                 initial_particles.data(), 
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, ssbo_);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GpuParticle), (void*)offsetof(GpuParticle, pos_life));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GpuParticle), (void*)offsetof(GpuParticle, vel_mass));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &ubo_);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(AudioPhysicsUbo), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void ParticleSystem::init_shaders() {
    compute_shader_ = Shader::load_compute_from_file("shaders/particle_physics.comp");
    render_shader_ = Shader::load_graphics_from_files("shaders/particle_render.vert", "shaders/particle_render.frag");

    if (compute_shader_.is_valid() && render_shader_.is_valid()) {
        std::cout << "[ParticleSystem] Shaders compiled and linked successfully.\n";
    } else {
        std::cerr << "[ParticleSystem] Failed to compile particle shaders.\n";
    }
}

void ParticleSystem::update(float dt, float total_time, const core::PhysicsAudioState& audio_state, const core::PhysicsTuners& tuners) {
    if (!compute_shader_.is_valid()) return;

    float raw_centroid = audio_state.stream_b.spectral_centroid_norm;
    float raw_dissonance = audio_state.stream_b.dissonance;
    float raw_onset = audio_state.stream_b.onset_strength;
    float raw_rms = audio_state.stream_b.rms;
    float raw_sub_bass = audio_state.stream_a.spectrum_bands[0];

    float smooth_c = smooth_centroid_.update(raw_centroid, dt, tuners.decay_rate);
    float smooth_d = smooth_dissonance_.update(raw_dissonance, dt, tuners.decay_rate);
    float smooth_o = smooth_onset_.update(raw_onset, dt, tuners.decay_rate * 2.5f);
    float smooth_r = smooth_rms_.update(raw_rms, dt, tuners.decay_rate);
    float smooth_sb = smooth_sub_bass_.update(raw_sub_bass, dt, tuners.decay_rate);

    AudioPhysicsUbo ubo_data{};
    ubo_data.audio_physics[0] = smooth_c;
    ubo_data.audio_physics[1] = smooth_d;
    ubo_data.audio_physics[2] = smooth_o;
    ubo_data.audio_physics[3] = audio_state.stream_b.is_onset ? 1.0f : 0.0f;

    ubo_data.audio_energy[0] = smooth_r;
    ubo_data.audio_energy[1] = audio_state.stream_a.peak_amplitude;
    ubo_data.audio_energy[2] = smooth_sb;
    ubo_data.audio_energy[3] = audio_state.stream_a.spectrum_bands[7];

    ubo_data.sim_params[0] = std::min(dt, 0.033f);
    ubo_data.sim_params[1] = total_time;
    ubo_data.sim_params[2] = static_cast<float>(particle_count_);
    ubo_data.sim_params[3] = tuners.damping;

    ubo_data.physics_scales[0] = tuners.gravity_scale;
    ubo_data.physics_scales[1] = tuners.vorticity_scale;
    ubo_data.physics_scales[2] = tuners.shockwave_scale;
    ubo_data.physics_scales[3] = tuners.attraction_scale;

    ubo_data.color_base[0] = tuners.color_base[0];
    ubo_data.color_base[1] = tuners.color_base[1];
    ubo_data.color_base[2] = tuners.color_base[2];
    ubo_data.color_base[3] = tuners.point_size_scale;

    ubo_data.color_peak[0] = tuners.color_peak[0];
    ubo_data.color_peak[1] = tuners.color_peak[1];
    ubo_data.color_peak[2] = tuners.color_peak[2];
    ubo_data.color_peak[3] = 0.0f;

    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(AudioPhysicsUbo), &ubo_data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    compute_shader_.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo_);

    const GLuint num_workgroups = static_cast<GLuint>((particle_count_ + 255) / 256);
    glDispatchCompute(num_workgroups, 1, 1);

    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    compute_shader_.unbind();
}

void ParticleSystem::render(const glm::mat4& view_proj) {
    if (!render_shader_.is_valid()) return;

    render_shader_.bind();
    render_shader_.set_mat4("u_view_proj", view_proj);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo_);

    glBindVertexArray(vao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(particle_count_));
    glBindVertexArray(0);

    render_shader_.unbind();
}

} // namespace audio_voyager::graphics
