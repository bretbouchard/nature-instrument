/*
  ==============================================================================

    AetherPureDSP.h
    Created: December 30, 2025
    Author: Bret Bouchard

    Pure DSP implementation of Aether String for tvOS
    - Inherits from DSP::InstrumentDSP (no JUCE dependencies)
    - Headless operation (no GUI)
    - Physical modeling synthesis (Karplus-Strong waveguide)
    - JSON preset save/load system
    - Factory-creatable for dynamic instantiation

    v2 Features:
    - Scale physics (stringLengthMeters, stringGauge, pickPosition)
    - Gesture parameters (force, speed, contactArea, roughness)
    - Shared bridge coupling (multi-string interaction)
    - Sympathetic strings (resonant halo effect)

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

namespace DSP {

//==============================================================================
// Forward Declarations
//==============================================================================

class AetherVoiceManager;
class Pedalboard;
class SharedBridgeCoupling;
class SympatheticStringBank;

//==============================================================================
// Pure DSP Building Blocks (JUCE-free implementations)
//==============================================================================

/**
 * @brief Fractional delay line with Lagrange interpolation
 */
class FractionalDelayLine
{
public:
    FractionalDelayLine();
    ~FractionalDelayLine() = default;

    void prepare(double sampleRate, int maximumDelay);
    void reset();

    void setDelay(float delayInSamples);
    float popSample();
    void pushSample(float sample);

    float getDelay() const { return delay_; }
    int getMaximumDelay() const { return maxDelay_; }

private:
    std::vector<float> buffer_;
    int writeIndex_ = 0;
    float delay_ = 0.0f;
    int maxDelay_ = 0;
    float interpolate(float fractionalDelay);
};

/**
 * @brief Topology-Preserving TPT Filter (Zolzer style)
 */
class TPTFilter
{
public:
    enum class Type { lowpass, highpass, allpass, bandpass };

    TPTFilter();
    ~TPTFilter() = default;

    void prepare(double sampleRate);
    void reset();

    void setType(Type type);
    void setCutoffFrequency(float freq);
    float processSample(float input);

private:
    Type type_ = Type::lowpass;
    double sampleRate_ = 48000.0;
    float cutoff_ = 1000.0f;
    float z1_ = 0.0f;
    float g_ = 0.0f;
    float h_ = 0.0f;
};

/**
 * @brief Single mode resonator for body simulation with frequency-dependent Q
 *
 * Implements per-mode Q calculation based on real string physics:
 * - Higher frequencies damp faster (real string behavior)
 * - Material parameter (wood vs metal strings)
 * - Frequency-dependent decay time
 */
struct ModalFilter
{
    float frequency = 440.0f;
    float amplitude = 0.5f;
    float decay = 1.0f;
    float baseAmplitude = 0.5f;
    float phase = 0.0f;
    float energy = 0.0f;
    double sr = 48000.0;

    // Per-mode Q calculation parameters
    float materialFactor = 1.0f;  // 0.5 = soft wood, 1.0 = standard, 1.5 = bright metal
    float modeIndex = 0.0f;       // Which mode this is (for harmonic scaling)
    float computedQ = 50.0f;      // Calculated Q value for this mode

    void prepare(double sampleRate);
    float processSample(float excitation);
    void reset();

    /**
     * @brief Compute frequency-dependent Q (quality factor)
     * Based on Mutable Instruments' Rings resonator design
     *
     * Higher frequency modes have lower Q (damp faster)
     * Material factor affects overall brightness
     */
    float computeQ(float freq, float damping, float structure);
};

//==============================================================================
// Physical Modeling Components
//==============================================================================

/**
 * @brief Waveguide String (Karplus-Strong) with improved dispersion
 *
 * Enhancements:
 * - Dispersion allpass filters for realistic high-frequency propagation
 * - Sympathetic coupling between modes
 * - Bridge impedance modeling
 */
class WaveguideString
{
public:
    struct Parameters
    {
        float frequency = 440.0f;
        float damping = 0.996f;
        float stiffness = 0.0f;
        float brightness = 0.5f;
        float bridgeCoupling = 0.3f;
        float nonlinearity = 0.1f;
        float stringLengthMeters = 0.65f;
        int stringGauge = 1;
        float pickPosition = 0.15f;
        float dispersion = 0.5f;  // Dispersion amount (0-1)
        float sympatheticCoupling = 0.1f;  // Coupling to other strings
    };

    enum class StringGauge { Thin = 0, Normal = 1, Thick = 2, Massive = 3 };

    WaveguideString();
    ~WaveguideString() = default;

    void prepare(double sampleRate);
    void reset();

    void excite(const float* exciterSignal, int exciterLength, float velocity);
    float processSample();

    void setFrequency(float freq);
    void setDamping(float damping);
    void setStiffness(float stiffness);
    void setBrightness(float brightness);
    void setBridgeCoupling(float coupling);
    void setNonlinearity(float nonlinearity);
    void setStringLengthMeters(float length);
    void setStringGauge(StringGauge gauge);
    void setPickPosition(float position);
    void setDispersion(float dispersion);
    void setSympatheticCoupling(float coupling);

    float getBridgeEnergy() const { return lastBridgeEnergy_; }

private:
    Parameters params_;
    FractionalDelayLine fractionalDelay_;
    TPTFilter stiffnessFilter_;
    TPTFilter dampingFilter_;

    // Dispersion filters (3 cascaded allpass for realistic dispersion)
    TPTFilter dispersionFilter1_;
    TPTFilter dispersionFilter2_;
    TPTFilter dispersionFilter3_;

    // Sympathetic resonance state
    float sympatheticEnergy_ = 0.0f;

    double sr = 48000.0;
    float lastBridgeEnergy_ = 0.0f;
    int maxDelayInSamples = 0;

    // Bridge impedance modeling
    float bridgeImpedance_ = 1000.0f;  // Ohms
    void updateBridgeImpedance();
};

/**
 * @brief Bridge Coupling
 */
class BridgeCoupling
{
public:
    BridgeCoupling();
    ~BridgeCoupling() = default;

    void prepare(double sampleRate);
    void reset();

    float processString(float stringOutput);
    float getBridgeEnergy() const { return bridgeEnergy_; }

    void setCouplingCoefficient(float coeff) { couplingCoefficient_ = coeff; }
    void setNonlinearity(float nonlinearity) { nonlinearity_ = nonlinearity; }

private:
    float couplingCoefficient_ = 0.3f;
    float nonlinearity_ = 0.1f;
    float bridgeEnergy_ = 0.0f;
};

/**
 * @brief Modal Body Resonator with per-mode Q calculation
 *
 * Features:
 * - Frequency-dependent damping per mode
 * - Material parameter (wood vs metal)
 * - Realistic decay profiles
 */
class ModalBodyResonator
{
public:
    enum class MaterialType { SoftWood = 0, StandardWood = 1, HardWood = 2, Metal = 3 };

    ModalBodyResonator();
    ~ModalBodyResonator() = default;

    void prepare(double sampleRate);
    void reset();

    float processSample(float bridgeEnergy);
    void setResonance(float amount);
    void setMaterial(MaterialType material);
    void loadGuitarBodyPreset();
    void loadPianoBodyPreset();
    void loadOrchestralStringPreset();
    float getModeFrequency(int index) const;

    // Advanced: Re-calculate Q values for all modes based on material
    void recalculateModeQ(float damping, float structure);

private:
    std::vector<ModalFilter> modes_;
    double sr = 48000.0;
    MaterialType material_ = MaterialType::StandardWood;
};

//==============================================================================
// Articulation State Machine
//==============================================================================

enum class ArticulationState { IDLE, ATTACK_PLUCK, DECAY, SUSTAIN_BOW, RELEASE_GHOST, RELEASE_DAMP };

class ArticulationStateMachine
{
public:
    ArticulationStateMachine();
    ~ArticulationStateMachine() = default;

    void prepare(double sampleRate);
    void reset();

    void triggerPluck(float velocity);
    void triggerBow(float velocity, float bowPressure);
    void triggerScrape(float velocity);
    void triggerHarmonic(float velocity);
    void triggerDamp();

    void update(float deltaTime);

    float getPreviousGain() const;
    float getCurrentGain() const;
    float getCurrentExcitation();

    ArticulationState getCurrentState() const { return currentState_; }
    ArticulationState getPreviousState() const { return previousState_; }

private:
    void transitionTo(ArticulationState newState);

    ArticulationState currentState_ = ArticulationState::IDLE;
    ArticulationState previousState_ = ArticulationState::IDLE;
    double crossfadeProgress = 0.0;
    double crossfadeTime = 0.01;
    double stateTimer = 0.0;
    double sr = 48000.0;

    static constexpr int exciterBufferSize = 4800;
    float exciterBuffer[exciterBufferSize];
    int exciterIndex = 0;
    int exciterLength = 0;
    float exciterAmplitude = 0.0f;
    unsigned int seed_ = 12345;
    float randomFloat();
};

//==============================================================================
// v2: Giant Instrument Features
//==============================================================================

class SharedBridgeCoupling
{
public:
    SharedBridgeCoupling();
    ~SharedBridgeCoupling() = default;

    void prepare(double sampleRate, int numVoices);
    void reset();

    float addStringEnergy(float stringEnergy, int voiceIndex);
    float getBridgeMotion() const;

private:
    std::vector<float> bridgeEnergies_;
    float totalBridgeMotion_ = 0.0f;
    double sr = 48000.0;
};

class SympatheticStringBank
{
public:
    struct SympatheticStringConfig
    {
        bool enabled = false;
        int numStrings = 12;
        float detune = 0.05f;
    };

    SympatheticStringBank();
    ~SympatheticStringBank() = default;

    void prepare(double sampleRate, const SympatheticStringConfig& config);
    void reset();

    void exciteFromBridge(float bridgeEnergy);
    float processSample();

private:
    std::vector<WaveguideString> strings_;
    bool enabled_ = false;
    double sr = 48000.0;
};

//==============================================================================
// AetherVoice and AetherVoice Manager
//==============================================================================

struct AetherVoice
{
    WaveguideString string;
    BridgeCoupling bridge;
    ModalBodyResonator body;
    ArticulationStateMachine fsm;

    Pedalboard* pedalboard = nullptr;
    SharedBridgeCoupling* sharedBridge = nullptr;
    SympatheticStringBank* sympatheticStrings = nullptr;

    bool isActive = false;
    int currentNote = 0;
    float currentVelocity = 0.0f;
    float age = 0.0f;

    void prepare(double sampleRate);
    void noteOn(int note, float velocity);
    void noteOff();
    void processBlock(float* output, int numSamples, double sampleRate);
};

class AetherVoiceManager
{
public:
    AetherVoiceManager();
    ~AetherVoiceManager() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    AetherVoice* findFreeVoice();
    AetherVoice* findVoiceForNote(int note);

    void handleNoteOn(int note, float velocity);
    void handleNoteOff(int note);
    void allNotesOff();

    void processBlock(float* output, int numSamples, double sampleRate);
    int getActiveVoiceCount() const;

    void enableSharedBridge(bool enabled);
    void enableSympatheticStrings(const SympatheticStringBank::SympatheticStringConfig& config);

    // Apply parameters to all voices (called by loadPreset)
    void applyVoiceParameters(const AetherPureDSP& dsp);

private:
    std::array<AetherVoice, 6> voices_;
    std::unique_ptr<SharedBridgeCoupling> sharedBridge_;
    std::unique_ptr<SympatheticStringBank> sympatheticStrings_;
};

//==============================================================================
// Pedalboard Effects
//==============================================================================

enum class PedalType { Bypass, Compressor, Octaver, Overdrive, Distortion, RAT, Phaser, Reverb };
enum class DiodeType { Silicon, Germanium, LED };

class RATDistortion
{
public:
    RATDistortion();
    ~RATDistortion() = default;

    void prepare(double sampleRate);
    void reset();

    void setDiodeType(DiodeType type);
    float processSample(float input);

    float drive = 1.0f;
    float filter = 0.5f;
    float output = 1.0f;

private:
    DiodeType diodeType_ = DiodeType::Silicon;
    float threshold = 0.7f;
    float asymmetry = 1.0f;
    TPTFilter preFilter_;
    TPTFilter toneFilter_;
    double sr = 48000.0;
};

struct Pedal
{
    PedalType type = PedalType::Bypass;
    bool enabled = false;
    float param1 = 0.0f;
    float param2 = 0.0f;
    float mix = 1.0f;
    RATDistortion rat;

    void prepare(double sampleRate);
    float processSample(float input);
};

class Pedalboard
{
public:
    Pedalboard();
    ~Pedalboard() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    float processSample(float input);

    void setPedal(int index, PedalType type, bool enable);
    void setRouting(int index, int pedalIndex);
    void setParallelMode(bool parallel) { parallelMode_ = parallel; }

private:
    std::array<Pedal, 8> pedals_;
    std::array<int, 8> routingOrder_ = {0, 1, 2, 3, 4, 5, 6, 7};
    bool parallelMode_ = false;
};

//==============================================================================
// Main Aether DSP Instrument
//==============================================================================

class AetherPureDSP : public InstrumentDSP
{
public:
    AetherPureDSP();
    ~AetherPureDSP() override;

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

    const char* getInstrumentName() const override { return "NatureAether"; }
    const char* getInstrumentVersion() const override { return "2.0.0"; }

    void enableSharedBridge(bool enabled);
    void enableSympatheticStrings(bool enabled);
    void setPedal(int index, PedalType type, bool enable);

    // Expose parameters publicly for easier access by voice manager
    struct Parameters
    {
        double masterVolume = 3.0;  // Increased for normalization (was 1.5, too quiet at -20 dB)
        double pitchBendRange = 2.0;
        double baseFrequency = 440.0;
        double damping = 0.996;
        double stiffness = 0.0;
        double brightness = 0.5;
        double bridgeCoupling = 0.6;  // Boosted for normalization (was 0.3)
        double nonlinearity = 0.1;
        double stringLengthMeters = 0.65;
        int stringGauge = 1;
        double pickPosition = 0.15;
        double bodyResonance = 1.0;
        double attackVelocity = 0.8;
        double bowPressure = 0.5;
        double reverbMix = 0.0;
        double delayMix = 0.0;
        double drive = 0.0;

        // Advanced physical modeling parameters
        double dispersion = 0.5;  // Dispersion amount (0-1)
        double sympatheticCoupling = 0.1;  // Sympathetic resonance (0-1)
        double material = 1.0;  // Material factor (0.5=soft wood, 1.0=standard, 1.5=bright metal)
        int bodyPreset = 0;  // 0=guitar, 1=piano, 2=orchestral
    } params_;

private:
    AetherVoiceManager voiceManager_;
    Pedalboard pedalboard_;

    double sampleRate_ = 48000.0;
    int blockSize_ = 512;
    double pitchBend_ = 0.0;

    // Real-time safe temporary buffer for audio processing
    static constexpr int MAX_BLOCK_SIZE = 512;
    alignas(32) float tempBuffer_[MAX_BLOCK_SIZE];

    void applyParameters();
    void processStereoSample(float& left, float& right);
    inline float softClip(float x) const;
    bool writeJsonParameter(const char* name, double value, char* buffer, int& offset, int bufferSize) const;
    bool parseJsonParameter(const char* json, const char* param, double& value) const;

    // UPFS v1.0 preset loading support
    bool isUPFSFormat(const char* jsonData) const;
    bool loadUPFSPreset(const char* jsonData);

    friend class AetherDSPTest;
};

//==============================================================================
// Utility Function Declarations (defined in NexSynthDSP.h)
//==============================================================================
// Note: midiToFrequency, lerp, and clamp are defined in NexSynthDSP.h
// to avoid ODR violations across multiple instrument headers

} // namespace DSP
