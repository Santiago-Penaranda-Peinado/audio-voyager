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
    // Curvature scale is 100% music-driven: 0.0 in silence (dead straight), scaling with musical energy
    // 4 incommensurate spatial harmonics based on golden ratio phi ~ 1.618034
    static constexpr float SPLINE_KX1 = 0.0350f;
    static constexpr float SPLINE_KX2 = 0.07204f; // 0.0350 * phi^1.5
    static constexpr float SPLINE_KX3 = 0.05663f; // 0.0350 * phi
    static constexpr float SPLINE_KX4 = 0.02163f; // 0.0350 / phi
    static constexpr float SPLINE_AX1 = 2.4f;
    static constexpr float SPLINE_AX2 = 1.2f;
    static constexpr float SPLINE_AX3 = 0.3f;
    static constexpr float SPLINE_AX4 = 0.3f;

    static constexpr float SPLINE_KY1 = 0.0250f;
    static constexpr float SPLINE_KY2 = 0.05145f; // 0.0250 * phi^1.5
    static constexpr float SPLINE_KY3 = 0.04045f; // 0.0250 * phi
    static constexpr float SPLINE_KY4 = 0.01545f; // 0.0250 / phi
    static constexpr float SPLINE_AY1 = 0.45f;
    static constexpr float SPLINE_AY2 = 0.22f;
    static constexpr float SPLINE_AY3 = 0.12f;
    static constexpr float SPLINE_AY4 = 0.08f;

    [[nodiscard]] static inline glm::vec2 get_track_spline(float z, float curvature_scale = 1.0f) noexcept {
        float x = (SPLINE_AX1 * std::sin(z * SPLINE_KX1) +
                   SPLINE_AX2 * std::sin(z * SPLINE_KX2) +
                   SPLINE_AX3 * std::sin(z * SPLINE_KX3) +
                   SPLINE_AX4 * std::sin(z * SPLINE_KX4)) * curvature_scale;

        float y = 2.7f + (SPLINE_AY1 * std::sin(z * SPLINE_KY1) +
                          SPLINE_AY2 * std::sin(z * SPLINE_KY2) +
                          SPLINE_AY3 * std::sin(z * SPLINE_KY3) +
                          SPLINE_AY4 * std::sin(z * SPLINE_KY4)) * curvature_scale;
        return glm::vec2(x, y);
    }

    [[nodiscard]] static inline glm::vec2 get_track_spline_deriv(float z, float curvature_scale = 1.0f) noexcept {
        float dx = (SPLINE_AX1 * SPLINE_KX1 * std::cos(z * SPLINE_KX1) +
                    SPLINE_AX2 * SPLINE_KX2 * std::cos(z * SPLINE_KX2) +
                    SPLINE_AX3 * SPLINE_KX3 * std::cos(z * SPLINE_KX3) +
                    SPLINE_AX4 * SPLINE_KX4 * std::cos(z * SPLINE_KX4)) * curvature_scale;

        float dy = (SPLINE_AY1 * SPLINE_KY1 * std::cos(z * SPLINE_KY1) +
                    SPLINE_AY2 * SPLINE_KY2 * std::cos(z * SPLINE_KY2) +
                    SPLINE_AY3 * SPLINE_KY3 * std::cos(z * SPLINE_KY3) +
                    SPLINE_AY4 * SPLINE_KY4 * std::cos(z * SPLINE_KY4)) * curvature_scale;
        return glm::vec2(dx, dy);
    }

    [[nodiscard]] static inline glm::vec2 get_track_spline_deriv2(float z, float curvature_scale = 1.0f) noexcept {
        float d2x = (-SPLINE_AX1 * SPLINE_KX1 * SPLINE_KX1 * std::sin(z * SPLINE_KX1) -
                     SPLINE_AX2 * SPLINE_KX2 * SPLINE_KX2 * std::sin(z * SPLINE_KX2) -
                     SPLINE_AX3 * SPLINE_KX3 * SPLINE_KX3 * std::sin(z * SPLINE_KX3) -
                     SPLINE_AX4 * SPLINE_KX4 * SPLINE_KX4 * std::sin(z * SPLINE_KX4)) * curvature_scale;

        float d2y = (-SPLINE_AY1 * SPLINE_KY1 * SPLINE_KY1 * std::sin(z * SPLINE_KY1) -
                     SPLINE_AY2 * SPLINE_KY2 * SPLINE_KY2 * std::sin(z * SPLINE_KY2) -
                     SPLINE_AY3 * SPLINE_KY3 * SPLINE_KY3 * std::sin(z * SPLINE_KY3) -
                     SPLINE_AY4 * SPLINE_KY4 * SPLINE_KY4 * std::sin(z * SPLINE_KY4)) * curvature_scale;
        return glm::vec2(d2x, d2y);
    }

    [[nodiscard]] glm::vec3 get_camera_pos() const noexcept { return camera_pos_; }
    [[nodiscard]] glm::vec3 get_camera_dir() const noexcept { return camera_dir_; }
    [[nodiscard]] glm::vec3 get_camera_up() const noexcept { return camera_up_; }
    [[nodiscard]] float get_camera_roll() const noexcept { return smooth_roll_; }
    [[nodiscard]] float get_fov_radians() const noexcept { return fov_radians_; }
    [[nodiscard]] glm::vec3 get_laser_pos() const noexcept { return laser_pos_; }
    [[nodiscard]] float get_trauma() const noexcept { return trauma_; }
    [[nodiscard]] float get_curvature_scale() const noexcept { return smooth_curvature_scale_; }
    [[nodiscard]] float get_arch_emergence() const noexcept { return smooth_arch_emergence_; }

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
    float smooth_speed_{7.5f};
    float smooth_curvature_scale_{0.0f};
    float smooth_arch_emergence_{0.0f};
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
