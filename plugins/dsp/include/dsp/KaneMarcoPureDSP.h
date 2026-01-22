/*
  ==============================================================================

    NaturePureDSP.h
    Created: December 30, 2025
    Author: Bret Bouchard

    Pure DSP implementation of Nature Marco Hybrid Virtual Analog Synthesizer
    - Inherits from DSP::InstrumentDSP (no JUCE dependencies)
    - Headless operation (no GUI)
    - PolyBLEP anti-aliasing oscillators
    - WARP phase manipulation (-1.0 to +1.0)
    - FM synthesis with carrier/modulator swap
    - 16-slot modulation matrix with lock-free std::atomic
    - 8 macro controls (Serum-style)
    - SVF multimode filter
    - 16-voice polyphony with monophonic/legato modes
    - JSON preset save/load system
    - Factory-creatable for dynamic instantiation

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
#include <atomic>
#include <random>

// Forward declaration for UPFS namespace
namespace UPFS {
    struct Preset;
}

namespace DSP {

//==============================================================================
// Forward Declarations
//==============================================================================

class VoiceManager;
class ModulationMatrix;
class MacroSystem;
class NaturePureDSP;

//==============================================================================
// Oscillator with PolyBLEP Anti-Aliasing
//==============================================================================

enum class Waveform { SAW, SQUARE, TRIANGLE, SINE, PULSE };

class Oscillator
{
public:
    Oscillator();
    ~Oscillator() = default;

    void prepare(double sampleRate);
    void reset();

    void setFrequency(float freqHz, double sampleRate);
    void setWarp(float warpAmount);
    void setWaveform(int waveformIndex);
    void setPulseWidth(float pw);
    void setFMDepth(float depth);
    void setIsFMCarrier(bool isCarrier);

    float processSample();
    float processSampleWithFM(float modulationInput);

    double phase = 0.0;
    double phaseIncrement = 0.0;
    float warp = 0.0f;
    float pulseWidth = 0.5f;
    Waveform waveform = Waveform::SAW;
    bool isFMCcarrier = false;
    float fmDepth = 0.0f;

private:
    float generateWaveform(double p) const;
    float polyBlep(double t, double dt) const;
    float polyBlepSaw(double p) const;
    float polyBlepSquare(double p) const;
    float polyBlepTriangle(double p) const;
    float polyBlepPulse(double p, double pw) const;
};

//==============================================================================
// Sub-Oscillator (-1 Octave Square Wave)
//==============================================================================

class SubOscillator
{
public:
    SubOscillator();
    ~SubOscillator() = default;

    void prepare(double sampleRate);
    void reset();

    void setFrequency(float baseFreq, double sampleRate);
    void setEnabled(bool e) { enabled = e; }
    void setLevel(float l) { level = l; }

    float processSample();

    double phase = 0.0;
    bool enabled = true;
    float level = 0.5f;

private:
    double phaseIncrement = 0.0f;
};

//==============================================================================
// Noise Generator
//==============================================================================

class NoiseGenerator
{
public:
    NoiseGenerator();
    ~NoiseGenerator() = default;

    void prepare(double sampleRate);
    void reset();

    float nextFloat();
    void setLevel(float level) { level_ = level; }

private:
    float level_ = 0.0f;
    std::mt19937 generator_;
    std::uniform_real_distribution<float> distribution_;
};

//==============================================================================
// SVF Multimode Filter (State Variable Filter)
//==============================================================================

enum class FilterType { LOWPASS, HIGHPASS, BANDPASS, NOTCH };

class SVFFilter
{
public:
    SVFFilter();
    ~SVFFilter() = default;

    void prepare(double sampleRate);
    void reset();

    void setType(FilterType type);
    void setCutoff(float freqHz);
    void setResonance(float res);

    float processSample(float input);

    FilterType type = FilterType::LOWPASS;
    float cutoff = 1000.0f;
    float resonance = 0.5f;

private:
    double sampleRate_ = 48000.0;
    float v0 = 0.0f;  // Input
    float v1 = 0.0f;  // Lowpass
    float v2 = 0.0f;  // Bandpass
    float v3 = 0.0f;  // Highpass
};

//==============================================================================
// ADSR Envelope
//==============================================================================

class Envelope
{
public:
    struct Parameters
    {
        float attack = 0.005f;
        float decay = 0.1f;
        float sustain = 0.6f;
        float release = 0.2f;
    };

    Envelope();
    ~Envelope() = default;

    void prepare(double sampleRate);
    void reset();

    void setParameters(const Parameters& params);
    void noteOn();
    void noteOff();

    float processSample();
    bool isActive() const;

    Parameters params;
    float amount = 1.0f;  // Envelope depth

private:
    enum class State { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    State state = State::IDLE;
    float currentLevel = 0.0f;
    double sampleRate_ = 48000.0;
};

//==============================================================================
// LFO
//==============================================================================

enum class LFOWaveform { SINE, TRIANGLE, SAWTOOTH, SQUARE, SAMPLE_AND_HOLD };

class LFO
{
public:
    LFO();
    ~LFO() = default;

    void prepare(double sampleRate);
    void reset();

    void setRate(float rateHz, double sampleRate);
    void setDepth(float depth);
    void setWaveform(LFOWaveform waveform);
    void setBipolar(bool bipolar);

    float processSample();

    float rate = 5.0f;
    float depth = 0.5f;
    LFOWaveform waveform = LFOWaveform::SINE;
    bool bipolar = true;

    float output = 0.0f;  // Current output (for modulation matrix)

private:
    float generateWaveform();

    double phase = 0.0;
    double phaseIncrement = 0.0;
    float lastSandHValue = 0.0f;
    double sampleRate_ = 48000.0;
    std::mt19937 generator_;
    std::uniform_real_distribution<float> distribution_;
};

//==============================================================================
// Modulation Matrix (16-slot with std::atomic)
//==============================================================================

enum class ModSource {
    LFO1, LFO2, VELOCITY, AFTERTOUCH, PITCH_WHEEL, MOD_WHEEL,
    FILTER_ENV, AMP_ENV, MACRO_1, MACRO_2, MACRO_3, MACRO_4,
    MACRO_5, MACRO_6, MACRO_7, MACRO_8
};

enum class ModDestination {
    OSC1_FREQ, OSC1_WARP, OSC1_PULSE_WIDTH, OSC1_LEVEL,
    OSC2_FREQ, OSC2_WARP, OSC2_PULSE_WIDTH, OSC2_LEVEL,
    SUB_LEVEL, NOISE_LEVEL,
    FILTER_CUTOFF, FILTER_RESONANCE,
    FILTER_ENV_AMOUNT, AMP_ENV_ATTACK, AMP_ENV_DECAY, AMP_ENV_SUSTAIN, AMP_ENV_RELEASE,
    LFO1_RATE, LFO1_DEPTH, LFO2_RATE, LFO2_DEPTH
};

struct ModulationSlot
{
    ModSource source = ModSource::LFO1;
    ModDestination destination = ModDestination::OSC1_FREQ;
    std::atomic<float> amount{0.0f};
    bool bipolar = true;
    int curveType = 0;  // 0=Linear, 1=Exponential
    float maxValue = 1.0f;
};

class ModulationMatrix
{
public:
    ModulationMatrix();
    ~ModulationMatrix() = default;

    void prepare(double sampleRate);
    void reset();

    void setSlot(int index, const ModulationSlot& slot);
    const ModulationSlot& getSlot(int index) const;

    float getModulationValue(int slotIndex) const;
    float getCurrentModSourceValue(ModSource source) const;

    void processModulationSources();

    std::array<std::atomic<float>, 16> modulationAmounts;
    float sourceValues[16];  // Updated each sample
    std::array<ModulationSlot, 16> slots;

    LFO lfo1;
    LFO lfo2;

private:
    float applyCurve(float value, int curveType) const;
};

//==============================================================================
// Macro System (8 macros, Serum-style)
//==============================================================================

struct MacroDestination
{
    std::string paramID;
    float amount = 0.0f;
    float minValue = 0.0f;
    float maxValue = 1.0f;
};

struct MacroControl
{
    float value = 0.5f;
    std::string name = "Macro";
    MacroDestination destinations[4];
    int numDestinations = 0;
};

class MacroSystem
{
public:
    MacroSystem();
    ~MacroSystem() = default;

    void setMacroValue(int macroIndex, float value);
    float getMacroValue(int macroIndex) const;

    void setMacroName(int macroIndex, const std::string& name);
    std::string getMacroName(int macroIndex) const;

    void addDestination(int macroIndex, const std::string& paramID,
                       float amount, float minVal, float maxVal);

    float applyMacroModulation(const std::string& paramID, float baseValue) const;

    std::array<MacroControl, 8> macros;
};

//==============================================================================
// Voice Structure
//==============================================================================

struct Voice
{
    Oscillator osc1;
    Oscillator osc2;
    SubOscillator subOsc;
    NoiseGenerator noiseGen;

    SVFFilter filter;
    Envelope filterEnv;
    Envelope ampEnv;

    ModulationMatrix* modMatrix = nullptr;
    MacroSystem* macros = nullptr;

    // Voice parameters
    bool active = false;
    int midiNote = 0;
    float velocity = 0.0f;
    double startTime = 0.0;

    // Oscillator levels
    float osc1Level = 0.7f;
    float osc2Level = 0.5f;
    float subLevel = 0.3f;
    float noiseLevel = 0.0f;

    // Filter parameters
    float filterEnvelopeAmount = 0.0f;

    // FM synthesis
    bool fmEnabled = false;
    int fmCarrierIndex = 0;  // 0=OSC1, 1=OSC2
    float fmDepth = 0.0f;

    // Pan
    float pan = 0.0f;

    void prepare(double sampleRate);
    void reset();

    void noteOn(int note, float vel, double currentSampleRate);
    void noteOff(float vel);

    bool isActive() const;
    float renderSample();
};

//==============================================================================
// Voice Manager
//==============================================================================

enum class PolyphonyMode { POLY, MONO, LEGATO };

class VoiceManager
{
public:
    VoiceManager();
    ~VoiceManager() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    Voice* findFreeVoice();
    Voice* findVoiceForNote(int note);

    void handleNoteOn(int note, float velocity);
    void handleNoteOff(int note);
    void allNotesOff();

    void processBlock(float* output, int numSamples, double sampleRate);
    int getActiveVoiceCount() const;

    void setPolyphonyMode(PolyphonyMode mode) { polyMode_ = mode; }
    PolyphonyMode getPolyphonyMode() const { return polyMode_; }

    void enableGlide(bool enable) { glideEnabled_ = enable; }
    void setGlideTime(float time) { glideTime_ = time; }

    // Update all voices with current parameters
    void updateVoiceParameters(const NaturePureDSP& synth);

private:
    std::array<Voice, 16> voices_;
    PolyphonyMode polyMode_ = PolyphonyMode::POLY;
    int monoVoiceIndex_ = -1;
    bool glideEnabled_ = false;
    float glideTime_ = 0.1f;
    double currentSampleRate_ = 48000.0;
};

//==============================================================================
// Main Nature Marco DSP Instrument
//==============================================================================

class NaturePureDSP : public InstrumentDSP
{
public:
    NaturePureDSP();
    ~NaturePureDSP() override;

    // Allow VoiceManager to access private members
    friend class VoiceManager;

    bool prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void process(float** outputs, int numChannels, int numSamples) override;
    void handleEvent(const ScheduledEvent& event) override;

    float getParameter(const char* paramId) const override;
    void setParameter(const char* paramId, float value) override;

    bool savePreset(char* jsonBuffer, int jsonBufferSize) const override;
    bool loadPreset(const char* jsonData) override;

    int getActiveVoiceCount() const override;
    int getMaxPolyphony() const override { return 16; }

    const char* getInstrumentName() const override { return "Nature"; }
    const char* getInstrumentVersion() const override { return "1.0.0"; }

private:
    VoiceManager voiceManager_;
    ModulationMatrix modMatrix_;
    MacroSystem macros_;

    struct Parameters
    {
        // OSC1
        float osc1Shape = 0.0f;
        float osc1Warp = 0.0f;
        float osc1PulseWidth = 0.5f;
        float osc1Detune = 0.0f;
        float osc1Pan = 0.0f;
        float osc1Level = 0.7f;

        // OSC2
        float osc2Shape = 0.0f;
        float osc2Warp = 0.0f;
        float osc2PulseWidth = 0.5f;
        float osc2Detune = 0.0f;
        float osc2Pan = 0.0f;
        float osc2Level = 0.5f;

        // Sub
        float subEnabled = 1.0f;
        float subLevel = 0.3f;

        // Noise
        float noiseLevel = 0.0f;

        // FM
        float fmEnabled = 0.0f;
        float fmCarrierOsc = 0.0f;
        float fmMode = 0.0f;
        float fmDepth = 0.0f;
        float fmModulatorRatio = 1.0f;

        // Filter
        float filterType = 0.0f;
        float filterCutoff = 0.5f;
        float filterResonance = 0.5f;
        float filterKeyTrack = 0.0f;
        float filterVelTrack = 0.0f;

        // Filter Envelope
        float filterEnvAttack = 0.01f;
        float filterEnvDecay = 0.1f;
        float filterEnvSustain = 0.5f;
        float filterEnvRelease = 0.2f;
        float filterEnvAmount = 0.0f;

        // Amp Envelope
        float ampEnvAttack = 0.005f;
        float ampEnvDecay = 0.1f;
        float ampEnvSustain = 0.6f;
        float ampEnvRelease = 0.2f;

        // LFO1
        float lfo1Waveform = 0.0f;
        float lfo1Rate = 5.0f;
        float lfo1Depth = 0.5f;
        float lfo1Bipolar = 1.0f;

        // LFO2
        float lfo2Waveform = 0.0f;
        float lfo2Rate = 3.0f;
        float lfo2Depth = 0.5f;
        float lfo2Bipolar = 1.0f;

        // Modulation Matrix (16 slots)
        float modSource[16] = {0};
        float modDestination[16] = {0};
        float modAmount[16] = {0};
        float modBipolar[16] = {1};
        float modCurve[16] = {0};

        // Macros (8 macros)
        float macroValue[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

        // Structure (Mutable Instruments-style harmonic complexity)
        // 0.0 = simple, pure VA (clean oscillators, minimal FM/modulation)
        // 0.5 = balanced (default)
        // 1.0 = complex, rich (heavy warp, FM depth, modulation matrix activity)
        float structure = 0.5f;

        // Global
        float polyMode = 0.0f;  // 0=Poly, 1=Mono, 2=Legato
        float glideEnabled = 0.0f;
        float glideTime = 0.1f;
        float masterTune = 0.0f;
        float masterVolume = 3.0f;  // Reduced to prevent clipping (was 4.2)
        double pitchBendRange = 2.0;
    } params_;

    double sampleRate_ = 48000.0;
    int blockSize_ = 512;
    double pitchBend_ = 0.0;

    void applyParameters();
    void processStereoSample(float& left, float& right);

    float calculateFrequency(int midiNote, float bend = 0.0f) const;

    // UPFS v1.0 preset loading
    bool loadUPFSPreset(const UPFS::Preset& preset);
    bool loadLegacyPreset(const char* jsonData);

    bool writeJsonParameter(const char* name, double value, char* buffer, int& offset, int bufferSize) const;
    bool parseJsonParameter(const char* json, const char* param, double& value) const;
};

//==============================================================================
// Utility Function Declarations (defined in NexSynthDSP.h)
//==============================================================================
// Note: midiToFrequency, lerp, and clamp are defined in NexSynthDSP.h
// to avoid ODR violations across multiple instrument headers

} // namespace DSP
