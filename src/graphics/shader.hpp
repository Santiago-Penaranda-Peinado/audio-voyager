#pragma once

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace audio_voyager::graphics {

class Shader {
public:
    Shader() = default;
    ~Shader();

    // Non-copyable, movable
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    static Shader load_graphics_from_files(const std::string& vertex_path, const std::string& fragment_path);
    static Shader load_compute_from_file(const std::string& compute_path);

    static Shader load_graphics_from_source(const std::string& vertex_src, const std::string& fragment_src);
    static Shader load_compute_from_source(const std::string& compute_src);

    void bind() const;
    void unbind() const;
    [[nodiscard]] uint32_t get_id() const noexcept { return program_id_; }
    [[nodiscard]] bool is_valid() const noexcept { return program_id_ != 0; }

    void set_mat4(const std::string& name, const glm::mat4& matrix);
    void set_vec4(const std::string& name, const glm::vec4& value);
    void set_vec3(const std::string& name, const glm::vec3& value);
    void set_vec2(const std::string& name, const glm::vec2& value);
    void set_float(const std::string& name, float value);
    void set_int(const std::string& name, int value);

private:
    uint32_t program_id_{0};
    std::unordered_map<std::string, int32_t> uniform_location_cache_;

    int32_t get_uniform_location(const std::string& name);
    static uint32_t compile_shader_stage(uint32_t stage_type, const std::string& source, const std::string& debug_name);
    static std::string read_file_to_string(const std::string& path);
};

} // namespace audio_voyager::graphics
