#pragma once

#include "core/types.hpp"
#include "director/critically_damped_spring.hpp"
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

private:
    glm::vec3 compute_entity_path(float z) const noexcept;

    glm::vec3 camera_pos_{0.0f, 2.8f, -6.0f};
    glm::vec3 camera_dir_{0.0f, -0.05f, 1.0f};
    glm::vec3 camera_up_{0.0f, 1.0f, 0.0f};
    glm::vec3 laser_pos_{0.0f, 1.0f, 0.0f};

    float current_z_{0.0f};
    float camera_roll_{0.0f};
    float fov_radians_{glm::radians(70.0f)};

    CriticallyDampedSpring<glm::vec3> camera_pos_spring_{glm::vec3(0.0f, 2.8f, -6.0f), 8.0f};
    CriticallyDampedSpring<glm::vec3> laser_pos_spring_{glm::vec3(0.0f, 1.0f, 0.0f), 10.0f};
    CriticallyDampedSpring<float>     pitch_spring_{0.0f, 10.0f};
    CriticallyDampedSpring<float>     roll_spring_{0.0f, 6.0f};
    CriticallyDampedSpring<float>     fov_spring_{glm::radians(70.0f), 5.0f};
};

} // namespace audio_voyager::director
