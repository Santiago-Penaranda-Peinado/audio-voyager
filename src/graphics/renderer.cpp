#include "graphics/renderer.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <iomanip>

namespace audio_voyager::graphics {

namespace {
void check_gl_error_step(const char* location) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "[OpenGL ERROR][Renderer Step: " << location << "] Code: 0x" 
                  << std::hex << err << std::dec << "\n" << std::flush;
    }
}
}

Renderer::Renderer(const WindowConfig& config)
    : context_(config) {
}

Renderer::~Renderer() {
    if (raymarching_ubo_) glDeleteBuffers(1, &raymarching_ubo_);
    if (quad_vao_) glDeleteVertexArrays(1, &quad_vao_);
    if (quad_vbo_) glDeleteBuffers(1, &quad_vbo_);
}

bool Renderer::init() {
    if (!context_.init()) {
        std::cerr << "[Renderer AUDIT][ERROR] Failed to initialize GLContext.\n";
        return false;
    }

    int width = context_.get_width();
    int height = context_.get_height();

    std::cout << "[Renderer AUDIT] Initializing renderer at " << width << "x" << height << "...\n";

    // 1. Initialize HDR Scene Framebuffer (GL_RGBA16F)
    if (!scene_fbo_.init(width, height, true)) {
        std::cerr << "[Renderer AUDIT][ERROR] Failed to initialize Scene Framebuffer.\n";
        return false;
    }

    // 2. Initialize Optical Post-Processing Pipeline
    if (!postprocess_.init(width, height)) {
        std::cerr << "[Renderer AUDIT][ERROR] Failed to initialize PostProcessPipeline.\n";
        return false;
    }

    // 3. Initialize Screen Quad & Raymarching Shader
    init_screen_quad();
    raymarching_shader_ = Shader::load_graphics_from_files("shaders/screen_quad.vert", "shaders/raymarching.frag");
    if (!raymarching_shader_.is_valid()) {
        std::cerr << "[Renderer AUDIT][ERROR] Failed to compile Raymarching Shader.\n";
        return false;
    }

    // 4. Initialize Raymarching Uniform Buffer Object (UBO)
    glGenBuffers(1, &raymarching_ubo_);
    glBindBuffer(GL_UNIFORM_BUFFER, raymarching_ubo_);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(RaymarchingUboData), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // 5. Initialize 3D Laser Ribbon Oscilloscope
    if (!oscilloscope_.init()) {
        std::cerr << "[Renderer AUDIT][ERROR] Failed to initialize OscilloscopeRenderer.\n";
        return false;
    }

    // 6. Initialize ImGui Debug Overlay (F12)
    imgui_.init(context_.get_window());

    check_gl_error_step("init");
    std::cout << "[Renderer AUDIT] Master Procedural Gyroid SDF & AGC Semantic Engine Ready.\n";
    return true;
}

void Renderer::init_screen_quad() {
    float quad_vertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &quad_vao_);
    glGenBuffers(1, &quad_vbo_);

    glBindVertexArray(quad_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    check_gl_error_step("init_screen_quad");
}

void Renderer::toggle_fullscreen() {
    GLFWwindow* win = context_.get_window();
    if (!win) return;

    is_fullscreen_ = !is_fullscreen_;
    tuners_.fullscreen = is_fullscreen_;

    if (is_fullscreen_) {
        glfwGetWindowPos(win, &windowed_x_, &windowed_y_);
        glfwGetWindowSize(win, &windowed_w_, &windowed_h_);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        std::cout << "[Renderer AUDIT] Entering Borderless Fullscreen: " << mode->width << "x" << mode->height << "\n";
        glfwSetWindowMonitor(win, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        std::cout << "[Renderer AUDIT] Exiting Fullscreen to Windowed: " << windowed_w_ << "x" << windowed_h_ << "\n";
        glfwSetWindowMonitor(win, nullptr, windowed_x_, windowed_y_, windowed_w_, windowed_h_, 0);
    }
}

void Renderer::handle_keyboard_shortcuts() {
    GLFWwindow* win = context_.get_window();
    if (!win) return;

    // F11: Fullscreen
    static bool f11_pressed = false;
    if (glfwGetKey(win, GLFW_KEY_F11) == GLFW_PRESS) {
        if (!f11_pressed) {
            toggle_fullscreen();
            f11_pressed = true;
        }
    } else {
        f11_pressed = false;
    }

    // F12: Secret Debug HUD toggle
    static bool f12_pressed = false;
    if (glfwGetKey(win, GLFW_KEY_F12) == GLFW_PRESS) {
        if (!f12_pressed) {
            tuners_.show_hud = !tuners_.show_hud;
            f12_pressed = true;
            std::cout << "[Renderer AUDIT] Debug HUD (ImGui) Toggled: " << (tuners_.show_hud ? "ON" : "OFF") << "\n";
        }
    } else {
        f12_pressed = false;
    }
}

void Renderer::render_frame(const core::PhysicsAudioState& audio_state) {
    poll_events();
    handle_keyboard_shortcuts();

    const float dt = context_.get_delta_time();
    const float time = context_.get_total_time();
    const int width = context_.get_width();
    const int height = context_.get_height();
    const float aspect = context_.get_aspect_ratio();

    if (width <= 0 || height <= 0) return;

    // 1. Semantic Brain: DSP & AGC Auto-Gain Calibration
    brain_.update(audio_state, dt);
    const core::AudioSemanticVector& semantic = brain_.get_semantic_vector();

    // 2. Autonomous Art Director: Continuous Z-forward Camera Navigation
    director_.update(semantic, dt * tuners_.speed_multiplier);

    // Camera Telemetry Logging (every 60 frames)
    static uint64_t frame_log_counter = 0;
    if (++frame_log_counter % 60 == 0) {
        glm::vec3 cpos = director_.get_camera_pos();
        glm::vec3 cdir = director_.get_camera_dir();
        std::cout << "[AutonomousArtDirector AUDIT] Pos: (" 
                  << std::fixed << std::setprecision(2) << cpos.x << ", " << cpos.y << ", " << cpos.z << ")"
                  << " | Diss_AGC: " << semantic.norm_dissonance 
                  << " | Energy_AGC: " << semantic.norm_energy
                  << " | Centroid_AGC: " << semantic.norm_centroid
                  << " | Speed: " << semantic.speed_forward << " m/s | FPS: " << context_.get_fps() << "\n" << std::flush;
    }

    // 3. Render Continuous SDF Raymarching into HDR Scene Framebuffer
    scene_fbo_.resize(width, height);
    scene_fbo_.bind();

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    RaymarchingUboData ubo_data{};
    ubo_data.resolution_time[0] = static_cast<float>(width);
    ubo_data.resolution_time[1] = static_cast<float>(height);
    ubo_data.resolution_time[2] = time;
    ubo_data.resolution_time[3] = dt;

    glm::vec3 cam_pos = director_.get_camera_pos();
    ubo_data.cam_pos[0] = cam_pos.x;
    ubo_data.cam_pos[1] = cam_pos.y;
    ubo_data.cam_pos[2] = cam_pos.z;
    ubo_data.cam_pos[3] = director_.get_camera_roll();

    glm::vec3 cam_dir = director_.get_camera_dir();
    ubo_data.cam_dir[0] = cam_dir.x;
    ubo_data.cam_dir[1] = cam_dir.y;
    ubo_data.cam_dir[2] = cam_dir.z;
    ubo_data.cam_dir[3] = director_.get_fov_radians();

    glm::vec3 cam_up = director_.get_camera_up();
    ubo_data.cam_up[0] = cam_up.x;
    ubo_data.cam_up[1] = cam_up.y;
    ubo_data.cam_up[2] = cam_up.z;
    ubo_data.cam_up[3] = semantic.speed_forward;

    ubo_data.audio_params_1[0] = semantic.norm_dissonance;
    ubo_data.audio_params_1[1] = semantic.norm_centroid;
    ubo_data.audio_params_1[2] = semantic.norm_energy;
    ubo_data.audio_params_1[3] = semantic.norm_sub_bass;

    ubo_data.audio_params_2[0] = semantic.norm_treble;
    ubo_data.audio_params_2[1] = semantic.cavity_scale;
    ubo_data.audio_params_2[2] = semantic.glitch_intensity;
    ubo_data.audio_params_2[3] = semantic.is_onset ? 1.0f : 0.0f;

    glm::vec3 laser_pos = director_.get_laser_pos();
    ubo_data.laser_pos[0] = laser_pos.x;
    ubo_data.laser_pos[1] = laser_pos.y;
    ubo_data.laser_pos[2] = laser_pos.z;
    ubo_data.laser_pos[3] = semantic.speed_lines;

    ubo_data.tuners[0] = tuners_.bloom_intensity;
    ubo_data.tuners[1] = tuners_.chromatic_aberration;
    ubo_data.tuners[2] = tuners_.speed_multiplier;
    ubo_data.tuners[3] = 0.0f;

    glBindBuffer(GL_UNIFORM_BUFFER, raymarching_ubo_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(RaymarchingUboData), &ubo_data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    raymarching_shader_.bind();
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, raymarching_ubo_);

    glBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    raymarching_shader_.unbind();

    // 4. Render 3D Laser Ribbon Oscilloscope with Additive Blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    oscilloscope_.update(audio_state.stream_a, laser_pos, cam_dir, cam_up);
    oscilloscope_.render(director_.get_view_projection_matrix(aspect), time, semantic.norm_energy, audio_state.stream_a.peak_amplitude);

    scene_fbo_.unbind();

    // 5. Optical Post-Processing & 2D Hybrid Effects (Speed lines, Glitch scanlines & ACES)
    glDisable(GL_BLEND);
    postprocess_.render(scene_fbo_.get_texture(), width, height, 
                        tuners_.bloom_intensity, tuners_.chromatic_aberration, 
                        semantic.glitch_intensity, semantic.speed_lines, time);

    // 6. Debug HUD (ImGui) - Toggle via F12
    if (tuners_.show_hud) {
        imgui_.begin_frame();
        imgui_.render_dashboard(tuners_, audio_state, semantic, context_.get_fps(), dt);
        imgui_.end_frame();
    }

    check_gl_error_step("render_frame");
    context_.swap_buffers();
}

} // namespace audio_voyager::graphics
