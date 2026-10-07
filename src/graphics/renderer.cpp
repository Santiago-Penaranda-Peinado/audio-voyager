#include "graphics/renderer.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
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
    : context_(config)
    , particle_system_(32768) {
}

Renderer::~Renderer() {
    if (waterfall_texture_) glDeleteTextures(1, &waterfall_texture_);
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

    // 4.5. Initialize 2D Waterfall Energy Matrix Texture (GL_R16F for raymarching terrain sculpting)
    glGenTextures(1, &waterfall_texture_);
    glBindTexture(GL_TEXTURE_2D, waterfall_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, 
                 static_cast<GLsizei>(core::WATERFALL_BANDS), 
                 static_cast<GLsizei>(core::WATERFALL_FRAMES), 
                 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // 5. Initialize 3D Laser Ribbon Oscilloscope
    if (!oscilloscope_.init()) {
        std::cerr << "[Renderer AUDIT][ERROR] Failed to initialize OscilloscopeRenderer.\n";
        return false;
    }

    // 5.5. Awaken GPU Compute Particle System (1,048,576 particles)
    if (!particle_system_.init()) {
        std::cerr << "[Renderer AUDIT][ERROR] Failed to initialize ParticleSystem.\n";
        return false;
    }

    // 5.6. Register Mouse Scroll Callback for Instant Sensitivity Tuning
    context_.set_scroll_callback([this](double /*xoffset*/, double yoffset) {
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) return;
        tuners_.input_gain = std::clamp(tuners_.input_gain + static_cast<float>(yoffset) * 0.10f, 0.10f, 5.00f);
        std::cout << "[Audio AGC] Input Gain Sensitivity: " << std::fixed << std::setprecision(2) << tuners_.input_gain << "x\n";
    });

    // 6. Initialize ImGui Debug Overlay (F12)
    imgui_.init(context_.get_window());

    check_gl_error_step("init");
    std::cout << "[Renderer AUDIT] Master SDF Raymarching & Autonomous Art Director Engine Ready.\n";
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

    // 1. Semantic Brain: Asynchronous ML & Real-time Multi-Band DSP Fusion
    brain_.update(audio_state, dt);
    core::AudioSemanticVector semantic = brain_.get_semantic_vector();

    // 2. Autonomous Art Director: Dynamic Rhythmic Head-Bobbing & Clamped Altitudes
    director_.update(semantic, dt * tuners_.speed_multiplier);
    semantic.camera_roll = director_.get_camera_roll();
    glm::mat4 view_proj = director_.get_view_projection_matrix(aspect);
    glm::vec3 cam_pos = director_.get_camera_pos();
    glm::vec3 cam_dir = director_.get_camera_dir();
    glm::vec3 cam_up = director_.get_camera_up();
    glm::vec3 laser_pos = director_.get_laser_pos();

    // Camera Telemetry Logging (every 60 frames)
    static uint64_t frame_log_counter = 0;
    if (++frame_log_counter % 60 == 0) {
        std::cout << "[AutonomousArtDirector AUDIT] Pos: (" 
                  << std::fixed << std::setprecision(2) << cam_pos.x << ", " << cam_pos.y << ", " << cam_pos.z << ")"
                  << " | Biomes: [L: " << semantic.weight_liquid 
                  << " | M: " << semantic.weight_metal 
                  << " | C: " << semantic.weight_cyber 
                  << " | D: " << semantic.weight_dubstep << "]"
                  << " | Speed: " << semantic.speed_forward << " m/s"
                  << " | Roll: " << std::setprecision(1) << glm::degrees(director_.get_camera_roll()) << " deg"
                  << " | BPM: " << semantic.bpm << " (" << static_cast<int>(semantic.bpm_confidence * 100.0f) << "%)"
                  << (semantic.is_silent ? " [QUIESCENCE / SILENCE]" : "") 
                  << " | FPS: " << context_.get_fps() << "\n" << std::flush;
    }

    // 3. Upload Live 2D Waterfall Energy Matrix for Terrain Sculpting (GL_R16F)
    static std::vector<float> s_wf_energy;
    brain_.get_waterfall_analyzer().get_raw_energy_matrix(s_wf_energy);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, waterfall_texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                    static_cast<GLsizei>(core::WATERFALL_BANDS), 
                    static_cast<GLsizei>(core::WATERFALL_FRAMES), 
                    GL_RED, GL_FLOAT, s_wf_energy.data());

    // 4. Render Continuous SDF Raymarching into HDR Scene Framebuffer
    scene_fbo_.resize(width, height);
    scene_fbo_.bind();
    glViewport(0, 0, width, height);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS); // Always pass fullscreen quad so all color fragments and gl_FragDepth values are written
    glDepthMask(GL_TRUE);

    RaymarchingUboData ubo_data{};
    ubo_data.resolution_time[0] = static_cast<float>(width);
    ubo_data.resolution_time[1] = static_cast<float>(height);
    ubo_data.resolution_time[2] = time;
    ubo_data.resolution_time[3] = dt;

    ubo_data.cam_pos[0] = cam_pos.x;
    ubo_data.cam_pos[1] = cam_pos.y;
    ubo_data.cam_pos[2] = cam_pos.z;
    ubo_data.cam_pos[3] = director_.get_camera_roll();

    ubo_data.cam_dir[0] = cam_dir.x;
    ubo_data.cam_dir[1] = cam_dir.y;
    ubo_data.cam_dir[2] = cam_dir.z;
    ubo_data.cam_dir[3] = director_.get_fov_radians();

    ubo_data.cam_up[0] = cam_up.x;
    ubo_data.cam_up[1] = cam_up.y;
    ubo_data.cam_up[2] = cam_up.z;
    ubo_data.cam_up[3] = semantic.speed_forward;

    ubo_data.biome_weights[0] = semantic.weight_liquid;
    ubo_data.biome_weights[1] = semantic.weight_metal;
    ubo_data.biome_weights[2] = semantic.weight_cyber;
    ubo_data.biome_weights[3] = semantic.weight_dubstep;

    ubo_data.physical_params[0] = semantic.elastic_dilation;
    ubo_data.physical_params[1] = semantic.surface_ripple;
    ubo_data.physical_params[2] = semantic.emission_pulse;
    ubo_data.physical_params[3] = semantic.norm_centroid;

    ubo_data.laser_pos[0] = laser_pos.x;
    ubo_data.laser_pos[1] = laser_pos.y;
    ubo_data.laser_pos[2] = laser_pos.z;
    ubo_data.laser_pos[3] = semantic.arousal;

    ubo_data.extra_physics[0] = semantic.melodic_mids;
    ubo_data.extra_physics[1] = semantic.treble_sparkle;
    ubo_data.extra_physics[2] = semantic.is_silent ? 1.0f : 0.0f;
    ubo_data.extra_physics[3] = semantic.bpm;

    ubo_data.color_primary[0] = semantic.color_primary.r;
    ubo_data.color_primary[1] = semantic.color_primary.g;
    ubo_data.color_primary[2] = semantic.color_primary.b;
    ubo_data.color_primary[3] = 1.0f;

    ubo_data.color_accent[0] = semantic.color_accent.r;
    ubo_data.color_accent[1] = semantic.color_accent.g;
    ubo_data.color_accent[2] = semantic.color_accent.b;
    ubo_data.color_accent[3] = 1.0f;

    ubo_data.color_zenith[0] = semantic.color_zenith.r;
    ubo_data.color_zenith[1] = semantic.color_zenith.g;
    ubo_data.color_zenith[2] = semantic.color_zenith.b;
    ubo_data.color_zenith[3] = 1.0f;

    glBindBuffer(GL_UNIFORM_BUFFER, raymarching_ubo_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(RaymarchingUboData), &ubo_data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    raymarching_shader_.bind();
    raymarching_shader_.set_mat4("u_view_proj", view_proj);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, waterfall_texture_);
    raymarching_shader_.set_int("u_waterfall_energy", 2);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, raymarching_ubo_);

    glBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    raymarching_shader_.unbind();

    // 5. Aesthetic GPU Particle System (Harmonic Stardust & Wake Embers)
    if (tuners_.enable_particles) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE); // Read-only depth test: particles occluded by tunnel walls without overwriting
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive luminous blending

        particle_system_.update(dt, time, audio_state, semantic, cam_pos, cam_dir, laser_pos,
                                tuners_.particle_size, tuners_.particle_opacity);
        particle_system_.render(view_proj);
    }

    // 6. Render 3D Laser Ribbon Oscilloscope Entity
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    oscilloscope_.update(audio_state.stream_a, laser_pos, cam_dir, cam_up);
    oscilloscope_.render(view_proj, time, 
                        semantic.is_silent ? 0.05f : (semantic.emission_pulse * 0.4f), 
                        audio_state.stream_a.peak_amplitude,
                        semantic.color_primary,
                        semantic.color_accent);

    glDisable(GL_DEPTH_TEST);
    scene_fbo_.unbind();

    // 7. Optical Post-Processing (Dynamic Bloom, CA, Radial God Rays & dynamic speed lines)
    float dyn_bloom = tuners_.bloom_intensity * (0.35f + 0.40f * semantic.emission_pulse + (semantic.is_onset ? 0.25f : 0.0f));
    dyn_bloom = std::clamp(dyn_bloom, 0.0f, 0.60f);
    if (semantic.is_silent) dyn_bloom = tuners_.bloom_intensity * 0.15f;

    float dyn_ca = tuners_.chromatic_aberration * (0.40f + 1.10f * semantic.surface_ripple + 0.50f * semantic.arousal);
    if (semantic.is_silent) dyn_ca = tuners_.chromatic_aberration * 0.20f;

    float speed_factor = std::clamp((semantic.speed_forward - 6.0f) / 20.0f, 0.0f, 1.0f);
    float dyn_speed_lines = speed_factor * 0.75f + (semantic.is_onset && semantic.speed_forward > 12.0f ? 0.35f : 0.0f);
    dyn_speed_lines = std::clamp(dyn_speed_lines, 0.0f, 1.0f) * tuners_.speed_multiplier;
    if (semantic.is_silent) dyn_speed_lines = 0.0f;

    // Dynamic drop flash & contrast pop on sudden rhythm/tempo shifts or heavy kick drops
    if (semantic.gear_shift_pulse > 0.22f || (semantic.is_onset && semantic.emission_pulse > 1.30f)) {
        drop_flash_ = std::min(drop_flash_ + 0.45f * std::max(semantic.gear_shift_pulse, 0.60f), 0.70f);
    }
    drop_flash_ *= std::exp(-14.0f * dt);
    if (semantic.is_silent) drop_flash_ = 0.0f;

    glDisable(GL_BLEND);
    postprocess_.render(scene_fbo_.get_texture(), width, height, 
                        dyn_bloom, dyn_ca, 
                        semantic.surface_ripple, dyn_speed_lines, time,
                        semantic.melodic_mids, glm::vec2(0.5f, 0.52f),
                        drop_flash_);

    // 6. Debug HUD (ImGui) - Toggle via F12
    if (tuners_.show_hud) {
        imgui_.begin_frame();
        imgui_.render_dashboard(tuners_, audio_state, semantic, brain_.get_waterfall_analyzer(), context_.get_fps(), dt);
        imgui_.end_frame();
    }

    check_gl_error_step("render_frame");
    context_.swap_buffers();
}

} // namespace audio_voyager::graphics
