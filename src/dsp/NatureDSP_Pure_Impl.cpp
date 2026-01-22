/*
 * NatureDSP_Pure.cpp
 *
 * Pure DSP implementation for Nature instrument
 *
 * Architecture: Headless DSP (no JUCE dependencies)
 * Inherits: DSP::InstrumentDSP
 *
 * Created: January 19, 2026
 */

#include "dsp/NatureDSP_Pure.h"
#include <cstring>
#include <cstdlib>

namespace DSP {

//==============================================================================
// Constructor/Destructor
//==============================================================================

NatureDSP::NatureDSP() {
    // Allocate synthesis modules
    waterSynth_ = std::make_unique<WaterSynthesis>();
    windSynth_ = std::make_unique<WindSynthesis>();
    insectSynth_ = std::make_unique<InsectSynthesis>();
    birdSynth_ = std::make_unique<BirdSynthesis>();
    amphibianSynth_ = std::make_unique<AmphibianSynthesis>();
    mammalSynth_ = std::make_unique<MammalSynthesis>();

    // Allocate voices
    for (auto& voice : voices_) {
        voice = std::make_unique<VoiceState>();
    }
}

NatureDSP::~NatureDSP() {
    // Cleanup handled by unique_ptr
}

//==============================================================================
// InstrumentDSP Interface Implementation
//==============================================================================

bool NatureDSP::prepare(double sampleRate, int blockSize) {
    if (sampleRate <= 0.0 || blockSize <= 0) {
        return false;
    }

    sampleRate_ = sampleRate;
    blockSize_ = blockSize;

    // Initialize synthesis modules
    waterSynth_->init(sampleRate, random_);
    windSynth_->init(sampleRate, random_);
    insectSynth_->init(sampleRate, random_);
    birdSynth_->init(sampleRate, random_);
    amphibianSynth_->init(sampleRate, random_);
    mammalSynth_->init(sampleRate, random_);

    // Initialize reverb
    reverb_.init(sampleRate);

    // Reset all voices
    reset();

    return true;
}

void NatureDSP::reset() {
    // Reset all voices
    for (auto& voice : voices_) {
        voice->active = false;
        voice->phase = VoiceState::Phase::Idle;
        voice->amplitude = 0.0f;
    }
    activeVoiceCount_.store(0);

    // Reset synthesis modules
    waterSynth_->reset();
    windSynth_->reset();
    insectSynth_->reset();
    birdSynth_->reset();
    amphibianSynth_->reset();
    mammalSynth_->reset();

    // Reset reverb
    reverb_.reset();
}

void NatureDSP::process(float** outputs, int numChannels, int numSamples) {
    // Clear output buffers
    for (int ch = 0; ch < numChannels; ++ch) {
        std::memset(outputs[ch], 0, sizeof(float) * numSamples);
    }

    // Process active voices
    for (auto& voice : voices_) {
        if (voice->active) {
            // Update envelope
            updateVoiceEnvelope(voice.get(), numSamples);

            // Generate sound based on category
            mixVoiceToOutput(voice.get(), outputs, numChannels, numSamples);

            // Free voice if release finished
            if (voice->phase == VoiceState::Phase::Idle) {
                voice->active = false;
                activeVoiceCount_.fetch_sub(1);
            }
        }
    }

    // Apply master level
    for (int ch = 0; ch < numChannels; ++ch) {
        for (int i = 0; i < numSamples; ++i) {
            outputs[ch][i] *= masterLevel_;
        }
    }

    // Apply reverb
    reverb_.process(outputs[0], outputs[1], numSamples,
                    reverbMix_, reverbRoomSize_, reverbDamping_);
}

void NatureDSP::handleEvent(const ScheduledEvent& event) {
    switch (event.type) {
        case ScheduledEvent::NOTE_ON: {
            // Check if note is already playing (retrigger)
            auto existingVoice = findVoice(event.data.note.midiNote);
            VoiceState* voice = nullptr;
            bool isNewVoice = false;

            if (existingVoice) {
                // Retrigger existing voice
                voice = existingVoice;
                isNewVoice = false;
            } else {
                // Allocate new voice
                voice = allocateVoice();
                isNewVoice = (voice != nullptr && !voice->active);
            }

            if (voice) {
                // Only increment count if this is a truly new voice (not retrigger or stealing)
                if (isNewVoice) {
                    activeVoiceCount_.fetch_add(1);
                }

                voice->active = true;
                voice->midiNote = event.data.note.midiNote;
                voice->velocity = event.data.note.velocity;

                // Map MIDI note to category and sound
                int note = event.data.note.midiNote;
                if (note >= 36 && note < 48) {  // C2-B2
                    voice->category = (note < 42) ? SoundCategory::Water : SoundCategory::Wind;
                    voice->soundIndex = (note < 42) ? note - 36 : note - 42;
                } else if (note >= 48 && note < 60) {  // C3-B3
                    voice->category = (note < 54) ? SoundCategory::Insect : SoundCategory::Amphibian;
                    voice->soundIndex = (note < 54) ? note - 48 : note - 54;
                } else if (note >= 60 && note < 72) {  // C4-B4
                    voice->category = (note < 66) ? SoundCategory::Bird : SoundCategory::Mammal;
                    voice->soundIndex = (note < 66) ? note - 60 : note - 66;
                } else {
                    voice->category = SoundCategory::Water;
                    voice->soundIndex = 0;
                }

                // Start attack
                voice->phase = VoiceState::Phase::Attack;
                voice->amplitude = 0.0f;
            }
            break;
        }

        case ScheduledEvent::NOTE_OFF: {
            auto voice = findVoice(event.data.note.midiNote);
            if (voice && voice->active) {
                voice->phase = VoiceState::Phase::Release;
            }
            break;
        }

        case ScheduledEvent::PARAM_CHANGE: {
            setParameter(event.data.param.paramId, event.data.param.value);
            break;
        }

        case ScheduledEvent::RESET: {
            panic();
            break;
        }

        default:
            break;
    }
}

float NatureDSP::getParameter(const char* paramId) const {
    if (std::strcmp(paramId, PARAM_MASTER_LEVEL) == 0) {
        return masterLevel_;
    } else if (std::strcmp(paramId, PARAM_REVERB_MIX) == 0) {
        return reverbMix_;
    } else if (std::strcmp(paramId, PARAM_REVERB_ROOM_SIZE) == 0) {
        return reverbRoomSize_;
    } else if (std::strcmp(paramId, PARAM_REVERB_DAMPING) == 0) {
        return reverbDamping_;
    }
    return 0.0f;
}

void NatureDSP::setParameter(const char* paramId, float value) {
    if (std::strcmp(paramId, PARAM_MASTER_LEVEL) == 0) {
        masterLevel_ = clamp(value, 0.0f, 1.0f);
    } else if (std::strcmp(paramId, PARAM_REVERB_MIX) == 0) {
        reverbMix_ = clamp(value, 0.0f, 1.0f);
    } else if (std::strcmp(paramId, PARAM_REVERB_ROOM_SIZE) == 0) {
        reverbRoomSize_ = clamp(value, 0.0f, 1.0f);
    } else if (std::strcmp(paramId, PARAM_REVERB_DAMPING) == 0) {
        reverbDamping_ = clamp(value, 0.0f, 1.0f);
    }
}

bool NatureDSP::savePreset(char* jsonBuffer, int jsonBufferSize) const {
    // Simple JSON serialization
    const char* format = "{"
        "\"master_level\":%.6f,"
        "\"reverb_mix\":%.6f,"
        "\"reverb_room_size\":%.6f,"
        "\"reverb_damping\":%.6f"
    "}";

    int written = std::snprintf(jsonBuffer, jsonBufferSize, format,
                                masterLevel_, reverbMix_,
                                reverbRoomSize_, reverbDamping_);

    return (written > 0 && written < jsonBufferSize);
}

bool NatureDSP::loadPreset(const char* jsonData) {
    // Simple JSON parsing (basic implementation)
    // In production, use a proper JSON parser
    float masterLevel = 0.8f;
    float reverbMix = 0.15f;
    float reverbRoomSize = 0.5f;
    float reverbDamping = 0.5f;

    if (std::sscanf(jsonData,
                    "{ \"master_level\":%f, \"reverb_mix\":%f, "
                    "\"reverb_room_size\":%f, \"reverb_damping\":%f }",
                    &masterLevel, &reverbMix, &reverbRoomSize, &reverbDamping) == 4) {
        masterLevel_ = masterLevel;
        reverbMix_ = reverbMix;
        reverbRoomSize_ = reverbRoomSize;
        reverbDamping_ = reverbDamping;
        return true;
    }

    return false;
}

int NatureDSP::getActiveVoiceCount() const {
    return activeVoiceCount_.load();
}

int NatureDSP::getMaxPolyphony() const {
    return MAX_VOICES;
}

void NatureDSP::panic() {
    for (auto& voice : voices_) {
        voice->active = false;
        voice->phase = VoiceState::Phase::Idle;
        voice->amplitude = 0.0f;
    }
    activeVoiceCount_.store(0);
}

//==============================================================================
// Helper Methods
//==============================================================================

NatureDSP::VoiceState* NatureDSP::allocateVoice() {
    // Find free voice
    for (auto& voice : voices_) {
        if (!voice->active) {
            return voice.get();
        }
    }

    // Voice stealing: find oldest voice in release phase
    for (auto& voice : voices_) {
        if (voice->active && voice->phase == VoiceState::Phase::Release) {
            return voice.get();
        }
    }

    // All voices active, steal oldest
    return voices_[0].get();
}

void NatureDSP::freeVoice(VoiceState* voice) {
    voice->active = false;
    voice->phase = VoiceState::Phase::Idle;
    voice->amplitude = 0.0f;
}

NatureDSP::VoiceState* NatureDSP::findVoice(int midiNote) {
    for (auto& voice : voices_) {
        if (voice->active && voice->midiNote == midiNote) {
            return voice.get();
        }
    }
    return nullptr;
}

void NatureDSP::updateVoiceEnvelope(VoiceState* voice, int numSamples) {
    if (!voice->active) return;

    for (int i = 0; i < numSamples; ++i) {
        switch (voice->phase) {
            case VoiceState::Phase::Attack:
                voice->amplitude += voice->attackRate;
                if (voice->amplitude >= 1.0f) {
                    voice->amplitude = 1.0f;
                    voice->phase = VoiceState::Phase::Decay;
                }
                break;

            case VoiceState::Phase::Decay:
                voice->amplitude -= voice->decayRate;
                if (voice->amplitude <= voice->sustainLevel) {
                    voice->amplitude = voice->sustainLevel;
                    voice->phase = VoiceState::Phase::Sustain;
                }
                break;

            case VoiceState::Phase::Sustain:
                // Sustain level is constant
                break;

            case VoiceState::Phase::Release:
                voice->amplitude -= voice->releaseRate;
                if (voice->amplitude <= 0.0f) {
                    voice->amplitude = 0.0f;
                    voice->phase = VoiceState::Phase::Idle;
                }
                break;

            case VoiceState::Phase::Idle:
                break;
        }
    }
}

void NatureDSP::mixVoiceToOutput(VoiceState* voice, float** outputs, int numChannels, int numSamples) {
    if (voice->amplitude <= 0.0f) return;

    // Generate sound based on category
    switch (voice->category) {
        case SoundCategory::Water:
            generateWaterSound(voice, outputs, numChannels, numSamples);
            break;
        case SoundCategory::Wind:
            generateWindSound(voice, outputs, numChannels, numSamples);
            break;
        case SoundCategory::Insect:
            generateInsectSound(voice, outputs, numChannels, numSamples);
            break;
        case SoundCategory::Bird:
            generateBirdSound(voice, outputs, numChannels, numSamples);
            break;
        case SoundCategory::Amphibian:
            generateAmphibianSound(voice, outputs, numChannels, numSamples);
            break;
        case SoundCategory::Mammal:
            generateMammalSound(voice, outputs, numChannels, numSamples);
            break;
    }
}

void NatureDSP::generateWaterSound(VoiceState* voice, float** outputs, int numChannels, int numSamples) {
    auto soundType = static_cast<WaterSynthesis::SoundType>(voice->soundIndex);
    float amplitude = voice->amplitude * voice->velocity;

    // Clamp to valid range
    if (soundType > WaterSynthesis::Drips) {
        soundType = WaterSynthesis::Rain;
    }

    waterSynth_->process(outputs, numChannels, numSamples, soundType, amplitude, voice->velocity, random_);
}

void NatureDSP::generateWindSound(VoiceState* voice, float** outputs, int numChannels, int numSamples) {
    auto soundType = static_cast<WindSynthesis::SoundType>(voice->soundIndex);
    float amplitude = voice->amplitude * voice->velocity;

    // Clamp to valid range
    if (soundType > WindSynthesis::Storm) {
        soundType = WindSynthesis::Breeze;
    }

    windSynth_->process(outputs, numChannels, numSamples, soundType, amplitude, voice->velocity, random_);
}

void NatureDSP::generateInsectSound(VoiceState* voice, float** outputs, int numChannels, int numSamples) {
    auto soundType = static_cast<InsectSynthesis::SoundType>(voice->soundIndex);
    float amplitude = voice->amplitude * voice->velocity;

    // Clamp to valid range
    if (soundType > InsectSynthesis::Swarm) {
        soundType = InsectSynthesis::Cricket;
    }

    insectSynth_->process(outputs, numChannels, numSamples, soundType, amplitude, voice->velocity, random_);
}

void NatureDSP::generateBirdSound(VoiceState* voice, float** outputs, int numChannels, int numSamples) {
    auto soundType = static_cast<BirdSynthesis::SoundType>(voice->soundIndex);
    float amplitude = voice->amplitude * voice->velocity;

    // Clamp to valid range
    if (soundType > BirdSynthesis::Flock) {
        soundType = BirdSynthesis::Songbird;
    }

    birdSynth_->process(outputs, numChannels, numSamples, soundType, amplitude, voice->velocity, random_);
}

void NatureDSP::generateAmphibianSound(VoiceState* voice, float** outputs, int numChannels, int numSamples) {
    auto soundType = static_cast<AmphibianSynthesis::SoundType>(voice->soundIndex);
    float amplitude = voice->amplitude * voice->velocity;

    // Clamp to valid range
    if (soundType > AmphibianSynthesis::TreeFrog) {
        soundType = AmphibianSynthesis::Frog;
    }

    amphibianSynth_->process(outputs, numChannels, numSamples, soundType, amplitude, voice->velocity, random_);
}

void NatureDSP::generateMammalSound(VoiceState* voice, float** outputs, int numChannels, int numSamples) {
    auto soundType = static_cast<MammalSynthesis::SoundType>(voice->soundIndex);
    float amplitude = voice->amplitude * voice->velocity;

    // Clamp to valid range
    if (soundType > MammalSynthesis::Fox) {
        soundType = MammalSynthesis::Wolf;
    }

    mammalSynth_->process(outputs, numChannels, numSamples, soundType, amplitude, voice->velocity, random_);
}

//==============================================================================
// Reverb Implementation (Simple Schroeder Reverb)
//==============================================================================

void NatureDSP::ReverbState::init(double sr) {
    sampleRate = sr;

    // Set delay times (in samples) for 8 parallel comb filters
    // Based on prime numbers for good diffusion
    delayTimes = {{
        static_cast<int>(sr * 0.030),  // 30ms
        static_cast<int>(sr * 0.037),  // 37ms
        static_cast<int>(sr * 0.047),  // 47ms
        static_cast<int>(sr * 0.053),  // 53ms
        static_cast<int>(sr * 0.061),  // 61ms
        static_cast<int>(sr * 0.071),  // 71ms
        static_cast<int>(sr * 0.079),  // 79ms
        static_cast<int>(sr * 0.087)   // 87ms
    }};

    reset();
}

void NatureDSP::ReverbState::reset() {
    delayLines.fill(0.0f);
    dampingFilters.fill(0.0f);
    writeIndex = 0;
}

void NatureDSP::ReverbState::process(float* outputL, float* outputR, int numSamples,
                                     float mix, float roomSize, float damping) {
    // Simple reverb using 8 parallel comb filters
    float feedback = roomSize * 0.5f;
    float damp = damping * 0.5f;

    for (int i = 0; i < numSamples; ++i) {
        float input = (outputL[i] + outputR[i]) * 0.5f;
        float output = 0.0f;

        for (int j = 0; j < 8; ++j) {
            // Read from delay line
            int readIndex = (writeIndex - delayTimes[j] + 65536) % 65536;
            float delayed = delayLines[j];

            // Apply damping filter
            dampingFilters[j] = delayed * (1.0f - damp) + dampingFilters[j] * damp;

            // Write to delay line with feedback
            delayLines[j] = input + dampingFilters[j] * feedback;

            output += delayed;
        }

        output *= 0.125f;  // Mix 8 parallel lines

        // Mix wet/dry
        outputL[i] = outputL[i] * (1.0f - mix) + output * mix;
        outputR[i] = outputR[i] * (1.0f - mix) + output * mix;

        // Advance write index
        writeIndex = (writeIndex + 1) % 65536;
    }
}

//==============================================================================
// Water Synthesis Implementation
//==============================================================================

void WaterSynthesis::init(double sampleRate, RandomState& rng) {
    sampleRate_ = sampleRate;
    rng_ = &rng;
    reset();
}

void WaterSynthesis::reset() {
    lfo_.phase = 0.0f;
    lfo_.frequency = 0.5f;
    lowpass_.z1 = 0.0f;
    lowpass_.z2 = 0.0f;
    bandpass_.z1 = 0.0f;
    bandpass_.z2 = 0.0f;
    grain_.position = 0.0f;
}

void WaterSynthesis::process(float** outputs, int numChannels, int numSamples,
                                        SoundType soundType, float amplitude,
                                        float velocity, RandomState& rng) {
    switch (soundType) {
        case Rain:
            generateRain(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Stream:
            generateStream(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Ocean:
            generateOcean(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Waterfall:
            generateWaterfall(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Drips:
            generateDrips(outputs, numChannels, numSamples, amplitude, velocity);
            break;
    }
}

void WaterSynthesis::generateRain(float** outputs, int numChannels, int numSamples,
                                              float intensity, float texture) {
    float noiseLevel = intensity * 0.3f;
    float cutoff = 3000.0f + texture * 2000.0f;

    for (int i = 0; i < numSamples; ++i) {
        float noise = rng_->nextFloat() * 2.0f - 1.0f;

        // Modulate with LFO for texture
        float modulation = 1.0f + (texture * 0.5f * std::sin(lfo_.phase));
        lfo_.phase += 2.0f * M_PI * lfo_.frequency / sampleRate_;
        if (lfo_.phase >= 2.0f * M_PI) lfo_.phase -= 2.0f * M_PI;

        noise *= modulation * noiseLevel;

        // Lowpass filter
        float filtered = processLowpass(noise, cutoff);

        // Stereo output with slight pan
        float panOffset = rng_->nextFloat() * 0.1f - 0.05f;
        outputs[0][i] += filtered * (1.0f - panOffset);
        if (numChannels > 1) {
            outputs[1][i] += filtered * (1.0f + panOffset);
        }
    }
}

void WaterSynthesis::generateStream(float** outputs, int numChannels, int numSamples,
                                                float intensity, float texture) {
    float baseFreq = 500.0f + texture * 500.0f;
    float noiseLevel = intensity * 0.2f;

    for (int i = 0; i < numSamples; ++i) {
        float noise = rng_->nextFloat() * 2.0f - 1.0f;

        // Bandpass filter for stream character
        float modFreq = baseFreq + texture * 100.0f * std::sin(lfo_.phase);
        float filtered = processBandpass(noise, modFreq, 2.0f);

        lfo_.phase += 2.0f * M_PI * lfo_.frequency / sampleRate_;
        if (lfo_.phase >= 2.0f * M_PI) lfo_.phase -= 2.0f * M_PI;

        outputs[0][i] += filtered * noiseLevel;
        if (numChannels > 1) {
            outputs[1][i] += filtered * noiseLevel * 0.9f;
        }
    }
}

void WaterSynthesis::generateOcean(float** outputs, int numChannels, int numSamples,
                                               float intensity, float texture) {
    float lowFreq = 100.0f;
    float highFreq = 800.0f + texture * 400.0f;
    float noiseLevel = intensity * 0.25f;

    for (int i = 0; i < numSamples; ++i) {
        float noise = rng_->nextFloat() * 2.0f - 1.0f;

        // Dual filter for ocean character
        float lowFiltered = processLowpass(noise, lowFreq);
        float highFiltered = processBandpass(noise, highFreq, 1.0f);

        float ocean = lowFiltered * 0.6f + highFiltered * 0.4f;

        // Modulate with LFO
        float modulation = 1.0f + 0.3f * std::sin(lfo_.phase);
        lfo_.phase += 2.0f * M_PI * 0.1f / sampleRate_;
        if (lfo_.phase >= 2.0f * M_PI) lfo_.phase -= 2.0f * M_PI;

        ocean *= modulation * noiseLevel;

        outputs[0][i] += ocean;
        if (numChannels > 1) {
            outputs[1][i] += ocean;
        }
    }
}

void WaterSynthesis::generateWaterfall(float** outputs, int numChannels, int numSamples,
                                                   float intensity, float texture) {
    float baseFreq = 1000.0f + texture * 1000.0f;
    float noiseLevel = intensity * 0.3f;

    for (int i = 0; i < numSamples; ++i) {
        float noise = rng_->nextFloat() * 2.0f - 1.0f;

        // High-pass filtered noise for waterfall roar
        float modFreq = baseFreq + texture * 200.0f * std::sin(lfo_.phase);
        float filtered = processBandpass(noise, modFreq, 1.5f);

        lfo_.phase += 2.0f * M_PI * 2.0f / sampleRate_;
        if (lfo_.phase >= 2.0f * M_PI) lfo_.phase -= 2.0f * M_PI;

        outputs[0][i] += filtered * noiseLevel;
        if (numChannels > 1) {
            outputs[1][i] += filtered * noiseLevel * 0.95f;
        }
    }
}

void WaterSynthesis::generateDrips(float** outputs, int numChannels, int numSamples,
                                               float intensity, float texture) {
    float dripRate = 2.0f + texture * 8.0f;  // Drips per second
    float samplesPerDrip = sampleRate_ / dripRate;
    float sampleCounter = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        sampleCounter += 1.0f;

        if (sampleCounter >= samplesPerDrip) {
            sampleCounter = 0.0f;

            // Generate drip
            float dripFreq = 800.0f + rng_->nextFloat() * 400.0f;
            float dripAmp = intensity * (0.3f + rng_->nextFloat() * 0.2f);

            // Short sine burst
            int dripLength = static_cast<int>(sampleRate_ * 0.05f);  // 50ms
            int endSample = std::min(i + dripLength, numSamples);

            for (int j = i; j < endSample; ++j) {
                float t = static_cast<float>(j - i) / dripLength;
                float envelope = std::sin(t * M_PI);  // Half sine envelope
                float drip = std::sin(2.0f * M_PI * dripFreq * t) * envelope * dripAmp;

                // Pan randomly
                float pan = rng_->nextFloat() * 2.0f - 1.0f;
                outputs[0][j] += drip * (1.0f - pan * 0.5f);
                if (numChannels > 1) {
                    outputs[1][j] += drip * (1.0f + pan * 0.5f);
                }
            }
        }
    }
}

float WaterSynthesis::processLowpass(float input, float cutoff) {
    float rc = 1.0f / (cutoff * 2.0f * M_PI);
    float dt = 1.0f / sampleRate_;
    float alpha = dt / (rc + dt);

    float output = lowpass_.z1 + alpha * (input - lowpass_.z1);
    lowpass_.z1 = output;

    return output;
}

float WaterSynthesis::processBandpass(float input, float cutoff, float resonance) {
    float omega = 2.0f * M_PI * cutoff / sampleRate_;
    float alpha = std::sin(omega) / (2.0f * resonance);

    float b0 = alpha;
    float b1 = 0.0f;
    float b2 = -alpha;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * std::cos(omega);
    float a2 = 1.0f - alpha;

    float output = (b0 * input + b1 * bandpass_.z1 + b2 * bandpass_.z2
                    - a1 * bandpass_.z1 - a2 * bandpass_.z2) / a0;

    bandpass_.z2 = bandpass_.z1;
    bandpass_.z1 = output;

    return output;
}

//==============================================================================
// Wind Synthesis Implementation
//==============================================================================

void WindSynthesis::init(double sampleRate, RandomState& rng) {
    sampleRate_ = sampleRate;
    rng_ = &rng;
    reset();
}

void WindSynthesis::reset() {
    lfo_.phase = 0.0f;
    lfo_.frequency = 0.2f;
    bandpass_.z1 = 0.0f;
    bandpass_.z2 = 0.0f;
}

void WindSynthesis::process(float** outputs, int numChannels, int numSamples,
                                       SoundType soundType, float amplitude,
                                       float velocity, RandomState& rng) {
    switch (soundType) {
        case Breeze:
            generateBreeze(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Gusts:
            generateGusts(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Whistle:
            generateWhistle(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Storm:
            generateStorm(outputs, numChannels, numSamples, amplitude, velocity);
            break;
    }
}

void WindSynthesis::generateBreeze(float** outputs, int numChannels, int numSamples,
                                               float intensity, float modulation) {
    float baseFreq = 400.0f + modulation * 200.0f;
    float noiseLevel = intensity * 0.15f;

    for (int i = 0; i < numSamples; ++i) {
        float noise = rng_->nextFloat() * 2.0f - 1.0f;

        // Modulate frequency with LFO
        float modFreq = baseFreq + 50.0f * std::sin(lfo_.phase);
        lfo_.phase += 2.0f * M_PI * lfo_.frequency / sampleRate_;
        if (lfo_.phase >= 2.0f * M_PI) lfo_.phase -= 2.0f * M_PI;

        float filtered = processBandpass(noise, modFreq, 1.0f);

        outputs[0][i] += filtered * noiseLevel;
        if (numChannels > 1) {
            outputs[1][i] += filtered * noiseLevel;
        }
    }
}

void WindSynthesis::generateGusts(float** outputs, int numChannels, int numSamples,
                                              float intensity, float gustSpeed) {
    float baseFreq = 300.0f;
    float noiseLevel = intensity * 0.2f;
    float gustFreq = 0.5f + gustSpeed * 1.0f;

    for (int i = 0; i < numSamples; ++i) {
        float noise = rng_->nextFloat() * 2.0f - 1.0f;

        // Modulate amplitude with slow LFO for gusts
        float gustEnvelope = 0.5f + 0.5f * std::sin(lfo_.phase);
        lfo_.phase += 2.0f * M_PI * gustFreq / sampleRate_;
        if (lfo_.phase >= 2.0f * M_PI) lfo_.phase -= 2.0f * M_PI;

        float modFreq = baseFreq + gustEnvelope * 200.0f;
        float filtered = processBandpass(noise, modFreq, 1.0f);

        outputs[0][i] += filtered * noiseLevel * gustEnvelope;
        if (numChannels > 1) {
            outputs[1][i] += filtered * noiseLevel * gustEnvelope;
        }
    }
}

void WindSynthesis::generateWhistle(float** outputs, int numChannels, int numSamples,
                                                float intensity, float frequency) {
    float baseFreq = 800.0f + frequency * 400.0f;
    float noiseLevel = intensity * 0.1f;

    for (int i = 0; i < numSamples; ++i) {
        float noise = rng_->nextFloat() * 2.0f - 1.0f;

        // Narrow bandpass for whistle
        float filtered = processBandpass(noise, baseFreq, 5.0f);

        outputs[0][i] += filtered * noiseLevel;
        if (numChannels > 1) {
            outputs[1][i] += filtered * noiseLevel;
        }
    }
}

void WindSynthesis::generateStorm(float** outputs, int numChannels, int numSamples,
                                              float intensity, float turbulence) {
    float baseFreq = 200.0f;
    float noiseLevel = intensity * 0.3f;

    for (int i = 0; i < numSamples; ++i) {
        float noise = rng_->nextFloat() * 2.0f - 1.0f;

        // Wide modulation for storm
        float modFreq = baseFreq + turbulence * 300.0f * std::sin(lfo_.phase);
        lfo_.phase += 2.0f * M_PI * 3.0f / sampleRate_;
        if (lfo_.phase >= 2.0f * M_PI) lfo_.phase -= 2.0f * M_PI;

        float filtered = processBandpass(noise, modFreq, 0.5f);

        outputs[0][i] += filtered * noiseLevel;
        if (numChannels > 1) {
            outputs[1][i] += filtered * noiseLevel;
        }
    }
}

float WindSynthesis::processBandpass(float input, float cutoff, float resonance) {
    float omega = 2.0f * M_PI * cutoff / sampleRate_;
    float alpha = std::sin(omega) / (2.0f * resonance);

    float b0 = alpha;
    float b1 = 0.0f;
    float b2 = -alpha;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * std::cos(omega);
    float a2 = 1.0f - alpha;

    float output = (b0 * input + b1 * bandpass_.z1 + b2 * bandpass_.z2
                    - a1 * bandpass_.z1 - a2 * bandpass_.z2) / a0;

    bandpass_.z2 = bandpass_.z1;
    bandpass_.z1 = output;

    return output;
}

//==============================================================================
// Insect Synthesis Implementation
//==============================================================================

void InsectSynthesis::init(double sampleRate, RandomState& rng) {
    sampleRate_ = sampleRate;
    rng_ = &rng;
    reset();
}

void InsectSynthesis::reset() {
    fm_.carrierPhase = 0.0f;
    fm_.modulatorPhase = 0.0f;
    am_.carrierPhase = 0.0f;
    am_.modulatorPhase = 0.0f;
}

void InsectSynthesis::process(float** outputs, int numChannels, int numSamples,
                                         SoundType soundType, float amplitude,
                                         float velocity, RandomState& rng) {
    switch (soundType) {
        case Cricket:
            generateCricket(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Cicada:
            generateCicada(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Bee:
            generateBee(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Fly:
            generateFly(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Mosquito:
            generateMosquito(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Swarm:
            generateSwarm(outputs, numChannels, numSamples, amplitude, velocity);
            break;
    }
}

void InsectSynthesis::generateCricket(float** outputs, int numChannels, int numSamples,
                                                   float intensity, float pitch) {
    float carrierFreq = 4000.0f + pitch * 1000.0f;
    float modulatorFreq = 80.0f;
    float modulationIndex = 50.0f;

    for (int i = 0; i < numSamples; ++i) {
        // FM synthesis
        float modulator = std::sin(2.0f * M_PI * fm_.modulatorPhase);
        float carrier = std::sin(2.0f * M_PI * fm_.carrierPhase + modulationIndex * modulator);

        fm_.carrierPhase += carrierFreq / sampleRate_;
        fm_.modulatorPhase += modulatorFreq / sampleRate_;

        // Wrap phases
        if (fm_.carrierPhase >= 1.0f) fm_.carrierPhase -= 1.0f;
        if (fm_.modulatorPhase >= 1.0f) fm_.modulatorPhase -= 1.0f;

        float cricket = carrier * intensity * 0.3f;

        outputs[0][i] += cricket;
        if (numChannels > 1) {
            outputs[1][i] += cricket * 0.8f;
        }
    }
}

void InsectSynthesis::generateCicada(float** outputs, int numChannels, int numSamples,
                                                  float intensity, float pitch) {
    float carrierFreq = 5000.0f + pitch * 1500.0f;
    float modulatorFreq = 100.0f;
    float modulationIndex = 80.0f;

    for (int i = 0; i < numSamples; ++i) {
        // FM synthesis with higher index for buzzing
        float modulator = std::sin(2.0f * M_PI * fm_.modulatorPhase);
        float carrier = std::sin(2.0f * M_PI * fm_.carrierPhase + modulationIndex * modulator);

        fm_.carrierPhase += carrierFreq / sampleRate_;
        fm_.modulatorPhase += modulatorFreq / sampleRate_;

        if (fm_.carrierPhase >= 1.0f) fm_.carrierPhase -= 1.0f;
        if (fm_.modulatorPhase >= 1.0f) fm_.modulatorPhase -= 1.0f;

        float cicada = carrier * intensity * 0.25f;

        outputs[0][i] += cicada;
        if (numChannels > 1) {
            outputs[1][i] += cicada * 0.9f;
        }
    }
}

void InsectSynthesis::generateBee(float** outputs, int numChannels, int numSamples,
                                              float intensity, float pitch) {
    float carrierFreq = 150.0f + pitch * 50.0f;
    float modulatorFreq = 20.0f;

    for (int i = 0; i < numSamples; ++i) {
        // Sawtooth + AM synthesis
        float sawtooth = generateSawtooth(am_.carrierPhase);
        float modulator = std::sin(2.0f * M_PI * am_.modulatorPhase);

        am_.carrierPhase += carrierFreq / sampleRate_;
        am_.modulatorPhase += modulatorFreq / sampleRate_;

        if (am_.carrierPhase >= 1.0f) am_.carrierPhase -= 1.0f;
        if (am_.modulatorPhase >= 1.0f) am_.modulatorPhase -= 1.0f;

        float bee = sawtooth * (1.0f + 0.5f * modulator) * intensity * 0.2f;

        outputs[0][i] += bee;
        if (numChannels > 1) {
            outputs[1][i] += bee;
        }
    }
}

void InsectSynthesis::generateFly(float** outputs, int numChannels, int numSamples,
                                              float intensity, float pitch) {
    float carrierFreq = 100.0f + pitch * 30.0f;
    float modulatorFreq = 15.0f;

    for (int i = 0; i < numSamples; ++i) {
        // Sawtooth + AM synthesis with higher modulation
        float sawtooth = generateSawtooth(am_.carrierPhase);
        float modulator = std::sin(2.0f * M_PI * am_.modulatorPhase);

        am_.carrierPhase += carrierFreq / sampleRate_;
        am_.modulatorPhase += modulatorFreq / sampleRate_;

        if (am_.carrierPhase >= 1.0f) am_.carrierPhase -= 1.0f;
        if (am_.modulatorPhase >= 1.0f) am_.modulatorPhase -= 1.0f;

        float fly = sawtooth * (1.0f + 0.8f * modulator) * intensity * 0.15f;

        outputs[0][i] += fly;
        if (numChannels > 1) {
            outputs[1][i] += fly;
        }
    }
}

void InsectSynthesis::generateMosquito(float** outputs, int numChannels, int numSamples,
                                                   float intensity, float pitch) {
    float carrierFreq = 800.0f + pitch * 200.0f;
    float modulatorFreq = 25.0f;

    for (int i = 0; i < numSamples; ++i) {
        // High-pitched sawtooth + AM
        float sawtooth = generateSawtooth(am_.carrierPhase);
        float modulator = std::sin(2.0f * M_PI * am_.modulatorPhase);

        am_.carrierPhase += carrierFreq / sampleRate_;
        am_.modulatorPhase += modulatorFreq / sampleRate_;

        if (am_.carrierPhase >= 1.0f) am_.carrierPhase -= 1.0f;
        if (am_.modulatorPhase >= 1.0f) am_.modulatorPhase -= 1.0f;

        float mosquito = sawtooth * (1.0f + 0.3f * modulator) * intensity * 0.1f;

        outputs[0][i] += mosquito;
        if (numChannels > 1) {
            outputs[1][i] += mosquito;
        }
    }
}

void InsectSynthesis::generateSwarm(float** outputs, int numChannels, int numSamples,
                                                float intensity, float density) {
    int numInsects = static_cast<int>(3 + density * 7);  // 3-10 insects

    for (int insect = 0; insect < numInsects; ++insect) {
        float insectFreq = 100.0f + rng_->nextFloat() * 4000.0f;
        float insectPhase = rng_->nextFloat();

        for (int i = 0; i < numSamples; ++i) {
            float sound = std::sin(2.0f * M_PI * insectPhase);
            insectPhase += insectFreq / sampleRate_;
            if (insectPhase >= 1.0f) insectPhase -= 1.0f;

            outputs[0][i] += sound * intensity * 0.05f;
            if (numChannels > 1) {
                outputs[1][i] += sound * intensity * 0.05f;
            }
        }
    }
}

float InsectSynthesis::generateSawtooth(float phase) {
    return 2.0f * phase - 1.0f;
}

float InsectSynthesis::generateSquare(float phase) {
    return (phase < 0.5f) ? 1.0f : -1.0f;
}

//==============================================================================
// Bird Synthesis Implementation
//==============================================================================

void BirdSynthesis::init(double sampleRate, RandomState& rng) {
    sampleRate_ = sampleRate;
    rng_ = &rng;
    reset();
}

void BirdSynthesis::reset() {
    fm_.carrierPhase = 0.0f;
    fm_.modulatorPhase = 0.0f;
    formant_.phase = 0.0f;
}

void BirdSynthesis::process(float** outputs, int numChannels, int numSamples,
                                       SoundType soundType, float amplitude,
                                       float velocity, RandomState& rng) {
    switch (soundType) {
        case Songbird:
            generateSongbird(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Owl:
            generateOwl(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Crow:
            generateCrow(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Flock:
            generateFlock(outputs, numChannels, numSamples, amplitude, velocity);
            break;
    }
}

void BirdSynthesis::generateSongbird(float** outputs, int numChannels, int numSamples,
                                                  float intensity, float pitch) {
    float carrierFreq = 2000.0f + pitch * 1000.0f;
    float modulatorFreq = 500.0f;
    float modulationIndex = 10.0f;

    for (int i = 0; i < numSamples; ++i) {
        // FM synthesis for melodic song
        float modulator = std::sin(2.0f * M_PI * fm_.modulatorPhase);
        float carrier = std::sin(2.0f * M_PI * fm_.carrierPhase + modulationIndex * modulator);

        fm_.carrierPhase += carrierFreq / sampleRate_;
        fm_.modulatorPhase += modulatorFreq / sampleRate_;

        if (fm_.carrierPhase >= 1.0f) fm_.carrierPhase -= 1.0f;
        if (fm_.modulatorPhase >= 1.0f) fm_.modulatorPhase -= 1.0f;

        float song = carrier * intensity * 0.2f;

        outputs[0][i] += song;
        if (numChannels > 1) {
            outputs[1][i] += song * 0.9f;
        }
    }
}

void BirdSynthesis::generateOwl(float** outputs, int numChannels, int numSamples,
                                            float intensity, float pitch) {
    float formantFreq = 400.0f + pitch * 200.0f;
    float pulseRate = 2.0f;  // 2 Hz hoot rate

    for (int i = 0; i < numSamples; ++i) {
        // Formant synthesis for hoot
        float pulse = (formant_.phase < 0.1f) ? 1.0f : 0.0f;  // Pulse train
        formant_.phase += pulseRate / sampleRate_;
        if (formant_.phase >= 1.0f) formant_.phase -= 1.0f;

        float hoot = pulse * std::sin(2.0f * M_PI * formantFreq * i / sampleRate_) * intensity * 0.3f;

        outputs[0][i] += hoot;
        if (numChannels > 1) {
            outputs[1][i] += hoot;
        }
    }
}

void BirdSynthesis::generateCrow(float** outputs, int numChannels, int numSamples,
                                              float intensity, float pitch) {
    float baseFreq = 800.0f + pitch * 400.0f;

    for (int i = 0; i < numSamples; ++i) {
        // Sawtooth + noise for harsh caw
        float phase = baseFreq * i / sampleRate_;
        float sawtooth = 2.0f * (phase - std::floor(phase + 0.5f));
        float noise = rng_->nextFloat() * 2.0f - 1.0f;

        float caw = (sawtooth * 0.7f + noise * 0.3f) * intensity * 0.25f;

        outputs[0][i] += caw;
        if (numChannels > 1) {
            outputs[1][i] += caw;
        }
    }
}

void BirdSynthesis::generateFlock(float** outputs, int numChannels, int numSamples,
                                              float intensity, float density) {
    int numBirds = static_cast<int>(2 + density * 8);  // 2-10 birds

    for (int bird = 0; bird < numBirds; ++bird) {
        float birdFreq = 1500.0f + rng_->nextFloat() * 2000.0f;
        float birdPhase = rng_->nextFloat();

        for (int i = 0; i < numSamples; ++i) {
            float sound = std::sin(2.0f * M_PI * birdPhase);
            birdPhase += birdFreq / sampleRate_;
            if (birdPhase >= 1.0f) birdPhase -= 1.0f;

            outputs[0][i] += sound * intensity * 0.05f;
            if (numChannels > 1) {
                outputs[1][i] += sound * intensity * 0.05f;
            }
        }
    }
}

//==============================================================================
// Amphibian Synthesis Implementation (TODO: Complete)
//==============================================================================

void AmphibianSynthesis::init(double sampleRate, RandomState& rng) {
    sampleRate_ = sampleRate;
    rng_ = &rng;
    reset();
}

void AmphibianSynthesis::reset() {
    formant_.phase = 0.0f;
}

void AmphibianSynthesis::process(float** outputs, int numChannels, int numSamples,
                                            SoundType soundType, float amplitude,
                                            float velocity, RandomState& rng) {
    switch (soundType) {
        case Frog:
            generateFrog(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Toad:
            generateToad(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case TreeFrog:
            generateTreeFrog(outputs, numChannels, numSamples, amplitude, velocity);
            break;
    }
}

void AmphibianSynthesis::generateFrog(float** outputs, int numChannels, int numSamples,
                                                  float intensity, float pitch) {
    float formantFreq = 150.0f + pitch * 100.0f;
    float pulseRate = 3.0f;

    for (int i = 0; i < numSamples; ++i) {
        float pulse = (formant_.phase < 0.05f) ? 1.0f : 0.0f;
        formant_.phase += pulseRate / sampleRate_;
        if (formant_.phase >= 1.0f) formant_.phase -= 1.0f;

        float croak = pulse * std::sin(2.0f * M_PI * formantFreq * i / sampleRate_) * intensity * 0.3f;

        outputs[0][i] += croak;
        if (numChannels > 1) {
            outputs[1][i] += croak;
        }
    }
}

void AmphibianSynthesis::generateToad(float** outputs, int numChannels, int numSamples,
                                                  float intensity, float pitch) {
    float formantFreq = 100.0f + pitch * 50.0f;
    float pulseRate = 2.0f;

    for (int i = 0; i < numSamples; ++i) {
        float pulse = (formant_.phase < 0.08f) ? 1.0f : 0.0f;
        formant_.phase += pulseRate / sampleRate_;
        if (formant_.phase >= 1.0f) formant_.phase -= 1.0f;

        float croak = pulse * std::sin(2.0f * M_PI * formantFreq * i / sampleRate_) * intensity * 0.3f;

        outputs[0][i] += croak;
        if (numChannels > 1) {
            outputs[1][i] += croak;
        }
    }
}

void AmphibianSynthesis::generateTreeFrog(float** outputs, int numChannels, int numSamples,
                                                      float intensity, float pitch) {
    float formantFreq = 2000.0f + pitch * 1000.0f;
    float pulseRate = 5.0f;

    for (int i = 0; i < numSamples; ++i) {
        float pulse = (formant_.phase < 0.03f) ? 1.0f : 0.0f;
        formant_.phase += pulseRate / sampleRate_;
        if (formant_.phase >= 1.0f) formant_.phase -= 1.0f;

        float chirp = pulse * std::sin(2.0f * M_PI * formantFreq * i / sampleRate_) * intensity * 0.2f;

        outputs[0][i] += chirp;
        if (numChannels > 1) {
            outputs[1][i] += chirp;
        }
    }
}

//==============================================================================
// Mammal Synthesis Implementation (TODO: Complete)
//==============================================================================

void MammalSynthesis::init(double sampleRate, RandomState& rng) {
    sampleRate_ = sampleRate;
    rng_ = &rng;
    reset();
}

void MammalSynthesis::reset() {
    formant_.phase = 0.0f;
}

void MammalSynthesis::process(float** outputs, int numChannels, int numSamples,
                                         SoundType soundType, float amplitude,
                                         float velocity, RandomState& rng) {
    switch (soundType) {
        case Wolf:
            generateWolf(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Coyote:
            generateCoyote(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Deer:
            generateDeer(outputs, numChannels, numSamples, amplitude, velocity);
            break;
        case Fox:
            generateFox(outputs, numChannels, numSamples, amplitude, velocity);
            break;
    }
}

void MammalSynthesis::generateWolf(float** outputs, int numChannels, int numSamples,
                                                float intensity, float pitch) {
    float formantFreq = 200.0f + pitch * 100.0f;

    for (int i = 0; i < numSamples; ++i) {
        // Formant synthesis with vibrato
        float vibrato = std::sin(2.0f * M_PI * 5.0f * i / sampleRate_);
        float howl = std::sin(2.0f * M_PI * (formantFreq + vibrato * 20.0f) * i / sampleRate_) * intensity * 0.2f;

        outputs[0][i] += howl;
        if (numChannels > 1) {
            outputs[1][i] += howl;
        }
    }
}

void MammalSynthesis::generateCoyote(float** outputs, int numChannels, int numSamples,
                                                  float intensity, float pitch) {
    float formantFreq = 300.0f + pitch * 150.0f;

    for (int i = 0; i < numSamples; ++i) {
        float yip = std::sin(2.0f * M_PI * formantFreq * i / sampleRate_) * intensity * 0.15f;

        outputs[0][i] += yip;
        if (numChannels > 1) {
            outputs[1][i] += yip;
        }
    }
}

void MammalSynthesis::generateDeer(float** outputs, int numChannels, int numSamples,
                                                float intensity, float pitch) {
    // Deer snort (noise burst)
    for (int i = 0; i < numSamples; ++i) {
        float noise = rng_->nextFloat() * 2.0f - 1.0f;
        float snort = noise * intensity * 0.2f;

        outputs[0][i] += snort;
        if (numChannels > 1) {
            outputs[1][i] += snort;
        }
    }
}

void MammalSynthesis::generateFox(float** outputs, int numChannels, int numSamples,
                                              float intensity, float pitch) {
    float formantFreq = 400.0f + pitch * 200.0f;

    for (int i = 0; i < numSamples; ++i) {
        float bark = std::sin(2.0f * M_PI * formantFreq * i / sampleRate_) * intensity * 0.2f;

        outputs[0][i] += bark;
        if (numChannels > 1) {
            outputs[1][i] += bark;
        }
    }
}

} // namespace DSP
