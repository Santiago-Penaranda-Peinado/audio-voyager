#pragma once

#include "core/types.hpp"
#include "graphics/shader.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace audio_voyager::graphics {

struct OscilloscopeVertex {
    float position[3];
    float progress;
};

class OscilloscopeRenderer {
public:
    OscilloscopeRenderer();
    ~OscilloscopeRenderer();

    // Non-copyable
    OscilloscopeRenderer(const OscilloscopeRenderer&) = delete;
    OscilloscopeRenderer& operator=(const OscilloscopeRenderer&) = delete;

    bool init();
    void update(const core::StreamASnapshot& stream_a, const glm::vec3& laser_pos, const glm::vec3& forward_dir, const glm::vec3& up_dir);
    void render(const glm::mat4& view_proj, float time, float rms, float peak,
                const glm::vec3& color_primary = glm::vec3(0.08f, 0.42f, 0.88f),
                const glm::vec3& color_accent = glm::vec3(1.00f, 0.72f, 0.22f));

private:
    uint32_t vao_{0};
    uint32_t vbo_{0};
    Shader shader_;
    std::vector<OscilloscopeVertex> vertices_;
};

} // namespace audio_voyager::graphics
