/*
  ==============================================================================

    StringPureDSP.cpp
    Created: December 30, 2025
    Author: Bret Bouchard

    Pure DSP implementation of Aether String
    Karplus-Strong waveguide synthesis with physical modeling

  ==============================================================================
*/

#include "dsp/StringPureDSP.h"
#include "../../../../include/dsp/DSPLogging.h"
#include <cstring>
#include <random>
#include <algorithm>
#include <cassert>

namespace DSP {

//==============================================================================
// Waveguide String Implementation
//==============================================================================

AetherStringWaveguideString::AetherStringWaveguideString()
{
    // Don't memset - let the struct default values apply
    // Default values are defined in the header: bridgeCoupling=0.3f, etc.
}

AetherStringWaveguideString::~AetherStringWaveguideString()
{
}

void AetherStringWaveguideString::prepare(double sampleRate, int maxDelaySamples)
{
    this->sampleRate = sampleRate;
    delayLine.resize(maxDelaySamples);
    std::fill(delayLine.begin(), delayLine.end(), 0.0f);

    writeIndex = 0;
    delayLength = calculateDelayLength(params.frequency);

    stiffnessState = 0.0f;
    dampingState = 0.0f;
}

void AetherStringWaveguideString::reset()
{
    std::fill(delayLine.begin(), delayLine.end(), 0.0f);
    writeIndex = 0;
    stiffnessState = 0.0f;
    dampingState = 0.0f;
    lastBridgeEnergy = 0.0f;
}

void AetherStringWaveguideString::excite(const float* exciterSignal, int numSamples, float velocity)
{
    // Fill the ENTIRE delay line with exciter signal to avoid initial silence
    // This simulates exciting the whole string at once (like a bow or wide pluck)
    // We loop the exciter signal to fill the entire delay line
    for (size_t i = 0; i < delayLine.size(); ++i)
    {
        int exciterIdx = i % numSamples;
        delayLine[i] = exciterSignal[exciterIdx] * velocity;
    }

    // Reset write pointer to beginning (excitation fills whole buffer)
    writeIndex = 0;
}

float AetherStringWaveguideString::processSample()
{
    // Read from delay line
    int readIndex = (writeIndex - delayLength + static_cast<int>(delayLine.size()))
                     % static_cast<int>(delayLine.size());

    float output = delayLine[readIndex];

    // Apply stiffness filter (allpass for inharmonicity)
    float stiffened = processStiffnessFilter(output);

    // Apply damping filter (lowpass for brightness)
    float damped = processDampingFilter(stiffened);

    // Write back to delay line
    delayLine[writeIndex] = damped;
    writeIndex = (writeIndex + 1) % delayLine.size();

    // Calculate bridge energy (output) - scale for better signal level
    lastBridgeEnergy = damped * params.bridgeCoupling * 5.0f;

    return lastBridgeEnergy;
}

void AetherStringWaveguideString::setParameters(const Parameters& p)
{
    params = p;
    delayLength = calculateDelayLength(p.frequency);
}

void AetherStringWaveguideString::injectReflection(float reflection)
{
    // Add reflected energy to the most recently written sample in delay line
    int lastWriteIndex = (writeIndex - 1 + static_cast<int>(delayLine.size()))
                         % static_cast<int>(delayLine.size());
    delayLine[lastWriteIndex] += reflection;
}

float AetherStringWaveguideString::processStiffnessFilter(float input)
{
    // First-order allpass filter for inharmonicity with NaN safety
    float coefficient = params.stiffness;

    // Check for NaN input
    if (std::isnan(input) || std::isinf(input))
    {
        input = 0.0f;
    }

    float output = coefficient * (input - stiffnessState) + stiffnessState;
    stiffnessState = input;

    // Clamp output to reasonable range
    output = std::max(-10.0f, std::min(10.0f, output));

    return output;
}

float AetherStringWaveguideString::processDampingFilter(float input)
{
    // First-order lowpass filter for brightness control with NaN safety
    float brightness = params.brightness;

    // Check for NaN input
    if (std::isnan(input) || std::isinf(input))
    {
        input = 0.0f;
    }

    // Convert to one-pole lowpass coefficient
    // Higher brightness = less filtering (more high frequencies)
    float alpha = 1.0f - (brightness * 0.1f);
    float output = alpha * dampingState + (1.0f - alpha) * input;
    dampingState = output;

    // Apply gentle damping per-sample (much less aggressive)
    // damping parameter: 0.996 means very slight decay per sample
    // This accumulates to natural decay over delay line period
    float damping = params.damping; // 0-1, where 1 is no damping
    float perSampleDecay = 1.0f - ((1.0f - damping) * 0.01f); // Much gentler

    output = output * perSampleDecay;

    // Clamp output to prevent explosion
    output = std::max(-10.0f, std::min(10.0f, output));

    return output;
}

int AetherStringWaveguideString::calculateDelayLength(float frequency)
{
    if (frequency <= 0.0f) return static_cast<int>(delayLine.size() / 2);

    double period = sampleRate / frequency;
    int length = static_cast<int>(period);

    // Clamp to valid range
    length = std::max(10, std::min(length, static_cast<int>(delayLine.size()) - 10));

    return length;
}

//==============================================================================
// Bridge Coupling Implementation
//==============================================================================

AetherStringBridgeCoupling::AetherStringBridgeCoupling()
{
}

void AetherStringBridgeCoupling::prepare(double sampleRate)
{
    this->sampleRate = sampleRate;
}

void AetherStringBridgeCoupling::reset()
{
    bridgeEnergy = 0.0f;
}

float AetherStringBridgeCoupling::processString(float stringOutput)
{
    // Apply nonlinear saturation to prevent explosion
    float input = stringOutput * couplingCoefficient * (1.0f + nonlinearity);
    bridgeEnergy = std::tanh(input);

    // Reflected energy
    float reflected = stringOutput - bridgeEnergy;

    return reflected;
}

void AetherStringBridgeCoupling::setCouplingCoefficient(float coeff)
{
    couplingCoefficient = std::max(0.0f, std::min(1.0f, coeff));
}

void AetherStringBridgeCoupling::setNonlinearity(float nonlin)
{
    nonlinearity = std::max(0.0f, std::min(1.0f, nonlin));
}

//==============================================================================
// Modal Filter Implementation
//==============================================================================

void AetherStringModalFilter::prepare(double sampleRate)
{
    // Store the actual sample rate for use in processSample
    this->sampleRate = sampleRate;
}

float AetherStringModalFilter::processSample(float excitation)
{
    // Simple resonant filter (2nd order harmonic oscillator)
    // Use stored sample rate instead of hardcoded 48000.0
    float safeSampleRate = static_cast<float>(sampleRate > 0.0 ? sampleRate : 48000.0);
    float omega = 2.0f * M_PI * frequency / safeSampleRate;

    phase += omega;
    if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;

    // Decay energy with NaN safety
    // Prevent division by zero: clamp decay to minimum value
    float safeDecay = std::max(0.001f, decay);
    float decayFactor = std::exp(-1.0f / (safeDecay * safeSampleRate));

    // Clamp energy to prevent NaN/Inf explosion
    energy = energy * decayFactor + excitation * amplitude * 0.1f;
    energy = std::max(-100.0f, std::min(100.0f, energy));  // Safety clamp

    float output = std::sin(phase) * energy * baseAmplitude;

    // Final NaN check - return 0.0f if NaN detected
    if (std::isnan(output) || std::isinf(output))
    {
        energy = 0.0f;  // Reset energy on NaN
        return 0.0f;
    }

    return output;
}

void AetherStringModalFilter::reset()
{
    phase = 0.0f;
    energy = 0.0f;
}

//==============================================================================
// Modal Body Resonator Implementation
//==============================================================================

AetherStringModalBodyResonator::AetherStringModalBodyResonator()
{
    modes.resize(8);
}

void AetherStringModalBodyResonator::prepare(double sampleRate)
{
    this->sampleRate = sampleRate;
    for (auto& mode : modes)
    {
        mode.prepare(sampleRate);
    }
}

void AetherStringModalBodyResonator::reset()
{
    for (auto& mode : modes)
    {
        mode.reset();
    }
}

float AetherStringModalBodyResonator::processSample(float bridgeEnergy)
{
    float output = 0.0f;
    for (auto& mode : modes)
    {
        output += mode.processSample(bridgeEnergy);
    }
    return output * resonanceAmount;
}

void AetherStringModalBodyResonator::setResonance(float amount)
{
    resonanceAmount = amount;
}

void AetherStringModalBodyResonator::loadGuitarBodyPreset()
{
    // Typical acoustic guitar body modes
    modes.resize(8);

    modes[0].frequency = 95.0f;   // Air resonance
    modes[0].amplitude = 1.0f;
    modes[0].decay = 2.0f;

    modes[1].frequency = 190.0f;  // Top plate
    modes[1].amplitude = 0.8f;
    modes[1].decay = 1.5f;

    modes[2].frequency = 280.0f;  // Back plate
    modes[2].amplitude = 0.6f;
    modes[2].decay = 1.2f;

    modes[3].frequency = 400.0f;  // Helmholtz
    modes[3].amplitude = 0.5f;
    modes[3].decay = 1.0f;

    modes[4].frequency = 580.0f;  // Higher stiffness
    modes[4].amplitude = 0.4f;
    modes[4].decay = 0.8f;

    modes[5].frequency = 750.0f;
    modes[5].amplitude = 0.3f;
    modes[5].decay = 0.6f;

    modes[6].frequency = 920.0f;
    modes[6].amplitude = 0.2f;
    modes[6].decay = 0.5f;

    modes[7].frequency = 1100.0f;
    modes[7].amplitude = 0.15f;
    modes[7].decay = 0.4f;
}

float AetherStringModalBodyResonator::getModeFrequency(int index) const
{
    if (index >= 0 && index < static_cast<int>(modes.size()))
        return modes[index].frequency;
    return 0.0f;
}

//==============================================================================
// Articulation State Machine Implementation
//==============================================================================

AetherStringArticulationStateMachine::AetherStringArticulationStateMachine()
{
}

void AetherStringArticulationStateMachine::prepare(double sampleRate)
{
    this->sampleRate = sampleRate;
}

void AetherStringArticulationStateMachine::reset()
{
    currentState = AetherStringArticulationState::IDLE;
    previousState = AetherStringArticulationState::IDLE;
    currentGain = 0.0f;
    targetGain = 0.0f;
    stateTime = 0.0;
}

void AetherStringArticulationStateMachine::noteOn()
{
    changeState(AetherStringArticulationState::ATTACK_PLUCK);
}

void AetherStringArticulationStateMachine::noteOff(bool damping)
{
    if (damping)
    {
        changeState(AetherStringArticulationState::RELEASE_DAMP);
    }
    else
    {
        changeState(AetherStringArticulationState::RELEASE_GHOST);
    }
}

void AetherStringArticulationStateMachine::setArticulation(AetherStringArticulationState state)
{
    changeState(state);
}

float AetherStringArticulationStateMachine::processSample()
{
    updateGain();
    stateTime += 1.0 / sampleRate;

    // State transitions
    switch (currentState)
    {
        case AetherStringArticulationState::ATTACK_PLUCK:
            if (stateTime >= attackTime)
            {
                changeState(AetherStringArticulationState::DECAY);
            }
            break;

        case AetherStringArticulationState::DECAY:
            if (stateTime >= decayTime)
            {
                changeState(AetherStringArticulationState::SUSTAIN_BOW);
            }
            break;

        case AetherStringArticulationState::SUSTAIN_BOW:
            // Stay here until noteOff
            break;

        case AetherStringArticulationState::RELEASE_GHOST:
            if (stateTime >= releaseTime)
            {
                changeState(AetherStringArticulationState::IDLE);
            }
            break;

        case AetherStringArticulationState::RELEASE_DAMP:
            if (stateTime >= dampingReleaseTime)
            {
                changeState(AetherStringArticulationState::IDLE);
            }
            break;

        case AetherStringArticulationState::IDLE:
            currentGain = 0.0f;
            break;
    }

    return currentGain;
}

void AetherStringArticulationStateMachine::changeState(AetherStringArticulationState newState)
{
    if (newState == currentState) return;

    previousState = currentState;
    currentState = newState;
    stateTime = 0.0;

    // Set target gain based on state
    switch (newState)
    {
        case AetherStringArticulationState::ATTACK_PLUCK:
            targetGain = 1.0f;
            break;

        case AetherStringArticulationState::DECAY:
            targetGain = sustainLevel;
            break;

        case AetherStringArticulationState::SUSTAIN_BOW:
            targetGain = sustainLevel;
            break;

        case AetherStringArticulationState::RELEASE_GHOST:
        case AetherStringArticulationState::RELEASE_DAMP:
            targetGain = 0.0f;
            break;

        case AetherStringArticulationState::IDLE:
            targetGain = 0.0f;
            break;
    }
}

void AetherStringArticulationStateMachine::updateGain()
{
    // Smooth gain changes with NaN safety
    float smoothingTime = 0.01f; // 10ms crossfade

    // Guard against invalid sample rate
    double safeSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    float samples = static_cast<float>(smoothingTime * safeSampleRate);

    // Prevent division by zero
    if (samples < 1.0f) samples = 1.0f;

    float coef = std::exp(-1.0f / samples);

    currentGain = currentGain * coef + targetGain * (1.0f - coef);

    // Clamp gain to valid range and check for NaN
    currentGain = std::max(0.0f, std::min(1.0f, currentGain));
    if (std::isnan(currentGain) || std::isinf(currentGain))
    {
        currentGain = 0.0f;
    }
}

float AetherStringArticulationStateMachine::crossfadeGain(float oldValue, float newValue, float progress)
{
    // Equal-power crossfade
    float oldGain = std::cos(progress * M_PI * 0.5f);
    float newGain = std::sin(progress * M_PI * 0.5f);
    return oldValue * oldGain + newValue * newGain;
}

void AetherStringArticulationStateMachine::setAttackTime(float timeMs)
{
    attackTime = timeMs;
}

void AetherStringArticulationStateMachine::setDecayTime(float timeMs)
{
    decayTime = timeMs;
}

void AetherStringArticulationStateMachine::setSustainLevel(float level)
{
    sustainLevel = level;
}

void AetherStringArticulationStateMachine::setReleaseTime(float timeMs)
{
    releaseTime = timeMs;
}

void AetherStringArticulationStateMachine::setDampingReleaseTime(float timeMs)
{
    dampingReleaseTime = timeMs;
}

//==============================================================================
// AetherStringVoice Implementation
//==============================================================================

void AetherStringVoice::prepare(double sampleRate, int maxDelaySamples)
{
    string.prepare(sampleRate, maxDelaySamples);
    bridge.prepare(sampleRate);
    body.prepare(sampleRate);
    articulation.prepare(sampleRate);
}

void AetherStringVoice::reset()
{
    string.reset();
    bridge.reset();
    body.reset();
    articulation.reset();

    midiNote = -1;
    velocity = 0.0f;
    active = false;
    startTime = 0.0;
}

void AetherStringVoice::noteOn(int note, float vel, double currentSampleRate)
{
    midiNote = note;
    velocity = vel;
    active = true;
    startTime = currentSampleRate;

    articulation.noteOn();

    // Set string frequency
    AetherStringWaveguideString::Parameters params = string.getParameters();
    params.frequency = static_cast<float>(440.0 * std::pow(2.0, (note - 69) / 12.0));
    string.setParameters(params);

    // Generate pluck excitation (with higher amplitude for testing)
    float excitation[100];
    std::mt19937 gen(static_cast<unsigned>(note)); // Deterministic PRNG seeded by note
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (int i = 0; i < 100; ++i)
    {
        excitation[i] = dist(gen) * vel * 5.0f; // Increased amplitude for better signal
    }

    string.excite(excitation, 100, vel);
}

void AetherStringVoice::noteOff(bool damping)
{
    articulation.noteOff(damping);
}

bool AetherStringVoice::isActive() const
{
    return active && articulation.getCurrentState() != AetherStringArticulationState::IDLE;
}

float AetherStringVoice::renderSample()
{
    // Process string (this reads from delay line, processes, and writes back)
    float stringOutput = string.processSample();

    // Check for NaN from string
    if (std::isnan(stringOutput) || std::isinf(stringOutput))
    {
        stringOutput = 0.0f;
    }

    // Process bridge coupling - this returns reflected energy
    float reflected = bridge.processString(stringOutput);

    // CRITICAL: Feed reflected energy back into string's delay line
    // This is the KEY to Karplus-Strong - energy must recirculate!
    string.injectReflection(reflected);

    // Get bridge energy for body
    float bridgeEnergy = bridge.getBridgeEnergy();

    // Check for NaN from bridge
    if (std::isnan(bridgeEnergy) || std::isinf(bridgeEnergy))
    {
        bridgeEnergy = 0.0f;
    }

    // Process body resonator
    float bodyOutput = body.processSample(bridgeEnergy);

    // Check for NaN from body
    if (std::isnan(bodyOutput) || std::isinf(bodyOutput))
    {
        bodyOutput = 0.0f;
    }

    // Get articulation gain
    float gain = articulation.processSample();

    // Check for NaN gain
    if (std::isnan(gain) || std::isinf(gain))
    {
        gain = 0.0f;
    }

    // Mix string and body (with higher body contribution for warmth)
    float output = (stringOutput * 0.5f + bodyOutput * 0.5f) * gain;

    // Final NaN check
    if (std::isnan(output) || std::isinf(output))
    {
        output = 0.0f;
    }

    // Check if voice is done
    if (!isActive())
    {
        active = false;
    }

    return output;
}

//==============================================================================
// AetherStringVoice Manager Implementation
//==============================================================================

AetherStringVoiceManager::AetherStringVoiceManager()
{
}

void AetherStringVoiceManager::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate_ = sampleRate;
    maxDelaySamples_ = static_cast<int>(sampleRate * 2.0); // 2 seconds max delay

    for (auto& voice : voices_)
    {
        voice.prepare(sampleRate, maxDelaySamples_);
    }
}

void AetherStringVoiceManager::reset()
{
    for (auto& voice : voices_)
    {
        voice.reset();
    }
}

AetherStringVoice* AetherStringVoiceManager::findFreeVoice()
{
    for (auto& voice : voices_)
    {
        if (!voice.active)
        {
            return &voice;
        }
    }

    // All voices active - find oldest
    AetherStringVoice* oldest = &voices_[0];
    double oldestTime = voices_[0].startTime;

    for (auto& voice : voices_)
    {
        if (voice.startTime < oldestTime)
        {
            oldest = &voice;
            oldestTime = voice.startTime;
        }
    }

    return oldest;
}

AetherStringVoice* AetherStringVoiceManager::findVoiceForNote(int note)
{
    for (auto& voice : voices_)
    {
        if (voice.active && voice.midiNote == note)
        {
            return &voice;
        }
    }
    return nullptr;
}

void AetherStringVoiceManager::handleNoteOn(int note, float velocity)
{
    AetherStringVoice* voice = findFreeVoice();
    if (voice)
    {
        voice->noteOn(note, velocity, currentSampleRate_);
    }
}

void AetherStringVoiceManager::handleNoteOff(int note, bool damping)
{
    AetherStringVoice* voice = findVoiceForNote(note);
    if (voice)
    {
        voice->noteOff(damping);
    }
}

void AetherStringVoiceManager::allNotesOff()
{
    for (auto& voice : voices_)
    {
        voice.noteOff(true);
    }
}

void AetherStringVoiceManager::processBlock(float* output, int numSamples)
{
    std::fill(output, output + numSamples, 0.0f);

    for (auto& voice : voices_)
    {
        if (voice.active)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                output[i] += voice.renderSample();
            }
        }
    }
}

int AetherStringVoiceManager::getActiveVoiceCount() const
{
    int count = 0;
    for (const auto& voice : voices_)
    {
        if (voice.active) count++;
    }
    return count;
}

void AetherStringVoiceManager::setStringParameters(const AetherStringWaveguideString::Parameters& params)
{
    for (auto& voice : voices_)
    {
        voice.string.setParameters(params);
    }
}

void AetherStringVoiceManager::setBodyResonance(float amount)
{
    for (auto& voice : voices_)
    {
        voice.body.setResonance(amount);
    }
}

void AetherStringVoiceManager::loadGuitarBodyPreset()
{
    for (auto& voice : voices_)
    {
        voice.body.loadGuitarBodyPreset();
    }
}

//==============================================================================
// Main Instrument Implementation
//==============================================================================

StringPureDSP::StringPureDSP()
{
    // Load guitar body preset (will be applied in prepare)
}

StringPureDSP::~StringPureDSP()
{
}

bool StringPureDSP::prepare(double sampleRate, int blockSize)
{
    sampleRate_ = sampleRate;
    blockSize_ = blockSize;

    int maxDelaySamples = static_cast<int>(sampleRate * 2.0);
    voiceManager_.prepare(sampleRate, blockSize);

    // Load guitar body preset on first prepare
    voiceManager_.loadGuitarBodyPreset();

    return true;
}

void StringPureDSP::reset()
{
    voiceManager_.reset();
    pitchBend_ = 0.0;
}

void StringPureDSP::process(float** outputs, int numChannels, int numSamples)
{
    // Clear output buffers
    for (int ch = 0; ch < numChannels; ++ch)
    {
        std::fill(outputs[ch], outputs[ch] + numSamples, 0.0f);
    }

    // Render mono using real-time safe stack buffer
    // Assert for safety in debug builds - block size should never exceed MAX_BLOCK_SIZE
    assert(numSamples <= MAX_BLOCK_SIZE && "Block size exceeds maximum buffer size");

    voiceManager_.processBlock(tempBuffer_, numSamples);

    // Apply master volume and copy to outputs with NaN safety
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = tempBuffer_[i] * params_.masterVolume;

        // Check for NaN/Inf in final output
        if (std::isnan(sample) || std::isinf(sample))
        {
            sample = 0.0f;
        }

        // Clamp to reasonable range
        sample = std::max(-1.0f, std::min(1.0f, sample));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            outputs[ch][i] = sample;
        }
    }
}

void StringPureDSP::handleEvent(const ScheduledEvent& event)
{
    switch (event.type)
    {
        case ScheduledEvent::NOTE_ON:
            voiceManager_.handleNoteOn(event.data.note.midiNote, event.data.note.velocity);
            break;

        case ScheduledEvent::NOTE_OFF:
            voiceManager_.handleNoteOff(event.data.note.midiNote);
            break;

        case ScheduledEvent::PITCH_BEND:
            pitchBend_ = event.data.pitchBend.bendValue * params_.pitchBendRange;
            break;

        default:
            break;
    }
}

float StringPureDSP::getParameter(const char* paramId) const
{
    if (std::strcmp(paramId, "master_volume") == 0)
        return params_.masterVolume;
    if (std::strcmp(paramId, "string_damping") == 0)
        return params_.stringDamping;
    if (std::strcmp(paramId, "string_stiffness") == 0)
        return params_.stringStiffness;
    if (std::strcmp(paramId, "string_brightness") == 0)
        return params_.stringBrightness;
    if (std::strcmp(paramId, "bridge_coupling") == 0)
        return params_.bridgeCoupling;
    if (std::strcmp(paramId, "body_resonance") == 0)
        return params_.bodyResonance;
    if (std::strcmp(paramId, "attack_time") == 0)
        return params_.attackTime;
    if (std::strcmp(paramId, "decay_time") == 0)
        return params_.decayTime;
    if (std::strcmp(paramId, "sustain_level") == 0)
        return params_.sustainLevel;
    if (std::strcmp(paramId, "release_time") == 0)
        return params_.releaseTime;

    return 0.0f;
}

void StringPureDSP::setParameter(const char* paramId, float value)
{
    // Get old value for logging (before change)
    float oldValue = getParameter(paramId);

    if (std::strcmp(paramId, "master_volume") == 0)
        params_.masterVolume = value;
    else if (std::strcmp(paramId, "string_damping") == 0)
        params_.stringDamping = value;
    else if (std::strcmp(paramId, "string_stiffness") == 0)
        params_.stringStiffness = value;
    else if (std::strcmp(paramId, "string_brightness") == 0)
        params_.stringBrightness = value;
    else if (std::strcmp(paramId, "bridge_coupling") == 0)
        params_.bridgeCoupling = value;
    else if (std::strcmp(paramId, "body_resonance") == 0)
        params_.bodyResonance = value;
    else if (std::strcmp(paramId, "attack_time") == 0)
        params_.attackTime = value;
    else if (std::strcmp(paramId, "decay_time") == 0)
        params_.decayTime = value;
    else if (std::strcmp(paramId, "sustain_level") == 0)
        params_.sustainLevel = value;
    else if (std::strcmp(paramId, "release_time") == 0)
        params_.releaseTime = value;

    // Log parameter change (shared telemetry infrastructure)
    LOG_PARAMETER_CHANGE("NatureAetherString", paramId, oldValue, value);

    applyParameters();
}

bool StringPureDSP::savePreset(char* jsonBuffer, int jsonBufferSize) const
{
    int offset = 0;

    // Write opening brace
    int remaining = jsonBufferSize - offset;
    if (remaining < 10) return false;
    std::snprintf(jsonBuffer + offset, remaining, "{");
    offset = 1;

    writeJsonParameter("master_volume", params_.masterVolume, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("string_damping", params_.stringDamping, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("string_stiffness", params_.stringStiffness, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("string_brightness", params_.stringBrightness, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("bridge_coupling", params_.bridgeCoupling, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("body_resonance", params_.bodyResonance, jsonBuffer, offset, jsonBufferSize);

    // Remove trailing comma and add closing brace
    if (offset > 1 && jsonBuffer[offset - 1] == ',')
    {
        offset--;
    }

    remaining = jsonBufferSize - offset;
    if (remaining < 2) return false;
    std::snprintf(jsonBuffer + offset, remaining, "}");

    return true;
}

bool StringPureDSP::loadPreset(const char* jsonData)
{
    // Check if this is UPFS v1.0 format by looking for "version" and "format" fields
    bool isUPFS = (std::strstr(jsonData, "\"version\"") != nullptr) &&
                  (std::strstr(jsonData, "\"format\"") != nullptr) &&
                  (std::strstr(jsonData, "\"UPFS\"") != nullptr);

    if (isUPFS)
    {
        return loadUPFSPreset(jsonData);
    }
    else
    {
        return loadLegacyPreset(jsonData);
    }
}

bool StringPureDSP::loadUPFSPreset(const char* jsonData)
{
    double value;

    // UPFS v1.0 format parameters
    if (parseJsonParameter(jsonData, "string_damping", value))
        params_.stringDamping = static_cast<float>(value);
    if (parseJsonParameter(jsonData, "string_stiffness", value))
        params_.stringStiffness = static_cast<float>(value);
    if (parseJsonParameter(jsonData, "string_brightness", value))
        params_.stringBrightness = static_cast<float>(value);
    if (parseJsonParameter(jsonData, "bridge_coupling", value))
        params_.bridgeCoupling = static_cast<float>(value);
    if (parseJsonParameter(jsonData, "body_resonance", value))
        params_.bodyResonance = static_cast<float>(value);

    // Master volume (optional in UPFS, default if not present)
    if (parseJsonParameter(jsonData, "master_volume", value))
        params_.masterVolume = static_cast<float>(value);
    else
        params_.masterVolume = 0.85f; // Default value

    applyParameters();

    return true;
}

bool StringPureDSP::loadLegacyPreset(const char* jsonData)
{
    double value;

    // Legacy format parameters
    if (parseJsonParameter(jsonData, "master_volume", value))
        params_.masterVolume = static_cast<float>(value);
    if (parseJsonParameter(jsonData, "string_damping", value))
        params_.stringDamping = static_cast<float>(value);
    if (parseJsonParameter(jsonData, "string_stiffness", value))
        params_.stringStiffness = static_cast<float>(value);
    if (parseJsonParameter(jsonData, "string_brightness", value))
        params_.stringBrightness = static_cast<float>(value);
    if (parseJsonParameter(jsonData, "bridge_coupling", value))
        params_.bridgeCoupling = static_cast<float>(value);
    if (parseJsonParameter(jsonData, "body_resonance", value))
        params_.bodyResonance = static_cast<float>(value);

    applyParameters();

    return true;
}

int StringPureDSP::getActiveVoiceCount() const
{
    return voiceManager_.getActiveVoiceCount();
}

void StringPureDSP::applyParameters()
{
    AetherStringWaveguideString::Parameters stringParams;
    stringParams.damping = params_.stringDamping;
    stringParams.stiffness = params_.stringStiffness;
    stringParams.brightness = params_.stringBrightness;
    stringParams.bridgeCoupling = params_.bridgeCoupling;

    voiceManager_.setStringParameters(stringParams);
    voiceManager_.setBodyResonance(params_.bodyResonance);
}

void StringPureDSP::processStereoSample(float& left, float& right)
{
    // Not used in current implementation
}

float StringPureDSP::calculateFrequency(int midiNote, float bend) const
{
    return static_cast<float>(440.0 * std::pow(2.0, (midiNote - 69 + bend) / 12.0));
}

void StringPureDSP::generatePluckExcitation(float* output, int numSamples)
{
    std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        output[i] = dist(gen);
    }
}

void StringPureDSP::generateBowExcitation(float* output, int numSamples)
{
    std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);

    for (int i = 0; i < numSamples; ++i)
    {
        output[i] = dist(gen);
    }
}

bool StringPureDSP::writeJsonParameter(const char* name, double value,
                                                       char* buffer, int& offset,
                                                       int bufferSize) const
{
    int remaining = bufferSize - offset;
    if (remaining < 50) return false;

    int written = std::snprintf(buffer + offset, remaining,
                               "\"%s\":%.6f,",
                               name, value);

    if (written < 0 || written >= remaining) return false;

    offset += written;
    return true;
}

bool StringPureDSP::parseJsonParameter(const char* json, const char* param,
                                                       double& value) const
{
    const char* search = json;
    char pattern[100];
    std::snprintf(pattern, sizeof(pattern), "\"%s\":", param);

    const char* found = std::strstr(search, pattern);
    if (!found) return false;

    found += std::strlen(pattern);
    value = std::atof(found);

    return true;
}

//==============================================================================
// Static Factory (No runtime registration for tvOS hardening)
//==============================================================================

// Pure DSP instruments are instantiated directly, not through dynamic factory
// This ensures tvOS compatibility (no static initialization, no global state)

} // namespace DSP
