#include "graphics/fbo.hpp"
#include <glad/glad.h>
#include <iostream>

namespace audio_voyager::graphics {

namespace {
void check_gl_error_fbo(const char* location) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "[OpenGL ERROR][FBO][" << location << "] Code: 0x" 
                  << std::hex << err << std::dec << "\n";
    }
}
}

Framebuffer::~Framebuffer() {
    cleanup();
}

void Framebuffer::cleanup() {
    if (rbo_id_) {
        glDeleteRenderbuffers(1, &rbo_id_);
        rbo_id_ = 0;
    }
    if (texture_id_) {
        glDeleteTextures(1, &texture_id_);
        texture_id_ = 0;
    }
    if (fbo_id_) {
        glDeleteFramebuffers(1, &fbo_id_);
        fbo_id_ = 0;
    }
}

bool Framebuffer::init(int width, int height, bool hdr) {
    if (width <= 0 || height <= 0) {
        std::cerr << "[Framebuffer AUDIT] Invalid dimensions: " << width << "x" << height << "\n";
        return false;
    }

    cleanup();
    width_ = width;
    height_ = height;
    hdr_ = hdr;

    glGenFramebuffers(1, &fbo_id_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id_);

    // Generate color texture attachment
    glGenTextures(1, &texture_id_);
    glBindTexture(GL_TEXTURE_2D, texture_id_);

    GLenum internal_format = hdr ? GL_RGBA16F : GL_RGBA8;
    GLenum type = hdr ? GL_FLOAT : GL_UNSIGNED_BYTE;

    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internal_format), width_, height_, 0, GL_RGBA, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id_, 0);

    // Generate Depth & Stencil RBO
    glGenRenderbuffers(1, &rbo_id_);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_id_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width_, height_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_id_);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[Framebuffer AUDIT][ERROR] Incomplete Framebuffer: 0x" << std::hex << status << std::dec << "\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    std::cout << "[Framebuffer AUDIT] FBO created successfully: " << width_ << "x" << height_ 
              << " | Format: " << (hdr ? "GL_RGBA16F (HDR)" : "GL_RGBA8 (LDR)") 
              << " | Status: GL_FRAMEBUFFER_COMPLETE (0x" << std::hex << status << std::dec << ")\n" << std::flush;

    check_gl_error_fbo("init");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void Framebuffer::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width == width_ && height == height_) return;

    std::cout << "[Framebuffer AUDIT] Resizing FBO: " << width_ << "x" << height_ 
              << " -> " << width << "x" << height << "\n" << std::flush;
    init(width, height, hdr_);
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id_);
    glViewport(0, 0, width_, height_);
}

void Framebuffer::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace audio_voyager::graphics
