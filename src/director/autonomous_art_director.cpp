#include "director/autonomous_art_director.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>

namespace audio_voyager::director {

AutonomousArtDirector::AutonomousArtDirector() {
    music_altitude_ = 2.6f;
    glm::vec3 entity_p = compute_entity_path(0.0f, 0.0f);
    laser_pos_ = entity_p;
    camera_pos_ = glm::vec3(entity_p.x, 2.7f, entity_p.z - 6.0f);
    camera_dir_ = glm::vec3(0.0f, 0.0f, 1.0f);
    camera_up_ = glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::vec3 AutonomousArtDirector::compute_entity_path(float z, float time) const noexcept {
    // Sinuous lateral curves
    float x = 2.2f * std::sin(z * 0.040f + time * 0.18f) + 0.8f * std::cos(z * 0.08f);
    // Vertical axis: 100% music-driven altitude (NO scripted sine/cosine waves!)
    float y = music_altitude_;
    return glm::vec3(x, y, z);
}

void AutonomousArtDirector::update(const core::AudioSemanticVector& semantic, float dt) {
    dt = std::clamp(dt, 0.001f, 0.05f);
    elapsed_time_ += dt;

    // 1. Musical Speed: Directly driven by SemanticBrain's multi-genre kinetic DSP
    float target_speed = semantic.is_silent ? 0.20f : semantic.speed_forward;
    float speed_diff = std::abs(target_speed - smooth_speed_);
    float director_speed_rate = (semantic.gear_shift_pulse > 0.15f || speed_diff > 3.5f) ? 22.0f : 8.0f;
    smooth_speed_ += (target_speed - smooth_speed_) * (1.0f - std::exp(-director_speed_rate * dt));

    current_z_ += smooth_speed_ * dt;

    // 2. Purely Music-Driven Vertical Elevation (Tunnel Elevation/Undulation)
    // USER REQUIREMENT: The vertical axis must NOT be a scripted sine wave or pre-baked animation!
    // Driven dynamically by extracted audio features:
    // - Buildup/Energy lift: energy & arousal accumulate upward climb
    // - Melodic vocal expression: mid-band resonance expands altitude
    // - Transient beat drop dive: explosive kicks trigger sudden visceral dip
    if (semantic.is_silent) {
        music_target_altitude_ = 2.6f;
        music_dive_impulse_ *= std::exp(-10.0f * dt);
    } else {
        float energy_lift = semantic.arousal * 0.85f + semantic.melodic_mids * 0.45f + semantic.elastic_dilation * 0.30f;
        if (semantic.is_onset && (semantic.arousal > 0.35f || semantic.weight_metal > 0.30f || semantic.weight_dubstep > 0.30f)) {
            music_dive_impulse_ = std::min(music_dive_impulse_ + 0.25f * (semantic.arousal + 0.2f), 0.50f);
        }
        music_dive_impulse_ *= std::exp(-7.0f * dt);
        music_target_altitude_ = 2.6f + energy_lift - music_dive_impulse_;
    }

    music_altitude_ += (music_target_altitude_ - music_altitude_) * (1.0f - std::exp(-3.5f * dt));
    music_altitude_ = std::clamp(music_altitude_, 2.2f, 4.2f);

    // 3. Compute Entity Ribbon Path ahead (symmetric 8-meter steps for unbiased curvature)
    glm::vec3 entity_pos   = compute_entity_path(current_z_ + 8.0f,  elapsed_time_);
    glm::vec3 entity_ahead = compute_entity_path(current_z_ + 16.0f, elapsed_time_);
    glm::vec3 entity_far   = compute_entity_path(current_z_ + 24.0f, elapsed_time_);
    laser_pos_ = entity_pos;

    // 4. Rhythmic Headbanging Nod & Vertical Bobbing:
    // Reactive headbanging pitch nod (cabeceo) to drum kicks with physical recoil
    if (semantic.is_onset && (semantic.weight_metal > 0.20f || semantic.weight_dubstep > 0.20f || semantic.arousal > 0.40f)) {
        headbang_impulse_ = std::min(headbang_impulse_ + 0.06f * (semantic.weight_metal + semantic.weight_dubstep + 0.6f), 0.12f);
    }
    headbang_impulse_ *= std::exp(-12.0f * dt);

    // 5. Heavy Rock & Bass Drop Trauma Shake (Visceral Acoustic Impact):
    if (semantic.is_onset) {
        float kick_strength = semantic.weight_metal * 1.4f + semantic.weight_dubstep * 1.1f;
        trauma_ = std::clamp(trauma_ + 0.35f * kick_strength * semantic.arousal, 0.0f, 1.0f);
    }
    trauma_ = std::max(0.0f, trauma_ - 2.5f * dt);

    float shake_intensity = trauma_ * trauma_ * 0.05f;
    float shake_x = std::sin(current_z_ * 37.0f) * std::cos(elapsed_time_ * 45.0f) * shake_intensity;
    float shake_y = std::cos(current_z_ * 29.0f) * shake_intensity * 0.6f;

    // 6. Camera Positioning: Strictly follows the tunnel elevation with guaranteed clearance (pos.y >= 2.0m)
    float target_cam_y = std::max(music_altitude_ + 0.10f + shake_y, 2.2f);
    glm::vec3 target_cam_pos(
        entity_pos.x * 0.65f + shake_x,
        target_cam_y,
        current_z_
    );

    camera_pos_ += (target_cam_pos - camera_pos_) * (1.0f - std::exp(-7.0f * dt));
    camera_pos_.y = std::max(camera_pos_.y, 2.0f);

    // 7. Look Direction: Proudly forward toward horizon, following tunnel path
    // USER REQUIREMENT: Camera must NOT pitch down excessively (never stare at floor!)
    // Subtle rhythmic cabeceo (head nod) on heavy kicks that immediately recoils
    glm::vec3 look_target = entity_ahead + glm::vec3(0.0f, 0.10f - headbang_impulse_ * 3.2f, 0.0f);
    glm::vec3 to_target = look_target - camera_pos_;
    to_target.y = std::clamp(to_target.y, -0.75f, 2.0f); // strictly prevent looking down at floor
    if (glm::length(to_target) > 0.001f) {
        camera_dir_ = glm::normalize(to_target);
    }

    // 8. True 6DoF Camera Banking (Roll):
    // Symmetric central difference for lateral trajectory curvature (d^2 x / dz^2)
    float turn_dx = (entity_far.x - 2.0f * entity_ahead.x + entity_pos.x);
    float speed_normalized = std::clamp(smooth_speed_ / 12.0f, 0.4f, 2.5f);
    float target_roll = -std::clamp(turn_dx * 0.40f * speed_normalized, -0.32f, 0.32f) + shake_x * 0.4f;
    smooth_roll_ += (target_roll - smooth_roll_) * (1.0f - std::exp(-5.0f * dt));

    // Rotate world UP vector around camera direction by bank angle
    glm::vec3 world_up(0.0f, 1.0f, 0.0f);
    glm::mat4 roll_mat = glm::rotate(glm::mat4(1.0f), smooth_roll_, camera_dir_);
    camera_up_ = glm::normalize(glm::vec3(roll_mat * glm::vec4(world_up, 0.0f)));

    // 9. Cinematic Dynamic Field of View with Elastic Drop Punch:
    if (semantic.gear_shift_pulse > 0.20f || (semantic.is_onset && (semantic.arousal > 0.25f || semantic.weight_metal > 0.20f || semantic.weight_dubstep > 0.20f))) {
        float kick_strength = std::max(semantic.gear_shift_pulse, semantic.arousal * 0.85f + 0.25f);
        fov_punch_impulse_ = std::min(fov_punch_impulse_ + 0.20f * kick_strength, 0.35f);
    }
    fov_punch_impulse_ *= std::exp(-9.5f * dt);

    float warp_fov_boost = std::clamp((smooth_speed_ - 3.0f) / 30.0f, 0.0f, 1.0f) * 12.0f;
    float target_fov = glm::radians(66.0f + warp_fov_boost + semantic.arousal * 5.0f + (semantic.is_onset ? 3.0f : 0.0f))
                       + fov_punch_impulse_ * glm::radians(24.0f);
    float fov_rate = (fov_punch_impulse_ > 0.04f || semantic.gear_shift_pulse > 0.15f) ? 22.0f : 6.0f;
    smooth_fov_ += (target_fov - smooth_fov_) * (1.0f - std::exp(-fov_rate * dt));
    fov_radians_ = smooth_fov_;

    // Safety checks
    if (std::isnan(camera_pos_.x) || std::isnan(camera_pos_.y) || std::isnan(camera_pos_.z) ||
        std::isinf(camera_pos_.x) || std::isinf(camera_pos_.y) || std::isinf(camera_pos_.z)) {
        current_z_ = 0.0f;
        camera_pos_ = glm::vec3(0.0f, 2.7f, -5.0f);
        camera_dir_ = glm::vec3(0.0f, 0.0f, 1.0f);
        camera_up_ = glm::vec3(0.0f, 1.0f, 0.0f);
        smooth_roll_ = 0.0f;
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
