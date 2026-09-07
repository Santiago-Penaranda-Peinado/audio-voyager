#pragma once

#include "core/types.hpp"

namespace audio_voyager::audio {

// Forward aliases for audio types from core namespace
using core::DEFAULT_SAMPLE_RATE;
using core::DEFAULT_CHANNELS;
using core::RAW_OSCILLOSCOPE_SAMPLES;
using core::FFT_SIZE_STREAM_A;
using core::FFT_BANDS_COUNT;
using core::ANALYSIS_FRAME_SIZE_STREAM_B;
using core::ANALYSIS_HOP_SIZE_STREAM_B;
using core::FFT_BAND_NAMES;

using StreamASnapshot = core::StreamASnapshot;
using StreamBSnapshot = core::StreamBSnapshot;
using RawAudioFrame = core::RawAudioFrame;
using PhysicsAudioState = core::PhysicsAudioState;
using AudioSemanticVector = core::AudioSemanticVector;
using PhysicsTuners = core::PhysicsTuners;

} // namespace audio_voyager::audio
