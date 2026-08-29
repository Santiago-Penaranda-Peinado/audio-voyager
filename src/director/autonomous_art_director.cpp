#include "director/autonomous_art_director.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>

namespace audio_voyager::director {

AutonomousArtDirector::AutonomousArtDirector() {
    glm::vec3 entity_p = compute_entity_path(0.0f, 0.0f);
    laser_pos_ = entity_p;
    camera_pos_ = glm::vec3(entity_p.x, 2.8f, entity_p.z - 6.0f);
    camera_dir_ = glm::normalize(entity_p + glm::vec3(0.0f, -0.2f, 8.0f) - camera_pos_);
}

glm::vec3 AutonomousArtDirector::compute_entity_path(float z, float time) const noexcept {
    // Sinuous flight of the laser ribbon entity at comfortable altitude
    float x = 2.2f * std::sin(z * 0.040f + time * 0.18f) + 0.8f * std::cos(z * 0.08f);
    float y = 0.8f + 0.6f * std::cos(z * 0.030f + time * 0.15f);
    return glm::vec3(x, y, z);
}

void AutonomousArtDirector::update(const core::AudioSemanticVector& semantic, float dt) {
    dt = std::clamp(dt, 0.001f, 0.05f);

    // 1. Musical Speed: Scaled with detected BPM, energy and speed multiplier
    float bpm_factor = std::clamp((semantic.bpm - 60.0f) / 120.0f, 0.0f, 1.5f);
    float target_speed = semantic.is_silent ? 0.15f : (1.1f + 0.8f * bpm_factor + semantic.arousal * 0.6f);
    smooth_speed_ += (target_speed - smooth_speed_) * (1.0f - std::exp(-5.0f * dt));

    current_z_ += smooth_speed_ * dt * 2.2f;

    // 2. Compute Entity Ribbon Path ahead
    glm::vec3 entity_pos = compute_entity_path(current_z_ + 7.0f, current_z_ * 0.10f);
    glm::vec3 entity_ahead = compute_entity_path(current_z_ + 16.0f, current_z_ * 0.10f);
    laser_pos_ = entity_pos;

    // 3. Gentle Musical Breathing & Pitch Nod (Bobs smoothly, never looks straight down)
    float target_bob_y = 2.6f + semantic.elastic_dilation * 0.50f + (semantic.is_onset ? 0.20f : 0.0f);
    smooth_bob_y_ += (target_bob_y - smooth_bob_y_) * (1.0f - std::exp(-12.0f * dt));

    // Subtle pitch nod (+/- 1.5 degrees, looking forward toward horizon)
    float target_pitch = -0.015f * semantic.elastic_dilation - (semantic.is_onset ? 0.020f : 0.0f) + 0.010f;
    smooth_pitch_ += (target_pitch - smooth_pitch_) * (1.0f - std::exp(-16.0f * dt));

    // 4. Camera Positioning (Strict Altitude Clearance >= 2.0m)
    glm::vec3 target_cam_pos(
        entity_pos.x * 0.65f,
        std::max(smooth_bob_y_, 2.2f),
        current_z_
    );

    camera_pos_ += (target_cam_pos - camera_pos_) * (1.0f - std::exp(-7.0f * dt));
    camera_pos_.y = std::max(camera_pos_.y, 2.0f);

    // 5. Look Direction (Proudly forward toward horizon and entity)
    glm::vec3 look_target = entity_ahead + glm::vec3(0.0f, -0.4f + smooth_pitch_ * 8.0f, 0.0f);
    glm::vec3 to_target = look_target - camera_pos_;
    if (glm::length(to_target) > 0.001f) {
        camera_dir_ = glm::normalize(to_target);
    }

    // Up vector is strictly world UP
    camera_up_ = glm::vec3(0.0f, 1.0f, 0.0f);

    // 6. Cinematic Field of View
    float target_fov = glm::radians(67.0f + semantic.arousal * 6.0f + (semantic.is_onset ? 3.0f : 0.0f));
    smooth_fov_ += (target_fov - smooth_fov_) * (1.0f - std::exp(-6.0f * dt));
    fov_radians_ = smooth_fov_;

    // Safety checks
    if (std::isnan(camera_pos_.x) || std::isnan(camera_pos_.y) || std::isnan(camera_pos_.z) ||
        std::isinf(camera_pos_.x) || std::isinf(camera_pos_.y) || std::isinf(camera_pos_.z)) {
        current_z_ = 0.0f;
        camera_pos_ = glm::vec3(0.0f, 2.8f, -5.0f);
        camera_dir_ = glm::vec3(0.0f, -0.05f, 1.0f);
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
