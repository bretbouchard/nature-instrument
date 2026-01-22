/*
  ==============================================================================

    StringPureDSP.h
    Created: December 30, 2025
    Author: Bret Bouchard

    Pure DSP implementation of Aether String
    - Karplus-Strong waveguide synthesis
    - Physical string modeling with bridge coupling
    - Modal body resonator
    - Articulation state machine
    - 6-voice polyphony
    - Factory-creatable for dynamic instantiation
    - Zero JUCE dependencies

  ==============================================================================
*/

#pragma once

#include "../../../../include/dsp/InstrumentDSP.h"
#include <vector>
#include <array>
#include <memory>
#include <cmath>
#include <functional>
#include <algorithm>
#include <string>

namespace DSP {

//==============================================================================
// Waveguide String (Karplus-Strong Extension)
//==============================================================================

class AetherStringWaveguideString
{
public:
    struct Parameters
    {
        float frequency = 440.0f;      // String fundamental (Hz)
        float damping = 0.996f;        // Energy loss coefficient (0-1)
        float stiffness = 0.0f;        // Allpass coefficient for inharmonicity (0-0.5)
        float brightness = 0.5f;       // High-frequency damping (0-1)
        float bridgeCoupling = 0.3f;   // Bridge coupling coefficient (0-1)
        float nonlinearity = 0.1f;     // Bridge nonlinearity (0-1)
    };

    AetherStringWaveguideString();
    ~AetherStringWaveguideString();

    void prepare(double sampleRate, int maxDelaySamples);
    void reset();

    void excite(const float* exciterSignal, int numSamples, float velocity);
    float processSample();

    void setParameters(const Parameters& p);
    Parameters getParameters() const { return params; }

    float getBridgeEnergy() const { return lastBridgeEnergy; }

    // Inject reflection back into delay line (for Karplus-Strong feedback)
    void injectReflection(float reflection);

private:
    Parameters params;

    // Fractional delay line
    std::vector<float> delayLine;
    int writeIndex = 0;
    int delayLength = 0;

    // Filter states
    float stiffnessState = 0.0f;
    float dampingState = 0.0f;

    // State
    double sampleRate = 48000.0;
    float lastBridgeEnergy = 0.0f;

    // Internal processing
    float processStiffnessFilter(float input);
    float processDampingFilter(float input);
    int calculateDelayLength(float frequency);
};

//==============================================================================
// Bridge Coupling
//==============================================================================

class AetherStringBridgeCoupling
{
public:
    AetherStringBridgeCoupling();
    ~AetherStringBridgeCoupling() = default;

    void prepare(double sampleRate);
    void reset();

    float processString(float stringOutput);
    float getBridgeEnergy() const { return bridgeEnergy; }

    void setCouplingCoefficient(float coeff);
    void setNonlinearity(float nonlin);

private:
    float couplingCoefficient = 0.3f;
    float nonlinearity = 0.1f;
    float bridgeEnergy = 0.0f;
    double sampleRate = 48000.0;
};

//==============================================================================
// Modal Filter (Single Body Mode)
//==============================================================================

struct AetherStringModalFilter
{
    float frequency = 440.0f;
    float amplitude = 1.0f;
    float decay = 1.0f;
    float phase = 0.0f;
    float energy = 0.0f;
    float baseAmplitude = 1.0f;
    double sampleRate = 48000.0;  // Store actual sample rate

    void prepare(double sampleRate);
    float processSample(float excitation);
    void reset();
};

//==============================================================================
// Modal Body Resonator
//==============================================================================

class AetherStringModalBodyResonator
{
public:
    AetherStringModalBodyResonator();
    ~AetherStringModalBodyResonator() = default;

    void prepare(double sampleRate);
    void reset();

    float processSample(float bridgeEnergy);
    void setResonance(float amount);
    void loadGuitarBodyPreset();

    int getNumModes() const { return static_cast<int>(modes.size()); }
    float getModeFrequency(int index) const;

private:
    std::vector<AetherStringModalFilter> modes;
    double sampleRate = 48000.0;
    float resonanceAmount = 1.0f;
};

//==============================================================================
// Articulation State Machine
//==============================================================================

enum class AetherStringArticulationState
{
    IDLE,
    ATTACK_PLUCK,
    DECAY,
    SUSTAIN_BOW,
    RELEASE_GHOST,
    RELEASE_DAMP
};

class AetherStringArticulationStateMachine
{
public:
    AetherStringArticulationStateMachine();
    ~AetherStringArticulationStateMachine() = default;

    void prepare(double sampleRate);
    void reset();

    void noteOn();
    void noteOff(bool damping);
    void setArticulation(AetherStringArticulationState state);

    AetherStringArticulationState getCurrentState() const { return currentState; }
    float getGain() const { return currentGain; }

    void changeState(AetherStringArticulationState newState);

    void setAttackTime(float timeMs);
    void setDecayTime(float timeMs);
    void setSustainLevel(float level);
    void setReleaseTime(float timeMs);
    void setDampingReleaseTime(float timeMs);

    float processSample();

private:
    AetherStringArticulationState currentState = AetherStringArticulationState::IDLE;
    AetherStringArticulationState previousState = AetherStringArticulationState::IDLE;

    float currentGain = 0.0f;
    float targetGain = 0.0f;
    float smoothingCoefficient = 0.001f;

    double sampleRate = 48000.0;
    double stateTime = 0.0;

    // Envelope parameters
    float attackTime = 0.05f;     // 50ms
    float decayTime = 1.0f;       // 1s
    float sustainLevel = 0.7f;
    float releaseTime = 2.0f;     // 2s
    float dampingReleaseTime = 0.3f; // 300ms

    void updateGain();
    float crossfadeGain(float oldValue, float newValue, float progress);
};

//==============================================================================
// Voice (6-Voice Polyphony)
//==============================================================================

struct AetherStringVoice
{
    int midiNote = -1;
    float velocity = 0.0f;
    bool active = false;
    double startTime = 0.0;

    // DSP components
    AetherStringWaveguideString string;
    AetherStringBridgeCoupling bridge;
    AetherStringModalBodyResonator body;
    AetherStringArticulationStateMachine articulation;

    void prepare(double sampleRate, int maxDelaySamples);
    void reset();
    void noteOn(int note, float vel, double currentSampleRate);
    void noteOff(bool damping = false);
    bool isActive() const;
    float renderSample();
};

//==============================================================================
// Voice Manager
//==============================================================================

class AetherStringVoiceManager
{
public:
    AetherStringVoiceManager();
    ~AetherStringVoiceManager() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    AetherStringVoice* findFreeVoice();
    AetherStringVoice* findVoiceForNote(int note);

    void handleNoteOn(int note, float velocity);
    void handleNoteOff(int note, bool damping = false);
    void allNotesOff();

    void processBlock(float* output, int numSamples);
    int getActiveVoiceCount() const;

    void setStringParameters(const AetherStringWaveguideString::Parameters& params);
    void setBodyResonance(float amount);
    void loadGuitarBodyPreset();

private:
    std::array<AetherStringVoice, 6> voices_;
    double currentSampleRate_ = 48000.0;
    int maxDelaySamples_ = 0;
};

//==============================================================================
// Main Aether String Pure DSP Instrument
//==============================================================================

class StringPureDSP : public InstrumentDSP
{
public:
    StringPureDSP();
    ~StringPureDSP() override;

    bool prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void process(float** outputs, int numChannels, int numSamples) override;
    void handleEvent(const ScheduledEvent& event) override;

    float getParameter(const char* paramId) const override;
    void setParameter(const char* paramId, float value) override;

    bool savePreset(char* jsonBuffer, int jsonBufferSize) const override;
    bool loadPreset(const char* jsonData) override;

    int getActiveVoiceCount() const override;
    int getMaxPolyphony() const override { return 6; }

    const char* getInstrumentName() const override { return "NatureAetherString"; }
    const char* getInstrumentVersion() const override { return "1.0.0"; }

private:
    AetherStringVoiceManager voiceManager_;

    struct Parameters
    {
        // String parameters
        float stringDamping = 0.996f;
        float stringStiffness = 0.0f;
        float stringBrightness = 0.5f;
        float bridgeCoupling = 0.3f;
        float bridgeNonlinearity = 0.1f;

        // Body resonator
        float bodyResonance = 1.0f;

        // Structure (Mutable Instruments-style harmonic complexity)
        // 0.0 = simple, pure string (clean fundamental, minimal overtones)
        // 0.5 = balanced (default)
        // 1.0 = complex, rich string (stiffness inharmonicity, bridge nonlinearity, body resonance)
        float structure = 0.5f;

        // Articulation
        float attackTime = 0.05f;
        float decayTime = 1.0f;
        float sustainLevel = 0.7f;
        float releaseTime = 2.0f;
        float dampingReleaseTime = 0.3f;

        // Excitation
        float pluckNoiseMix = 0.3f;
        float bowNoiseMix = 0.5f;

        // Global
        float masterVolume = 0.8f;
        float pitchBendRange = 2.0f;
    } params_;

    double sampleRate_ = 48000.0;
    int blockSize_ = 512;
    double pitchBend_ = 0.0;

    // Real-time safe temporary buffer for audio processing
    static constexpr int MAX_BLOCK_SIZE = 512;
    alignas(32) float tempBuffer_[MAX_BLOCK_SIZE];

    void applyParameters();
    void processStereoSample(float& left, float& right);

    float calculateFrequency(int midiNote, float bend = 0.0f) const;
    void generatePluckExcitation(float* output, int numSamples);
    void generateBowExcitation(float* output, int numSamples);

    bool writeJsonParameter(const char* name, double value, char* buffer,
                            int& offset, int bufferSize) const;
    bool parseJsonParameter(const char* json, const char* param, double& value) const;

    // UPFS v1.0 preset loading support
    bool loadUPFSPreset(const char* jsonData);
    bool loadLegacyPreset(const char* jsonData);
};

//==============================================================================
// Utility Function Declarations (defined in NexSynthDSP.h)
//==============================================================================
// Note: midiToFrequency, lerp, and clamp are defined in NexSynthDSP.h
// to avoid ODR violations across multiple instrument headers

} // namespace DSP
