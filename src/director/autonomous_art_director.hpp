#pragma once

#include "core/types.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace audio_voyager::director {

class AutonomousArtDirector {
public:
    AutonomousArtDirector();
    ~AutonomousArtDirector() = default;

    void update(const core::AudioSemanticVector& semantic, float dt);

    [[nodiscard]] glm::vec3 get_camera_pos() const noexcept { return camera_pos_; }
    [[nodiscard]] glm::vec3 get_camera_dir() const noexcept { return camera_dir_; }
    [[nodiscard]] glm::vec3 get_camera_up() const noexcept { return camera_up_; }
    [[nodiscard]] float get_camera_roll() const noexcept { return smooth_roll_; }
    [[nodiscard]] float get_fov_radians() const noexcept { return fov_radians_; }
    [[nodiscard]] glm::vec3 get_laser_pos() const noexcept { return laser_pos_; }
    [[nodiscard]] float get_trauma() const noexcept { return trauma_; }

    [[nodiscard]] glm::mat4 get_view_matrix() const;
    [[nodiscard]] glm::mat4 get_projection_matrix(float aspect) const;
    [[nodiscard]] glm::mat4 get_view_projection_matrix(float aspect) const;

private:
    glm::vec3 compute_entity_path(float z, float time) const noexcept;

    glm::vec3 camera_pos_{0.0f, 2.8f, -5.0f};
    glm::vec3 camera_dir_{0.0f, -0.05f, 1.0f};
    glm::vec3 camera_up_{0.0f, 1.0f, 0.0f};
    glm::vec3 laser_pos_{0.0f, 1.0f, 0.0f};

    float current_z_{0.0f};
    float elapsed_time_{0.0f};
    float fov_radians_{glm::radians(68.0f)};

    float smooth_pitch_{0.0f};
    float smooth_roll_{0.0f};
    float smooth_bob_y_{2.8f};
    float smooth_fov_{glm::radians(68.0f)};
    float smooth_speed_{1.4f};
    float headbang_impulse_{0.0f};
    float trauma_{0.0f};
};

} // namespace audio_voyager::director
