#include "director/autonomous_art_director.hpp"
#include <cmath>
#include <algorithm>

namespace audio_voyager::director {

AutonomousArtDirector::AutonomousArtDirector() {
    glm::vec3 entity_p = compute_entity_path(0.0f);
    laser_pos_ = entity_p;
    camera_pos_ = glm::vec3(entity_p.x * 0.65f, 2.8f, -6.0f);
    camera_dir_ = glm::normalize(entity_p + glm::vec3(0.0f, -0.3f, 8.0f) - camera_pos_);
    camera_up_ = glm::vec3(0.0f, 1.0f, 0.0f);

    camera_pos_spring_.reset(camera_pos_);
    laser_pos_spring_.reset(laser_pos_);
    pitch_spring_.reset(0.0f);
    roll_spring_.reset(0.0f);
    fov_spring_.reset(glm::radians(70.0f));
}

glm::vec3 AutonomousArtDirector::compute_entity_path(float z) const noexcept {
    // Sinuous organic flight of the laser ribbon entity at comfortable altitude
    float x = 2.0f * std::sin(z * 0.035f) + 0.6f * std::cos(z * 0.070f);
    float y = 1.0f + 0.4f * std::cos(z * 0.030f);
    return glm::vec3(x, y, z);
}

void AutonomousArtDirector::update(const core::AudioSemanticVector& semantic, float dt) {
    dt = std::clamp(dt, 0.001f, 0.05f);

    // 1. Kinetic Warp Drive Velocity along Z axis
    current_z_ += semantic.speed_forward * dt * 2.2f;

    // 2. Compute Entity Ribbon Path ahead
    glm::vec3 entity_pos = compute_entity_path(current_z_ + 7.0f);
    glm::vec3 entity_ahead = compute_entity_path(current_z_ + 16.0f);
    laser_pos_spring_.update(entity_pos, dt);
    laser_pos_ = laser_pos_spring_.get_value();

    // 3. Audio-Driven Dynamic Pitch Nod
    float target_pitch = -0.012f * semantic.elastic_dilation - (semantic.is_onset ? 0.015f : 0.0f) + 0.005f;
    pitch_spring_.update(target_pitch, dt);
    float pitch_val = pitch_spring_.get_value();

    // 4. Camera Positioning (Strict Altitude Clearance >= 2.2m)
    float target_alt = 2.6f + semantic.elastic_dilation * 0.40f + (semantic.is_onset ? 0.15f : 0.0f);
    target_alt = std::max(target_alt, 2.2f);

    glm::vec3 target_cam_pos(
        entity_pos.x * 0.65f,
        target_alt,
        current_z_
    );

    camera_pos_spring_.update(target_cam_pos, dt);
    camera_pos_ = camera_pos_spring_.get_value();
    camera_pos_.y = std::max(camera_pos_.y, 2.1f);

    // 5. Look Direction (Proudly forward toward horizon and entity)
    glm::vec3 look_target = entity_ahead + glm::vec3(0.0f, -0.30f + pitch_val * 8.0f, 0.0f);
    glm::vec3 to_target = look_target - camera_pos_;
    if (glm::length(to_target) > 0.001f) {
        camera_dir_ = glm::normalize(to_target);
    }

    // 6. Aerodynamic Banking Roll
    float turn_curvature = -2.0f * 0.035f * 0.035f * std::sin(current_z_ * 0.035f);
    float target_roll = std::clamp(turn_curvature * 8.0f + semantic.camera_roll, -0.15f, 0.15f);
    roll_spring_.update(target_roll, dt);
    camera_roll_ = roll_spring_.get_value();

    glm::vec3 world_up(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(camera_dir_, world_up));
    if (glm::length(right) > 0.001f) {
        camera_up_ = glm::normalize(std::cos(camera_roll_) * world_up + std::sin(camera_roll_) * right);
    } else {
        camera_up_ = world_up;
    }

    // 7. Dynamic Warp FOV [68 deg to 84 deg]
    float speed_ratio = std::clamp((semantic.speed_forward - 1.2f) / 3.0f, 0.0f, 1.0f);
    float target_fov = glm::radians(68.0f + speed_ratio * 14.0f + (semantic.is_onset ? 2.5f : 0.0f));
    fov_spring_.update(target_fov, dt);
    fov_radians_ = fov_spring_.get_value();

    // Safety checks
    if (std::isnan(camera_pos_.x) || std::isnan(camera_pos_.y) || std::isnan(camera_pos_.z) ||
        std::isinf(camera_pos_.x) || std::isinf(camera_pos_.y) || std::isinf(camera_pos_.z)) {
        current_z_ = 0.0f;
        camera_pos_ = glm::vec3(0.0f, 2.8f, -6.0f);
        camera_dir_ = glm::vec3(0.0f, -0.05f, 1.0f);
        camera_up_ = glm::vec3(0.0f, 1.0f, 0.0f);
        camera_pos_spring_.reset(camera_pos_);
        laser_pos_spring_.reset(compute_entity_path(0.0f));
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
