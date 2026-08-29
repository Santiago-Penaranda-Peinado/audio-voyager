#include "graphics/gl_context.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

namespace audio_voyager::graphics {

GLContext::GLContext(const WindowConfig& config)
    : config_(config)
    , width_(config.width)
    , height_(config.height) {
}

GLContext::~GLContext() {
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

bool GLContext::init() {
    if (!glfwInit()) {
        std::cerr << "[GLContext] Failed to initialize GLFW.\n";
        return false;
    }

    // Modern OpenGL 4.5 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWmonitor* monitor = config_.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    window_ = glfwCreateWindow(width_, height_, config_.title.c_str(), monitor, nullptr);

    if (!window_) {
        std::cerr << "[GLContext] Failed to create GLFW window with OpenGL 4.5 Core profile.\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSetWindowUserPointer(window_, this);

    // Callbacks
    glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);
    glfwSetCursorPosCallback(window_, mouse_position_callback);
    glfwSetScrollCallback(window_, mouse_scroll_callback);

    // VSync configuration
    glfwSwapInterval(config_.vsync ? 1 : 0);

    // Initialize GLAD OpenGL 4.5 Loader
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "[GLContext] Failed to initialize GLAD OpenGL loader.\n";
        return false;
    }

    std::cout << "[GLContext] OpenGL Context Initialized Successfully:\n"
              << "            Vendor:   " << glGetString(GL_VENDOR) << "\n"
              << "            Renderer: " << glGetString(GL_RENDERER) << "\n"
              << "            Version:  " << glGetString(GL_VERSION) << "\n"
              << "            GLSL:     " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";

    // Global GL States
    glViewport(0, 0, width_, height_);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // High-speed additive blending
    glDisable(GL_DEPTH_TEST);

    last_frame_time_ = glfwGetTime();
    fps_timer_ = last_frame_time_;
    return true;
}

void GLContext::poll_events() {
    glfwPollEvents();

    double current_time = glfwGetTime();
    delta_time_ = static_cast<float>(current_time - last_frame_time_);
    last_frame_time_ = current_time;
    total_time_ = static_cast<float>(current_time);

    // FPS calculation
    ++frame_count_;
    if (current_time - fps_timer_ >= 0.5) {
        fps_ = static_cast<float>(frame_count_) / static_cast<float>(current_time - fps_timer_);
        frame_count_ = 0;
        fps_timer_ = current_time;
    }

    // Check ESC key to close
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        set_should_close(true);
    }
}

void GLContext::swap_buffers() {
    if (window_) {
        glfwSwapBuffers(window_);
    }
}

bool GLContext::should_close() const {
    return window_ ? glfwWindowShouldClose(window_) : true;
}

void GLContext::set_should_close(bool close) {
    if (window_) {
        glfwSetWindowShouldClose(window_, close ? GLFW_TRUE : GLFW_FALSE);
    }
}

void GLContext::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<GLContext*>(glfwGetWindowUserPointer(window));
    if (self) {
        std::cout << "[GLContext AUDIT] Viewport / Framebuffer Resize: " 
                  << self->width_ << "x" << self->height_ << " -> " << width << "x" << height << "\n";
        self->width_ = width;
        self->height_ = height;
        if (width > 0 && height > 0) {
            glViewport(0, 0, width, height);
        }
    }
}

void GLContext::mouse_position_callback(GLFWwindow* window, double xpos, double ypos) {
    auto* self = static_cast<GLContext*>(glfwGetWindowUserPointer(window));
    if (self && self->mouse_callback_) {
        self->mouse_callback_(xpos, ypos);
    }
}

void GLContext::mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* self = static_cast<GLContext*>(glfwGetWindowUserPointer(window));
    if (self && self->scroll_callback_) {
        self->scroll_callback_(xoffset, yoffset);
    }
}

} // namespace audio_voyager::graphics
