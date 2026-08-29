#pragma once

#include "graphics/shader.hpp"
#include "graphics/fbo.hpp"
#include <cstdint>
#include <memory>

namespace audio_voyager::graphics {

class PostProcessPipeline {
public:
    PostProcessPipeline() = default;
    ~PostProcessPipeline();

    // Non-copyable
    PostProcessPipeline(const PostProcessPipeline&) = delete;
    PostProcessPipeline& operator=(const PostProcessPipeline&) = delete;

    bool init(int width, int height);
    void resize(int width, int height);
    void render(uint32_t scene_hdr_tex, int width, int height, 
                float bloom_intensity, float chromatic_aberration, 
                float glitch_amount, float speed_lines, float time);

private:
    void init_screen_quad();
    void blur_bloom(uint32_t input_tex, int width, int height);

    uint32_t quad_vao_{0};
    uint32_t quad_vbo_{0};

    Shader bloom_blur_shader_;
    Shader postprocess_shader_;

    Framebuffer pingpong_fbo_[2];
    int width_{0};
    int height_{0};
};

} // namespace audio_voyager::graphics
