#include "director/autonomous_art_director.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>

namespace audio_voyager::director {

AutonomousArtDirector::AutonomousArtDirector() {
    glm::vec2 t0 = get_track_spline(0.0f);
    suspension_y_.reset(t0.y);
    suspension_x_.reset(t0.x);
    suspension_roll_.reset(0.0f);
    suspension_pitch_.reset(0.0f);
    suspension_fov_.reset(glm::radians(68.0f));

    camera_pos_ = glm::vec3(t0.x, t0.y, 0.0f);
    camera_dir_ = glm::vec3(0.0f, 0.0f, 1.0f);
    camera_up_ = glm::vec3(0.0f, 1.0f, 0.0f);
    laser_pos_ = glm::vec3(t0.x, t0.y, 8.0f);
}

void AutonomousArtDirector::update(const core::AudioSemanticVector& semantic, float dt) {
    dt = std::clamp(dt, 0.001f, 0.05f);
    elapsed_time_ += dt;

    // 1. Musical Speed: Dynamic acceleration/deceleration directly driven by DSP
    float target_speed = semantic.is_silent ? 0.20f : semantic.speed_forward;
    float speed_diff = std::abs(target_speed - smooth_speed_);
    float director_speed_rate = (semantic.gear_shift_pulse > 0.15f || speed_diff > 3.5f) ? 22.0f : 8.0f;
    smooth_speed_ += (target_speed - smooth_speed_) * (1.0f - std::exp(-director_speed_rate * dt));
    current_z_ += smooth_speed_ * dt;

    // 2. World-Space Spline Sampling C(z) = (curve_x(z), curve_y(z))
    glm::vec2 track_now    = get_track_spline(current_z_);
    glm::vec2 track_deriv2 = get_track_spline_deriv2(current_z_);
    glm::vec2 track_look   = get_track_spline(current_z_ + 22.0f);
    glm::vec2 track_entity = get_track_spline(current_z_ + 8.0f);

    // 3. Centrifugal Acceleration: Physical lateral acceleration outward from track curvature
    // a_centrifugal = -v^2 * (d^2 x / dz^2)
    float speed_sq = smooth_speed_ * smooth_speed_;
    float a_centrifugal = -track_deriv2.x * speed_sq;

    // Discrete Onset Event Detection with Refractory Cooldown (handles 1300 BPM drum rolls, ignores sustained tones)
    bool onset_triggered = semantic.is_onset && (!prev_is_onset_ || onset_rearm_timer_ <= 0.0f);
    if (semantic.is_onset) {
        onset_rearm_timer_ -= dt;
    } else {
        onset_rearm_timer_ = 0.045f;
    }
    prev_is_onset_ = semantic.is_onset;

    // 4. Heavy Rock & Bass Drop Trauma Shake:
    if (semantic.is_onset) {
        float kick_strength = semantic.weight_metal * 1.4f + semantic.weight_dubstep * 1.1f;
        trauma_ = std::clamp(trauma_ + 0.35f * kick_strength * semantic.arousal, 0.0f, 1.0f);
    }
    trauma_ = std::max(0.0f, trauma_ - 2.5f * dt);

    float shake_intensity = trauma_ * trauma_ * 0.05f;
    float shake_x = std::sin(current_z_ * 37.0f) * std::cos(elapsed_time_ * 45.0f) * shake_intensity;
    float shake_y = std::cos(current_z_ * 29.0f) * shake_intensity * 0.6f;

    // 5. Vertical Spring-Mass-Damper Suspension (Kinetic Bounce & Compression):
    // Purely music-driven vertical altitude target:
    // Buildup/arousal lifts camera elevation toward ceiling vault, while silent passages return to nominal track
    float energy_lift = semantic.arousal * 0.85f + semantic.melodic_mids * 0.45f + semantic.elastic_dilation * 0.30f;
    float target_y = track_now.y + (semantic.is_silent ? 0.0f : energy_lift);
    target_y = std::max(target_y, 2.2f);

    // Frame-rate invariant physical kick impulse on drum kicks, metal double-bass, and drops:
    float kick_weight = semantic.weight_metal * 1.5f + semantic.weight_dubstep * 1.3f + semantic.arousal * 0.9f + semantic.gear_shift_pulse * 1.6f;
    if (onset_triggered && kick_weight > 0.15f) {
        // Calibrated impulse (corresponds to -36 m/s^2 * 0.02s) imparting deterministic physical compression
        suspension_y_.apply_impulse(-kick_weight * 0.72f);
    }

    // 2nd-order harmonic spring-mass-damper (omega=15 rad/s, zeta=0.60 sub-critical damping)
    // Physical bump stop at 1.95m prevents negative suspension plunge under any audio input
    suspension_y_.step(target_y, 0.0f, 15.0f, 0.60f, dt, 1.95f, 6.0f);
    float cam_y = std::max(suspension_y_.pos, 2.0f);

    // 6. Lateral Spring-Mass-Damper Suspension (Centrifugal Sway):
    // Suspension mass sways outward under centrifugal acceleration when cornering along curves
    float f_sway = a_centrifugal * 0.45f;
    suspension_x_.step(track_now.x, f_sway, 13.0f, 0.65f, dt);

    // Final Camera Position with guaranteed flight clearance (Y >= 2.0m)
    camera_pos_ = glm::vec3(
        suspension_x_.pos + shake_x,
        cam_y + shake_y,
        current_z_
    );
    camera_pos_.y = std::max(camera_pos_.y, 2.0f);

    // 7. Dynamic Centrifugal Suspension Banking (Roll):
    // 2nd-order rotational spring-mass-damper rolls into curves (banks into turn) and recoils elastically
    float target_roll = std::clamp(a_centrifugal * 0.055f, -0.38f, 0.38f) + shake_x * 0.35f;
    suspension_roll_.step(target_roll, 0.0f, 11.0f, 0.62f, dt);
    smooth_roll_ = suspension_roll_.pos;

    // 8. Dynamic Rhythmic Headbanging Pitch Nod (Cabeceo):
    // Spring-mass-damper nods down momentarily on heavy drum kicks and immediately rebounds
    if (onset_triggered && kick_weight > 0.20f) {
        suspension_pitch_.apply_impulse(-kick_weight * 0.44f);
    }
    suspension_pitch_.step(0.0f, 0.0f, 18.0f, 0.68f, dt);

    // 9. Look Direction: Proudly forward toward horizon, following winding spline track with speed-adaptive preview
    // USER REQUIREMENT: Camera must NOT pitch down excessively (never stare at floor!)
    float look_ahead_dist = std::clamp(18.0f + smooth_speed_ * 0.45f, 20.0f, 36.0f);
    glm::vec2 track_look_dyn = get_track_spline(current_z_ + look_ahead_dist);
    glm::vec3 look_target(
        track_look_dyn.x,
        track_look_dyn.y + 0.10f + suspension_pitch_.pos,
        current_z_ + look_ahead_dist
    );
    glm::vec3 to_target = look_target - camera_pos_;
    to_target.y = std::clamp(to_target.y, -0.55f, 1.8f); // strictly prevent looking down at floor
    if (glm::length(to_target) > 0.001f) {
        camera_dir_ = glm::normalize(to_target);
    }

    // Rotate world UP vector around camera direction by bank angle
    glm::vec3 world_up(0.0f, 1.0f, 0.0f);
    glm::mat4 roll_mat = glm::rotate(glm::mat4(1.0f), smooth_roll_, camera_dir_);
    camera_up_ = glm::normalize(glm::vec3(roll_mat * glm::vec4(world_up, 0.0f)));

    // 10. 3D Laser Ribbon Entity Point Light (Flies along track ahead of camera)
    laser_pos_ = glm::vec3(track_entity.x, track_entity.y + 0.15f, current_z_ + 8.0f);

    // 11. Cinematic Dynamic Field of View with Elastic Drop Punch (2nd-Order Spring):
    float warp_fov_boost = std::clamp((smooth_speed_ - 3.0f) / 30.0f, 0.0f, 1.0f) * glm::radians(12.0f);
    float target_fov = glm::radians(66.0f) + warp_fov_boost + semantic.arousal * glm::radians(5.0f);

    if (onset_triggered && (semantic.gear_shift_pulse > 0.18f || semantic.arousal > 0.25f || semantic.weight_metal > 0.20f || semantic.weight_dubstep > 0.20f)) {
        float kick = std::max(semantic.gear_shift_pulse, semantic.arousal * 0.85f + 0.25f);
        suspension_fov_.apply_impulse(kick * 0.90f);
    }
    suspension_fov_.step(target_fov, 0.0f, 16.0f, 0.65f, dt);
    fov_radians_ = suspension_fov_.pos;

    // Safety checks
    if (std::isnan(camera_pos_.x) || std::isnan(camera_pos_.y) || std::isnan(camera_pos_.z) ||
        std::isinf(camera_pos_.x) || std::isinf(camera_pos_.y) || std::isinf(camera_pos_.z)) {
        current_z_ = 0.0f;
        glm::vec2 t0 = get_track_spline(0.0f);
        camera_pos_ = glm::vec3(t0.x, t0.y, 0.0f);
        camera_dir_ = glm::vec3(0.0f, 0.0f, 1.0f);
        camera_up_ = glm::vec3(0.0f, 1.0f, 0.0f);
        smooth_roll_ = 0.0f;
        suspension_y_.reset(t0.y);
        suspension_x_.reset(t0.x);
        suspension_roll_.reset(0.0f);
        suspension_pitch_.reset(0.0f);
        suspension_fov_.reset(glm::radians(68.0f));
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
