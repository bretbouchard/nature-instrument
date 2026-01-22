/*
  ==============================================================================

    NaturePureDSP.cpp
    Created: December 30, 2025
    Author: Bret Bouchard

    Pure DSP implementation of Nature Marco Hybrid Virtual Analog Synthesizer
    - No JUCE dependencies
    - Factory-creatable

  ==============================================================================
*/

#include "dsp/NaturePureDSP.h"
#include "../../../../include/dsp/LookupTables.h"
#include "../../../../include/dsp/DSPLogging.h"
#include "../../../../../libraries/upfs/PresetParser.h"
#include <cstring>
#include <cstdio>
#include <cmath>

namespace DSP {

//==============================================================================
// Utility Functions (defined locally to avoid ODR violations)
//==============================================================================

static inline double midiToFrequency(int midiNote, double pitchBendSemitones)
{
    // Use LookupTables for MIDI to frequency conversion
    using namespace SchillingerEcosystem::DSP;
    float freq = LookupTables::getInstance().midiToFreq(static_cast<float>(midiNote));
    if (pitchBendSemitones != 0.0)
    {
        freq = LookupTables::getInstance().midiToFreqWithBend(
            static_cast<float>(midiNote),
            static_cast<float>(pitchBendSemitones)
        );
    }
    return static_cast<double>(freq);
}

static inline double lerp(double a, double b, double t)
{
    return a + t * (b - a);
}

static inline double clamp(double x, double min, double max)
{
    return (x < min) ? min : (x > max) ? max : x;
}

//==============================================================================
// OSCILLATOR IMPLEMENTATION
//==============================================================================

Oscillator::Oscillator()
{
    reset();
}

void Oscillator::prepare(double sampleRate)
{
    reset();
}

void Oscillator::reset()
{
    phase = 0.0;
    phaseIncrement = 0.0;
    warp = 0.0f;
    pulseWidth = 0.5f;
    waveform = Waveform::SAW;
    isFMCcarrier = false;
    fmDepth = 0.0f;
}

void Oscillator::setFrequency(float freqHz, double sampleRate)
{
    phaseIncrement = freqHz / sampleRate;
}

void Oscillator::setWarp(float warpAmount)
{
    warp = std::max(-1.0f, std::min(1.0f, warpAmount));
}

void Oscillator::setWaveform(int waveformIndex)
{
    waveform = static_cast<Waveform>(std::max(0, std::min(4, waveformIndex)));
}

void Oscillator::setPulseWidth(float pw)
{
    pulseWidth = std::max(0.0f, std::min(1.0f, pw));
}

void Oscillator::setFMDepth(float depth)
{
    fmDepth = depth;
}

void Oscillator::setIsFMCarrier(bool isCarrier)
{
    isFMCcarrier = isCarrier;
}

float Oscillator::processSample()
{
    // Apply phase warp: phase_warped = phase + (warp * sin(2π * phase))
    // Use LookupTables for sine calculation
    double warpedPhase = phase + (warp * SchillingerEcosystem::DSP::fastSineLookup(static_cast<float>(phase * 2.0 * M_PI)));

    // Generate waveform from warped phase
    float output = generateWaveform(warpedPhase);

    // Advance phase
    phase += phaseIncrement;
    if (phase >= 1.0)
        phase -= 1.0;

    return output;
}

float Oscillator::processSampleWithFM(float modulationInput)
{
    // Phase modulation from FM input
    double modulatedPhase = phase + (fmDepth * modulationInput);

    // Apply warp using LookupTables for sine calculation
    double warpedPhase = modulatedPhase + (warp * SchillingerEcosystem::DSP::fastSineLookup(static_cast<float>(modulatedPhase * 2.0 * M_PI)));

    // Generate waveform
    float output = generateWaveform(warpedPhase);

    // Advance phase
    phase += phaseIncrement;
    if (phase >= 1.0)
        phase -= 1.0;

    return output;
}

float Oscillator::generateWaveform(double p) const
{
    p = std::fmod(p, 1.0);
    if (p < 0.0) p += 1.0;

    switch (waveform)
    {
        case Waveform::SAW:
            return polyBlepSaw(p);
        case Waveform::SQUARE:
            return polyBlepSquare(p);
        case Waveform::TRIANGLE:
            return polyBlepTriangle(p);
        case Waveform::SINE:
            // Use LookupTables for sine calculation
            return SchillingerEcosystem::DSP::fastSineLookup(static_cast<float>(p * 2.0 * M_PI));
        case Waveform::PULSE:
            return polyBlepPulse(p, pulseWidth);
        default:
            return 0.0f;
    }
}

// PolyBLEP anti-aliasing correction
float Oscillator::polyBlep(double t, double dt) const
{
    if (t < dt)
    {
        t /= dt;
        return static_cast<float>(t + t - t * t - 1.0);
    }
    else if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return static_cast<float>(t + t + t * t + 1.0);
    }
    return 0.0f;
}

float Oscillator::polyBlepSaw(double p) const
{
    double dt = phaseIncrement;
    float naive = static_cast<float>(2.0 * p - 1.0);
    return naive - polyBlep(p, dt);
}

float Oscillator::polyBlepSquare(double p) const
{
    double dt = phaseIncrement;
    float naive = (p < 0.5) ? 1.0f : -1.0f;
    return naive + polyBlep(p, dt) - polyBlep(std::fmod(p + 0.5, 1.0), dt);
}

float Oscillator::polyBlepTriangle(double p) const
{
    double dt = phaseIncrement * 2.0;
    float naive = static_cast<float>(2.0 * std::abs(2.0 * p - 1.0) - 1.0);
    return naive;  // Simplified triangle PolyBLEP
}

float Oscillator::polyBlepPulse(double p, double pw) const
{
    double dt = phaseIncrement;
    float naive = (p < pw) ? 1.0f : -1.0f;

    float blep1 = polyBlep(p, dt);
    float blep2 = polyBlep(std::fmod(p + (1.0 - pw), 1.0), dt);

    return naive + blep1 - blep2;
}

//==============================================================================
// SUB-OSCILLATOR IMPLEMENTATION
//==============================================================================

SubOscillator::SubOscillator()
    : phaseIncrement(0.0)
{
    reset();
}

void SubOscillator::prepare(double sampleRate)
{
    reset();
}

void SubOscillator::reset()
{
    phase = 0.0;
    // Don't reset enabled or level - they are controlled by synth parameters
    // enabled = true;
    // level = 0.5f;
    phaseIncrement = 0.0f;
}

void SubOscillator::setFrequency(float baseFreq, double sampleRate)
{
    // Sub-oscillator is always -1 octave
    phaseIncrement = (baseFreq * 0.5) / sampleRate;
}

float SubOscillator::processSample()
{
    if (!enabled)
        return 0.0f;

    // Square wave at -1 octave
    float output = (phase < 0.5) ? 1.0f : -1.0f;

    // Advance phase
    phase += phaseIncrement;
    if (phase >= 1.0)
        phase -= 1.0;

    return output * level;
}

//==============================================================================
// NOISE GENERATOR IMPLEMENTATION
//==============================================================================

NoiseGenerator::NoiseGenerator()
    : level_(0.0f)
    , generator_(std::random_device{}())
    , distribution_(0.0f, 1.0f)
{
}

void NoiseGenerator::prepare(double sampleRate)
{
    reset();
}

void NoiseGenerator::reset()
{
    level_ = 0.0f;
}

float NoiseGenerator::nextFloat()
{
    return distribution_(generator_) * 2.0f - 1.0f;
}

//==============================================================================
// SVF FILTER IMPLEMENTATION
//==============================================================================

SVFFilter::SVFFilter()
    : v0(0.0f), v1(0.0f), v2(0.0f), v3(0.0f)
{
}

void SVFFilter::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    reset();
}

void SVFFilter::reset()
{
    v0 = 0.0f;
    v1 = 0.0f;
    v2 = 0.0f;
    v3 = 0.0f;
    type = FilterType::LOWPASS;
    cutoff = 1000.0f;
    resonance = 0.5f;
}

void SVFFilter::setType(FilterType t)
{
    type = t;
}

void SVFFilter::setCutoff(float freqHz)
{
    cutoff = std::max(20.0f, std::min(20000.0f, freqHz));
}

void SVFFilter::setResonance(float res)
{
    resonance = std::max(0.0f, std::min(1.0f, res));
}

float SVFFilter::processSample(float input)
{
    // State Variable Filter (Zölzer style)
    // Based on "Designing Audio Effect Plugins in C++" by Will Pirkle

    float fc = cutoff / static_cast<float>(sampleRate_);
    if (fc > 0.5f) fc = 0.5f;

    float fs = fc;  // Normalized frequency

    // Damping factor (resonance)
    float q = 1.0f - resonance;
    if (q < 0.001f) q = 0.001f;

    v0 = input;

    // Integrator 1
    float v1_new = v1 + fs * v3;
    v3 = fs * (v0 - v1 - q * v1);

    // Integrator 2
    float v2_new = v2 + fs * v1;
    v1 = v1_new;
    v2 = v2_new;

    // Outputs
    float lowpass = v2;
    float bandpass = v1;
    float highpass = v0 - q * v1 - v2;
    float notch = v0 - q * v1;

    // Select output based on type
    switch (type)
    {
        case FilterType::LOWPASS:
            return lowpass;
        case FilterType::HIGHPASS:
            return highpass;
        case FilterType::BANDPASS:
            return bandpass;
        case FilterType::NOTCH:
            return notch;
        default:
            return lowpass;
    }
}

//==============================================================================
// ENVELOPE IMPLEMENTATION
//==============================================================================

Envelope::Envelope()
    : state(State::IDLE)
    , currentLevel(0.0f)
{
}

void Envelope::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    reset();
}

void Envelope::reset()
{
    state = State::IDLE;
    currentLevel = 0.0f;
}

void Envelope::setParameters(const Parameters& p)
{
    params.attack = p.attack;
    params.decay = p.decay;
    params.sustain = p.sustain;
    params.release = p.release;
}

void Envelope::noteOn()
{
    state = State::ATTACK;
}

void Envelope::noteOff()
{
    if (state != State::IDLE)
        state = State::RELEASE;
}

float Envelope::processSample()
{
    float increment = 1.0f / static_cast<float>(sampleRate_);

    switch (state)
    {
        case State::ATTACK:
            currentLevel += increment / params.attack;
            if (currentLevel >= 1.0f)
            {
                currentLevel = 1.0f;
                state = State::DECAY;
            }
            break;

        case State::DECAY:
            currentLevel -= increment / params.decay;
            if (currentLevel <= params.sustain)
            {
                currentLevel = params.sustain;
                state = State::SUSTAIN;
            }
            break;

        case State::SUSTAIN:
            currentLevel = params.sustain;
            break;

        case State::RELEASE:
            currentLevel -= increment / params.release;
            if (currentLevel <= 0.0f)
            {
                currentLevel = 0.0f;
                state = State::IDLE;
            }
            break;

        case State::IDLE:
            currentLevel = 0.0f;
            break;
    }

    return currentLevel;
}

bool Envelope::isActive() const
{
    return state != State::IDLE;
}

//==============================================================================
// LFO IMPLEMENTATION
//==============================================================================

LFO::LFO()
    : phase(0.0)
    , phaseIncrement(0.0)
    , lastSandHValue(0.0f)
    , generator_(std::random_device{}())
    , distribution_(0.0f, 1.0f)
{
}

void LFO::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    setRate(rate, sampleRate);
    reset();
}

void LFO::reset()
{
    phase = 0.0;
    phaseIncrement = 0.0;
    output = 0.0f;
    lastSandHValue = 0.0f;
}

void LFO::setRate(float rateHz, double sampleRate)
{
    rate = rateHz;
    phaseIncrement = rateHz / sampleRate;
}

void LFO::setDepth(float depth)
{
    this->depth = depth;
}

void LFO::setWaveform(LFOWaveform waveform)
{
    this->waveform = waveform;
}

void LFO::setBipolar(bool bipolar)
{
    this->bipolar = bipolar;
}

float LFO::processSample()
{
    output = generateWaveform();

    // Advance phase
    phase += phaseIncrement;
    if (phase >= 1.0)
        phase -= 1.0;

    // Apply depth and bipolar/unipolar
    float scaledOutput = output * depth;
    if (!bipolar)
        scaledOutput = (scaledOutput + 1.0f) * 0.5f;  // Convert -1..1 to 0..1

    return scaledOutput;
}

float LFO::generateWaveform()
{
    double p = phase;

    switch (waveform)
    {
        case LFOWaveform::SINE:
            return static_cast<float>(std::sin(p * 2.0 * M_PI));

        case LFOWaveform::TRIANGLE:
            return static_cast<float>(2.0 * std::abs(2.0 * p - 1.0) - 1.0);

        case LFOWaveform::SAWTOOTH:
            return static_cast<float>(2.0 * p - 1.0);

        case LFOWaveform::SQUARE:
            return (p < 0.5) ? 1.0f : -1.0f;

        case LFOWaveform::SAMPLE_AND_HOLD:
            if (phase < phaseIncrement)  // New sample
            {
                lastSandHValue = distribution_(generator_) * 2.0f - 1.0f;
            }
            return lastSandHValue;
    }

    return 0.0f;
}

//==============================================================================
// MODULATION MATRIX IMPLEMENTATION
//==============================================================================

ModulationMatrix::ModulationMatrix()
{
    // Initialize all modulation amounts to zero
    for (auto& amount : modulationAmounts)
        amount.store(0.0f);

    // Initialize source values to zero
    for (float& val : sourceValues)
        val = 0.0f;
}

void ModulationMatrix::prepare(double sampleRate)
{
    lfo1.prepare(sampleRate);
    lfo2.prepare(sampleRate);
}

void ModulationMatrix::reset()
{
    lfo1.reset();
    lfo2.reset();

    for (auto& amount : modulationAmounts)
        amount.store(0.0f);

    for (float& val : sourceValues)
        val = 0.0f;
}

void ModulationMatrix::setSlot(int index, const ModulationSlot& slot)
{
    if (index >= 0 && index < 16)
    {
        slots[index].source = slot.source;
        slots[index].destination = slot.destination;
        slots[index].amount.store(slot.amount.load());
        slots[index].bipolar = slot.bipolar;
        slots[index].curveType = slot.curveType;
        slots[index].maxValue = slot.maxValue;
    }
}

const ModulationSlot& ModulationMatrix::getSlot(int index) const
{
    if (index >= 0 && index < 16)
        return slots[index];

    static ModulationSlot dummy;  // Return dummy on error
    return dummy;
}

float ModulationMatrix::getModulationValue(int slotIndex) const
{
    if (slotIndex >= 0 && slotIndex < 16)
        return modulationAmounts[slotIndex].load();

    return 0.0f;
}

float ModulationMatrix::getCurrentModSourceValue(ModSource source) const
{
    switch (source)
    {
        case ModSource::LFO1: return lfo1.output;
        case ModSource::LFO2: return lfo2.output;
        case ModSource::VELOCITY: return sourceValues[2];
        case ModSource::AFTERTOUCH: return sourceValues[3];
        case ModSource::PITCH_WHEEL: return sourceValues[4];
        case ModSource::MOD_WHEEL: return sourceValues[5];
        case ModSource::FILTER_ENV: return sourceValues[6];
        case ModSource::AMP_ENV: return sourceValues[7];
        case ModSource::MACRO_1: return sourceValues[8];
        case ModSource::MACRO_2: return sourceValues[9];
        case ModSource::MACRO_3: return sourceValues[10];
        case ModSource::MACRO_4: return sourceValues[11];
        case ModSource::MACRO_5: return sourceValues[12];
        case ModSource::MACRO_6: return sourceValues[13];
        case ModSource::MACRO_7: return sourceValues[14];
        case ModSource::MACRO_8: return sourceValues[15];
        default: return 0.0f;
    }
}

float ModulationMatrix::applyCurve(float value, int curveType) const
{
    if (curveType == 1)  // Exponential
    {
        float sign = (value >= 0.0f) ? 1.0f : -1.0f;
        return sign * std::pow(std::abs(value), 2.0f);
    }

    return value;  // Linear (default)
}

void ModulationMatrix::processModulationSources()
{
    lfo1.processSample();
    lfo2.processSample();
}

//==============================================================================
// MACRO SYSTEM IMPLEMENTATION
//==============================================================================

MacroSystem::MacroSystem()
{
    // Initialize macros with default values
    for (int i = 0; i < 8; ++i)
    {
        macros[i].value = 0.5f;
        macros[i].name = "Macro " + std::to_string(i + 1);
        macros[i].numDestinations = 0;
    }
}

void MacroSystem::setMacroValue(int macroIndex, float value)
{
    if (macroIndex >= 0 && macroIndex < 8)
        macros[macroIndex].value = std::max(0.0f, std::min(1.0f, value));
}

float MacroSystem::getMacroValue(int macroIndex) const
{
    if (macroIndex >= 0 && macroIndex < 8)
        return macros[macroIndex].value;

    return 0.0f;
}

void MacroSystem::setMacroName(int macroIndex, const std::string& name)
{
    if (macroIndex >= 0 && macroIndex < 8)
        macros[macroIndex].name = name;
}

std::string MacroSystem::getMacroName(int macroIndex) const
{
    if (macroIndex >= 0 && macroIndex < 8)
        return macros[macroIndex].name;

    return "Macro";
}

void MacroSystem::addDestination(int macroIndex, const std::string& paramID,
                                 float amount, float minVal, float maxVal)
{
    if (macroIndex >= 0 && macroIndex < 8)
    {
        auto& macro = macros[macroIndex];
        if (macro.numDestinations < 4)
        {
            macro.destinations[macro.numDestinations] = { paramID, amount, minVal, maxVal };
            macro.numDestinations++;
        }
    }
}

float MacroSystem::applyMacroModulation(const std::string& paramID, float baseValue) const
{
    float totalModulation = 0.0f;

    for (const auto& macro : macros)
    {
        for (int i = 0; i < macro.numDestinations; ++i)
        {
            if (macro.destinations[i].paramID == paramID)
            {
                // Map macro value to parameter range
                float modValue = macro.value;
                float range = macro.destinations[i].maxValue - macro.destinations[i].minValue;
                float offset = macro.destinations[i].amount * modValue * range;
                totalModulation += offset;
            }
        }
    }

    return baseValue + totalModulation;
}

//==============================================================================
// VOICE IMPLEMENTATION
//==============================================================================

void Voice::prepare(double sampleRate)
{
    osc1.prepare(sampleRate);
    osc2.prepare(sampleRate);
    subOsc.prepare(sampleRate);
    noiseGen.prepare(sampleRate);

    filter.prepare(sampleRate);

    filterEnv.prepare(sampleRate);
    ampEnv.prepare(sampleRate);
}

void Voice::reset()
{
    osc1.reset();
    osc2.reset();
    subOsc.reset();
    filter.reset();
    filterEnv.reset();
    ampEnv.reset();
    active = false;
}

void Voice::noteOn(int note, float vel, double currentSampleRate)
{
    midiNote = note;
    velocity = vel;
    active = true;

    // Calculate base frequency
    float freq = static_cast<float>(midiToFrequency(note, 0.0));

    // Set oscillator frequencies
    osc1.setFrequency(freq, currentSampleRate);
    osc2.setFrequency(freq, currentSampleRate);
    subOsc.setFrequency(freq, currentSampleRate);

    // Start envelopes
    filterEnv.noteOn();
    ampEnv.noteOn();
}

void Voice::noteOff(float vel)
{
    filterEnv.noteOff();
    ampEnv.noteOff();
}

bool Voice::isActive() const
{
    return active || ampEnv.isActive() || filterEnv.isActive();
}

float Voice::renderSample()
{
    // FIX: Check if voice should be silent
    // Voice is active if note is on OR envelopes are still processing
    if (!active && !ampEnv.isActive() && !filterEnv.isActive())
        return 0.0f;

    // FM synthesis
    float fmModulation = 0.0f;
    if (fmEnabled)
    {
        if (fmCarrierIndex == 0)  // OSC1 is carrier
        {
            // OSC2 modulates OSC1
            float modulatorOutput = osc2.processSample() * fmDepth;
            fmModulation = modulatorOutput;
        }
        else  // OSC2 is carrier
        {
            // OSC1 modulates OSC2
            float modulatorOutput = osc1.processSample() * fmDepth;
            fmModulation = modulatorOutput;
        }
    }

    // Generate oscillator outputs
    float osc1Out = (fmEnabled && fmCarrierIndex == 0) ?
        osc1.processSampleWithFM(fmModulation) : osc1.processSample();

    float osc2Out = (fmEnabled && fmCarrierIndex == 1) ?
        osc2.processSampleWithFM(fmModulation) : osc2.processSample();

    // Mix oscillators
    float mix = (osc1Out * osc1Level) + (osc2Out * osc2Level);

    // Add sub-oscillator if enabled
    if (subOsc.enabled)
    {
        float subOut = subOsc.processSample();
        mix += subOut * subLevel;
    }

    // Add noise
    if (noiseLevel > 0.0f)
    {
        mix += noiseGen.nextFloat() * noiseLevel;
    }

    // Process through filter
    float filtered = filter.processSample(mix);

    // Apply filter envelope modulation
    float filterEnvValue = filterEnv.processSample();
    float filterMod = filterEnvValue * filterEnvelopeAmount;

    // Apply amp envelope
    float ampEnvValue = ampEnv.processSample();
    filtered *= ampEnvValue;

    return filtered;
}

//==============================================================================
// VOICE MANAGER IMPLEMENTATION
//==============================================================================

VoiceManager::VoiceManager()
    : monoVoiceIndex_(-1)
    , glideEnabled_(false)
    , glideTime_(0.1f)
    , currentSampleRate_(48000.0)
{
}

void VoiceManager::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate_ = sampleRate;

    for (auto& voice : voices_)
    {
        voice.prepare(sampleRate);
        // Initialize modMatrix pointer for each voice
        // Note: This is a temporary fix - ideally the Voice structure
        // should be refactored to store a reference or index to the modMatrix
        // rather than a raw pointer that needs to be manually set
    }
}

void VoiceManager::reset()
{
    for (auto& voice : voices_)
    {
        voice.reset();
    }
    monoVoiceIndex_ = -1;
}

Voice* VoiceManager::findFreeVoice()
{
    // Find inactive voice
    for (int i = 0; i < 16; ++i)
    {
        if (!voices_[i].isActive())
        {
            return &voices_[i];
        }
    }

    // Voice stealing: steal oldest voice
    int oldestVoice = 0;
    double oldestTime = voices_[0].startTime;

    for (int i = 1; i < 16; ++i)
    {
        if (voices_[i].startTime < oldestTime)
        {
            oldestTime = voices_[i].startTime;
            oldestVoice = i;
        }
    }

    return &voices_[oldestVoice];
}

Voice* VoiceManager::findVoiceForNote(int note)
{
    for (int i = 0; i < 16; ++i)
    {
        if (voices_[i].midiNote == note && voices_[i].isActive())
        {
            return &voices_[i];
        }
    }
    return nullptr;
}

void VoiceManager::handleNoteOn(int note, float velocity)
{
    if (polyMode_ == PolyphonyMode::MONO || polyMode_ == PolyphonyMode::LEGATO)
    {
        // Monophonic/legato mode
        if (monoVoiceIndex_ == -1)
        {
            Voice* voice = findFreeVoice();
            if (voice)
            {
                voice->noteOn(note, velocity, currentSampleRate_);
                monoVoiceIndex_ = static_cast<int>(voice - voices_.data());
            }
        }
        else
        {
            if (polyMode_ == PolyphonyMode::LEGATO)
            {
                // Legato: change pitch without retriggering envelopes
                voices_[monoVoiceIndex_].midiNote = note;
                float freq = static_cast<float>(midiToFrequency(note, 0.0));
                voices_[monoVoiceIndex_].osc1.setFrequency(freq, currentSampleRate_);
                voices_[monoVoiceIndex_].osc2.setFrequency(freq, currentSampleRate_);
                voices_[monoVoiceIndex_].subOsc.setFrequency(freq, currentSampleRate_);
            }
            else
            {
                // Mono: retrigger
                voices_[monoVoiceIndex_].noteOn(note, velocity, currentSampleRate_);
            }
        }
    }
    else
    {
        // Polyphonic
        Voice* voice = findFreeVoice();
        if (voice)
        {
            voice->noteOn(note, velocity, currentSampleRate_);
        }
    }
}

void VoiceManager::handleNoteOff(int note)
{
    if (polyMode_ == PolyphonyMode::MONO || polyMode_ == PolyphonyMode::LEGATO)
    {
        // Check if this is the current mono voice
        if (monoVoiceIndex_ >= 0 && voices_[monoVoiceIndex_].midiNote == note)
        {
            voices_[monoVoiceIndex_].noteOff(0.0f);
        }
    }
    else
    {
        // Find and release voice
        Voice* voice = findVoiceForNote(note);
        if (voice)
        {
            voice->noteOff(0.0f);
        }
    }
}

void VoiceManager::allNotesOff()
{
    for (auto& voice : voices_)
    {
        voice.noteOff(0.0f);
    }
}

void VoiceManager::processBlock(float* output, int numSamples, double sampleRate)
{
    // Render all active voices
    for (int i = 0; i < numSamples; ++i)
    {
        float mix = 0.0f;

        for (auto& voice : voices_)
        {
            if (voice.isActive())
            {
                mix += voice.renderSample();
            }
        }

        output[i] = mix;
    }
}

int VoiceManager::getActiveVoiceCount() const
{
    int count = 0;
    for (const auto& voice : voices_)
    {
        if (voice.isActive() || voice.ampEnv.isActive())
            count++;
    }
    return count;
}

void VoiceManager::updateVoiceParameters(const NaturePureDSP& synth)
{
    for (auto& voice : voices_)
    {
        // Update oscillator levels
        voice.osc1Level = synth.params_.osc1Level;
        voice.osc2Level = synth.params_.osc2Level;
        voice.subLevel = synth.params_.subLevel;
        voice.noiseLevel = synth.params_.noiseLevel;

        // Update FM synthesis parameters
        voice.fmEnabled = synth.params_.fmEnabled != 0.0f;
        voice.fmDepth = synth.params_.fmDepth;
        voice.fmCarrierIndex = static_cast<int>(synth.params_.fmCarrierOsc);

        // Update filter envelope amount
        voice.filterEnvelopeAmount = synth.params_.filterEnvAmount;

        // Update oscillator waveforms
        voice.osc1.setWaveform(static_cast<int>(synth.params_.osc1Shape));
        voice.osc2.setWaveform(static_cast<int>(synth.params_.osc2Shape));

        // Update oscillator warp
        voice.osc1.setWarp(synth.params_.osc1Warp);
        voice.osc2.setWarp(synth.params_.osc2Warp);

        // Update oscillator pulse width
        voice.osc1.setPulseWidth(synth.params_.osc1PulseWidth);
        voice.osc2.setPulseWidth(synth.params_.osc2PulseWidth);

        // Update sub oscillator enable and level
        voice.subOsc.setEnabled(synth.params_.subEnabled != 0.0f);
        voice.subOsc.setLevel(synth.params_.subLevel);
        voice.subLevel = synth.params_.subLevel;

        // Update noise level
        voice.noiseGen.setLevel(synth.params_.noiseLevel);

        // Update filter parameters
        voice.filter.setType(static_cast<FilterType>(static_cast<int>(synth.params_.filterType)));
        voice.filter.setCutoff(synth.params_.filterCutoff * 20000.0f); // Normalize to Hz
        voice.filter.setResonance(synth.params_.filterResonance);

        // Update filter envelope
        Envelope::Parameters filterEnvParams;
        filterEnvParams.attack = synth.params_.filterEnvAttack;
        filterEnvParams.decay = synth.params_.filterEnvDecay;
        filterEnvParams.sustain = synth.params_.filterEnvSustain;
        filterEnvParams.release = synth.params_.filterEnvRelease;
        voice.filterEnv.setParameters(filterEnvParams);

        // Update amp envelope
        Envelope::Parameters ampEnvParams;
        ampEnvParams.attack = synth.params_.ampEnvAttack;
        ampEnvParams.decay = synth.params_.ampEnvDecay;
        ampEnvParams.sustain = synth.params_.ampEnvSustain;
        ampEnvParams.release = synth.params_.ampEnvRelease;
        voice.ampEnv.setParameters(ampEnvParams);

        // Update LFOs (if modMatrix pointer is set)
        // Note: The modMatrix pointer is currently null in Voice structure
        // This needs to be initialized properly during voice creation
        // For now, skip LFO updates to avoid crash
    }
}

//==============================================================================
// MAIN NATURE MARCO PURE DSP IMPLEMENTATION
//==============================================================================

NaturePureDSP::NaturePureDSP()
{
    // Initialize with default preset values to ensure silence is not due to zero parameters
    params_.osc1Level = 0.7f;
    params_.osc2Level = 0.6f;
    params_.subLevel = 0.4f;
    params_.subEnabled = 1.0f;
    params_.masterVolume = 0.85f;
    params_.osc1Shape = 0.0f;  // Saw
    params_.osc2Shape = 1.0f;  // Square
    params_.ampEnvAttack = 0.01f;
    params_.ampEnvDecay = 0.3f;
    params_.ampEnvSustain = 0.7f;
    params_.ampEnvRelease = 0.4f;
    params_.filterCutoff = 0.5f;
    params_.filterResonance = 0.7f;
}

NaturePureDSP::~NaturePureDSP()
{
}

bool NaturePureDSP::prepare(double sampleRate, int blockSize)
{
    sampleRate_ = sampleRate;
    blockSize_ = blockSize;

    voiceManager_.prepare(sampleRate, blockSize);
    modMatrix_.prepare(sampleRate);

    // CRITICAL: Apply current parameters to all voices after preparation
    // This ensures voices have proper oscillator levels and envelope settings
    voiceManager_.updateVoiceParameters(*this);

    return true;
}

void NaturePureDSP::reset()
{
    voiceManager_.reset();
    modMatrix_.reset();
    pitchBend_ = 0.0;
}

void NaturePureDSP::process(float** outputs, int numChannels, int numSamples)
{
    // Clear output buffers
    for (int ch = 0; ch < numChannels; ++ch)
    {
        std::memset(outputs[ch], 0, sizeof(float) * numSamples);
    }

    // Update modulation sources (LFOs, envelopes)
    for (int i = 0; i < numSamples; ++i)
    {
        modMatrix_.processModulationSources();
    }

    // Render all active voices
    float tempBuffer[512];
    voiceManager_.processBlock(tempBuffer, numSamples, sampleRate_);

    // Process stereo output
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = tempBuffer[i] * params_.masterVolume;
        processStereoSample(outputs[0][i], outputs[1][i]);
        outputs[0][i] = sample;
        outputs[1][i] = sample;
    }
}

void NaturePureDSP::handleEvent(const ScheduledEvent& event)
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
            pitchBend_ = event.data.pitchBend.bendValue;
            break;

        default:
            break;
    }
}

float NaturePureDSP::getParameter(const char* paramId) const
{
    // OSC1
    if (std::strcmp(paramId, "osc1_shape") == 0) return params_.osc1Shape;
    if (std::strcmp(paramId, "osc1_warp") == 0) return params_.osc1Warp;
    if (std::strcmp(paramId, "osc1_pulse_width") == 0) return params_.osc1PulseWidth;
    if (std::strcmp(paramId, "osc1_detune") == 0) return params_.osc1Detune;
    if (std::strcmp(paramId, "osc1_level") == 0) return params_.osc1Level;

    // OSC2
    if (std::strcmp(paramId, "osc2_shape") == 0) return params_.osc2Shape;
    if (std::strcmp(paramId, "osc2_warp") == 0) return params_.osc2Warp;
    if (std::strcmp(paramId, "osc2_pulse_width") == 0) return params_.osc2PulseWidth;
    if (std::strcmp(paramId, "osc2_detune") == 0) return params_.osc2Detune;
    if (std::strcmp(paramId, "osc2_level") == 0) return params_.osc2Level;

    // Sub
    if (std::strcmp(paramId, "sub_enabled") == 0) return params_.subEnabled;
    if (std::strcmp(paramId, "sub_level") == 0) return params_.subLevel;

    // FM
    if (std::strcmp(paramId, "fm_enabled") == 0) return params_.fmEnabled;
    if (std::strcmp(paramId, "fm_depth") == 0) return params_.fmDepth;

    // Filter
    if (std::strcmp(paramId, "filter_type") == 0) return params_.filterType;
    if (std::strcmp(paramId, "filter_cutoff") == 0) return params_.filterCutoff;
    if (std::strcmp(paramId, "filter_resonance") == 0) return params_.filterResonance;

    // Envelopes
    if (std::strcmp(paramId, "filter_env_attack") == 0) return params_.filterEnvAttack;
    if (std::strcmp(paramId, "filter_env_decay") == 0) return params_.filterEnvDecay;
    if (std::strcmp(paramId, "filter_env_sustain") == 0) return params_.filterEnvSustain;
    if (std::strcmp(paramId, "filter_env_release") == 0) return params_.filterEnvRelease;
    if (std::strcmp(paramId, "filter_env_amount") == 0) return params_.filterEnvAmount;

    if (std::strcmp(paramId, "amp_env_attack") == 0) return params_.ampEnvAttack;
    if (std::strcmp(paramId, "amp_env_decay") == 0) return params_.ampEnvDecay;
    if (std::strcmp(paramId, "amp_env_sustain") == 0) return params_.ampEnvSustain;
    if (std::strcmp(paramId, "amp_env_release") == 0) return params_.ampEnvRelease;

    // LFOs
    if (std::strcmp(paramId, "lfo1_rate") == 0) return params_.lfo1Rate;
    if (std::strcmp(paramId, "lfo1_depth") == 0) return params_.lfo1Depth;

    if (std::strcmp(paramId, "lfo2_rate") == 0) return params_.lfo2Rate;
    if (std::strcmp(paramId, "lfo2_depth") == 0) return params_.lfo2Depth;

    // Global
    if (std::strcmp(paramId, "master_volume") == 0) return params_.masterVolume;
    if (std::strcmp(paramId, "poly_mode") == 0) return params_.polyMode;

    return 0.0f;
}

void NaturePureDSP::setParameter(const char* paramId, float value)
{
    // Get old value for logging (before change)
    float oldValue = getParameter(paramId);

    // OSC1
    if (std::strcmp(paramId, "osc1_shape") == 0) params_.osc1Shape = value;
    if (std::strcmp(paramId, "osc1_warp") == 0) params_.osc1Warp = value;
    if (std::strcmp(paramId, "osc1_pulse_width") == 0) params_.osc1PulseWidth = value;
    if (std::strcmp(paramId, "osc1_detune") == 0) params_.osc1Detune = value;
    if (std::strcmp(paramId, "osc1_level") == 0) params_.osc1Level = value;

    // OSC2
    if (std::strcmp(paramId, "osc2_shape") == 0) params_.osc2Shape = value;
    if (std::strcmp(paramId, "osc2_warp") == 0) params_.osc2Warp = value;
    if (std::strcmp(paramId, "osc2_pulse_width") == 0) params_.osc2PulseWidth = value;
    if (std::strcmp(paramId, "osc2_detune") == 0) params_.osc2Detune = value;
    if (std::strcmp(paramId, "osc2_level") == 0) params_.osc2Level = value;

    // Sub
    if (std::strcmp(paramId, "sub_enabled") == 0) params_.subEnabled = value;
    if (std::strcmp(paramId, "sub_level") == 0) params_.subLevel = value;

    // FM
    if (std::strcmp(paramId, "fm_enabled") == 0) params_.fmEnabled = value;
    if (std::strcmp(paramId, "fm_depth") == 0) params_.fmDepth = value;

    // Filter
    if (std::strcmp(paramId, "filter_type") == 0) params_.filterType = value;
    if (std::strcmp(paramId, "filter_cutoff") == 0) params_.filterCutoff = value;
    if (std::strcmp(paramId, "filter_resonance") == 0) params_.filterResonance = value;

    // Envelopes
    if (std::strcmp(paramId, "filter_env_attack") == 0) params_.filterEnvAttack = value;
    if (std::strcmp(paramId, "filter_env_decay") == 0) params_.filterEnvDecay = value;
    if (std::strcmp(paramId, "filter_env_sustain") == 0) params_.filterEnvSustain = value;
    if (std::strcmp(paramId, "filter_env_release") == 0) params_.filterEnvRelease = value;
    if (std::strcmp(paramId, "filter_env_amount") == 0) params_.filterEnvAmount = value;

    if (std::strcmp(paramId, "amp_env_attack") == 0) params_.ampEnvAttack = value;
    if (std::strcmp(paramId, "amp_env_decay") == 0) params_.ampEnvDecay = value;
    if (std::strcmp(paramId, "amp_env_sustain") == 0) params_.ampEnvSustain = value;
    if (std::strcmp(paramId, "amp_env_release") == 0) params_.ampEnvRelease = value;

    // LFOs
    if (std::strcmp(paramId, "lfo1_rate") == 0) params_.lfo1Rate = value;
    if (std::strcmp(paramId, "lfo1_depth") == 0) params_.lfo1Depth = value;

    if (std::strcmp(paramId, "lfo2_rate") == 0) params_.lfo2Rate = value;
    if (std::strcmp(paramId, "lfo2_depth") == 0) params_.lfo2Depth = value;

    // Global
    if (std::strcmp(paramId, "master_volume") == 0) params_.masterVolume = value;
    if (std::strcmp(paramId, "poly_mode") == 0) params_.polyMode = value;

    // Log parameter change (shared telemetry infrastructure)
    LOG_PARAMETER_CHANGE("Nature", paramId, oldValue, value);

    applyParameters();
}

void NaturePureDSP::applyParameters()
{
    // Update all voices with current synth parameters
    voiceManager_.updateVoiceParameters(*this);
}

int NaturePureDSP::getActiveVoiceCount() const
{
    return voiceManager_.getActiveVoiceCount();
}

float NaturePureDSP::calculateFrequency(int midiNote, float bend) const
{
    float noteOffset = bend * static_cast<float>(params_.pitchBendRange);
    float adjustedNote = midiNote + noteOffset + params_.masterTune;
    return static_cast<float>(midiToFrequency(static_cast<int>(adjustedNote), 0.0));
}

void NaturePureDSP::processStereoSample(float& left, float& right)
{
    // Stereo processing (currently mono)
    // Pan could be applied here
}

bool NaturePureDSP::savePreset(char* jsonBuffer, int jsonBufferSize) const
{
    int offset = 0;

    // Opening brace
    int written = snprintf(jsonBuffer + offset, jsonBufferSize - offset, "{");
    if (written < 0 || offset + written >= jsonBufferSize)
        return false;
    offset += written;

    // OSC1 parameters
    writeJsonParameter("osc1_shape", params_.osc1Shape, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("osc1_warp", params_.osc1Warp, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("osc1_level", params_.osc1Level, jsonBuffer, offset, jsonBufferSize);

    // OSC2 parameters
    writeJsonParameter("osc2_shape", params_.osc2Shape, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("osc2_warp", params_.osc2Warp, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("osc2_level", params_.osc2Level, jsonBuffer, offset, jsonBufferSize);

    // Filter parameters
    writeJsonParameter("filter_cutoff", params_.filterCutoff, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("filter_resonance", params_.filterResonance, jsonBuffer, offset, jsonBufferSize);

    // Envelope parameters
    writeJsonParameter("amp_env_attack", params_.ampEnvAttack, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("amp_env_decay", params_.ampEnvDecay, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("amp_env_sustain", params_.ampEnvSustain, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("amp_env_release", params_.ampEnvRelease, jsonBuffer, offset, jsonBufferSize);

    // Master volume
    writeJsonParameter("master_volume", params_.masterVolume, jsonBuffer, offset, jsonBufferSize);

    // Remove trailing comma and add closing brace
    if (offset > 1 && jsonBuffer[offset - 1] == ',')
    {
        offset--;
        jsonBuffer[offset] = '}';
        jsonBuffer[offset + 1] = '\0';
    }

    return true;
}

bool NaturePureDSP::loadPreset(const char* jsonData)
{
    std::string jsonStr(jsonData);

    // Check if this is UPFS v1.0 format by looking for format field
    // Simple check without full JSON parse
    if (jsonStr.find("\"format\"") != std::string::npos &&
        jsonStr.find("\"UPFS\"") != std::string::npos &&
        jsonStr.find("\"version\"") != std::string::npos)
    {
        // Try UPFS v1.0 format
        try
        {
            UPFS::Preset preset = UPFS::PresetParser::parseFromString(jsonStr);
            return loadUPFSPreset(preset);
        }
        catch (const UPFS::ParsingError& error)
        {
            // UPFS parsing failed, fall through to legacy format
            printf("[Nature DSP] UPFS parsing failed: %s, falling back to legacy format\n", error.toString().c_str());
        }
    }

    // Legacy format parsing
    return loadLegacyPreset(jsonData);
}

bool NaturePureDSP::loadUPFSPreset(const UPFS::Preset& preset)
{
    // Map UPFS parameters to DSP parameters
    // Parameters are stored in a flat map in the preset

    // OSC1 parameters
    if (auto param = preset.getParameter("osc1_shape"))
        params_.osc1Shape = static_cast<float>(param->value);
    if (auto param = preset.getParameter("osc1_warp"))
        params_.osc1Warp = static_cast<float>(param->value);
    if (auto param = preset.getParameter("osc1_pulse_width"))
        params_.osc1PulseWidth = static_cast<float>(param->value);
    if (auto param = preset.getParameter("osc1_detune"))
        params_.osc1Detune = static_cast<float>(param->value);
    if (auto param = preset.getParameter("osc1_pan"))
        params_.osc1Pan = static_cast<float>(param->value);
    if (auto param = preset.getParameter("osc1_level"))
        params_.osc1Level = static_cast<float>(param->value);

    // OSC2 parameters
    if (auto param = preset.getParameter("osc2_shape"))
        params_.osc2Shape = static_cast<float>(param->value);
    if (auto param = preset.getParameter("osc2_warp"))
        params_.osc2Warp = static_cast<float>(param->value);
    if (auto param = preset.getParameter("osc2_pulse_width"))
        params_.osc2PulseWidth = static_cast<float>(param->value);
    if (auto param = preset.getParameter("osc2_detune"))
        params_.osc2Detune = static_cast<float>(param->value);
    if (auto param = preset.getParameter("osc2_pan"))
        params_.osc2Pan = static_cast<float>(param->value);
    if (auto param = preset.getParameter("osc2_level"))
        params_.osc2Level = static_cast<float>(param->value);

    // Sub oscillator
    if (auto param = preset.getParameter("sub_enabled"))
        params_.subEnabled = static_cast<float>(param->value);
    if (auto param = preset.getParameter("sub_level"))
        params_.subLevel = static_cast<float>(param->value);

    // Noise
    if (auto param = preset.getParameter("noise_level"))
        params_.noiseLevel = static_cast<float>(param->value);

    // FM synthesis
    if (auto param = preset.getParameter("fm_enabled"))
        params_.fmEnabled = static_cast<float>(param->value);
    if (auto param = preset.getParameter("fm_carrier_osc"))
        params_.fmCarrierOsc = static_cast<float>(param->value);
    if (auto param = preset.getParameter("fm_mode"))
        params_.fmMode = static_cast<float>(param->value);
    if (auto param = preset.getParameter("fm_depth"))
        params_.fmDepth = static_cast<float>(param->value);
    if (auto param = preset.getParameter("fm_modulator_ratio"))
        params_.fmModulatorRatio = static_cast<float>(param->value);

    // Filter
    if (auto param = preset.getParameter("filter_type"))
        params_.filterType = static_cast<float>(param->value);
    if (auto param = preset.getParameter("filter_cutoff"))
        params_.filterCutoff = static_cast<float>(param->value);
    if (auto param = preset.getParameter("filter_resonance"))
        params_.filterResonance = static_cast<float>(param->value);
    if (auto param = preset.getParameter("filter_key_track"))
        params_.filterKeyTrack = static_cast<float>(param->value);
    if (auto param = preset.getParameter("filter_vel_track"))
        params_.filterVelTrack = static_cast<float>(param->value);

    // Filter envelope
    if (auto param = preset.getParameter("filter_env_attack"))
        params_.filterEnvAttack = static_cast<float>(param->value);
    if (auto param = preset.getParameter("filter_env_decay"))
        params_.filterEnvDecay = static_cast<float>(param->value);
    if (auto param = preset.getParameter("filter_env_sustain"))
        params_.filterEnvSustain = static_cast<float>(param->value);
    if (auto param = preset.getParameter("filter_env_release"))
        params_.filterEnvRelease = static_cast<float>(param->value);
    if (auto param = preset.getParameter("filter_env_amount"))
        params_.filterEnvAmount = static_cast<float>(param->value);

    // Amp envelope
    if (auto param = preset.getParameter("amp_env_attack"))
        params_.ampEnvAttack = static_cast<float>(param->value);
    if (auto param = preset.getParameter("amp_env_decay"))
        params_.ampEnvDecay = static_cast<float>(param->value);
    if (auto param = preset.getParameter("amp_env_sustain"))
        params_.ampEnvSustain = static_cast<float>(param->value);
    if (auto param = preset.getParameter("amp_env_release"))
        params_.ampEnvRelease = static_cast<float>(param->value);

    // LFO1
    if (auto param = preset.getParameter("lfo1_waveform"))
        params_.lfo1Waveform = static_cast<float>(param->value);
    if (auto param = preset.getParameter("lfo1_rate"))
        params_.lfo1Rate = static_cast<float>(param->value);
    if (auto param = preset.getParameter("lfo1_depth"))
        params_.lfo1Depth = static_cast<float>(param->value);
    if (auto param = preset.getParameter("lfo1_bipolar"))
        params_.lfo1Bipolar = static_cast<float>(param->value);

    // LFO2
    if (auto param = preset.getParameter("lfo2_waveform"))
        params_.lfo2Waveform = static_cast<float>(param->value);
    if (auto param = preset.getParameter("lfo2_rate"))
        params_.lfo2Rate = static_cast<float>(param->value);
    if (auto param = preset.getParameter("lfo2_depth"))
        params_.lfo2Depth = static_cast<float>(param->value);
    if (auto param = preset.getParameter("lfo2_bipolar"))
        params_.lfo2Bipolar = static_cast<float>(param->value);

    // Modulation matrix (16 slots)
    for (int i = 0; i < 16; ++i)
    {
        std::string prefix = "mod_" + std::to_string(i) + "_";
        if (auto param = preset.getParameter(prefix + "source"))
            params_.modSource[i] = static_cast<float>(param->value);
        if (auto param = preset.getParameter(prefix + "destination"))
            params_.modDestination[i] = static_cast<float>(param->value);
        if (auto param = preset.getParameter(prefix + "amount"))
            params_.modAmount[i] = static_cast<float>(param->value);
        if (auto param = preset.getParameter(prefix + "bipolar"))
            params_.modBipolar[i] = static_cast<float>(param->value);
        if (auto param = preset.getParameter(prefix + "curve"))
            params_.modCurve[i] = static_cast<float>(param->value);
    }

    // Macros (8 macros)
    for (int i = 0; i < 8; ++i)
    {
        std::string macroName = "macro_" + std::to_string(i) + "_value";
        if (auto param = preset.getParameter(macroName))
            params_.macroValue[i] = static_cast<float>(param->value);
    }

    // Global parameters
    if (auto param = preset.getParameter("poly_mode"))
        params_.polyMode = static_cast<float>(param->value);
    if (auto param = preset.getParameter("glide_enabled"))
        params_.glideEnabled = static_cast<float>(param->value);
    if (auto param = preset.getParameter("glide_time"))
        params_.glideTime = static_cast<float>(param->value);
    if (auto param = preset.getParameter("master_tune"))
        params_.masterTune = static_cast<float>(param->value);
    if (auto param = preset.getParameter("master_volume"))
        params_.masterVolume = static_cast<float>(param->value);

    applyParameters();
    return true;
}

bool NaturePureDSP::loadLegacyPreset(const char* jsonData)
{
    // Simplified JSON parsing for legacy format
    double value;

    if (parseJsonParameter(jsonData, "osc1_shape", value))
        params_.osc1Shape = static_cast<float>(value);

    if (parseJsonParameter(jsonData, "osc1_warp", value))
        params_.osc1Warp = static_cast<float>(value);

    if (parseJsonParameter(jsonData, "osc1_level", value))
        params_.osc1Level = static_cast<float>(value);

    if (parseJsonParameter(jsonData, "osc2_shape", value))
        params_.osc2Shape = static_cast<float>(value);

    if (parseJsonParameter(jsonData, "osc2_warp", value))
        params_.osc2Warp = static_cast<float>(value);

    if (parseJsonParameter(jsonData, "osc2_level", value))
        params_.osc2Level = static_cast<float>(value);

    if (parseJsonParameter(jsonData, "filter_cutoff", value))
        params_.filterCutoff = static_cast<float>(value);

    if (parseJsonParameter(jsonData, "filter_resonance", value))
        params_.filterResonance = static_cast<float>(value);

    if (parseJsonParameter(jsonData, "amp_env_attack", value))
        params_.ampEnvAttack = static_cast<float>(value);

    if (parseJsonParameter(jsonData, "amp_env_decay", value))
        params_.ampEnvDecay = static_cast<float>(value);

    if (parseJsonParameter(jsonData, "amp_env_sustain", value))
        params_.ampEnvSustain = static_cast<float>(value);

    if (parseJsonParameter(jsonData, "amp_env_release", value))
        params_.ampEnvRelease = static_cast<float>(value);

    if (parseJsonParameter(jsonData, "master_volume", value))
        params_.masterVolume = static_cast<float>(value);

    applyParameters();
    return true;
}

bool NaturePureDSP::writeJsonParameter(const char* name, double value, char* buffer, int& offset, int bufferSize) const
{
    int written = snprintf(buffer + offset, bufferSize - offset, "\"%s\": %g,",
                           name, value);
    if (written < 0 || offset + written >= bufferSize)
        return false;
    offset += written;
    return true;
}

bool NaturePureDSP::parseJsonParameter(const char* json, const char* param, double& value) const
{
    // Very simple JSON parsing (sufficient for test cases)
    char searchPattern[256];
    snprintf(searchPattern, sizeof(searchPattern), "\"%s\":", param);

    const char* found = std::strstr(json, searchPattern);
    if (!found)
        return false;

    found += std::strlen(searchPattern);
    value = std::atof(found);

    return true;
}

//==============================================================================
// Static Factory (No runtime registration for tvOS hardening)
//==============================================================================

// Pure DSP instruments are instantiated directly, not through dynamic factory
// This ensures tvOS compatibility (no static initialization, no global state)

} // namespace DSP
