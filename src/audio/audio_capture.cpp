#include "audio/audio_capture.hpp"
#include "third_party/miniaudio.h"
#include <iostream>
#include <cmath>
#include <chrono>
#include <algorithm>

namespace audio_voyager::audio {

namespace {
// Miniaudio C-style callback forwarding to C++ AudioCapture instance
void miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pOutput; // Loopback / capture only
    if (!pDevice || !pInput || frameCount == 0) return;

    auto* capture = static_cast<AudioCapture*>(pDevice->pUserData);
    if (capture) {
        capture->process_audio_frames(static_cast<const float*>(pInput), static_cast<size_t>(frameCount));
    }
}
} // anonymous namespace

AudioCapture::AudioCapture(SampleRingBuffer& stream_a_buffer, 
                           SampleRingBuffer& stream_b_buffer, 
                           AudioCaptureConfig config)
    : config_(config)
    , stream_a_buffer_(stream_a_buffer)
    , stream_b_buffer_(stream_b_buffer) {
}

AudioCapture::~AudioCapture() {
    stop();
}

void AudioCapture::process_audio_frames(const float* input_frames, size_t frame_count) noexcept {
    if (!input_frames || frame_count == 0) return;

    // Stack-allocated temporary mono scratch buffer (Zero-allocation guarantee)
    constexpr size_t MAX_STACK_FRAMES = 2048;
    float mono_buffer[MAX_STACK_FRAMES];

    size_t processed = 0;
    while (processed < frame_count) {
        const size_t batch_size = std::min(frame_count - processed, MAX_STACK_FRAMES);

        if (config_.channels == 2) {
            // Stereo to Mono downmix: 0.5 * (Left + Right)
            for (size_t i = 0; i < batch_size; ++i) {
                const size_t in_idx = (processed + i) * 2;
                mono_buffer[i] = 0.5f * (input_frames[in_idx] + input_frames[in_idx + 1]);
            }
        } else {
            // Mono copy
            std::copy_n(&input_frames[processed], batch_size, mono_buffer);
        }

        // Push non-blocking to Stream A and Stream B lock-free ring buffers
        stream_a_buffer_.push_n(mono_buffer, batch_size);
        stream_b_buffer_.push_n(mono_buffer, batch_size);

        processed += batch_size;
    }
}

bool AudioCapture::init_miniaudio_device() {
    ma_context_ = std::make_unique<ma_context>();
    if (ma_context_init(nullptr, 0, nullptr, ma_context_.get()) != MA_SUCCESS) {
        std::cerr << "[AudioCapture] Failed to initialize miniaudio context.\n";
        return false;
    }

    ma_device_ = std::make_unique<ma_device>();

    // First attempt: Loopback mode (Captures what the PC is playing: Spotify, YouTube, DAWs)
    ma_device_config config = ma_device_config_init(ma_device_type_loopback);
    config.capture.format = ma_format_f32;
    config.capture.channels = config_.channels;
    config.sampleRate = config_.sample_rate;
    config.dataCallback = miniaudio_data_callback;
    config.pUserData = this;
    config.periodSizeInFrames = config_.period_size_in_frames;

    ma_result result = ma_device_init(ma_context_.get(), &config, ma_device_.get());
    if (result != MA_SUCCESS) {
        // Second attempt: Standard capture (Microphone / Line-In / Pulse Monitor)
        config.deviceType = ma_device_type_capture;
        result = ma_device_init(ma_context_.get(), &config, ma_device_.get());
    }

    if (result != MA_SUCCESS) {
        std::cerr << "[AudioCapture] Hardware audio loopback/capture device unavailable (code: " 
                  << result << ").\n";
        return false;
    }

    if (ma_device_start(ma_device_.get()) != MA_SUCCESS) {
        std::cerr << "[AudioCapture] Failed to start miniaudio device.\n";
        ma_device_uninit(ma_device_.get());
        return false;
    }

    std::cout << "[AudioCapture] Successfully connected to system audio backend: " 
              << ma_get_backend_name(ma_context_->backend) 
              << " (" << (config.deviceType == ma_device_type_loopback ? "Loopback" : "Capture") << ")"
              << " @ " << config_.sample_rate << "Hz, " << config_.channels << "ch.\n";

    return true;
}

void AudioCapture::start_synthetic_generator() {
    is_synthetic_.store(true, std::memory_order_release);
    stop_synthetic_.store(false, std::memory_order_release);

    std::cout << "[AudioCapture] Running in High-Fidelity Synthetic Signal Generator mode.\n"
              << "               Injecting Dynamic Chords, Bass Transients, Frequency Sweeps & Dissonance.\n";

    synthetic_thread_ = std::thread([this]() {
        constexpr size_t BATCH_SIZE = 256;
        float frame_buffer[BATCH_SIZE];
        double phase1 = 0.0, phase2 = 0.0, phase3 = 0.0, phase_sweep = 0.0;
        double kick_phase = 0.0;
        double kick_decay = 0.0;
        double sweep_freq = 200.0;
        double time_sec = 0.0;

        const double dt = 1.0 / static_cast<double>(config_.sample_rate);
        const auto sleep_duration = std::chrono::microseconds(
            static_cast<int64_t>((static_cast<double>(BATCH_SIZE) / config_.sample_rate) * 1e6)
        );

        auto next_tick = std::chrono::steady_clock::now();

        while (!stop_synthetic_.load(std::memory_order_relaxed)) {
            for (size_t i = 0; i < BATCH_SIZE; ++i) {
                time_sec += dt;

                // 1. Kick transient pulse every 0.5 seconds (triggers Onset Detection)
                if (std::fmod(time_sec, 0.5) < dt * 1.5) {
                    kick_decay = 1.0;
                    kick_phase = 0.0;
                }

                float kick_sample = 0.0f;
                if (kick_decay > 0.001) {
                    double kick_freq = 50.0 + 100.0 * kick_decay; // Pitch drop
                    kick_phase += 2.0 * M_PI * kick_freq * dt;
                    kick_sample = static_cast<float>(std::sin(kick_phase) * kick_decay * 0.7);
                    kick_decay *= 0.9995; // Exponential decay
                }

                // 2. Dynamic chord: Root (220Hz A3) + Fifth (330Hz E4) + Third or Dissonant Minor Second
                bool inject_dissonance = std::fmod(time_sec, 6.0) > 3.0;
                double f1 = 220.0;
                double f2 = inject_dissonance ? 233.08 : 329.63; // A#3 (Dissonance) vs E4 (Consonance)
                double f3 = 440.0;

                phase1 += 2.0 * M_PI * f1 * dt;
                phase2 += 2.0 * M_PI * f2 * dt;
                phase3 += 2.0 * M_PI * f3 * dt;

                // 3. Spectral centroid sweep (200Hz -> 4000Hz -> 200Hz)
                sweep_freq = 1000.0 + 800.0 * std::sin(time_sec * 0.8);
                phase_sweep += 2.0 * M_PI * sweep_freq * dt;

                // Sum all components
                float sample = static_cast<float>(
                    0.25 * std::sin(phase1) +
                    0.20 * std::sin(phase2) +
                    0.15 * std::sin(phase3) +
                    0.10 * std::sin(phase_sweep) +
                    kick_sample
                );

                frame_buffer[i] = sample;
            }

            // Push generated batch
            stream_a_buffer_.push_n(frame_buffer, BATCH_SIZE);
            stream_b_buffer_.push_n(frame_buffer, BATCH_SIZE);

            next_tick += sleep_duration;
            std::this_thread::sleep_until(next_tick);
        }
    });
}

bool AudioCapture::start() {
    if (is_running_.load(std::memory_order_acquire)) {
        return true;
    }

    if (config_.synthetic_test_mode) {
        start_synthetic_generator();
        is_running_.store(true, std::memory_order_release);
        return true;
    }

    if (!init_miniaudio_device()) {
        std::cout << "[AudioCapture] Falling back to synthetic signal generator mode.\n";
        start_synthetic_generator();
    }

    is_running_.store(true, std::memory_order_release);
    return true;
}

void AudioCapture::stop() {
    if (!is_running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    if (is_synthetic_.load(std::memory_order_acquire)) {
        stop_synthetic_.store(true, std::memory_order_release);
        if (synthetic_thread_.joinable()) {
            synthetic_thread_.join();
        }
        is_synthetic_.store(false, std::memory_order_release);
    }

    if (ma_device_) {
        ma_device_stop(ma_device_.get());
        ma_device_uninit(ma_device_.get());
        ma_device_.reset();
    }

    if (ma_context_) {
        ma_context_uninit(ma_context_.get());
        ma_context_.reset();
    }
}

} // namespace audio_voyager::audio
