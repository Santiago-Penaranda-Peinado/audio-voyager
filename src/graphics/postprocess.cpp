#include "graphics/postprocess.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace audio_voyager::graphics {

PostProcessPipeline::~PostProcessPipeline() {
    if (quad_vao_) glDeleteVertexArrays(1, &quad_vao_);
    if (quad_vbo_) glDeleteBuffers(1, &quad_vbo_);
}

bool PostProcessPipeline::init(int width, int height) {
    width_ = width;
    height_ = height;

    init_screen_quad();

    bloom_blur_shader_ = Shader::load_graphics_from_files("shaders/screen_quad.vert", "shaders/bloom_blur.frag");
    postprocess_shader_ = Shader::load_graphics_from_files("shaders/screen_quad.vert", "shaders/postprocess.frag");

    int blur_w = std::max(1, width / 2);
    int blur_h = std::max(1, height / 2);

    if (!pingpong_fbo_[0].init(blur_w, blur_h, true) ||
        !pingpong_fbo_[1].init(blur_w, blur_h, true)) {
        std::cerr << "[PostProcessPipeline] Failed to initialize Ping-Pong Bloom Framebuffers.\n";
        return false;
    }

    std::cout << "[PostProcessPipeline] HDR Optical Post-Processing Pipeline Ready.\n";
    return bloom_blur_shader_.is_valid() && postprocess_shader_.is_valid();
}

void PostProcessPipeline::init_screen_quad() {
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

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));

    // UV attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void PostProcessPipeline::resize(int width, int height) {
    width_ = width;
    height_ = height;

    int blur_w = std::max(1, width / 2);
    int blur_h = std::max(1, height / 2);

    pingpong_fbo_[0].resize(blur_w, blur_h);
    pingpong_fbo_[1].resize(blur_w, blur_h);
}

void PostProcessPipeline::blur_bloom(uint32_t input_tex, int width, int height) {
    int blur_w = std::max(1, width / 2);
    int blur_h = std::max(1, height / 2);
    glViewport(0, 0, blur_w, blur_h);

    bool horizontal = true;
    bool first_iteration = true;
    int amount = 6; // 3 full horizontal/vertical blur passes

    bloom_blur_shader_.bind();
    bloom_blur_shader_.set_int("u_image", 0);
    bloom_blur_shader_.set_float("u_bloom_threshold", 1.20f);
    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(quad_vao_);
    for (int i = 0; i < amount; ++i) {
        pingpong_fbo_[horizontal ? 1 : 0].bind();
        glClear(GL_COLOR_BUFFER_BIT);

        bloom_blur_shader_.set_int("u_horizontal", horizontal ? 1 : 0);
        bloom_blur_shader_.set_int("u_first_pass", first_iteration ? 1 : 0);

        glBindTexture(GL_TEXTURE_2D, first_iteration ? input_tex : pingpong_fbo_[horizontal ? 0 : 1].get_texture());
        glDrawArrays(GL_TRIANGLES, 0, 6);

        horizontal = !horizontal;
        if (first_iteration) first_iteration = false;
    }
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessPipeline::render(uint32_t scene_hdr_tex, int width, int height, 
                                 float bloom_intensity, float chromatic_aberration, 
                                 float glitch_amount, float speed_lines, float time,
                                 float audio_rms, float dt) {
    // 1. Generate thresholded bloom blur in half-resolution ping-pong framebuffers
    blur_bloom(scene_hdr_tex, width, height);

    // 2. Global Dynamic Exposure Adaptation (extended range [0.12, 1.35])
    float target_exposure = 1.0f / std::sqrt(0.35f + audio_rms * 2.5f + (glitch_amount > 0.45f ? 0.35f : 0.0f));
    target_exposure = std::clamp(target_exposure, 0.12f, 1.35f);
    float adapt_rate = (target_exposure < smoothed_exposure_) ? 12.0f : 4.0f; // Fast drop on attack, gradual recovery
    smoothed_exposure_ += (target_exposure - smoothed_exposure_) * std::clamp(dt * adapt_rate, 0.01f, 1.0f);
    smoothed_exposure_ = std::clamp(smoothed_exposure_, 0.12f, 1.35f);

    // 3. Final Composite pass to backbuffer (Full Resolution)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);

    postprocess_shader_.bind();
    postprocess_shader_.set_int("u_scene_hdr", 0);
    postprocess_shader_.set_int("u_bloom_blur", 1);
    postprocess_shader_.set_float("u_bloom_intensity", bloom_intensity);
    postprocess_shader_.set_float("u_chromatic_aberration", chromatic_aberration);
    postprocess_shader_.set_float("u_glitch_amount", glitch_amount);
    postprocess_shader_.set_float("u_speed_lines", speed_lines);
    postprocess_shader_.set_float("u_time", time);
    postprocess_shader_.set_vec2("u_resolution", glm::vec2(static_cast<float>(width), static_cast<float>(height)));
    postprocess_shader_.set_float("u_exposure", smoothed_exposure_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene_hdr_tex);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pingpong_fbo_[0].get_texture());

    glBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace audio_voyager::graphics
