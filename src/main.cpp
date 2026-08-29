#include "engine/audio_engine.hpp"
#include "graphics/renderer.hpp"
#include "core/types.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <csignal>
#include <string>
#include <cmath>
#include <sstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
std::atomic<bool> g_keep_running{true};

void signal_handler(int) {
    g_keep_running.store(false, std::memory_order_release);
}

void setup_terminal() {
#if defined(_WIN32)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
    std::cout << "\033[?25l\033[2J\033[H" << std::flush;
}

void restore_terminal() {
    std::cout << "\033[?25h\033[0m\n" << std::flush;
}

std::string make_bar(float value, int width = 24, const std::string& fill_char = "#") {
    value = std::clamp(value, 0.0f, 1.0f);
    int filled = static_cast<int>(std::round(value * static_cast<float>(width)));
    filled = std::clamp(filled, 0, width);
    std::string bar;
    bar.reserve(width);
    for (int i = 0; i < filled; ++i) bar += fill_char;
    for (int i = filled; i < width; ++i) bar += " ";
    return bar;
}

std::string format_oscilloscope(const std::array<float, audio_voyager::core::RAW_OSCILLOSCOPE_SAMPLES>& wave, int width = 52, int height = 5) {
    std::vector<std::string> grid(height, std::string(width, ' '));
    const int mid_y = height / 2;

    for (int x = 0; x < width; ++x) {
        grid[mid_y][x] = '-';
    }

    const float step = static_cast<float>(wave.size()) / static_cast<float>(width);
    for (int x = 0; x < width; ++x) {
        size_t idx = std::min(static_cast<size_t>(x * step), wave.size() - 1);
        float val = std::clamp(wave[idx], -1.0f, 1.0f);
        int y = mid_y - static_cast<int>(std::round(val * static_cast<float>(mid_y)));
        y = std::clamp(y, 0, height - 1);
        grid[y][x] = (y == mid_y) ? '+' : '*';
    }

    std::ostringstream oss;
    for (const auto& line : grid) {
        oss << "      | " << line << " |\n";
    }
    return oss.str();
}

void run_terminal_dashboard(audio_voyager::engine::AudioEngine& audio_engine, uint32_t sample_rate) {
    setup_terminal();
    audio_voyager::core::PhysicsAudioState state;
    auto last_time = std::chrono::steady_clock::now();
    uint64_t frame_count = 0;
    float fps = 60.0f;

    while (g_keep_running.load(std::memory_order_relaxed)) {
        auto now = std::chrono::steady_clock::now();
        float delta_time = std::chrono::duration<float>(now - last_time).count();
        if (delta_time >= 0.5f) {
            fps = static_cast<float>(frame_count) / delta_time;
            frame_count = 0;
            last_time = now;
        }
        ++frame_count;

        if (audio_engine.poll_state(state)) {
            std::cout << "\033[H";

            std::cout << "\033[1;36m================================================================================\033[0m\n";
            std::cout << "\033[1;37m   AUDIO-VOYAGER // CONTINUOUS SDF RAYMARCHING & SEMANTIC ML TELEMETRY          \033[0m\n";
            std::cout << "\033[1;36m================================================================================\033[0m\n";
            
            std::cout << " \033[1;33mPipeline Status:\033[0m "
                      << (audio_engine.is_synthetic() ? "\033[1;35mSYNTHETIC DSP SIGNAL GENERATOR (ACTIVE)\033[0m" : "\033[1;32mSYSTEM AUDIO LOOPBACK (ACTIVE)\033[0m")
                      << " | Rate: \033[1;37m" << sample_rate << " Hz\033[0m"
                      << " | UI FPS: \033[1;37m" << std::fixed << std::setprecision(1) << fps << "\033[0m\n";
            std::cout << " \033[1;33mFrame Index:\033[0m     " << std::setw(8) << state.frame_index 
                      << " | Fast Path Latency: \033[1;32m" << std::setprecision(2) << state.stream_a.latency_ms << " ms\033[0m"
                      << " | DSP Worker: \033[1;32m" << std::setprecision(1) << state.stream_b.compute_time_us << " us\033[0m\n";
            std::cout << "\033[0;34m--------------------------------------------------------------------------------\033[0m\n";

            // FLUJO A: REACTIVIDAD CRUDA
            std::cout << "\033[1;32m[ FLUJO A: REACTIVIDAD CRUDA (Osciloscopio & Fast-FFT 8 Bandas) ]\033[0m\n";
            std::cout << "   \033[1mOscilloscope Waveform (Real-Time 256 PCM):\033[0m\n";
            std::cout << format_oscilloscope(state.stream_a.waveform, 52, 5);

            std::cout << "   \033[1mFast-FFT Spectrum Frequency Bands:\033[0m\n";
            for (size_t b = 0; b < audio_voyager::core::FFT_BANDS_COUNT; ++b) {
                float val = state.stream_a.spectrum_bands[b];
                std::string bar = make_bar(val, 26, "\033[1;32m=\033[0m");
                std::cout << "    " << audio_voyager::core::FFT_BAND_NAMES[b] << ": [" << bar << "] " 
                          << std::fixed << std::setw(3) << static_cast<int>(val * 100.0f) << "%\n";
            }
            std::cout << "    Peak Amplitude: [" << make_bar(state.stream_a.peak_amplitude, 16, "\033[1;33m#\033[0m") 
                      << "] " << std::fixed << std::setprecision(2) << state.stream_a.peak_amplitude
                      << " | RMS Volume: [" << make_bar(state.stream_a.rms, 16, "\033[1;33m#\033[0m") 
                      << "] " << state.stream_a.rms << "\n";

            std::cout << "\033[0;34m--------------------------------------------------------------------------------\033[0m\n";

            // FLUJO B: FÍSICAS DEL ALMA (Essentia C++)
            std::cout << "\033[1;35m[ FLUJO B: FÍSICAS DEL ALMA (Essentia C++ Semantic Physics Forces) ]\033[0m\n";
            
            // 1. Spectral Centroid
            std::cout << "   \033[1mSpectral Centroid (Brillo y Centro de Masa):\033[0m\n";
            std::cout << "    Freq: \033[1;37m" << std::setw(7) << std::fixed << std::setprecision(1) << state.stream_b.spectral_centroid_hz << " Hz\033[0m "
                      << " [" << make_bar(state.stream_b.spectral_centroid_norm, 24, "\033[1;35m█\033[0m") << "]\n";

            // 2. Sethares Dissonance
            std::cout << "   \033[1mSethares Dissonance (Aspereza y Vorticidad 3D):\033[0m\n";
            std::cout << "    Diss: \033[1;37m" << std::setw(7) << std::fixed << std::setprecision(3) << state.stream_b.dissonance << "\033[0m     "
                      << " [" << make_bar(state.stream_b.dissonance, 24, "\033[1;31m█\033[0m") << "]\n";

            // 3. Onset Detection
            std::cout << "   \033[1mOnset Novelty (Ondas de Choque Cinéticas):\033[0m\n";
            std::cout << "    Strength: \033[1;37m" << std::setw(7) << std::fixed << std::setprecision(2) << state.stream_b.onset_strength << "\033[0m [" 
                      << make_bar(state.stream_b.onset_strength, 24, "\033[1;33m█\033[0m") << "] "
                      << (state.stream_b.is_onset ? "\033[1;41;37m 💥 SHOCKWAVE TRIGGERED! 💥 \033[0m" : "                          ") << "\n";

            // 4. Kinetic Energy
            std::cout << "   \033[1mMasa Cinética Dinámica & RMS:\033[0m\n";
            std::cout << "    Energía:  \033[1;37m" << std::setw(7) << std::fixed << std::setprecision(2) << state.stream_b.energy << "\033[0m ["
                      << make_bar(state.stream_b.energy, 24, "\033[1;32m█\033[0m") << "]\n";

            std::cout << "\033[1;36m================================================================================\033[0m\n";
            std::cout << " \033[90mPresiona Ctrl+C para detener la simulación.\033[0m\n";
            std::cout << std::flush;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    restore_terminal();
}

} // anonymous namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    bool force_synthetic = false;
    bool force_headless = false;
    int window_width = 1920;
    int window_height = 1080;
    bool fullscreen = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--synthetic" || arg == "-s") {
            force_synthetic = true;
        } else if (arg == "--headless" || arg == "--no-gui" || arg == "-t") {
            force_headless = true;
        } else if (arg == "--fullscreen" || arg == "-f") {
            fullscreen = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: audio-voyager [OPTIONS]\n"
                      << "Options:\n"
                      << "  --synthetic, -s   Force synthetic harmonic generator mode\n"
                      << "  --headless, -t    Run full interactive terminal dashboard (no X11 window needed)\n"
                      << "  --fullscreen, -f  Launch graphical visualizer in full screen\n"
                      << "  --help, -h        Show this help message\n";
            return 0;
        }
    }

    // 1. Initialize Audio Engine
    audio_voyager::audio::AudioCaptureConfig audio_config;
    audio_config.sample_rate = audio_voyager::core::DEFAULT_SAMPLE_RATE;
    audio_config.channels = audio_voyager::core::DEFAULT_CHANNELS;
    audio_config.synthetic_test_mode = force_synthetic;

    audio_voyager::engine::AudioEngine audio_engine(audio_config);
    if (!audio_engine.start()) {
        std::cerr << "[Main] Failed to start AudioEngine.\n";
        return 1;
    }

    // 2. Terminal mode requested
    if (force_headless) {
        run_terminal_dashboard(audio_engine, audio_config.sample_rate);
        audio_engine.stop();
        std::cout << "[Main] audio-voyager stopped.\n";
        return 0;
    }

    // 3. Initialize Modern OpenGL 4.5 Core Renderer (Raymarching + HDR Bloom)
    audio_voyager::graphics::WindowConfig win_config;
    win_config.width = window_width;
    win_config.height = window_height;
    win_config.fullscreen = fullscreen;
    win_config.vsync = true;
    win_config.title = "🌌 AUDIO-VOYAGER // CONTINUOUS SDF RAYMARCHING";

    audio_voyager::graphics::Renderer renderer(win_config);
    if (!renderer.init()) {
        std::cerr << "[Main][ERROR] Graphical engine initialization failed.\n"
                  << "             Check shader logs and GPU OpenGL 4.5 Core Profile support.\n";
        audio_engine.stop();
        return 1;
    }

    std::cout << "[Main] Continuous SDF Raymarching Engine running.\n"
              << "       Autonomous Art Director active. [F11] Fullscreen | [F12] Debug HUD | [ESC] Exit\n";

    // 4. Main Simulation & Rendering Loop
    audio_voyager::core::PhysicsAudioState audio_state;
    while (!renderer.should_close() && g_keep_running.load(std::memory_order_relaxed)) {
        audio_engine.poll_state(audio_state);
        renderer.render_frame(audio_state);
    }

    audio_engine.stop();
    std::cout << "[Main] audio-voyager gracefully shut down. Goodbye!\n";
    return 0;
}
