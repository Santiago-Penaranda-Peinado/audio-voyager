#pragma once

#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

namespace audio_voyager::director {

/**
 * @brief Generic 2nd-order critically damped spring filter (damping ratio zeta = 1.0).
 * 
 * Provides an exact analytical exponential solution without numerical discretization error:
 *   x(t) = target + (c1 + c2 * dt) * exp(-omega * dt)
 *   v(t) = (c2 - omega * (c1 + c2 * dt)) * exp(-omega * dt)
 * where:
 *   c1 = x(0) - target
 *   c2 = v(0) + omega * c1
 * 
 * Guarantees zero overshoot, smooth C-infinity continuity, and timestep invariance.
 * Supports scalar float and vector types like glm::vec3.
 */
template <typename T>
class CriticallyDampedSpring {
public:
    constexpr explicit CriticallyDampedSpring(T initial_value = T{}, float omega = 6.0f) noexcept
        : value_(initial_value), velocity_(T{}), omega_(omega) {}

    void update(T target, float dt) noexcept {
        update(target, omega_, dt);
    }

    void update(T target, float omega, float dt) noexcept {
        if (dt <= 0.0f) [[unlikely]] {
            return;
        }

        omega_ = omega;
        if (omega_ <= 0.0001f) [[unlikely]] {
            value_ = target;
            velocity_ = T{};
            return;
        }

        // Exact analytical solution of 2nd-order critically damped ODE (zeta = 1.0)
        const T c1 = value_ - target;
        const T c2 = velocity_ + c1 * omega_;

        const float exp_term = std::exp(-omega_ * dt);
        const T displacement_term = c1 + c2 * dt;

        value_ = target + displacement_term * exp_term;
        velocity_ = (c2 - displacement_term * omega_) * exp_term;
    }

    void reset(T initial_value) noexcept {
        value_ = initial_value;
        velocity_ = T{};
    }

    void set_value(T val) noexcept { value_ = val; }
    void set_velocity(T vel) noexcept { velocity_ = vel; }
    void set_omega(float omega) noexcept { omega_ = omega; }

    [[nodiscard]] const T& get_value() const noexcept { return value_; }
    [[nodiscard]] const T& get_velocity() const noexcept { return velocity_; }
    [[nodiscard]] float get_omega() const noexcept { return omega_; }

    [[nodiscard]] T value() const noexcept { return value_; }
    [[nodiscard]] T velocity() const noexcept { return velocity_; }

private:
    T value_{};
    T velocity_{};
    float omega_{6.0f};
};

} // namespace audio_voyager::director
