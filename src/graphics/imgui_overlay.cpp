#include "graphics/imgui_overlay.hpp"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <iostream>

namespace audio_voyager::graphics {

ImGuiOverlay::~ImGuiOverlay() {
    shutdown();
}

bool ImGuiOverlay::init(GLFWwindow* window) {
    if (initialized_) return true;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    apply_custom_theme();

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        std::cerr << "[ImGuiOverlay] Failed to initialize ImGui GLFW backend.\n";
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 450")) {
        std::cerr << "[ImGuiOverlay] Failed to initialize ImGui OpenGL3 backend.\n";
        return false;
    }

    initialized_ = true;
    return true;
}

void ImGuiOverlay::apply_custom_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg]           = ImVec4(0.03f, 0.03f, 0.06f, 0.85f);
    colors[ImGuiCol_Header]             = ImVec4(0.08f, 0.35f, 0.55f, 0.70f);
    colors[ImGuiCol_HeaderHovered]      = ImVec4(0.12f, 0.55f, 0.85f, 0.85f);
    colors[ImGuiCol_HeaderActive]       = ImVec4(0.15f, 0.70f, 0.95f, 1.00f);
    colors[ImGuiCol_Button]             = ImVec4(0.08f, 0.28f, 0.45f, 0.75f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.12f, 0.45f, 0.75f, 0.90f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.18f, 0.65f, 0.95f, 1.00f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.08f, 0.08f, 0.14f, 0.80f);
    colors[ImGuiCol_SliderGrab]         = ImVec4(0.10f, 0.75f, 0.95f, 0.90f);
    colors[ImGuiCol_SliderGrabActive]   = ImVec4(1.00f, 0.75f, 0.20f, 1.00f);
    colors[ImGuiCol_TitleBg]            = ImVec4(0.02f, 0.02f, 0.04f, 0.95f);
    colors[ImGuiCol_TitleBgActive]      = ImVec4(0.05f, 0.20f, 0.35f, 0.95f);

    style.WindowRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 5.0f;
}

void ImGuiOverlay::begin_frame() {
    if (!initialized_) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiOverlay::render_dashboard(core::PhysicsTuners& tuners, 
                                    const core::PhysicsAudioState& audio_state, 
                                    const core::AudioSemanticVector& semantic,
                                    float fps, 
                                    float /*dt*/) {
    if (!initialized_ || !tuners.show_hud) return;

    ImGui::SetNextWindowPos(ImVec2(24, 24), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(440, 640), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("🌌 AUDIO-VOYAGER // SEMANTIC ML & RAYMARCHING DEBUG (F12)", nullptr)) {
        
        // 1. Performance & Hardware
        if (ImGui::CollapsingHeader("⚡ ENGINE PERFORMANCE", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("FPS: %.1f  |  Frame Time: %.2f ms", fps, (fps > 0.0f ? 1000.0f / fps : 0.0f));
            ImGui::Text("Fast Path Latency: %.2f ms  |  DSP Worker: %.1f us", 
                        audio_state.stream_a.latency_ms, audio_state.stream_b.compute_time_us);
            ImGui::Separator();
        }

        // 2. AGC Auto-Gain Calibration & Topology
        if (ImGui::CollapsingHeader("🧠 PROCEDURAL AGC TOPOLOGY", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Detected Tempo: %.1f BPM (Conf: %.0f%%)", semantic.bpm, semantic.bpm_confidence * 100.0f);
            
            ImGui::Text("Auto-Gain Control (Dynamic 0.0 to 1.0):");
            ImGui::ProgressBar(semantic.norm_dissonance, ImVec2(-1, 0), "Dissonance (Folding / Sharpness)");
            ImGui::ProgressBar(semantic.norm_energy, ImVec2(-1, 0), "Acoustic Energy (Emissivity / Scale)");
            ImGui::ProgressBar(semantic.norm_centroid, ImVec2(-1, 0), "Spectral Centroid (HSV Hue)");
            ImGui::ProgressBar(semantic.norm_sub_bass, ImVec2(-1, 0), "Sub-Bass (Cavity Dilation)");
            ImGui::ProgressBar(semantic.norm_treble, ImVec2(-1, 0), "High-Treble (Glitch Slicing)");

            if (semantic.is_onset) {
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.3f, 1.0f), "💥 TRANSIENT DROP DETECTED! 💥");
            }
            ImGui::Separator();
        }

        // 3. Autonomous Art Director
        if (ImGui::CollapsingHeader("🚀 AUTONOMOUS ART DIRECTOR", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Warp Flight Speed: %.2f m/s", semantic.speed_forward);
            ImGui::ProgressBar(semantic.topology_folding, ImVec2(-1, 0), "KIFS Space Folding");
            ImGui::ProgressBar(semantic.glitch_intensity, ImVec2(-1, 0), "Shockwave Space Ripple");
            ImGui::Separator();
        }

        // 4. Optical Post-Process Overrides
        if (ImGui::CollapsingHeader("✨ OPTICAL POST-PROCESS CONTROLS", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Bloom Intensity", &tuners.bloom_intensity, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat("Chromatic Aberration", &tuners.chromatic_aberration, 0.0f, 0.05f, "%.4f");
            ImGui::SliderFloat("Speed Multiplier", &tuners.speed_multiplier, 0.2f, 3.0f, "%.2f");
        }

        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "\nShortcuts: [F11] Fullscreen | [F12] Toggle Debug HUD | [ESC] Exit");
    }
    ImGui::End();
}

void ImGuiOverlay::end_frame() {
    if (!initialized_) return;
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiOverlay::shutdown() {
    if (initialized_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        initialized_ = false;
    }
}

} // namespace audio_voyager::graphics
