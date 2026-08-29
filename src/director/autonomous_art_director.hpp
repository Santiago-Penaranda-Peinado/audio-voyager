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
    [[nodiscard]] float get_camera_roll() const noexcept { return camera_roll_; }
    [[nodiscard]] float get_fov_radians() const noexcept { return fov_radians_; }
    [[nodiscard]] glm::vec3 get_laser_pos() const noexcept { return laser_pos_; }

    [[nodiscard]] glm::mat4 get_view_matrix() const;
    [[nodiscard]] glm::mat4 get_projection_matrix(float aspect) const;
    [[nodiscard]] glm::mat4 get_view_projection_matrix(float aspect) const;

    static glm::vec2 get_path_xy(float z) noexcept;

private:
    float current_z_{0.0f};
    glm::vec3 camera_pos_{0.0f, 0.0f, 0.0f};
    glm::vec3 camera_dir_{0.0f, 0.0f, 1.0f};
    glm::vec3 camera_up_{0.0f, 1.0f, 0.0f};
    float camera_roll_{0.0f};
    float fov_radians_{1.1868f}; // ~68 degrees

    glm::vec3 laser_pos_{0.0f, 0.0f, 4.0f};

    float smooth_roll_{0.0f};
    float smooth_fov_{1.1868f};
};

} // namespace audio_voyager::director
