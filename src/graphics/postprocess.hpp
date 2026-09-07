#pragma once

#include "graphics/shader.hpp"
#include "graphics/fbo.hpp"
#include <cstdint>
#include <memory>
#include <algorithm>

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
                float glitch_amount, float speed_lines, float time,
                float audio_rms = 0.0f, float dt = 0.016f);

    [[nodiscard]] float get_exposure() const noexcept { return smoothed_exposure_; }
    void set_exposure_instant(float exposure) noexcept { smoothed_exposure_ = exposure; }

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
    float smoothed_exposure_{1.0f};
};

} // namespace audio_voyager::graphics
