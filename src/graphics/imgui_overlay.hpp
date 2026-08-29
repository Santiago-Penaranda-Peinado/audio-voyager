#pragma once

#include "core/types.hpp"

struct GLFWwindow;

namespace audio_voyager::graphics {

class ImGuiOverlay {
public:
    ImGuiOverlay() = default;
    ~ImGuiOverlay();

    // Non-copyable
    ImGuiOverlay(const ImGuiOverlay&) = delete;
    ImGuiOverlay& operator=(const ImGuiOverlay&) = delete;

    bool init(GLFWwindow* window);
    void begin_frame();
    void render_dashboard(core::PhysicsTuners& tuners, 
                          const core::PhysicsAudioState& audio_state, 
                          const core::AudioSemanticVector& semantic,
                          float fps, 
                          float dt);
    void end_frame();
    void shutdown();

private:
    void apply_custom_theme();
    bool initialized_{false};
};

} // namespace audio_voyager::graphics
