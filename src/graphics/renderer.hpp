#pragma once

#include "graphics/gl_context.hpp"
#include "graphics/fbo.hpp"
#include "graphics/shader.hpp"
#include "graphics/oscilloscope_renderer.hpp"
#include "graphics/particle_system.hpp"
#include "graphics/postprocess.hpp"
#include "graphics/imgui_overlay.hpp"
#include "brain/semantic_brain.hpp"
#include "director/autonomous_art_director.hpp"
#include "core/types.hpp"
#include <memory>

namespace audio_voyager::graphics {

struct alignas(16) RaymarchingUboData {
    float resolution_time[4];   // xy: width, height, z: total_time, w: delta_time
    float cam_pos[4];           // xyz: camera pos, w: camera roll
    float cam_dir[4];           // xyz: forward dir, w: dynamic FOV
    float cam_up[4];            // xyz: up vector, w: camera speed
    float biome_weights[4];     // x: weight_liquid, y: weight_metal, z: weight_cyber, w: weight_dubstep
    float physical_params[4];   // x: elastic_dilation, y: surface_ripple, z: emission_pulse, w: norm_centroid
    float laser_pos[4];         // xyz: laser light pos, w: arousal
    float extra_physics[4];     // x: melodic_mids, y: treble_sparkle, z: is_silent, w: bpm
    float color_primary[4];     // rgb: smoothed primary color, w: spare
    float color_accent[4];      // rgb: smoothed accent color, w: spare
    float color_zenith[4];      // rgb: smoothed zenith sky color, w: spare
};

class Renderer {
public:
    explicit Renderer(const WindowConfig& config = WindowConfig{});
    ~Renderer();

    // Non-copyable
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init();
    void render_frame(const core::PhysicsAudioState& audio_state);

    [[nodiscard]] bool should_close() const { return context_.should_close(); }
    void poll_events() { context_.poll_events(); }

    [[nodiscard]] GLContext& get_context() { return context_; }
    [[nodiscard]] core::PhysicsTuners& get_tuners() noexcept { return tuners_; }
    [[nodiscard]] float get_fps() const { return context_.get_fps(); }

private:
    void init_screen_quad();
    void handle_keyboard_shortcuts();
    void toggle_fullscreen();

    GLContext context_;
    brain::SemanticBrain brain_;
    director::AutonomousArtDirector director_;

    Framebuffer scene_fbo_;
    Shader raymarching_shader_;
    uint32_t raymarching_ubo_{0};
    uint32_t waterfall_texture_{0};
    uint32_t quad_vao_{0};
    uint32_t quad_vbo_{0};

    ParticleSystem particle_system_;
    OscilloscopeRenderer oscilloscope_;
    PostProcessPipeline postprocess_;
    ImGuiOverlay imgui_;

    core::PhysicsTuners tuners_{};

    bool is_fullscreen_{false};
    int windowed_x_{100};
    int windowed_y_{100};
    int windowed_w_{1920};
    int windowed_h_{1080};
};

} // namespace audio_voyager::graphics
