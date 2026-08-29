#include "director/autonomous_art_director.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>

namespace audio_voyager::director {

AutonomousArtDirector::AutonomousArtDirector() {
    glm::vec2 p0 = get_path_xy(0.0f);
    camera_pos_ = glm::vec3(p0.x, p0.y, 0.0f);
    glm::vec2 p_ahead = get_path_xy(6.0f);
    camera_dir_ = glm::normalize(glm::vec3(p_ahead.x - p0.x, p_ahead.y - p0.y, 6.0f));
}

glm::vec2 AutonomousArtDirector::get_path_xy(float z) noexcept {
    float x = 2.4f * std::sin(z * 0.075f) + 1.2f * std::cos(z * 0.16f);
    float y = 1.6f * std::cos(z * 0.055f) + 0.8f * std::sin(z * 0.12f);
    return glm::vec2(x, y);
}

void AutonomousArtDirector::update(const core::AudioSemanticVector& semantic, float dt) {
    dt = std::clamp(dt, 0.001f, 0.05f);

    // 1. Advance along Z axis (Warp Speed)
    current_z_ += semantic.speed_forward * dt * 4.5f;

    // 2. Current Path Point & Target Path Point Ahead
    glm::vec2 current_xy = get_path_xy(current_z_);
    glm::vec2 ahead_xy = get_path_xy(current_z_ + 7.5f);

    glm::vec3 target_pos(current_xy.x, current_xy.y, current_z_);
    glm::vec3 target_ahead(ahead_xy.x, ahead_xy.y, current_z_ + 7.5f);

    // Kinetic turbulence on high dissonance / beat drop
    if (semantic.norm_dissonance > 0.45f || semantic.glitch_intensity > 0.3f) {
        float shake = semantic.glitch_intensity * 0.12f;
        target_pos.x += std::sin(current_z_ * 22.0f) * shake;
        target_pos.y += std::cos(current_z_ * 28.0f) * shake;
    }

    // Smooth camera position interpolation
    camera_pos_ += (target_pos - camera_pos_) * (1.0f - std::exp(-15.0f * dt));
    glm::vec3 to_ahead = target_ahead - camera_pos_;
    if (glm::length(to_ahead) > 0.001f) {
        camera_dir_ = glm::normalize(to_ahead);
    }

    // 3. Banking Roll Angle (Tilt in curves + dissonance twist)
    glm::vec2 path_tangent = get_path_xy(current_z_ + 0.5f) - get_path_xy(current_z_ - 0.5f);
    float curve_bank = -path_tangent.x * 0.45f;
    float audio_bank = semantic.norm_dissonance * 0.65f * std::sin(current_z_ * 0.20f);
    float target_roll = curve_bank + audio_bank;

    smooth_roll_ += (target_roll - smooth_roll_) * (1.0f - std::exp(-8.0f * dt));
    camera_roll_ = smooth_roll_;

    // Calculate Roll-adjusted Up vector
    glm::vec3 world_up(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::cross(camera_dir_, world_up);
    if (glm::length(right) > 0.001f) {
        right = glm::normalize(right);
        glm::vec3 base_up = glm::normalize(glm::cross(right, camera_dir_));
        camera_up_ = base_up * std::cos(camera_roll_) + right * std::sin(camera_roll_);
    } else {
        camera_up_ = world_up;
    }

    // 4. Dynamic Field of View
    float speed_ratio = std::clamp((semantic.speed_forward - 1.5f) / 4.5f, 0.0f, 1.0f);
    float fov_burst = semantic.is_onset ? 14.0f : (semantic.norm_energy * 10.0f);
    float target_fov = glm::radians(68.0f + speed_ratio * 20.0f + fov_burst);
    smooth_fov_ += (target_fov - smooth_fov_) * (1.0f - std::exp(-10.0f * dt));
    fov_radians_ = smooth_fov_;

    // 5. Position 3D Laser Ribbon Light in front of Camera
    laser_pos_ = camera_pos_ + camera_dir_ * 4.2f;

    // Safety checks for NaN / Inf
    if (std::isnan(camera_pos_.x) || std::isnan(camera_pos_.y) || std::isnan(camera_pos_.z) ||
        std::isinf(camera_pos_.x) || std::isinf(camera_pos_.y) || std::isinf(camera_pos_.z)) {
        current_z_ = 0.0f;
        camera_pos_ = glm::vec3(0.0f);
        camera_dir_ = glm::vec3(0.0f, 0.0f, 1.0f);
        camera_up_ = glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

[[nodiscard]] glm::mat4 AutonomousArtDirector::get_view_matrix() const {
    return glm::lookAt(camera_pos_, camera_pos_ + camera_dir_, camera_up_);
}

[[nodiscard]] glm::mat4 AutonomousArtDirector::get_projection_matrix(float aspect) const {
    return glm::perspective(fov_radians_, aspect, 0.1f, 300.0f);
}

[[nodiscard]] glm::mat4 AutonomousArtDirector::get_view_projection_matrix(float aspect) const {
    return get_projection_matrix(aspect) * get_view_matrix();
}

} // namespace audio_voyager::director
