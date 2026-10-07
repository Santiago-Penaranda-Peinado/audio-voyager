#pragma once

#include "core/types.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>

namespace audio_voyager::director {

// =============================================================================
// Second-Order Harmonic Spring-Mass-Damper System
// Equation: x'' + 2*zeta*omega*x' + omega^2*(x - target) = F_audio
// =============================================================================
struct SpringMassDamper1D {
    float pos{0.0f};
    float vel{0.0f};

    constexpr SpringMassDamper1D() noexcept = default;
    constexpr explicit SpringMassDamper1D(float initial_pos) noexcept : pos(initial_pos), vel(0.0f) {}

    void reset(float initial_pos = 0.0f) noexcept {
        pos = initial_pos;
        vel = 0.0f;
    }

    void apply_impulse(float impulse) noexcept {
        vel += impulse;
        vel = std::clamp(vel, -80.0f, 80.0f);
    }

    // Semi-implicit symplectic Euler integration with sub-stepping for unconditional stability
    // Optional min_pos / max_pos implement physical bump stops with inelastic contact
    void step(float target, float force, float omega, float zeta, float dt,
              float min_pos = -1e9f, float max_pos = 1e9f) noexcept {
        constexpr int SUB_STEPS = 4;
        float sub_dt = dt / static_cast<float>(SUB_STEPS);
        for (int i = 0; i < SUB_STEPS; ++i) {
            float accel = (target - pos) * (omega * omega) - 2.0f * zeta * omega * vel + force;
            vel += accel * sub_dt;
            vel = std::clamp(vel, -80.0f, 80.0f);
            pos += vel * sub_dt;

            // Physical bump stops (inelastic travel limits)
            if (pos < min_pos) {
                pos = min_pos;
                if (vel < 0.0f) vel = 0.0f;
            } else if (pos > max_pos) {
                pos = max_pos;
                if (vel > 0.0f) vel = 0.0f;
            }
        }
    }
};

class AutonomousArtDirector {
public:
    AutonomousArtDirector();
    ~AutonomousArtDirector() = default;

    void update(const core::AudioSemanticVector& semantic, float dt);

    // Continuous World-Space Spline Track C(z) = (curve_x(z), curve_y(z))
    // Shared symmetrically between Art Director CPU kinematics and Raymarching GPU shader
    [[nodiscard]] static inline glm::vec2 get_track_spline(float z) noexcept {
        float x = 2.8f * std::sin(z * 0.035f) + 1.2f * std::sin(z * 0.075f);
        float y = 2.7f + 0.55f * std::sin(z * 0.025f) + 0.25f * std::sin(z * 0.060f);
        return glm::vec2(x, y);
    }

    [[nodiscard]] static inline glm::vec2 get_track_spline_deriv(float z) noexcept {
        float dx = 2.8f * 0.035f * std::cos(z * 0.035f) + 1.2f * 0.075f * std::cos(z * 0.075f);
        float dy = 0.55f * 0.025f * std::cos(z * 0.025f) + 0.25f * 0.060f * std::cos(z * 0.060f);
        return glm::vec2(dx, dy);
    }

    [[nodiscard]] static inline glm::vec2 get_track_spline_deriv2(float z) noexcept {
        float d2x = -2.8f * 0.035f * 0.035f * std::sin(z * 0.035f) - 1.2f * 0.075f * 0.075f * std::sin(z * 0.075f);
        float d2y = -0.55f * 0.025f * 0.025f * std::sin(z * 0.025f) - 0.25f * 0.060f * 0.060f * std::sin(z * 0.060f);
        return glm::vec2(d2x, d2y);
    }

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

    // Suspension Telemetry (for physical debugging and unit testing)
    [[nodiscard]] const SpringMassDamper1D& get_suspension_y() const noexcept { return suspension_y_; }
    [[nodiscard]] const SpringMassDamper1D& get_suspension_x() const noexcept { return suspension_x_; }
    [[nodiscard]] const SpringMassDamper1D& get_suspension_roll() const noexcept { return suspension_roll_; }

private:
    glm::vec3 camera_pos_{0.0f, 2.7f, 0.0f};
    glm::vec3 camera_dir_{0.0f, 0.0f, 1.0f};
    glm::vec3 camera_up_{0.0f, 1.0f, 0.0f};
    glm::vec3 laser_pos_{0.0f, 2.7f, 8.0f};

    float current_z_{0.0f};
    float elapsed_time_{0.0f};
    float fov_radians_{glm::radians(68.0f)};

    float smooth_roll_{0.0f};
    float smooth_speed_{3.0f};
    float trauma_{0.0f};

    // Onset edge trigger and refractory re-arm state
    bool prev_is_onset_{false};
    float onset_rearm_timer_{0.0f};

    // Second-Order Harmonic Spring-Mass-Damper Suspension System
    SpringMassDamper1D suspension_y_{2.7f};        // Vertical bounce & compression
    SpringMassDamper1D suspension_x_{0.0f};        // Lateral centrifugal sway
    SpringMassDamper1D suspension_roll_{0.0f};     // Dynamic 6DoF centrifugal banking
    SpringMassDamper1D suspension_pitch_{0.0f};    // Drum-kick head nod & recoil
    SpringMassDamper1D suspension_fov_{glm::radians(68.0f)}; // Elastic drop punch
};

} // namespace audio_voyager::director
