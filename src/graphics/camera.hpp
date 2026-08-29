#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>

namespace audio_voyager::graphics {

class Camera {
public:
    Camera(float fov = 45.0f, float aspect = 16.0f / 9.0f, float near_plane = 0.1f, float far_plane = 500.0f)
        : fov_(fov)
        , aspect_(aspect)
        , near_plane_(near_plane)
        , far_plane_(far_plane) {
        update_camera_vectors();
    }

    void update(float dt, float total_time, float rms, float centroid_norm, bool contemplative_mode) {
        if (contemplative_mode) {
            // Contemplative Mode: Smooth cinematic orbit + acoustic breathing motion
            yaw_ += 0.08f * dt;
            pitch_ = 0.16f + 0.08f * std::sin(total_time * 0.35f);

            // Camera distance breathes subtly with RMS volume and high frequencies
            float target_distance = 22.0f - (rms * 5.0f) + (centroid_norm * 2.5f);
            distance_ += (target_distance - distance_) * (1.0f - std::exp(-2.5f * dt));
            update_camera_vectors();
        } else if (auto_rotate_) {
            yaw_ += 0.05f * dt;
            update_camera_vectors();
        }
    }

    void set_aspect_ratio(float aspect) {
        aspect_ = aspect;
    }

    void process_mouse_movement(float xoffset, float yoffset) {
        const float sensitivity = 0.005f;
        yaw_ += xoffset * sensitivity;
        pitch_ += yoffset * sensitivity;
        pitch_ = glm::clamp(pitch_, -1.5f, 1.5f);
        update_camera_vectors();
    }

    void process_mouse_scroll(float yoffset) {
        distance_ -= yoffset * 1.5f;
        distance_ = glm::clamp(distance_, 2.0f, 80.0f);
        update_camera_vectors();
    }

    [[nodiscard]] glm::mat4 get_view_matrix() const {
        return glm::lookAt(position_, target_, up_);
    }

    [[nodiscard]] glm::mat4 get_projection_matrix() const {
        return glm::perspective(glm::radians(fov_), aspect_, near_plane_, far_plane_);
    }

    [[nodiscard]] glm::mat4 get_view_projection_matrix() const {
        return get_projection_matrix() * get_view_matrix();
    }

    [[nodiscard]] glm::vec3 get_position() const noexcept { return position_; }

private:
    void update_camera_vectors() {
        position_.x = target_.x + distance_ * std::cos(pitch_) * std::sin(yaw_);
        position_.y = target_.y + distance_ * std::sin(pitch_);
        position_.z = target_.z + distance_ * std::cos(pitch_) * std::cos(yaw_);
    }

    glm::vec3 position_{0.0f, 0.0f, 22.0f};
    glm::vec3 target_{0.0f, 0.0f, 0.0f};
    glm::vec3 up_{0.0f, 1.0f, 0.0f};

    float yaw_{0.0f};
    float pitch_{0.15f};
    float distance_{22.0f};

    float fov_{45.0f};
    float aspect_{16.0f / 9.0f};
    float near_plane_{0.1f};
    float far_plane_{500.0f};
    bool auto_rotate_{true};
};

} // namespace audio_voyager::graphics
