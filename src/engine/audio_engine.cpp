#include "engine/audio_engine.hpp"
#include <iostream>

namespace audio_voyager::engine {

AudioEngine::AudioEngine(audio::AudioCaptureConfig config)
    : config_(config) {
    capture_ = std::make_unique<audio::AudioCapture>(ring_buffer_stream_a_, ring_buffer_stream_b_, config_);
    stream_a_ = std::make_unique<audio::StreamARaw>(ring_buffer_stream_a_, config_.sample_rate);
    stream_b_ = std::make_unique<audio::StreamBPhysics>(ring_buffer_stream_b_, config_.sample_rate);
}

AudioEngine::~AudioEngine() {
    stop();
}

bool AudioEngine::start() {
    if (is_running_.load(std::memory_order_acquire)) {
        return true;
    }

    std::cout << "[AudioEngine] Starting Dual-Stream Audio Pipeline...\n";

    // 1. Start Stream B DSP Worker
    stream_b_->start();

    // 2. Start Audio Capture (Loopback / Miniaudio)
    if (!capture_->start()) {
        std::cerr << "[AudioEngine] Failed to start audio capture.\n";
        stream_b_->stop();
        return false;
    }

    is_running_.store(true, std::memory_order_release);
    std::cout << "[AudioEngine] Pipeline active. Zero-latency stream processing running.\n";
    return true;
}

void AudioEngine::stop() {
    if (!is_running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    std::cout << "[AudioEngine] Stopping Dual-Stream Audio Pipeline...\n";
    if (capture_) capture_->stop();
    if (stream_b_) stream_b_->stop();
    std::cout << "[AudioEngine] Pipeline stopped safely.\n";
}

bool AudioEngine::is_running() const noexcept {
    return is_running_.load(std::memory_order_acquire);
}

bool AudioEngine::is_synthetic() const noexcept {
    return capture_ ? capture_->is_synthetic() : false;
}

bool AudioEngine::poll_state(core::PhysicsAudioState& out_state) {
    if (!is_running_.load(std::memory_order_relaxed)) {
        return false;
    }

    // 1. Process Stream A (Raw Reactivity / Fast Path)
    current_state_.stream_a = stream_a_->process();

    // 2. Poll latest snapshot from Stream B (Soul Physics DSP Worker)
    core::StreamBSnapshot b_snapshot;
    if (stream_b_->get_latest_snapshot(b_snapshot)) {
        current_state_.stream_b = b_snapshot;
    }

    current_state_.frame_index = ++frame_counter_;

    // 3. Publish to Lock-Free Triple Buffer
    state_buffer_.write(current_state_);

    // 4. Return the latest state
    out_state = current_state_;
    return true;
}

} // namespace audio_voyager::engine
