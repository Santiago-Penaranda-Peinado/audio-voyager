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

        // 2. The Mind (Machine Learning Biome Inception at 1.0 Hz)
        if (ImGui::CollapsingHeader("🧠 THE MIND (ML SEMANTIC BIOMES @ 1Hz)", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (semantic.is_silent) {
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Mode: 🌌 QUIESCENCE / ZEN RESTING STATE");
            } else {
                ImGui::Text("Detected Tempo: %.1f BPM (Conf: %.0f%%)", semantic.bpm, semantic.bpm_confidence * 100.0f);
            }
            ImGui::Text("Affective Coordinates: Valence %.2f | Arousal %.2f", semantic.valence, semantic.arousal);
            
            ImGui::Text("Continuous Biome Barycentric Weights:");
            ImGui::ProgressBar(semantic.weight_liquid,  ImVec2(-1, 0), "SDF_Liquid  (Ocean / Harmonious)");
            ImGui::ProgressBar(semantic.weight_crystal, ImVec2(-1, 0), "SDF_Crystal (Monolith / Tension)");
            ImGui::ProgressBar(semantic.weight_cyber,   ImVec2(-1, 0), "SDF_Cyber   (Matrix / Electronic)");
            ImGui::Separator();
        }

        // 3. The Muscle (Multi-Band Physical Real-Time DSP at 144 Hz)
        if (ImGui::CollapsingHeader("⚡ THE MUSCLE (DSP EXCITATION @ 144Hz)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::ProgressBar(semantic.elastic_dilation, ImVec2(-1, 0), "Sub-Bass Dilation / Ocean Waves (20-80Hz)");
            ImGui::ProgressBar(semantic.melodic_mids,     ImVec2(-1, 0), "Melodic Vocal Resonance (300-3000Hz)");
            ImGui::ProgressBar(semantic.treble_sparkle,   ImVec2(-1, 0), "Treble Stardust Sparkle (8-20kHz)");
            ImGui::ProgressBar(semantic.surface_ripple,   ImVec2(-1, 0), "Surface Shockwave Ripple (Onsets)");
            ImGui::ProgressBar(semantic.emission_pulse / 2.0f, ImVec2(-1, 0), "Volumetric HDR Emission (RMS)");

            if (semantic.is_onset) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.4f, 1.0f), "💥 TRANSIENT DROP DETECTED! 💥");
            }
            ImGui::Separator();
        }

        // 4. Autonomous Kinematics
        if (ImGui::CollapsingHeader("🚀 AUTONOMOUS ART DIRECTOR", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Cruising Flight Speed: %.2f m/s", semantic.speed_forward);
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
