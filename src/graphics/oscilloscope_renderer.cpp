#include "graphics/oscilloscope_renderer.hpp"
#include <glad/glad.h>
#include <iostream>

namespace audio_voyager::graphics {

OscilloscopeRenderer::OscilloscopeRenderer() {
    vertices_.resize(core::RAW_OSCILLOSCOPE_SAMPLES);
}

OscilloscopeRenderer::~OscilloscopeRenderer() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
}

bool OscilloscopeRenderer::init() {
    shader_ = Shader::load_graphics_from_files("shaders/oscilloscope.vert", "shaders/oscilloscope.frag");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 
                 static_cast<GLsizeiptr>(vertices_.size() * sizeof(OscilloscopeVertex)), 
                 nullptr, 
                 GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(OscilloscopeVertex), (void*)offsetof(OscilloscopeVertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(OscilloscopeVertex), (void*)offsetof(OscilloscopeVertex, progress));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return shader_.is_valid();
}

void OscilloscopeRenderer::update(const core::StreamASnapshot& stream_a, 
                                 const glm::vec3& laser_pos, 
                                 const glm::vec3& forward_dir, 
                                 const glm::vec3& up_dir) {
    const size_t count = core::RAW_OSCILLOSCOPE_SAMPLES;
    const float ribbon_width = 3.2f;
    const float amp_scale = 1.4f;

    glm::vec3 right = glm::normalize(glm::cross(forward_dir, up_dir));
    glm::vec3 up = glm::normalize(glm::cross(right, forward_dir));

    for (size_t i = 0; i < count; ++i) {
        float prog = static_cast<float>(i) / static_cast<float>(count - 1);
        float offset_x = (prog - 0.5f) * ribbon_width;
        float offset_y = stream_a.waveform[i] * amp_scale;
        float offset_z = std::sin(prog * 3.14159f) * 0.5f;

        glm::vec3 pos = laser_pos + right * offset_x + up * offset_y + forward_dir * offset_z;

        vertices_[i].position[0] = pos.x;
        vertices_[i].position[1] = pos.y;
        vertices_[i].position[2] = pos.z;
        vertices_[i].progress = prog;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 
                    static_cast<GLsizeiptr>(vertices_.size() * sizeof(OscilloscopeVertex)), 
                    vertices_.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void OscilloscopeRenderer::render(const glm::mat4& view_proj, float time, float rms, float peak,
                                  const glm::vec3& color_primary, const glm::vec3& color_accent) {
    if (!shader_.is_valid()) return;

    shader_.bind();
    shader_.set_mat4("u_view_proj", view_proj);
    shader_.set_float("u_time", time);
    shader_.set_float("u_rms", rms);
    shader_.set_float("u_peak", peak);
    shader_.set_vec3("u_color_primary", color_primary);
    shader_.set_vec3("u_color_accent", color_accent);

    glBindVertexArray(vao_);
    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(vertices_.size()));
    glBindVertexArray(0);

    shader_.unbind();
}

} // namespace audio_voyager::graphics
