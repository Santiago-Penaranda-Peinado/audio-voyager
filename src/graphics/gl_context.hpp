#pragma once

#include <string>
#include <functional>
#include <cstdint>

struct GLFWwindow;

namespace audio_voyager::graphics {

struct WindowConfig {
    std::string title{"AUDIO-VOYAGER // 1,000,000 Particle GPU Simulation (Phase 2)"};
    int width{1920};
    int height{1080};
    bool fullscreen{false};
    bool vsync{false}; // Unlocked FPS for maximum compute throughput benchmarking
};

class GLContext {
public:
    explicit GLContext(const WindowConfig& config = WindowConfig{});
    ~GLContext();

    // Non-copyable
    GLContext(const GLContext&) = delete;
    GLContext& operator=(const GLContext&) = delete;

    bool init();
    void poll_events();
    void swap_buffers();
    [[nodiscard]] bool should_close() const;
    void set_should_close(bool close);

    [[nodiscard]] GLFWwindow* get_window() const noexcept { return window_; }
    [[nodiscard]] int get_width() const noexcept { return width_; }
    [[nodiscard]] int get_height() const noexcept { return height_; }
    [[nodiscard]] float get_aspect_ratio() const noexcept { 
        return height_ > 0 ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f; 
    }
    [[nodiscard]] float get_delta_time() const noexcept { return delta_time_; }
    [[nodiscard]] float get_total_time() const noexcept { return total_time_; }
    [[nodiscard]] float get_fps() const noexcept { return fps_; }

    void set_mouse_callback(std::function<void(double x, double y)> callback) { mouse_callback_ = callback; }
    void set_scroll_callback(std::function<void(double xoffset, double yoffset)> callback) { scroll_callback_ = callback; }

private:
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void mouse_position_callback(GLFWwindow* window, double xpos, double ypos);
    static void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

    WindowConfig config_;
    GLFWwindow* window_{nullptr};
    int width_{1920};
    int height_{1080};

    double last_frame_time_{0.0};
    float delta_time_{0.016f};
    float total_time_{0.0f};
    float fps_{60.0f};
    uint64_t frame_count_{0};
    double fps_timer_{0.0};

    std::function<void(double, double)> mouse_callback_;
    std::function<void(double, double)> scroll_callback_;
};

} // namespace audio_voyager::graphics
