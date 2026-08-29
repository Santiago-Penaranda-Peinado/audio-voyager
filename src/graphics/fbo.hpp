#pragma once

#include <cstdint>

namespace audio_voyager::graphics {

class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();

    // Non-copyable
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    bool init(int width, int height, bool hdr = true);
    void resize(int width, int height);
    void bind() const;
    void unbind() const;

    [[nodiscard]] uint32_t get_texture() const noexcept { return texture_id_; }
    [[nodiscard]] int get_width() const noexcept { return width_; }
    [[nodiscard]] int get_height() const noexcept { return height_; }

private:
    void cleanup();

    uint32_t fbo_id_{0};
    uint32_t texture_id_{0};
    uint32_t rbo_id_{0};
    int width_{0};
    int height_{0};
    bool hdr_{true};
};

} // namespace audio_voyager::graphics
