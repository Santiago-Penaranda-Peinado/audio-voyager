#include "graphics/shader.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

namespace audio_voyager::graphics {

Shader::Shader(Shader&& other) noexcept
    : program_id_(other.program_id_)
    , uniform_location_cache_(std::move(other.uniform_location_cache_)) {
    other.program_id_ = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (program_id_) {
            glDeleteProgram(program_id_);
        }
        program_id_ = other.program_id_;
        uniform_location_cache_ = std::move(other.uniform_location_cache_);
        other.program_id_ = 0;
    }
    return *this;
}

Shader::~Shader() {
    if (program_id_) {
        glDeleteProgram(program_id_);
        program_id_ = 0;
    }
}

std::string Shader::read_file_to_string(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Shader] Failed to open shader file: " << path << "\n";
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

uint32_t Shader::compile_shader_stage(uint32_t stage_type, const std::string& source, const std::string& debug_name) {
    if (source.empty()) {
        std::cerr << "[Shader] Cannot compile empty shader source: " << debug_name << "\n";
        return 0;
    }

    uint32_t shader = glCreateShader(stage_type);
    const char* src_cstr = source.c_str();
    glShaderSource(shader, 1, &src_cstr, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        int log_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        std::vector<char> info_log(log_length > 0 ? log_length : 512);
        glGetShaderInfoLog(shader, static_cast<int>(info_log.size()), nullptr, info_log.data());
        std::cerr << "[Shader] Compilation Error in [" << debug_name << "]:\n" << info_log.data() << "\n";
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

Shader Shader::load_graphics_from_files(const std::string& vertex_path, const std::string& fragment_path) {
    std::string vert_src = read_file_to_string(vertex_path);
    std::string frag_src = read_file_to_string(fragment_path);
    return load_graphics_from_source(vert_src, frag_src);
}

Shader Shader::load_graphics_from_source(const std::string& vertex_src, const std::string& fragment_src) {
    Shader shader;
    uint32_t vs = compile_shader_stage(GL_VERTEX_SHADER, vertex_src, "Vertex Shader");
    uint32_t fs = compile_shader_stage(GL_FRAGMENT_SHADER, fragment_src, "Fragment Shader");

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return shader;
    }

    uint32_t program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        int log_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        std::vector<char> info_log(log_length > 0 ? log_length : 512);
        glGetProgramInfoLog(program, static_cast<int>(info_log.size()), nullptr, info_log.data());
        std::cerr << "[Shader] Graphics Program Link Error:\n" << info_log.data() << "\n";
        glDeleteProgram(program);
    } else {
        shader.program_id_ = program;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return shader;
}

Shader Shader::load_compute_from_file(const std::string& compute_path) {
    std::string comp_src = read_file_to_string(compute_path);
    return load_compute_from_source(comp_src);
}

Shader Shader::load_compute_from_source(const std::string& compute_src) {
    Shader shader;
    uint32_t cs = compile_shader_stage(GL_COMPUTE_SHADER, compute_src, "Compute Shader");
    if (!cs) return shader;

    uint32_t program = glCreateProgram();
    glAttachShader(program, cs);
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        int log_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        std::vector<char> info_log(log_length > 0 ? log_length : 512);
        glGetProgramInfoLog(program, static_cast<int>(info_log.size()), nullptr, info_log.data());
        std::cerr << "[Shader] Compute Program Link Error:\n" << info_log.data() << "\n";
        glDeleteProgram(program);
    } else {
        shader.program_id_ = program;
    }

    glDeleteShader(cs);
    return shader;
}

void Shader::bind() const {
    if (program_id_) {
        glUseProgram(program_id_);
    }
}

void Shader::unbind() const {
    glUseProgram(0);
}

int32_t Shader::get_uniform_location(const std::string& name) {
    auto it = uniform_location_cache_.find(name);
    if (it != uniform_location_cache_.end()) {
        return it->second;
    }

    int32_t loc = glGetUniformLocation(program_id_, name.c_str());
    uniform_location_cache_[name] = loc;
    return loc;
}

void Shader::set_mat4(const std::string& name, const glm::mat4& matrix) {
    int32_t loc = get_uniform_location(name);
    if (loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(matrix));
    }
}

void Shader::set_vec4(const std::string& name, const glm::vec4& value) {
    int32_t loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform4fv(loc, 1, glm::value_ptr(value));
    }
}

void Shader::set_vec3(const std::string& name, const glm::vec3& value) {
    int32_t loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform3fv(loc, 1, glm::value_ptr(value));
    }
}

void Shader::set_vec2(const std::string& name, const glm::vec2& value) {
    int32_t loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform2fv(loc, 1, glm::value_ptr(value));
    }
}

void Shader::set_float(const std::string& name, float value) {
    int32_t loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform1f(loc, value);
    }
}

void Shader::set_int(const std::string& name, int value) {
    int32_t loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform1i(loc, value);
    }
}

} // namespace audio_voyager::graphics
