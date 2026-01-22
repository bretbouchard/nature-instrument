/*
  ==============================================================================

    AetherPureDSP.cpp
    Pure DSP implementation of Aether String

    Physical modeling synthesis with Karplus-Strong waveguide algorithm.
    All JUCE dependencies removed - pure C++17 implementation.

  ==============================================================================
*/

#include "dsp/AetherPureDSP.h"
#include "../../../../include/dsp/LookupTables.h"
#include "../../../../include/dsp/DSPLogging.h"
#include <cstring>
#include <random>
#include <cmath>
#include <cassert>
#include <iostream>

namespace DSP {

//==============================================================================
// FractionalDelayLine Implementation
//==============================================================================

FractionalDelayLine::FractionalDelayLine()
{
    buffer_.resize(1024, 0.0f);
    writeIndex_ = 0;
    delay_ = 0.0f;
    maxDelay_ = 1024;
}

void FractionalDelayLine::prepare(double sampleRate, int maximumDelay)
{
    maxDelay_ = maximumDelay + 4;  // Extra space for interpolation
    buffer_.resize(maxDelay_, 0.0f);
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    writeIndex_ = 0;
}

void FractionalDelayLine::reset()
{
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    writeIndex_ = 0;
}

void FractionalDelayLine::setDelay(float delayInSamples)
{
    delay_ = std::max(0.0f, std::min(static_cast<float>(maxDelay_ - 4), delayInSamples));
}

float FractionalDelayLine::popSample()
{
    float readPos = writeIndex_ - delay_;
    if (readPos < 0.0f)
        readPos += maxDelay_;

    return interpolate(readPos);
}

void FractionalDelayLine::pushSample(float sample)
{
    buffer_[writeIndex_] = sample;
    writeIndex_ = (writeIndex_ + 1) % maxDelay_;
}

float FractionalDelayLine::interpolate(float fractionalDelay)
{
    int delayIndex = static_cast<int>(fractionalDelay);
    float frac = fractionalDelay - delayIndex;

    int index0 = (writeIndex_ - 1 - delayIndex + maxDelay_) % maxDelay_;
    int index1 = (index0 - 1 + maxDelay_) % maxDelay_;
    int index2 = (index0 - 2 + maxDelay_) % maxDelay_;
    int index3 = (index0 - 3 + maxDelay_) % maxDelay_;

    // 4-point Lagrange interpolation
    float y0 = buffer_[index0];
    float y1 = buffer_[index1];
    float y2 = buffer_[index2];
    float y3 = buffer_[index3];

    float frac2 = frac * frac;
    float frac3 = frac2 * frac;

    float w0 = -0.16666667f * frac3 + 0.5f * frac2 - 0.33333333f * frac;
    float w1 = 0.5f * frac3 - frac2 - 0.5f * frac + 1.0f;
    float w2 = -0.5f * frac3 + 0.5f * frac2 + frac;
    float w3 = 0.16666667f * frac3 - 0.33333333f * frac2;

    return w0 * y3 + w1 * y0 + w2 * y1 + w3 * y2;
}

//==============================================================================
// TPTFilter Implementation
//==============================================================================

TPTFilter::TPTFilter()
{
    z1_ = 0.0f;
    g_ = 0.0f;
    h_ = 0.0f;
}

void TPTFilter::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    reset();
}

void TPTFilter::reset()
{
    z1_ = 0.0f;
    setCutoffFrequency(cutoff_);
}

void TPTFilter::setType(Type type)
{
    type_ = type;
}

void TPTFilter::setCutoffFrequency(float freq)
{
    cutoff_ = freq;
    float wd = 2.0f * 3.14159265359f * cutoff_ / sampleRate_;
    // Use LookupTables for sine calculation
    float wa = SchillingerEcosystem::DSP::fastSineLookup(wd);
    g_ = wa / std::sqrt(1.0f + wa * wa);  // CRITICAL FIX: Removed 'float' to update member variable
    h_ = 1.0f / (1.0f - g_);
}

float TPTFilter::processSample(float input)
{
    float v1 = (input - z1_) * g_;
    float v2 = v1 + z1_;
    z1_ = v2 + v1;

    switch (type_)
    {
        case Type::lowpass:
            return v2;
        case Type::highpass:
            return input - v2;
        case Type::allpass:
            return input - 2.0f * g_ * v2;
        case Type::bandpass:
            return v2 * 2.0f;  // Simplified
        default:
            return input;
    }
}

//==============================================================================
// ModalFilter Implementation
//==============================================================================

void ModalFilter::prepare(double sampleRate)
{
    sr = sampleRate;
    // Compute initial Q value
    computedQ = computeQ(frequency, decay, 1.0f);
}

float ModalFilter::computeQ(float freq, float damping, float structure)
{
    // Based on Mutable Instruments' Rings resonator design
    // Higher frequencies damp faster (real string behavior)

    // Normalize frequency to 0-1 range (20Hz - 20kHz)
    float normalizedFreq = (freq - 20.0f) / 19980.0f;
    normalizedFreq = std::max(0.0f, std::min(1.0f, normalizedFreq));

    // Frequency-dependent damping factor
    // Higher modes (higher frequencies) have lower Q
    float frequencyDamping = 1.0f + (normalizedFreq * 2.0f);  // 1.0 to 3.0

    // Mode index scaling (harmonics damp faster)
    float modeDamping = 1.0f + (modeIndex * 0.15f);  // Each mode dampens 15% more

    // Material factor affects overall brightness
    // 0.5 = soft wood (darker), 1.0 = standard, 1.5 = bright metal
    float materialMod = materialFactor;

    // Structure parameter (from Rings) affects damping curve
    float structureMod = 1.0f + (structure * 0.5f);  // 1.0 to 1.5

    // Calculate Q
    // Base Q modified by frequency, mode, material, and structure
    float baseQ = 50.0f;  // Base quality factor
    computedQ = baseQ * materialMod / (frequencyDamping * modeDamping * structureMod);

    // Apply damping parameter (0.996 = very little damping, 0.9 = heavy damping)
    computedQ *= damping;

    // Clamp Q to reasonable range
    computedQ = std::max(5.0f, std::min(200.0f, computedQ));

    return computedQ;
}

float ModalFilter::processSample(float excitation)
{
    energy += excitation * amplitude;

    // Use frequency-dependent Q for more realistic decay
    // Q determines how quickly energy decays
    float decayFactor = 1.0f - (1.0f / (computedQ * sr * 0.001f));  // Scale Q for sample rate
    decayFactor = std::max(0.999f, std::min(0.99999f, decayFactor));  // Keep in reasonable range
    energy *= decayFactor;

    if (std::abs(energy) < 1e-10f)
        energy = 0.0f;

    float phaseIncrement = frequency / sr;
    phase += phaseIncrement;
    if (phase >= 1.0f)
        phase -= 1.0f;

    // Use LookupTables for sine calculation
    float output = energy * SchillingerEcosystem::DSP::fastSineLookup(phase * 2.0f * 3.14159265359f);
    return output;
}

void ModalFilter::reset()
{
    phase = 0.0f;
    energy = 0.0f;
    computedQ = computeQ(frequency, decay, 1.0f);
}

//==============================================================================
// WaveguideString Implementation
//==============================================================================

WaveguideString::WaveguideString()
{
    params_.frequency = 440.0f;
    params_.damping = 0.996f;
    params_.stiffness = 0.0f;
    params_.brightness = 0.5f;
    params_.bridgeCoupling = 0.6f;  // Boosted for normalization (was 0.3)
    params_.nonlinearity = 0.1f;
    params_.dispersion = 0.5f;
    params_.sympatheticCoupling = 0.1f;
}

void WaveguideString::prepare(double sampleRate)
{
    sr = sampleRate;

    // Max delay for lowest note (E2 = 82.4 Hz)
    int maxDelay = static_cast<int>(sampleRate / 82.4) + 100;
    maxDelayInSamples = maxDelay;

    fractionalDelay_.prepare(sampleRate, maxDelay);

    // Set initial frequency
    fractionalDelay_.setDelay(static_cast<float>(sr / params_.frequency));

    stiffnessFilter_.prepare(sampleRate);
    stiffnessFilter_.setType(TPTFilter::Type::allpass);
    // CRITICAL FIX: Use current stiffness parameter instead of hardcoded value
    float stiffnessCutoff = 1000.0f + params_.stiffness * 4000.0f;
    stiffnessFilter_.setCutoffFrequency(stiffnessCutoff);

    dampingFilter_.prepare(sampleRate);
    dampingFilter_.setType(TPTFilter::Type::lowpass);
    // CRITICAL FIX: Use current brightness parameter instead of hardcoded value
    float dampingCutoff = 1000.0f + params_.brightness * 9000.0f;
    dampingFilter_.setCutoffFrequency(dampingCutoff);

    // Prepare dispersion filters (cascaded allpass for realistic dispersion)
    // Each filter operates at a different frequency for broad dispersion
    dispersionFilter1_.prepare(sampleRate);
    dispersionFilter1_.setType(TPTFilter::Type::allpass);
    dispersionFilter1_.setCutoffFrequency(3000.0f);

    dispersionFilter2_.prepare(sampleRate);
    dispersionFilter2_.setType(TPTFilter::Type::allpass);
    dispersionFilter2_.setCutoffFrequency(6000.0f);

    dispersionFilter3_.prepare(sampleRate);
    dispersionFilter3_.setType(TPTFilter::Type::allpass);
    dispersionFilter3_.setCutoffFrequency(12000.0f);

    updateBridgeImpedance();
}

void WaveguideString::updateBridgeImpedance()
{
    // Bridge impedance depends on string gauge and material
    // Thicker strings have higher impedance
    float gaugeFactor = 1.0f + static_cast<float>(params_.stringGauge) * 0.5f;
    bridgeImpedance_ = 1000.0f * gaugeFactor;
}

void WaveguideString::reset()
{
    fractionalDelay_.reset();
    stiffnessFilter_.reset();
    dampingFilter_.reset();
    dispersionFilter1_.reset();
    dispersionFilter2_.reset();
    dispersionFilter3_.reset();
    lastBridgeEnergy_ = 0.0f;
    sympatheticEnergy_ = 0.0f;
}

void WaveguideString::excite(const float* exciterSignal, int exciterLength, float velocity)
{
    int length = fractionalDelay_.getMaximumDelay();
    
    for (int i = 0; i < length; ++i)
    {
        float sample = exciterSignal[i % exciterLength];
        fractionalDelay_.pushSample(sample * velocity);
    }
}

float WaveguideString::processSample()
{
    float output = fractionalDelay_.popSample();

    // Stiffness (allpass for inharmonicity)
    float stiffOutput = stiffnessFilter_.processSample(output);

    // Dispersion filters (cascaded allpass for realistic high-frequency propagation)
    // This creates frequency-dependent phase shift, mimicking real string dispersion
    float dispersed = stiffOutput;
    if (params_.dispersion > 0.01f)
    {
        // Apply dispersion based on dispersion parameter
        // More dispersion = more phase shift at high frequencies
        float dispersionAmount = params_.dispersion;

        // Blend between dispersed and non-dispersed signal
        float dispersed1 = dispersionFilter1_.processSample(dispersed);
        float dispersed2 = dispersionFilter2_.processSample(dispersed1);
        float dispersed3 = dispersionFilter3_.processSample(dispersed2);

        // Dry/wet mix for dispersion
        dispersed = dispersed * (1.0f - dispersionAmount) + dispersed3 * dispersionAmount;
    }

    // Damping (lowpass for brightness)
    float damped = dampingFilter_.processSample(dispersed);
    damped *= params_.damping;

    // Add sympathetic resonance from other strings
    damped += sympatheticEnergy_ * params_.sympatheticCoupling;

    // Bridge coupling with impedance modeling
    float linearBridgeEnergy = damped * params_.bridgeCoupling;

    // Bridge impedance affects reflection coefficient
    float impedanceFactor = bridgeImpedance_ / (bridgeImpedance_ + 1000.0f);  // Normalize to 0-1
    linearBridgeEnergy *= impedanceFactor;

    float nonlinearFactor = 1.0f + params_.nonlinearity;
    float saturatedBridge = std::tanh(linearBridgeEnergy * nonlinearFactor);

    lastBridgeEnergy_ = saturatedBridge;
    float reflectedEnergy = damped - saturatedBridge;

    // Store some energy for sympathetic coupling
    sympatheticEnergy_ = sympatheticEnergy_ * 0.99f + saturatedBridge * 0.01f;

    fractionalDelay_.pushSample(reflectedEnergy);

    return output;
}

void WaveguideString::setFrequency(float freq)
{
    params_.frequency = std::max(20.0f, std::min(20000.0f, freq));
    float delayInSamples = static_cast<float>(sr / params_.frequency);
    fractionalDelay_.setDelay(delayInSamples);
}

void WaveguideString::setDamping(float damping) {
    params_.damping = std::max(0.9f, std::min(1.0f, damping));
    // CRITICAL FIX: Update damping filter cutoff based on damping parameter
    // Higher damping = lower cutoff (darker sound)
    float cutoff = 1000.0f + params_.brightness * 9000.0f;
    dampingFilter_.setCutoffFrequency(cutoff);
}

void WaveguideString::setStiffness(float stiffness) {
    params_.stiffness = std::max(0.0f, std::min(0.5f, stiffness));
    // CRITICAL FIX: Update stiffness filter cutoff based on stiffness parameter
    // Higher stiffness = higher cutoff (brighter inharmonicity)
    float cutoff = 1000.0f + params_.stiffness * 4000.0f;
    stiffnessFilter_.setCutoffFrequency(cutoff);
}

void WaveguideString::setBrightness(float brightness) {
    params_.brightness = std::max(0.0f, std::min(1.0f, brightness));
    // CRITICAL FIX: Update damping filter cutoff based on brightness parameter
    float cutoff = 1000.0f + params_.brightness * 9000.0f;
    dampingFilter_.setCutoffFrequency(cutoff);
}

void WaveguideString::setBridgeCoupling(float coupling) { params_.bridgeCoupling = std::max(0.0f, std::min(1.0f, coupling)); }
void WaveguideString::setNonlinearity(float nonlinearity) { params_.nonlinearity = std::max(0.0f, std::min(1.0f, nonlinearity)); }
void WaveguideString::setDispersion(float dispersion) { params_.dispersion = std::max(0.0f, std::min(1.0f, dispersion)); }
void WaveguideString::setSympatheticCoupling(float coupling) { params_.sympatheticCoupling = std::max(0.0f, std::min(1.0f, coupling)); }

void WaveguideString::setStringLengthMeters(float length)
{
    params_.stringLengthMeters = std::max(0.1f, std::min(100.0f, length));
    float normalizedLength = params_.stringLengthMeters / 0.65f;
    
    float baseStiffness = params_.stiffness;
    if (baseStiffness < 0.001f) baseStiffness = 0.1f;
    params_.stiffness = std::max(0.0f, std::min(0.5f, baseStiffness / std::sqrt(normalizedLength)));
    
    float baseDamping = 0.996f;
    params_.damping = std::max(0.9f, std::min(0.99999f, baseDamping + 0.001f * (normalizedLength - 1.0f)));
    
    float baseCoupling = 0.3f;
    params_.bridgeCoupling = std::max(0.0f, std::min(1.0f, baseCoupling / std::sqrt(normalizedLength)));
}

void WaveguideString::setStringGauge(StringGauge gauge)
{
    params_.stringGauge = static_cast<int>(gauge);
    const float baseBrightness = 0.5f;
    const float baseDamping = 0.996f;

    switch (gauge)
    {
        case StringGauge::Thin:
            params_.brightness = std::max(0.0f, std::min(1.0f, baseBrightness * 1.2f));
            break;
        case StringGauge::Normal:
            params_.brightness = baseBrightness;
            break;
        case StringGauge::Thick:
            params_.brightness = std::max(0.0f, std::min(1.0f, baseBrightness * 0.75f));
            break;
        case StringGauge::Massive:
            params_.brightness = std::max(0.0f, std::min(1.0f, baseBrightness * 0.6f));
            break;
    }

    dampingFilter_.setCutoffFrequency(1000.0f + params_.brightness * 9000.0f);

    // Update bridge impedance based on gauge
    updateBridgeImpedance();
}

void WaveguideString::setPickPosition(float position)
{
    params_.pickPosition = std::max(0.0f, std::min(1.0f, position));
}

//==============================================================================
// BridgeCoupling Implementation
//==============================================================================

BridgeCoupling::BridgeCoupling() = default;

void BridgeCoupling::prepare(double sampleRate) {}
void BridgeCoupling::reset() { bridgeEnergy_ = 0.0f; }

float BridgeCoupling::processString(float stringOutput)
{
    float linearBridgeEnergy = stringOutput * couplingCoefficient_;
    float nonlinearBridge = std::tanh(linearBridgeEnergy * (1.0f + nonlinearity_));
    
    bridgeEnergy_ = nonlinearBridge;
    float reflectedEnergy = stringOutput - nonlinearBridge;
    
    return reflectedEnergy;
}

//==============================================================================
// ModalBodyResonator Implementation
//==============================================================================

ModalBodyResonator::ModalBodyResonator()
{
    modes_.reserve(16);
}

void ModalBodyResonator::prepare(double sampleRate)
{
    sr = sampleRate;
    for (auto& mode : modes_)
        mode.prepare(sampleRate);
}

void ModalBodyResonator::reset()
{
    for (auto& mode : modes_)
        mode.reset();
}

float ModalBodyResonator::processSample(float bridgeEnergy)
{
    float output = 0.0f;
    
    for (auto& mode : modes_)
        output += mode.processSample(bridgeEnergy);
    
    if (!modes_.empty())
        output /= static_cast<float>(modes_.size());
    
    return output;
}

void ModalBodyResonator::setResonance(float amount)
{
    amount = std::max(0.0f, std::min(2.0f, amount));
    for (auto& mode : modes_)
        mode.amplitude = mode.baseAmplitude * amount;
}

void ModalBodyResonator::setMaterial(MaterialType material)
{
    material_ = material;

    // Update material factor for all modes
    float materialFactor = 1.0f;
    switch (material)
    {
        case MaterialType::SoftWood:
            materialFactor = 0.5f;
            break;
        case MaterialType::StandardWood:
            materialFactor = 1.0f;
            break;
        case MaterialType::HardWood:
            materialFactor = 1.3f;
            break;
        case MaterialType::Metal:
            materialFactor = 1.5f;
            break;
    }

    for (auto& mode : modes_)
    {
        mode.materialFactor = materialFactor;
        mode.computedQ = mode.computeQ(mode.frequency, mode.decay, 1.0f);
    }
}

void ModalBodyResonator::recalculateModeQ(float damping, float structure)
{
    for (size_t i = 0; i < modes_.size(); ++i)
    {
        modes_[i].modeIndex = static_cast<float>(i);
        modes_[i].computedQ = modes_[i].computeQ(modes_[i].frequency, damping, structure);
    }
}

void ModalBodyResonator::loadGuitarBodyPreset()
{
    modes_.clear();

    // Typical acoustic guitar body modes with per-mode Q calculation
    // Set up mode 0 (fundamental)
    ModalFilter mode1;
    mode1.frequency = 95.0f;
    mode1.amplitude = 0.8f;
    mode1.baseAmplitude = 0.8f;
    mode1.decay = 2.0f;
    mode1.modeIndex = 0.0f;
    mode1.materialFactor = (material_ == MaterialType::StandardWood) ? 1.0f :
                           (material_ == MaterialType::SoftWood) ? 0.5f :
                           (material_ == MaterialType::HardWood) ? 1.3f : 1.5f;
    mode1.sr = sr;
    modes_.push_back(mode1);

    // Mode 1
    ModalFilter mode2;
    mode2.frequency = 190.0f;
    mode2.amplitude = 0.6f;
    mode2.baseAmplitude = 0.6f;
    mode2.decay = 1.5f;
    mode2.modeIndex = 1.0f;
    mode2.materialFactor = mode1.materialFactor;
    mode2.sr = sr;
    modes_.push_back(mode2);

    // Mode 2
    ModalFilter mode3;
    mode3.frequency = 280.0f;
    mode3.amplitude = 0.5f;
    mode3.baseAmplitude = 0.5f;
    mode3.decay = 1.2f;
    mode3.modeIndex = 2.0f;
    mode3.materialFactor = mode1.materialFactor;
    mode3.sr = sr;
    modes_.push_back(mode3);

    // Mode 3
    ModalFilter mode4;
    mode4.frequency = 400.0f;
    mode4.amplitude = 0.4f;
    mode4.baseAmplitude = 0.4f;
    mode4.decay = 0.8f;
    mode4.modeIndex = 3.0f;
    mode4.materialFactor = mode1.materialFactor;
    mode4.sr = sr;
    modes_.push_back(mode4);

    // Mode 4
    ModalFilter mode5;
    mode5.frequency = 580.0f;
    mode5.amplitude = 0.3f;
    mode5.baseAmplitude = 0.3f;
    mode5.decay = 0.6f;
    mode5.modeIndex = 4.0f;
    mode5.materialFactor = mode1.materialFactor;
    mode5.sr = sr;
    modes_.push_back(mode5);

    // Mode 5
    ModalFilter mode6;
    mode6.frequency = 850.0f;
    mode6.amplitude = 0.2f;
    mode6.baseAmplitude = 0.2f;
    mode6.decay = 0.4f;
    mode6.modeIndex = 5.0f;
    mode6.materialFactor = mode1.materialFactor;
    mode6.sr = sr;
    modes_.push_back(mode6);

    // Mode 6
    ModalFilter mode7;
    mode7.frequency = 1200.0f;
    mode7.amplitude = 0.15f;
    mode7.baseAmplitude = 0.15f;
    mode7.decay = 0.3f;
    mode7.modeIndex = 6.0f;
    mode7.materialFactor = mode1.materialFactor;
    mode7.sr = sr;
    modes_.push_back(mode7);

    // Mode 7
    ModalFilter mode8;
    mode8.frequency = 1800.0f;
    mode8.amplitude = 0.1f;
    mode8.baseAmplitude = 0.1f;
    mode8.decay = 0.2f;
    mode8.modeIndex = 7.0f;
    mode8.materialFactor = mode1.materialFactor;
    mode8.sr = sr;
    modes_.push_back(mode8);

    // Prepare all modes (this will compute Q values)
    for (auto& mode : modes_)
        mode.prepare(sr);
}

void ModalBodyResonator::loadPianoBodyPreset()
{
    modes_.clear();

    // Piano soundboard modes (more resonant, higher Q)
    std::vector<float> pianoFrequencies = {85.0f, 165.0f, 250.0f, 380.0f, 550.0f, 800.0f, 1150.0f, 1700.0f};
    std::vector<float> pianoAmplitudes = {0.9f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.15f};
    std::vector<float> pianoDecays = {3.0f, 2.5f, 2.0f, 1.5f, 1.2f, 0.9f, 0.7f, 0.5f};

    for (size_t i = 0; i < pianoFrequencies.size(); ++i)
    {
        ModalFilter mode;
        mode.frequency = pianoFrequencies[i];
        mode.amplitude = pianoAmplitudes[i];
        mode.baseAmplitude = pianoAmplitudes[i];
        mode.decay = pianoDecays[i];
        mode.modeIndex = static_cast<float>(i);
        mode.materialFactor = 1.3f;  // Hard wood for piano
        mode.sr = sr;
        modes_.push_back(mode);
    }

    for (auto& mode : modes_)
        mode.prepare(sr);
}

void ModalBodyResonator::loadOrchestralStringPreset()
{
    modes_.clear();

    // Orchestral string body modes (very resonant, metallic)
    std::vector<float> stringFrequencies = {110.0f, 220.0f, 350.0f, 520.0f, 750.0f, 1100.0f, 1600.0f, 2400.0f};
    std::vector<float> stringAmplitudes = {1.0f, 0.8f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.15f};
    std::vector<float> stringDecays = {4.0f, 3.5f, 3.0f, 2.5f, 2.0f, 1.5f, 1.0f, 0.8f};

    for (size_t i = 0; i < stringFrequencies.size(); ++i)
    {
        ModalFilter mode;
        mode.frequency = stringFrequencies[i];
        mode.amplitude = stringAmplitudes[i];
        mode.baseAmplitude = stringAmplitudes[i];
        mode.decay = stringDecays[i];
        mode.modeIndex = static_cast<float>(i);
        mode.materialFactor = 1.5f;  // Metal body for orchestral strings
        mode.sr = sr;
        modes_.push_back(mode);
    }

    for (auto& mode : modes_)
        mode.prepare(sr);
}

float ModalBodyResonator::getModeFrequency(int index) const
{
    if (index >= 0 && index < static_cast<int>(modes_.size()))
        return modes_[index].frequency;
    return 0.0f;
}

//==============================================================================
// ArticulationStateMachine Implementation
//==============================================================================

ArticulationStateMachine::ArticulationStateMachine()
{
    for (int i = 0; i < exciterBufferSize; ++i)
        exciterBuffer[i] = 0.0f;
}

void ArticulationStateMachine::prepare(double sampleRate) { sr = sampleRate; }

void ArticulationStateMachine::reset()
{
    currentState_ = ArticulationState::IDLE;
    previousState_ = ArticulationState::IDLE;
    crossfadeProgress = 1.0;
    stateTimer = 0.0;
    exciterIndex = 0;
    exciterLength = 0;
    exciterAmplitude = 0.0f;
    
    for (int i = 0; i < exciterBufferSize; ++i)
        exciterBuffer[i] = 0.0f;
}

float ArticulationStateMachine::randomFloat()
{
    seed_ = seed_ * 1664525u + 1013904223u;
    return (static_cast<float>(seed_ >> 16) / 65535.0f) * 2.0f - 1.0f;
}

void ArticulationStateMachine::triggerPluck(float velocity)
{
    static constexpr int pluckLength = 10;
    float noiseBurst[pluckLength] = {0.3f, 0.7f, 1.0f, 0.8f, 0.5f, 0.3f, 0.2f, 0.1f, 0.05f, 0.0f};
    float scaledVelocity = std::min(velocity * 1.5f, 1.0f);  // Boosted for normalization (was 0.8)
    
    for (int i = 0; i < pluckLength; ++i)
        exciterBuffer[i] = noiseBurst[i] * scaledVelocity;
    
    exciterLength = pluckLength;
    exciterIndex = 0;
    transitionTo(ArticulationState::ATTACK_PLUCK);
}

void ArticulationStateMachine::triggerBow(float velocity, float bowPressure)
{
    float bowNoise = randomFloat() * 0.5f * bowPressure * velocity;
    exciterBuffer[0] = bowNoise;
    exciterLength = 1;
    exciterIndex = 0;
    transitionTo(ArticulationState::SUSTAIN_BOW);
}

void ArticulationStateMachine::triggerScrape(float velocity)
{
    static constexpr int scrapeLength = 20;
    for (int i = 0; i < scrapeLength; ++i)
    {
        float scrapeNoise = randomFloat();
        exciterBuffer[i] = scrapeNoise * 0.8f * velocity * (1.0f - static_cast<float>(i) / scrapeLength);
    }
    exciterLength = scrapeLength;
    exciterIndex = 0;
    transitionTo(ArticulationState::ATTACK_PLUCK);
}

void ArticulationStateMachine::triggerHarmonic(float velocity)
{
    static constexpr int harmonicLength = 100;
    float harmonicFreq = 440.0f * 2.0f;
    
    for (int i = 0; i < harmonicLength; ++i)
    {
        float phase = static_cast<float>(i) / static_cast<float>(sr);
        // Use LookupTables for sine calculation
        exciterBuffer[i] = SchillingerEcosystem::DSP::fastSineLookup(harmonicFreq * phase * 2.0f * 3.14159265359f) * velocity;
    }
    exciterLength = harmonicLength;
    exciterIndex = 0;
    transitionTo(ArticulationState::SUSTAIN_BOW);
}

void ArticulationStateMachine::triggerDamp()
{
    transitionTo(ArticulationState::RELEASE_DAMP);
}

void ArticulationStateMachine::transitionTo(ArticulationState newState)
{
    if (newState == currentState_)
        return;
    
    previousState_ = currentState_;
    currentState_ = newState;
    crossfadeProgress = 0.0;
    stateTimer = 0.0;
}

void ArticulationStateMachine::update(float deltaTime)
{
    stateTimer += deltaTime;
    crossfadeProgress = std::min(stateTimer / crossfadeTime, 1.0);
    
    switch (currentState_)
    {
        case ArticulationState::ATTACK_PLUCK:
            if (stateTimer > 0.05)
                transitionTo(ArticulationState::DECAY);
            break;
        case ArticulationState::DECAY:
            if (stateTimer > 1.0)
                transitionTo(ArticulationState::RELEASE_GHOST);
            break;
        case ArticulationState::SUSTAIN_BOW:
            if (exciterLength == 1)
                exciterBuffer[0] = randomFloat() * exciterAmplitude;
            break;
        case ArticulationState::RELEASE_GHOST:
            if (stateTimer > 2.0)
                transitionTo(ArticulationState::IDLE);
            break;
        case ArticulationState::RELEASE_DAMP:
            if (stateTimer > 0.3)
                transitionTo(ArticulationState::IDLE);
            break;
        case ArticulationState::IDLE:
            break;
    }
}

float ArticulationStateMachine::getPreviousGain() const
{
    // Use LookupTables for cosine calculation
    return SchillingerEcosystem::DSP::fastCosineLookup(static_cast<float>(crossfadeProgress) * 1.570796327f);
}

float ArticulationStateMachine::getCurrentGain() const
{
    // Use LookupTables for sine calculation
    return SchillingerEcosystem::DSP::fastSineLookup(static_cast<float>(crossfadeProgress) * 1.570796327f);
}

float ArticulationStateMachine::getCurrentExcitation()
{
    if (exciterIndex >= exciterLength)
        return 0.0f;
    
    float sample = exciterBuffer[exciterIndex];
    exciterIndex++;
    return sample;
}

//==============================================================================
// SharedBridgeCoupling Implementation
//==============================================================================

SharedBridgeCoupling::SharedBridgeCoupling() = default;

void SharedBridgeCoupling::prepare(double sampleRate, int numVoices)
{
    sr = sampleRate;
    bridgeEnergies_.resize(numVoices, 0.0f);
}

void SharedBridgeCoupling::reset()
{
    std::fill(bridgeEnergies_.begin(), bridgeEnergies_.end(), 0.0f);
    totalBridgeMotion_ = 0.0f;
}

float SharedBridgeCoupling::addStringEnergy(float stringEnergy, int voiceIndex)
{
    if (voiceIndex >= 0 && voiceIndex < static_cast<int>(bridgeEnergies_.size()))
        bridgeEnergies_[voiceIndex] = stringEnergy;
    
    float total = 0.0f;
    for (float energy : bridgeEnergies_)
        total += energy;
    
    totalBridgeMotion_ = std::tanh(total * 0.3f);
    
    float reflected = stringEnergy - totalBridgeMotion_;
    return reflected;
}

float SharedBridgeCoupling::getBridgeMotion() const
{
    return totalBridgeMotion_;
}

//==============================================================================
// SympatheticStringBank Implementation
//==============================================================================

SympatheticStringBank::SympatheticStringBank() = default;

void SympatheticStringBank::prepare(double sampleRate, const SympatheticStringConfig& config)
{
    sr = sampleRate;
    enabled_ = config.enabled;
    
    if (!enabled_)
        return;
    
    strings_.clear();
    strings_.resize(config.numStrings);
    
    for (auto& string : strings_)
        string.prepare(sampleRate);
}

void SympatheticStringBank::reset()
{
    for (auto& string : strings_)
        string.reset();
}

void SympatheticStringBank::exciteFromBridge(float bridgeEnergy)
{
    if (!enabled_)
        return;
    
    float exciter[2] = {bridgeEnergy, 0.0f};
    for (auto& string : strings_)
        string.excite(exciter, 2, 0.1f);
}

float SympatheticStringBank::processSample()
{
    if (!enabled_)
        return 0.0f;
    
    float output = 0.0f;
    for (auto& string : strings_)
        output += string.processSample();
    
    return output * 0.3f / static_cast<float>(strings_.size());
}

//==============================================================================
// AetherVoice Implementation
//==============================================================================

void AetherVoice::prepare(double sampleRate)
{
    string.prepare(sampleRate);
    bridge.prepare(sampleRate);
    body.prepare(sampleRate);
    fsm.prepare(sampleRate);
    // CRITICAL FIX: Don't load guitar preset here - let applyVoiceParameters() handle it
    // body.loadGuitarBodyPreset();  // REMOVED: Was overriding preset body settings
    string.setFrequency(440.0f);
}

void AetherVoice::noteOn(int note, float velocity)
{
    currentNote = note;
    currentVelocity = velocity;
    age = 0.0f;

    // CRITICAL FIX: Reset the waveguide to clear energy from previous notes
    // This prevents muddy sound when changing notes
    string.reset();

    // Use LookupTables for MIDI to frequency conversion
    float frequency = SchillingerEcosystem::DSP::LookupTables::getInstance().midiToFreq(static_cast<float>(note));

    string.setFrequency(frequency);

    // CRITICAL FIX: Create excitation signal and inject it into the string
    static constexpr int exciterLength = 10;
    float noiseBurst[exciterLength] = {0.3f, 0.7f, 1.0f, 0.8f, 0.5f, 0.3f, 0.2f, 0.1f, 0.05f, 0.0f};
    float scaledVelocity = std::min(velocity * 1.5f, 1.0f);
    string.excite(noiseBurst, exciterLength, scaledVelocity);

    fsm.triggerPluck(velocity);
    isActive = true;
}

void AetherVoice::noteOff()
{
    fsm.triggerDamp();
}

void AetherVoice::processBlock(float* output, int numSamples, double sampleRate)
{
    if (!isActive)
    {
        std::fill(output, output + numSamples, 0.0f);
        return;
    }
    
    for (int i = 0; i < numSamples; ++i)
    {
        float excitation = fsm.getCurrentExcitation();
        float stringOut = string.processSample();
        
        float processed;
        if (sharedBridge != nullptr)
        {
            int voiceIndex = 0;
            float reflected = sharedBridge->addStringEnergy(stringOut + excitation, voiceIndex);
            stringOut = reflected;
            
            float bridgeEnergy = sharedBridge->getBridgeMotion();
            float bodyOut = body.processSample(bridgeEnergy);
            
            float sympOut = 0.0f;
            if (sympatheticStrings != nullptr)
            {
                if (i == 0)
                    sympatheticStrings->exciteFromBridge(bridgeEnergy);
                sympOut = sympatheticStrings->processSample();
            }
            
            processed = (pedalboard != nullptr) ? pedalboard->processSample(bodyOut + sympOut * 0.3f) : (bodyOut + sympOut * 0.3f);
        }
        else
        {
            float bridgeEnergy = bridge.processString(stringOut + excitation);
            float bodyOut = body.processSample(bridgeEnergy);
            processed = (pedalboard != nullptr) ? pedalboard->processSample(bodyOut) : bodyOut;
        }
        
        fsm.update(1.0f / sampleRate);
        
        float outputPrev = processed * fsm.getPreviousGain();
        float outputCurr = processed * fsm.getCurrentGain();
        output[i] = outputPrev + outputCurr;
        
        age += 1.0f / sampleRate;
        
        if (fsm.getCurrentState() == ArticulationState::IDLE)
            isActive = false;
    }
}

//==============================================================================
// AetherVoiceManager Implementation
//==============================================================================

AetherVoiceManager::AetherVoiceManager() = default;

void AetherVoiceManager::prepare(double sampleRate, int samplesPerBlock)
{
    for (auto& voice : voices_)
    {
        voice.string.prepare(sampleRate);
        voice.bridge.prepare(sampleRate);
        voice.body.prepare(sampleRate);
        voice.fsm.prepare(sampleRate);
        // CRITICAL FIX: Don't load guitar preset here - let applyVoiceParameters() handle it
        // voice.body.loadGuitarBodyPreset();  // REMOVED: Was overriding preset body settings
        voice.string.setFrequency(440.0f);
    }
}

void AetherVoiceManager::reset()
{
    for (auto& voice : voices_)
    {
        voice.string.reset();  // CRITICAL FIX: Reset string to clear delay lines and filters
        voice.bridge.reset();
        voice.body.reset();
        voice.fsm.reset();
        voice.isActive = false;
    }
}

AetherVoice* AetherVoiceManager::findFreeVoice()
{
    for (auto& voice : voices_)
    {
        if (!voice.isActive)
            return &voice;
    }
    
    AetherVoice* oldest = &voices_[0];
    for (auto& voice : voices_)
    {
        if (voice.age > oldest->age)
            oldest = &voice;
    }
    
    oldest->noteOff();
    return oldest;
}

AetherVoice* AetherVoiceManager::findVoiceForNote(int note)
{
    for (auto& voice : voices_)
    {
        if (voice.isActive && voice.currentNote == note)
            return &voice;
    }
    return nullptr;
}

void AetherVoiceManager::handleNoteOn(int note, float velocity)
{
    AetherVoice* voice = findVoiceForNote(note);
    if (voice)
    {
        voice->noteOn(note, velocity);
    }
    else
    {
        voice = findFreeVoice();
        voice->noteOn(note, velocity);
    }
}

void AetherVoiceManager::handleNoteOff(int note)
{
    AetherVoice* voice = findVoiceForNote(note);
    if (voice)
        voice->noteOff();
}

void AetherVoiceManager::allNotesOff()
{
    for (auto& voice : voices_)
    {
        if (voice.isActive)
            voice.noteOff();
    }
}

void AetherVoiceManager::processBlock(float* output, int numSamples, double sampleRate)
{
    std::fill(output, output + numSamples, 0.0f);
    
    float temp[6][512];
    
    for (int v = 0; v < 6; ++v)
    {
        if (voices_[v].isActive)
        {
            voices_[v].processBlock(temp[v], numSamples, sampleRate);
            
            for (int i = 0; i < numSamples; ++i)
                output[i] += temp[v][i];
        }
    }
    
    int activeCount = getActiveVoiceCount();
    if (activeCount > 0)
    {
        float normalization = 1.5f / std::sqrt(static_cast<float>(activeCount));  // Boosted 1.5x for normalization
        for (int i = 0; i < numSamples; ++i)
            output[i] *= normalization;
    }
}

int AetherVoiceManager::getActiveVoiceCount() const
{
    int count = 0;
    for (const auto& voice : voices_)
        if (voice.isActive) count++;
    return count;
}

void AetherVoiceManager::enableSharedBridge(bool enabled)
{
    if (enabled)
    {
        if (!sharedBridge_)
        {
            sharedBridge_ = std::make_unique<SharedBridgeCoupling>();
            sharedBridge_->prepare(48000.0, 6);
            
            for (auto& voice : voices_)
                voice.sharedBridge = sharedBridge_.get();
        }
    }
    else
    {
        for (auto& voice : voices_)
            voice.sharedBridge = nullptr;
        sharedBridge_.reset();
    }
}

void AetherVoiceManager::enableSympatheticStrings(const SympatheticStringBank::SympatheticStringConfig& config)
{
    if (!config.enabled)
    {
        for (auto& voice : voices_)
            voice.sympatheticStrings = nullptr;
        sympatheticStrings_.reset();
        return;
    }
    
    if (!sympatheticStrings_)
        sympatheticStrings_ = std::make_unique<SympatheticStringBank>();
    
    sympatheticStrings_->prepare(48000.0, config);
    
    for (auto& voice : voices_)
        voice.sympatheticStrings = sympatheticStrings_.get();
}

//==============================================================================
// RATDistortion Implementation
//==============================================================================

RATDistortion::RATDistortion()
{
    threshold = 0.7f;
    asymmetry = 1.0f;
}

void RATDistortion::prepare(double sampleRate)
{
    sr = sampleRate;
    preFilter_.prepare(sampleRate);
    preFilter_.setType(TPTFilter::Type::lowpass);
    preFilter_.setCutoffFrequency(4000.0f);
    
    toneFilter_.prepare(sampleRate);
    toneFilter_.setType(TPTFilter::Type::lowpass);
    toneFilter_.setCutoffFrequency(1000.0f);
}

void RATDistortion::reset()
{
    preFilter_.reset();
    toneFilter_.reset();
}

void RATDistortion::setDiodeType(DiodeType type)
{
    diodeType_ = type;
    
    switch (type)
    {
        case DiodeType::Silicon:
            threshold = 0.7f;
            asymmetry = 1.0f;
            break;
        case DiodeType::Germanium:
            threshold = 0.3f;
            asymmetry = 1.2f;
            break;
        case DiodeType::LED:
            threshold = 1.5f;
            asymmetry = 1.0f;
            break;
    }
}

float RATDistortion::processSample(float input)
{
    float filtered = preFilter_.processSample(input);
    float driven = filtered * drive;
    
    float sign = (driven >= 0.0f) ? 1.0f : -1.0f;
    float absIn = std::abs(driven);
    
    float clipped;
    if (absIn < threshold)
    {
        clipped = absIn;
    }
    else
    {
        float excess = absIn - threshold;
        clipped = threshold + std::tanh(excess * asymmetry) * 0.3f;
    }
    
    clipped *= sign;
    
    float cutoff = 200.0f + std::pow(filter, 0.3f) * 4800.0f;
    toneFilter_.setCutoffFrequency(cutoff);
    
    float toneFiltered = toneFilter_.processSample(clipped);
    return toneFiltered * output;
}

//==============================================================================
// Pedal Implementation
//==============================================================================

void Pedal::prepare(double sampleRate)
{
    rat.prepare(sampleRate);
}

float Pedal::processSample(float input)
{
    if (!enabled || type == PedalType::Bypass)
        return input;
    
    float wet = input;
    
    switch (type)
    {
        case PedalType::Overdrive:
            {
                float driveAmount = 1.0f + param1 * 4.0f;
                wet = std::tanh(input * driveAmount) * 0.8f;
            }
            break;
        case PedalType::Distortion:
            {
                float driveAmount = 1.0f + param1 * 9.0f;
                float driven = input * driveAmount;
                wet = std::max(-1.0f, std::min(1.0f, driven));
            }
            break;
        case PedalType::RAT:
            rat.drive = 1.0f + param1 * 9.0f;
            rat.filter = param2;
            wet = rat.processSample(input);
            break;
        default:
            wet = input;
            break;
    }
    
    return input * (1.0f - mix) + wet * mix;
}

//==============================================================================
// Pedalboard Implementation
//==============================================================================

Pedalboard::Pedalboard() = default;

void Pedalboard::prepare(double sampleRate, int samplesPerBlock)
{
    for (auto& pedal : pedals_)
        pedal.prepare(sampleRate);
}

void Pedalboard::reset() {}

float Pedalboard::processSample(float input)
{
    if (parallelMode_)
    {
        float output = 0.0f;
        int activeCount = 0;
        
        for (auto& pedal : pedals_)
        {
            if (pedal.enabled)
            {
                output += pedal.processSample(input);
                activeCount++;
            }
        }
        
        if (activeCount > 0)
            output /= std::sqrt(static_cast<float>(activeCount));
        else
            output = input;
        
        return output;
    }
    else
    {
        float output = input;
        for (int index : routingOrder_)
        {
            if (index >= 0 && index < 8)
            {
                auto& pedal = pedals_[index];
                if (pedal.enabled)
                    output = pedal.processSample(output);
            }
        }
        return output;
    }
}

void Pedalboard::setPedal(int index, PedalType type, bool enable)
{
    if (index >= 0 && index < 8)
    {
        pedals_[index].type = type;
        pedals_[index].enabled = enable;
    }
}

void Pedalboard::setRouting(int index, int pedalIndex)
{
    if (index >= 0 && index < 8 && pedalIndex >= 0 && pedalIndex < 8)
        routingOrder_[index] = pedalIndex;
}

//==============================================================================
// Main AetherPureDSP Implementation
//==============================================================================

AetherPureDSP::AetherPureDSP()
{
    voiceManager_.prepare(48000.0, 512);
    pedalboard_.prepare(48000.0, 512);
}

AetherPureDSP::~AetherPureDSP() = default;

bool AetherPureDSP::prepare(double sampleRate, int blockSize)
{
    sampleRate_ = sampleRate;
    blockSize_ = blockSize;
    
    voiceManager_.prepare(sampleRate, blockSize);
    pedalboard_.prepare(sampleRate, blockSize);
    
    return true;
}

void AetherPureDSP::reset()
{
    voiceManager_.reset();
}

void AetherPureDSP::process(float** outputs, int numChannels, int numSamples)
{
    // Clear all outputs
    for (int ch = 0; ch < numChannels; ++ch)
        std::fill(outputs[ch], outputs[ch] + numSamples, 0.0f);

    // Process voices (mono output) using real-time safe stack buffer
    // Assert for safety in debug builds - block size should never exceed MAX_BLOCK_SIZE
    assert(numSamples <= MAX_BLOCK_SIZE && "Block size exceeds maximum buffer size");

    voiceManager_.processBlock(tempBuffer_, numSamples, sampleRate_);

    // Copy to all channels
    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int i = 0; i < numSamples; ++i)
            outputs[ch][i] = tempBuffer_[i] * params_.masterVolume;
    }
}

void AetherPureDSP::handleEvent(const ScheduledEvent& event)
{
    if (event.type == ScheduledEvent::NOTE_ON)
    {
        voiceManager_.handleNoteOn(event.data.note.midiNote, event.data.note.velocity);
    }
    else if (event.type == ScheduledEvent::NOTE_OFF)
    {
        voiceManager_.handleNoteOff(event.data.note.midiNote);
    }
}

float AetherPureDSP::getParameter(const char* paramId) const
{
    std::string id(paramId);

    if (id == "masterVolume") return static_cast<float>(params_.masterVolume);
    if (id == "damping") return static_cast<float>(params_.damping);
    if (id == "brightness") return static_cast<float>(params_.brightness);
    if (id == "stiffness") return static_cast<float>(params_.stiffness);
    if (id == "dispersion") return static_cast<float>(params_.dispersion);
    if (id == "sympatheticCoupling") return static_cast<float>(params_.sympatheticCoupling);
    if (id == "material") return static_cast<float>(params_.material);
    if (id == "bodyPreset") return static_cast<float>(params_.bodyPreset);

    return 0.0f;
}

void AetherPureDSP::setParameter(const char* paramId, float value)
{
    std::string id(paramId);

    // Get old value for logging (before change)
    float oldValue = getParameter(paramId);

    if (id == "masterVolume") params_.masterVolume = value;
    else if (id == "damping") params_.damping = value;
    else if (id == "brightness") params_.brightness = value;
    else if (id == "stiffness") params_.stiffness = value;
    else if (id == "dispersion") params_.dispersion = value;
    else if (id == "sympatheticCoupling") params_.sympatheticCoupling = value;
    else if (id == "material") params_.material = value;
    else if (id == "bodyPreset") params_.bodyPreset = static_cast<int>(value);

    // Log parameter change (shared telemetry infrastructure)
    LOG_PARAMETER_CHANGE("NatureAether", paramId, oldValue, value);

    applyParameters();
}

bool AetherPureDSP::savePreset(char* jsonBuffer, int jsonBufferSize) const
{
    int offset = 0;

    // Opening brace
    int written = snprintf(jsonBuffer + offset, jsonBufferSize - offset, "{");
    if (written < 0 || offset + written >= jsonBufferSize)
        return false;
    offset += written;

    writeJsonParameter("masterVolume", params_.masterVolume, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("damping", params_.damping, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("brightness", params_.brightness, jsonBuffer, offset, jsonBufferSize);
    writeJsonParameter("stiffness", params_.stiffness, jsonBuffer, offset, jsonBufferSize);

    // Remove trailing comma and add closing brace
    if (offset > 1 && jsonBuffer[offset - 1] == ',')
    {
        offset--;
        jsonBuffer[offset] = '}';
        jsonBuffer[offset + 1] = '\0';
    }

    return true;
}

bool AetherPureDSP::isUPFSFormat(const char* jsonData) const
{
    // Simple check for UPFS format by looking for required fields
    // Check if "format" field exists and has value "UPFS"
    const char* formatPos = strstr(jsonData, "\"format\"");
    if (!formatPos)
        return false;

    // Look for "UPFS" value after format field
    const char* upfsPos = strstr(formatPos, "UPFS");
    if (!upfsPos)
        return false;

    // Check for version field (should be "1.0.0" for v1.0)
    const char* versionPos = strstr(jsonData, "\"version\"");
    if (!versionPos)
        return false;

    // Check if this is version 1.0.0
    const char* version100Pos = strstr(versionPos, "1.0.0");
    if (!version100Pos)
        return false;

    // Verify it's relatively close (within 100 chars)
    if (version100Pos - versionPos > 100)
        return false;

    return true;
}

bool AetherPureDSP::loadUPFSPreset(const char* jsonData)
{
    // UPFS v1.0 parameter mapping for Aether
    // In UPFS v1.0 for Aether, parameters are directly in "parameters" object
    // We need to map UPFS parameter names to Aether DSP parameter names

    double value;

    // Map exciter parameters to DSP parameters
    if (parseJsonParameter(jsonData, "exciter_noise_color", value))
        params_.brightness = value;  // Maps to brightness

    if (parseJsonParameter(jsonData, "exciter_gain", value))
        params_.attackVelocity = value;

    if (parseJsonParameter(jsonData, "exciter_attack", value))
        params_.attackVelocity = value * 0.8;

    if (parseJsonParameter(jsonData, "exciter_decay", value))
        params_.damping = 0.9 + (value * 0.099);

    if (parseJsonParameter(jsonData, "exciter_sustain", value))
        params_.bridgeCoupling = value;

    if (parseJsonParameter(jsonData, "exciter_release", value))
        params_.damping = std::max(0.9, params_.damping * (1.0 - (value * 0.01)));

    // Map resonator parameters to DSP parameters
    if (parseJsonParameter(jsonData, "resonator_brightness", value))
        params_.brightness = value;

    if (parseJsonParameter(jsonData, "resonator_decay", value))
        params_.damping = 0.9 + (value * 0.099);

    if (parseJsonParameter(jsonData, "resonator_mode_count", value))
        params_.stiffness = value / 128.0;  // Maps to stiffness

    // Map feedback parameters to DSP parameters
    if (parseJsonParameter(jsonData, "feedback_amount", value))
        params_.bridgeCoupling = value;

    if (parseJsonParameter(jsonData, "feedback_saturation", value))
        params_.nonlinearity = value / 3.0;  // Scale 0-3 to 0-1

    if (parseJsonParameter(jsonData, "feedback_mix", value))
        params_.bridgeCoupling = value;

    // Map filter parameters to DSP parameters
    if (parseJsonParameter(jsonData, "filter_cutoff", value))
        params_.brightness = value;

    if (parseJsonParameter(jsonData, "filter_resonance", value))
        params_.damping = 1.0 - (value * 0.1);  // Inverse mapping

    // Map amplitude envelope parameters to DSP parameters
    if (parseJsonParameter(jsonData, "amp_attack", value))
        params_.attackVelocity = value;

    if (parseJsonParameter(jsonData, "amp_decay", value))
    {
        double ampDamping = 0.9 + (value * 0.099);
        params_.damping = (params_.damping + ampDamping) / 2.0;
    }

    if (parseJsonParameter(jsonData, "amp_sustain", value))
        params_.bridgeCoupling = value;

    if (parseJsonParameter(jsonData, "amp_release", value))
        params_.damping = std::max(0.9, params_.damping * (1.0 - (value * 0.01)));

    // Direct parameter mappings (these override the calculated values)
    if (parseJsonParameter(jsonData, "masterVolume", value))
        params_.masterVolume = value;

    if (parseJsonParameter(jsonData, "damping", value))
        params_.damping = value;

    if (parseJsonParameter(jsonData, "brightness", value))
        params_.brightness = value;

    if (parseJsonParameter(jsonData, "stiffness", value))
        params_.stiffness = value;

    if (parseJsonParameter(jsonData, "dispersion", value))
        params_.dispersion = value;

    if (parseJsonParameter(jsonData, "sympatheticCoupling", value))
        params_.sympatheticCoupling = value;

    if (parseJsonParameter(jsonData, "material", value))
        params_.material = value;

    if (parseJsonParameter(jsonData, "bodyPreset", value))
        params_.bodyPreset = static_cast<int>(value);

    if (parseJsonParameter(jsonData, "bridgeCoupling", value))
        params_.bridgeCoupling = value;

    if (parseJsonParameter(jsonData, "nonlinearity", value))
        params_.nonlinearity = value;

    applyParameters();
    return true;
}

bool AetherPureDSP::loadPreset(const char* jsonData)
{
    // Check for UPFS v1.0 format
    if (isUPFSFormat(jsonData))
    {
        return loadUPFSPreset(jsonData);
    }

    // Legacy format support (original implementation)
    double value;
    int paramsFound = 0;

    // Initialize with default values
    double baseDamping = 0.996;
    double baseBrightness = 0.5;
    double baseStiffness = 0.0;
    double baseBridgeCoupling = 0.6;
    double baseNonlinearity = 0.1;

    // Excitation parameters (mapped to attackVelocity and bowPressure)
    if (parseJsonParameter(jsonData, "exciter_gain", value)) {
        params_.attackVelocity = value;
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "exciter_attack", value)) {
        params_.attackVelocity = value * 0.8;  // Scale to appropriate range
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "exciter_noise_color", value)) {
        baseBrightness = value;  // Map noise color to brightness
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "exciter_decay", value)) {
        // CRITICAL FIX: Combine with base damping instead of overwriting
        baseDamping = 0.9 + (value * 0.099);
        paramsFound++;
    }

    // Resonator parameters (mapped to damping, brightness, stiffness)
    if (parseJsonParameter(jsonData, "resonator_brightness", value)) {
        baseBrightness = value;
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "resonator_decay", value)) {
        // CRITICAL FIX: Combine with base damping instead of overwriting
        baseDamping = 0.9 + (value * 0.099);
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "resonator_mode_count", value)) {
        // Mode count affects stiffness (more modes = stiffer)
        baseStiffness = value / 128.0;  // Scale 0-64 to 0-0.5
        paramsFound++;
    }

    // Feedback parameters (mapped to nonlinearity and bridgeCoupling)
    if (parseJsonParameter(jsonData, "feedback_amount", value)) {
        baseBridgeCoupling = value;  // Feedback affects bridge coupling
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "feedback_saturation", value)) {
        // CRITICAL FIX: Scale 0-3 preset range to 0-1 DSP range
        baseNonlinearity = value / 3.0;  // Map preset saturation to DSP nonlinearity
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "feedback_mix", value)) {
        baseBridgeCoupling = value;  // Mix affects coupling
        paramsFound++;
    }

    // Filter parameters (mapped to brightness and damping)
    if (parseJsonParameter(jsonData, "filter_cutoff", value)) {
        baseBrightness = value;  // Cutoff affects brightness
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "filter_resonance", value)) {
        // CRITICAL FIX: Inverse mapping to 0.9-0.999 range
        baseDamping = 1.0 - (value * 0.1);  // Map preset resonance to DSP damping (inverse)
        paramsFound++;
    }

    // Amplitude envelope parameters (mapped to attackVelocity)
    if (parseJsonParameter(jsonData, "amp_attack", value)) {
        params_.attackVelocity = value;  // Attack affects velocity
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "amp_decay", value)) {
        // CRITICAL FIX: Combine with base damping instead of overwriting
        double ampDamping = 0.9 + (value * 0.099);
        baseDamping = (baseDamping + ampDamping) / 2.0;  // Average them
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "amp_sustain", value)) {
        baseBridgeCoupling = value;  // Sustain affects coupling
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "amp_release", value)) {
        // CRITICAL FIX: Don't multiply, instead reduce damping slightly based on release
        // Longer release = slightly more damping to prevent infinite sustain
        baseDamping = std::max(0.9, baseDamping * (1.0 - (value * 0.01)));
        paramsFound++;
    }

    // Apply combined base values to params_
    params_.damping = baseDamping;
    params_.brightness = baseBrightness;
    params_.stiffness = baseStiffness;
    params_.bridgeCoupling = baseBridgeCoupling;
    params_.nonlinearity = baseNonlinearity;

    // Direct parameter mappings (names match) - these override the calculated values
    if (parseJsonParameter(jsonData, "masterVolume", value)) {
        params_.masterVolume = value;
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "damping", value)) {
        params_.damping = value;
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "brightness", value)) {
        params_.brightness = value;
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "stiffness", value)) {
        params_.stiffness = value;
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "dispersion", value)) {
        params_.dispersion = value;
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "sympatheticCoupling", value)) {
        params_.sympatheticCoupling = value;
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "material", value)) {
        params_.material = value;
        paramsFound++;
    }
    if (parseJsonParameter(jsonData, "bodyPreset", value)) {
        params_.bodyPreset = static_cast<int>(value);
        paramsFound++;
    }

    applyParameters();
    return true;
}

int AetherPureDSP::getActiveVoiceCount() const
{
    return voiceManager_.getActiveVoiceCount();
}

void AetherPureDSP::enableSharedBridge(bool enabled)
{
    voiceManager_.enableSharedBridge(enabled);
}

void AetherPureDSP::enableSympatheticStrings(bool enabled)
{
    SympatheticStringBank::SympatheticStringConfig config;
    config.enabled = enabled;
    voiceManager_.enableSympatheticStrings(config);
}

void AetherPureDSP::setPedal(int index, PedalType type, bool enable)
{
    pedalboard_.setPedal(index, type, enable);
}

void AetherPureDSP::applyParameters()
{
    // Apply loaded parameters to all voices via the voice manager
    voiceManager_.applyVoiceParameters(*this);
}

void AetherVoiceManager::applyVoiceParameters(const AetherPureDSP& dsp)
{
    // Apply loaded parameters to all voices in the voice manager
    // This ensures that preset-loaded parameters actually affect the DSP

    for (auto& voice : voices_)
    {
        // Apply string parameters using direct access to public params_
        voice.string.setDamping(static_cast<float>(dsp.params_.damping));
        voice.string.setStiffness(static_cast<float>(dsp.params_.stiffness));
        voice.string.setBrightness(static_cast<float>(dsp.params_.brightness));
        voice.string.setBridgeCoupling(static_cast<float>(dsp.params_.bridgeCoupling));
        voice.string.setNonlinearity(static_cast<float>(dsp.params_.nonlinearity));
        voice.string.setDispersion(static_cast<float>(dsp.params_.dispersion));
        voice.string.setSympatheticCoupling(static_cast<float>(dsp.params_.sympatheticCoupling));

        // Apply body resonator parameters
        voice.body.setResonance(static_cast<float>(dsp.params_.bodyResonance));

        // Apply material type
        ModalBodyResonator::MaterialType materialType =
            static_cast<ModalBodyResonator::MaterialType>(static_cast<int>(dsp.params_.material));
        voice.body.setMaterial(materialType);

        // Load appropriate body preset
        switch (dsp.params_.bodyPreset)
        {
            case 0:
                voice.body.loadGuitarBodyPreset();
                break;
            case 1:
                voice.body.loadPianoBodyPreset();
                break;
            case 2:
                voice.body.loadOrchestralStringPreset();
                break;
            default:
                voice.body.loadGuitarBodyPreset();
                break;
        }

        // Recalculate mode Q values based on new damping and material
        voice.body.recalculateModeQ(static_cast<float>(dsp.params_.damping), 1.0);
    }
}

float AetherPureDSP::softClip(float x) const
{
    return std::tanh(x);
}

bool AetherPureDSP::writeJsonParameter(const char* name, double value, char* buffer, int& offset, int bufferSize) const
{
    // Simple JSON writing (simplified)
    int written = snprintf(buffer + offset, bufferSize - offset, "\"%s\":%g,", name, value);
    if (written < 0 || offset + written >= bufferSize)
        return false;
    offset += written;
    return true;
}

bool AetherPureDSP::parseJsonParameter(const char* json, const char* param, double& value) const
{
    // Handle nested JSON structure where parameters are inside "parameters": { ... }
    const char* parametersStart = strstr(json, "\"parameters\":");
    if (!parametersStart)
    {
        // Fall back to top-level search for backward compatibility
        std::string search = "\"";
        search += param;
        search += "\":";

        const char* pos = strstr(json, search.c_str());
        if (!pos)
            return false;

        pos += search.length();
        value = atof(pos);
        return true;
    }

    // Find the opening brace after "parameters":
    const char* braceStart = strchr(parametersStart, '{');
    if (!braceStart)
        return false;

    // Search for the parameter within the parameters object
    std::string search = "\"";
    search += param;
    search += "\":";

    const char* pos = strstr(braceStart, search.c_str());
    if (!pos)
        return false;

    // Move past the parameter name and colon
    pos += search.length();

    // Skip whitespace
    while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')
        pos++;

    // Parse the value (handle both numbers and booleans)
    if (strncmp(pos, "true", 4) == 0)
    {
        value = 1.0;
        return true;
    }
    else if (strncmp(pos, "false", 5) == 0)
    {
        value = 0.0;
        return true;
    }
    else
    {
        value = atof(pos);
        return true;
    }
}

//==============================================================================
// Static Factory (No runtime registration for tvOS hardening)
//==============================================================================

// Pure DSP instruments are instantiated directly, not through dynamic factory
// This ensures tvOS compatibility (no static initialization, no global state)

} // namespace DSP
