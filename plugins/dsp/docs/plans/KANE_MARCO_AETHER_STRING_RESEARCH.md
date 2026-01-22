# Aether String - Deep Research & Implementation Strategy

**Research Date:** 2025-12-25
**Project:** Nature Marco Instrument Family
**Instrument:** Aether String (Physical String Modeling with Pedalboard)
**Researcher:** DSP Engineer (Level 3 Deep Research)
**Status:** ✅ COMPLETE - Ready for Implementation

---

## Executive Summary

This document provides comprehensive Level 3 research for implementing **Aether String**, a physical string modeling synthesizer with:

- **Digital Waveguide String** using Karplus-Strong algorithm with bridge coupling
- **Articulation State Machine** (BOW, PICK, SCRAPE, HARMONIC, TREMOLO, NORMAL) with smooth crossfading
- **Bridge Coupling System** for string ↔ body energy transfer
- **Modal Body Resonator** (8-16 modes for guitar body simulation)
- **8-Pedal Pedalboard** (Compressor, Octave, OD, Distortion, RAT with switchable diodes, Phaser, Reverb)
- **Configurable Signal Routing** for pedal ordering

**Recommended Approach:** Hybrid Waveguide + Modal Body (Approach B)
**Implementation Complexity:** HIGH
**Estimated Implementation Time:** 80-120 hours
**CPU Budget:** Moderate (optimized for real-time guitar playing)

---

## Table of Contents

1. [Instrument Specifications](#1-instrument-specifications)
2. [Implementation Approaches Analysis](#2-implementation-approaches-analysis)
3. [Recommended Implementation Strategy](#3-recommended-implementation-strategy)
4. [String Modeling Deep Dive](#4-string-modeling-deep-dive)
5. [Bridge Coupling Implementation](#5-bridge-coupling-implementation)
6. [Articulation State Machine](#6-articulation-state-machine)
7. [Exciter Signal Generation](#7-exciter-signal-generation)
8. [Body Resonator Design](#8-body-resonator-design)
9. [Pedalboard Architecture](#9-pedalboard-architecture)
10. [TDD Implementation Plan](#10-tdd-implementation-plan)
11. [FFI Bridge Design](#11-ffi-bridge-design)
12. [Testing Strategy](#12-testing-strategy)
13. [File-by-File Implementation Checklist](#13-file-by-file-implementation-checklist)

---

## 1. Instrument Specifications

### 1.1 Core Architecture

```
Input (MIDI) → Exciter → String Model (Waveguide) → Bridge Coupling → Body Resonator → Pedalboard → Output
                         ↑                                    ↑
                    Articulation FSM                    Modal Synthesis
                    (6 states)                          (8-16 modes)
```

### 1.2 String Model

- **Algorithm:** Karplus-Strong extended with digital waveguide
- **Fundamental Range:** E2 (82.4 Hz) to E6 (1318.5 Hz) - guitar range
- **Polyphony:** 6 voices (guitar strings)
- **Tuning:** Real-time delay line length adjustment for pitch tracking

### 1.3 Articulation States

| State | Excitation Type | Character | Use Case |
|-------|----------------|-----------|----------|
| BOW | Sinusoidal sustained | Smooth, singing | Legato melodies |
| PICK | Impulse + decay | Plucky, percussive | Strumming, picking |
| SCRAPE | Filtered noise | Textural, raspy | Extended techniques |
| HARMONIC | High-freq emphasis | Bell-like, clear | Harmonic articulation |
| TREMOLO | Amplitude-modulated | Shaking, vibrato | Expression |
| NORMAL | Standard KS pluck | Balanced | Default string sound |

### 1.4 Pedalboard Chain

1. **Compressor** - Threshold, ratio (1:1 to 20:1), attack (0.1-100ms), release (10-1000ms)
2. **Octave** - -1 octave pitch shift (monophonic or polyphonic)
3. **Overdrive** - Soft clipping asymmetry, warm breakup
4. **Distortion** - Hard clipping symmetry, aggressive
5. **RAT** - Switchable diodes (Si/Ge/LED) + filter
6. **Phaser** - Rate (0.1-10Hz), depth (0-1), feedback (-1 to 1), mix (0-1)
7. **Reverb** - Room size (0-1), damping (0-1), decay (0.1-5s)
8. **User Selectable** - From palette of available effects

### 1.5 Performance Requirements

- **Latency:** < 10ms total (critical for guitar playing feel)
- **CPU:** < 20% on modern Apple Silicon (M1/M2)
- **Sample Rates:** 44.1k, 48k, 88.2k, 96k supported
- **Buffer Sizes:** 64-512 samples (optimized for 128-256)

---

## 2. Implementation Approaches Analysis

### Approach A: Digital Waveguide with Commuted Body Synthesis

**Description:** Use full digital waveguide string model with recorded body impulse responses (commuted synthesis).

**Architecture:**
```
String (Waveguide) → Bridge Filter → Convolution with Body IR → Pedalboard
```

**Pros:**
- Most authentic string sound (waveguide is physically accurate)
- Commuted body synthesis is computationally efficient
- Realistic body resonance from actual recordings
- Proven in research (Smith/Jaffe 1995, Karplus-Strong 1983)

**Cons:**
- Convolution is CPU-intensive (especially with long IRs)
- Requires high-quality body impulse recordings (Guitar body IRs)
- Limited flexibility (body response is "baked in")
- Complex delay line tuning for accurate pitch tracking
- Harder to TDD (IR validation is subjective)

**Implementation Complexity:** HIGH
**CPU Performance:** MEDIUM-HIGH (convolution cost)
**Authenticity:** VERY HIGH
**TDD Testability:** MEDIUM

**References:**
- Smith, J. "Physical Audio Signal Processing - Waveguide Synthesis" (CCRMA)
- Jaffe, D. & Smith, J. "Performance Expression in Commuted Waveguide Synthesis" (ICMC 1995)
- JUCE Tutorial: "Create a string model with delay lines"

---

### Approach B: Hybrid Waveguide + Modal Body Resonator (RECOMMENDED)

**Description:** Waveguide string model coupled to modal synthesis body resonator.

**Architecture:**
```
String (Waveguide) → Bridge Coupling → Modal Body Resonator → Pedalboard
                             ↑                     ↑
                       Energy Transfer         8-16 Resonant Modes
```

**Pros:**
- Waveguide provides authentic string behavior
- Modal body is CPU-efficient (8-16 biquad filters)
- Highly flexible (body modes adjustable in real-time)
- Easier to TDD (modal frequencies are measurable)
- No need for IR recordings
- Matches existing codebase patterns (LocalGalDSP uses modal filters)

**Cons:**
- Modal body requires careful tuning for realism
- Bridge coupling algorithm adds complexity
- More parameters to expose (body mode frequencies, decay times)
- Less "authentic" than commuted synthesis (but still very good)

**Implementation Complexity:** MEDIUM-HIGH
**CPU Performance:** MEDIUM (excellent for real-time)
**Authenticity:** HIGH
**TDD Testability:** HIGH

**References:**
- Desvages, C. "Physical Modelling of the Bowed String" (PhD Thesis 2018)
- "Circuit Based Classical Guitar Model" (ScienceDirect 2015)
- LEVEL2_RESEARCH_BEST_PRACTICES.md (existing codebase patterns)

---

### Approach C: Physical Modeling + ML-Based Articulation Transitions

**Description:** Waveguide string with machine-learned excitation signals and transition models.

**Architecture:**
```
String (Waveguide) → ML Exciter Generator → Bridge → Modal Body → Pedalboard
                      ↑
                 Trained on Real Instrument
                 Recordings (Bow, Pick, Scrape)
```

**Pros:**
- Most realistic articulation transitions (if trained well)
- Can capture subtle playing techniques
- Novel approach (research-worthy)

**Cons:**
- Requires large dataset of recorded articulations
- ML inference adds CPU overhead
- Training process is complex and time-consuming
- Harder to TDD (ML model validation is subjective)
- Risk of "uncanny valley" (almost real but not quite)
- Not production-ready without extensive R&D

**Implementation Complexity:** VERY HIGH
**CPU Performance:** MEDIUM (ML inference cost)
**Authenticity:** VARIABLE (depends on training data)
**TDD Testability:** LOW

**References:**
- "Physical Modelling with Machine Learning" (DAFx 2020)
- "Neural Audio Synthesis of Musical Notes" (ICML 2019)

---

## 3. Recommended Implementation Strategy

### 3.1 Approach Selection: **Approach B (Hybrid Waveguide + Modal Body)**

**Rationale:**

1. **Authenticity vs Complexity Balance:** Waveguide string is physically accurate; modal body is flexible and efficient
2. **Real-Time Performance:** Modal synthesis is much lighter than convolution (critical for guitar playing latency)
3. **TDD Friendliness:** All components are testable with known input/output pairs
4. **Codebase Consistency:** Matches existing patterns from LocalGalDSP and NexSynthDSP
5. **Production-Ready:** Proven techniques with ample reference implementations
6. **Extensibility:** Easy to add more modes, adjust coupling, or swap to commuted synthesis later

### 3.2 Core Design Principles

**From LEVEL2_RESEARCH_BEST_PRACTICES.md:**

- ✅ Use `juce::dsp::DelayLine` for waveguide delay lines
- ✅ Use `juce::dsp::FirstOrderTPTFilter` for damping filters (zero-delay)
- ✅ Use `juce::dsp::StateVariableTPTFilter` for body modes (zero-delay)
- ✅ Use `juce::dsp::Compressor<float>` for compressor pedal
- ✅ Use `juce::dsp::Phaser<float>` for phaser pedal
- ✅ Use `juce::dsp::Reverb` for reverb pedal
- ✅ Custom implementation for Karplus-Strong extension
- ✅ Custom implementation for RAT distortion (switchable diodes)
- ✅ Custom implementation for Octave pitch shifting (time-domain or FFT-based)
- ✅ **Thread-safe parameter updates** (use `std::atomic<float>`)
- ✅ **Realtime-safe audio processing** (no allocations in processBlock)
- ✅ **TDD methodology** (RED-GREEN-REFACTOR cycle)

---

## 4. String Modeling Deep Dive

### 4.1 Karplus-Strong Algorithm (Baseline)

**Basic Karplus-Strong (for reference):**

```cpp
class KarplusStrongString
{
public:
    void prepare(double sampleRate)
    {
        // Delay line length = sampleRate / frequency
        // For E2 (82.4 Hz) at 48kHz: 48000 / 82.4 = 582 samples
        delayLine.prepare({sampleRate, 512, 1});
    }

    void pluck(float velocity)
    {
        // Fill delay line with noise burst
        int length = static_cast<int>(delayLine.getMaximumDelayInSamples());
        for (int i = 0; i < length; ++i)
        {
            float noise = random.nextFloat() * 2.0f - 1.0f;
            delayLine.push(noise * velocity);
        }
    }

    float processSample()
    {
        float output = delayLine.pop();

        // Energy loss filter (first-order lowpass)
        float filtered = output * (1.0f - damping) + lastOutput * damping;
        lastOutput = filtered;

        // Recirculate
        delayLine.push(filtered);

        return output;
    }

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine;
    float damping = 0.996f;  // Energy loss coefficient
    float lastOutput = 0.0f;
    juce::Random random;
};
```

**Problems with Basic KS:**
- ❌ No pitch bending (delay line length is fixed)
- ❌ No bridge coupling (string doesn't "drive" a body)
- ❌ No stiffness (real strings have inharmonicity)
- ❌ Single articulation only (pluck)

### 4.2 Digital Waveguide Extension

**Key Improvements:**

1. **Fractional Delay Lines** for accurate pitch tracking
2. **Stiffness Filter** for realistic inharmonicity
3. **Bridge Coupling** for energy transfer to body
4. **Multiple Excitation Types** for articulation states

**Waveguide String with Extension:**

```cpp
class WaveguideString
{
public:
    struct Parameters
    {
        float frequency = 440.0f;      // String fundamental
        float damping = 0.996f;        // Energy loss
        float stiffness = 0.0f;        // Allpass filter coefficient (inharmonicity)
        float brightness = 0.5f;       // High-frequency damping
        float coupling = 0.3f;         // Bridge coupling coefficient
    };

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        fractionalDelay.prepare({sampleRate, 512, 1});
        stiffnessFilter.prepare({sampleRate, 512, 1});
        dampingFilter.prepare({sampleRate, 512, 1});

        // Stiffness filter: allpass for inharmonicity
        stiffnessFilter.setCutoffFrequency(2000.0f);
    }

    void excite(const juce::AudioBuffer<float>& exciterSignal, float velocity)
    {
        // Fill delay line with exciter signal (pick, bow, scrape, etc.)
        int length = static_cast<int>(fractionalDelay.getMaximumDelayInSamples());
        for (int i = 0; i < length; ++i)
        {
            float sample = exciterSignal.getSample(0, i % exciterSignal.getNumSamples());
            fractionalDelay.push(sample * velocity);
        }
    }

    float processSample()
    {
        // 1. Read from delay line
        float output = fractionalDelay.pop();

        // 2. Apply stiffness (allpass filter for dispersion)
        float stiffOutput = stiffnessFilter.processSample(output);

        // 3. Apply damping (frequency-dependent energy loss)
        float damped = dampingFilter.processSample(stiffOutput);

        // 4. Bridge coupling
        float bridgeEnergy = damped * params.coupling;
        float reflectedEnergy = damped - bridgeEnergy;  // Reflected back to string

        // 5. Recirculate reflected energy
        fractionalDelay.push(reflectedEnergy);

        // 6. Output bridge energy (goes to body resonator)
        lastBridgeEnergy = bridgeEnergy;
        return output;  // Direct string output
    }

    float getBridgeEnergy() const { return lastBridgeEnergy; }

    void setFrequency(float freq)
    {
        params.frequency = freq;
        float delayInSamples = sr / freq;
        fractionalDelay.setDelay(delayInSamples);
    }

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange> fractionalDelay;
    juce::dsp::FirstOrderTPTFilter<float> dampingFilter;  // Lowpass for brightness
    juce::dsp::FirstOrderTPTFilter<float> stiffnessFilter; // Allpass for inharmonicity
    Parameters params;
    double sr = 48000.0;
    float lastBridgeEnergy = 0.0f;
};
```

**Key Technical Challenges:**

1. **Fractional Delay Tuning:** Need Lagrange interpolation for smooth pitch changes
2. **Stiffness Stability:** Allpass coefficients must stay within stability bounds
3. **Bridge Coupling:** Must prevent feedback explosion (use saturation/limiting)
4. **Real-Time Pitch Tracking:** Update delay length every sample for vibrato

**Solution (from LEVEL2 research):**
- ✅ Use `juce::dsp::DelayLineInterpolationTypes::Lagrange` for fractional delay
- ✅ Limit stiffness coefficient to < 0.5 for stability
- ✅ Use `std::tanh()` on bridge feedback to prevent runaway
- ✅ Update frequency in control rate (every 16-32 samples), not per-sample

---

## 5. Bridge Coupling Implementation

### 5.1 Physics of Bridge Coupling

**Energy Transfer:**
- String vibrates → Bridge moves → Body resonates
- Bridge reflects some energy back to string
- Body radiates sound to air

**Coupling Coefficient (β):**
- β = 0.0: No coupling (string vibrates freely, no body sound)
- β = 0.3: Typical acoustic guitar (moderate coupling)
- β = 0.7: Strong coupling (solid body with hard bridge)
- β = 1.0: Maximum coupling (string dies quickly, all energy to body)

### 5.2 Bridge Coupling Algorithm

```cpp
class BridgeCoupling
{
public:
    struct Parameters
    {
        float couplingCoefficient = 0.3f;  // β: 0-1
        float reflection = 0.7f;            // 1 - β
        float nonlinearity = 0.1f;          // Bridge nonlinearity (saturation)
    };

    float processString(float stringOutput)
    {
        // String energy transfers to bridge
        float linearBridgeEnergy = stringOutput * params.couplingCoefficient;

        // Apply bridge nonlinearity (real bridges are slightly nonlinear)
        float nonlinearBridge = std::tanh(linearBridgeEnergy * (1.0f + params.nonlinearity));

        // Store for body processing
        bridgeEnergy = nonlinearBridge;

        // Bridge reflects energy back to string
        float reflectedEnergy = stringOutput - nonlinearBridge;

        return reflectedEnergy;  // Goes back to string
    }

    float getBridgeEnergy() const { return bridgeEnergy; }  // Goes to body

private:
    Parameters params;
    float bridgeEnergy = 0.0f;
};
```

**Critical Design Decision:**
- ✅ Use `std::tanh()` for soft clipping (prevents explosion)
- ✅ Keep coupling < 0.7 for stability
- ✅ Add slight nonlinearity for realism (real bridges aren't perfectly linear)

---

## 6. Articulation State Machine

### 6.1 State Machine Design

**States:**
```cpp
enum class ArticulationType
{
    BOW,      // Sustained sinusoidal excitation
    PICK,     // Impulse + exponential decay
    SCRAPE,   // Filtered white noise
    HARMONIC, // High-frequency emphasis pluck
    TREMOLO,  // Amplitude-modulated excitation
    NORMAL    // Default Karplus-Strong pluck
};
```

**Transitions:**
- User changes articulation → Crossfade begins (50-200ms)
- Previous articulation fades out
- Current articulation fades in
- Both states active during crossfade (no glitch)

### 6.2 Articulation FSM Implementation

```cpp
class ArticulationStateMachine
{
public:
    struct Parameters
    {
        float crossfadeTime = 0.1f;  // 100ms default
    };

    void setArticulation(ArticulationType newType)
    {
        if (newType != currentType)
        {
            previousType = currentType;
            currentType = newType;
            crossfadeProgress = 0.0f;
            inTransition = true;

            // Reset previous articulation generator
            previousGenerator.reset();
        }
    }

    float processSample()
    {
        // Generate outputs from both articulations
        float outputPrev = generateArticulation(previousType, previousGenerator);
        float outputCurr = generateArticulation(currentType, currentGenerator);

        // Crossfade
        if (inTransition)
        {
            float increment = 1.0f / (params.crossfadeTime * currentSampleRate);
            crossfadeProgress += increment;

            if (crossfadeProgress >= 1.0f)
            {
                crossfadeProgress = 1.0f;
                inTransition = false;
            }
        }

        // Equal-power crossfade (prevents volume dip)
        float gainPrev = std::cos(crossfadeProgress * juce::MathConstants<float>::halfPi);
        float gainCurr = std::sin(crossfadeProgress * juce::MathConstants<float>::halfPi);

        float output = outputPrev * gainPrev + outputCurr * gainCurr;
        return output;
    }

private:
    ArticulationType currentType = ArticulationType::NORMAL;
    ArticulationType previousType = ArticulationType::NORMAL;
    float crossfadeProgress = 1.0f;
    bool inTransition = false;

    ExciterGenerator currentGenerator;
    ExciterGenerator previousGenerator;

    Parameters params;
    double currentSampleRate = 48000.0;

    float generateArticulation(ArticulationType type, ExciterGenerator& generator)
    {
        switch (type)
        {
            case ArticulationType::BOW:
                return generator.generateBow();
            case ArticulationType::PICK:
                return generator.generatePick();
            case ArticulationType::SCRAPE:
                return generator.generateScrape();
            case ArticulationType::HARMONIC:
                return generator.generateHarmonic();
            case ArticulationType::TREMOLO:
                return generator.generateTremolo();
            case ArticulationType::NORMAL:
            default:
                return generator.generateNormal();
        }
    }
};
```

**Critical Implementation Detail:**
- ✅ Use equal-power crossfade (sin/cos gains) to prevent volume dip
- ✅ Keep both generators active during crossfade (no glitch)
- ✅ Configurable crossfade time (50-200ms)
- ✅ Reset previous generator after crossfade completes

---

## 7. Exciter Signal Generation

### 7.1 Exciter Types

**Bow (Sustained Sinusoidal):**
```cpp
float generateBow()
{
    // Bow friction produces sustained oscillation
    float phaseIncrement = bowFrequency / currentSampleRate;
    bowPhase += phaseIncrement;
    if (bowPhase >= 1.0f) bowPhase -= 1.0f;

    float sinusoid = std::sin(bowPhase * juce::MathConstants<float>::twoPi);

    // Add slight noise for bow hair texture
    float noise = random.nextFloat() * 0.02f - 0.01f;

    return sinusoid + noise;
}
```

**Pick (Impulse + Decay):**
```cpp
float generatePick()
{
    if (pickPhase < pickImpulseLength)
    {
        // Impulse (short burst of noise)
        float impulse = random.nextFloat() * 2.0f - 1.0f;
        pickPhase++;
        return impulse;
    }
    else
    {
        // Exponential decay
        float decay = std::exp(-pickPhase / pickDecayTime);
        pickPhase++;
        return decay;
    }
}
```

**Scrape (Filtered Noise):**
```cpp
float generateScrape()
{
    // White noise
    float noise = random.nextFloat() * 2.0f - 1.0f;

    // Bandpass filter (300-3000 Hz for "scrape" texture)
    float filtered = scrapeFilter.processSample(noise);

    // Amplitude envelope (attack, sustain, release)
    float amplitude = scrapeEnvelope.processSample();

    return filtered * amplitude;
}
```

**Harmonic (High-Freq Emphasis):**
```cpp
float generateHarmonic()
{
    // Standard pluck with high-pass pre-filter
    float noise = random.nextFloat() * 2.0f - 1.0f;

    // High-pass filter (emphasize harmonics)
    float bright = harmonicFilter.processSample(noise);

    return bright * harmonicVelocity;
}
```

**Tremolo (Amplitude-Modulated):**
```cpp
float generateTremolo()
{
    // Base pluck
    float pluck = random.nextFloat() * 2.0f - 1.0f;

    // Amplitude modulation
    float lfo = std::sin(tremoloPhase * juce::MathConstants<float>::twoPi);
    float modulated = pluck * (1.0f + lfo * tremoloDepth);

    tremoloPhase += tremoloRate / currentSampleRate;

    return modulated;
}
```

**NORMAL (Standard KS Pluck):**
```cpp
float generateNormal()
{
    // Standard Karplus-Strong noise burst
    return random.nextFloat() * 2.0f - 1.0f;
}
```

### 7.2 Exciter Generator Class

```cpp
class ExciterGenerator
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        scrapeFilter.prepare({sampleRate, 512, 1});
        scrapeFilter.setCutoffFrequency(1500.0f);  // Bandpass center

        harmonicFilter.prepare({sampleRate, 512, 1});
        harmonicFilter.setType(juce::dsp::StateVariableTPTFilter<float>::FilterType::highpass);
        harmonicFilter.setCutoffFrequency(2000.0f);
    }

    void trigger(ArticulationType type, float velocity)
    {
        currentType = type;
        pickPhase = 0.0f;
        bowPhase = 0.0f;
        tremoloPhase = 0.0f;

        scrapeEnvelope.noteOn();
        harmonicVelocity = velocity;
    }

    float generateSample()
    {
        switch (currentType)
        {
            case ArticulationType::BOW:
                return generateBow();
            case ArticulationType::PICK:
                return generatePick();
            case ArticulationType::SCRAPE:
                return generateScrape();
            case ArticulationType::HARMONIC:
                return generateHarmonic();
            case ArticulationType::TREMOLO:
                return generateTremolo();
            case ArticulationType::NORMAL:
            default:
                return generateNormal();
        }
    }

    void reset()
    {
        pickPhase = 0.0f;
        scrapeEnvelope.reset();
        bowPhase = 0.0f;
        tremoloPhase = 0.0f;
    }

private:
    ArticulationType currentType = ArticulationType::NORMAL;
    double sr = 48000.0;

    // Exciter state variables
    float pickPhase = 0.0f;
    float bowPhase = 0.0f;
    float tremoloPhase = 0.0f;
    float harmonicVelocity = 0.0f;

    // Filters
    juce::dsp::StateVariableTPTFilter<float> scrapeFilter;  // Bandpass
    juce::dsp::StateVariableTPTFilter<float> harmonicFilter; // Highpass

    // Envelopes
    juce::ADSR scrapeEnvelope;
};
```

---

## 8. Body Resonator Design

### 8.1 Modal Synthesis Architecture

**Guitar Body Modes (Simplified):**

| Mode | Frequency | Amplitude | Decay | Character |
|------|-----------|-----------|-------|-----------|
| 1 (Air) | 95 Hz | 0.8 | 2.0s | Body cavity resonance |
| 2 (Top) | 190 Hz | 0.6 | 1.5s | Top plate main mode |
| 3 (Back) | 280 Hz | 0.5 | 1.2s | Back plate |
| 4 (Helmholtz) | 400 Hz | 0.4 | 0.8s | Air resonance |
| 5 | 580 Hz | 0.3 | 0.6s | Wood stiffness |
| 6 | 850 Hz | 0.2 | 0.4s | Higher stiffness |
| 7 | 1200 Hz | 0.15 | 0.3s | Bridge modes |
| 8 | 1800 Hz | 0.1 | 0.2s | Body brilliance |

**Note:** These are approximate values for a typical acoustic guitar. Real instruments vary widely.

### 8.2 Modal Filter Implementation

```cpp
struct ModalFilter
{
    float frequency = 440.0f;      // Modal frequency (Hz)
    float amplitude = 1.0f;        // Mode amplitude (0-1)
    float decay = 1.0f;            // Decay time (seconds)
    float phase = 0.0f;            // Current phase
    float energy = 0.0f;           // Current energy level

    void prepare(double sampleRate)
    {
        sr = sampleRate;
    }

    float processSample(float excitation)
    {
        // Add excitation energy
        energy += excitation * amplitude;

        // Decay energy
        float decayFactor = 1.0f - (1.0f / (decay * sr));
        energy *= decayFactor;

        // Generate sinusoidal output at modal frequency
        float phaseIncrement = frequency / sr;
        phase += phaseIncrement;
        if (phase >= 1.0f) phase -= 1.0f;

        float output = energy * std::sin(phase * juce::MathConstants<float>::twoPi);

        return output;
    }

    void reset()
    {
        phase = 0.0f;
        energy = 0.0f;
    }

private:
    double sr = 48000.0;
};
```

### 8.3 Modal Bank Implementation

```cpp
class ModalBodyResonator
{
public:
    void prepare(double sampleRate)
    {
        for (auto& mode : modes)
        {
            mode.prepare(sampleRate);
        }
    }

    float processSample(float bridgeEnergy)
    {
        float output = 0.0f;

        // Sum all modal contributions
        for (auto& mode : modes)
        {
            output += mode.processSample(bridgeEnergy);
        }

        // Normalize output (prevent clipping with many modes)
        output /= static_cast<float>(modes.size());

        return output;
    }

    void setResonance(float amount)  // 0-1
    {
        // Adjust all mode amplitudes
        for (auto& mode : modes)
        {
            mode.amplitude = mode.baseAmplitude * amount;
        }
    }

    void reset()
    {
        for (auto& mode : modes)
        {
            mode.reset();
        }
    }

    // Preset guitar body modes
    void loadGuitarBodyPreset()
    {
        modes = {
            {95.0f, 0.8f, 2.0f, 0.0f, 0.0f},   // Air resonance
            {190.0f, 0.6f, 1.5f, 0.0f, 0.0f},  // Top plate
            {280.0f, 0.5f, 1.2f, 0.0f, 0.0f},  // Back plate
            {400.0f, 0.4f, 0.8f, 0.0f, 0.0f},  // Helmholtz
            {580.0f, 0.3f, 0.6f, 0.0f, 0.0f},  // Stiffness 1
            {850.0f, 0.2f, 0.4f, 0.0f, 0.0f},  // Stiffness 2
            {1200.0f, 0.15f, 0.3f, 0.0f, 0.0f}, // Bridge mode
            {1800.0f, 0.1f, 0.2f, 0.0f, 0.0f}   // Brilliance
        };

        // Store base amplitudes for resonance scaling
        for (auto& mode : modes)
        {
            mode.baseAmplitude = mode.amplitude;
        }
    }

private:
    std::vector<ModalFilter> modes;
};
```

**Optimization Note:**
- ✅ Use fixed-size array (8-16 modes) for cache efficiency
- ✅ Process modes in parallel (SIMD potential)
- ✅ Normalize output to prevent clipping

---

## 9. Pedalboard Architecture

### 9.1 Pedal Slot Design

```cpp
struct PedalSlot
{
    int id = -1;                     // 0-7
    bool enabled = false;
    juce::String type;               // "compressor", "octave", etc.
    juce::String name;               // User-defined name
    std::unique_ptr<juce::AudioProcessor> processor;
    juce::AudioProcessorValueTreeState* parameters = nullptr;

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        if (enabled && processor != nullptr)
        {
            processor->processBlock(buffer, midi);
        }
    }
};
```

### 9.2 Pedalboard Implementation

```cpp
class Pedalboard
{
public:
    void prepare(double sampleRate, int samplesPerBlock)
    {
        sr = sampleRate;

        // Prepare all pedals
        for (auto& pedal : pedals)
        {
            if (pedal.processor != nullptr)
            {
                pedal.processor->setPlayHead(getPlayHead());
                pedal.processor->prepareToPlay(sampleRate, samplesPerBlock);
            }
        }
    }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        juce::MidiBuffer dummyMidi;  // Most pedals don't process MIDI

        // Process in order (configurable)
        for (auto& pedal : activePedals)
        {
            if (pedal.enabled)
            {
                pedal.processBlock(buffer, dummyMidi);
            }
        }
    }

    void setPedalOrder(const juce::Array<int>& newOrder)
    {
        // Reorder active pedals based on user configuration
        juce::Array<PedalSlot> reordered;
        for (int index : newOrder)
        {
            if (index >= 0 && index < pedals.size())
            {
                reordered.add(pedals[index]);
            }
        }
        activePedals = reordered;
    }

    void enablePedal(int pedalIndex, bool enable)
    {
        if (pedalIndex >= 0 && pedalIndex < pedals.size())
        {
            pedals[pedalIndex].enabled = enable;
        }
    }

private:
    std::array<PedalSlot, 8> pedals;
    juce::Array<PedalSlot> activePedals;  // Ordered list
    double sr = 48000.0;
};
```

### 9.3 Pedal Implementations

#### Compressor (JUCE Built-in)

```cpp
class CompressorPedal : public juce::AudioProcessor
{
public:
    CompressorPedal()
        : juce::AudioProcessor(BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        juce::dsp::ProcessSpec spec {sampleRate, static_cast<juce::uint32>(samplesPerBlock), 2};
        compressor.prepare(spec);
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        compressor.process(context);
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "threshold", "Threshold",
            juce::NormalisableRange<float>(-60.0f, 0.0f), -10.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "ratio", "Ratio",
            juce::NormalisableRange<float>(1.0f, 20.0f), 4.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "attack", "Attack",
            juce::NormalisableRange<float>(0.1f, 100.0f), 5.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "release", "Release",
            juce::NormalisableRange<float>(10.0f, 1000.0f), 50.0f));

        return { params.begin(), params.end() };
    }

    const juce::String getName() const override { return "Compressor"; }
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }

private:
    juce::dsp::Compressor<float> compressor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorPedal)
};
```

#### RAT Distortion with Switchable Diodes

```cpp
class RATDistortionPedal : public juce::AudioProcessor
{
public:
    enum DiodeType
    {
        Silicon = 0,   // 1N914 ~0.7V forward voltage
        Germanium,     // 1N270 ~0.3V forward voltage
        LED            // ~1.5V forward voltage
    };

    RATDistortionPedal()
        : juce::AudioProcessor(BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        juce::dsp::ProcessSpec spec {sampleRate, static_cast<juce::uint32>(samplesPerBlock), 2};

        preFilter.prepare(spec);
        preFilter.setType(juce::dsp::FirstOrderTPTFilter<float>::Type::lowpass);
        preFilter.setCutoffFrequency(4000.0f);

        toneFilter.prepare(spec);
        toneFilter.setType(juce::dsp::FirstOrderTPTFilter<float>::Type::lowpass);
        toneFilter.setCutoffFrequency(1000.0f);
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        // Apply diode clipping per-channel
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* channelData = buffer.getWritePointer(channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                // Pre-filter
                float filtered = preFilter.processSample(channelData[sample]);

                // Asymmetric diode clipping
                float pos = filtered > 0 ? filtered : 0.0f;
                float neg = filtered < 0 ? -filtered : 0.0f;

                pos = std::tanh(pos / threshold) * threshold * asymmetry;
                neg = std::tanh(neg / threshold) * threshold;

                float clipped = pos - neg;

                // Tone filter
                channelData[sample] = toneFilter.processSample(clipped) * drive;
            }
        }
    }

    void setDiodeType(DiodeType type)
    {
        switch (type)
        {
            case Silicon:
                threshold = 0.7f;
                asymmetry = 1.0f;
                break;
            case Germanium:
                threshold = 0.3f;
                asymmetry = 1.2f;  // Softer, more asymmetrical
                break;
            case LED:
                threshold = 1.5f;
                asymmetry = 1.0f;
                break;
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "drive", "Drive",
            juce::NormalisableRange<float>(1.0f, 10.0f), 2.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "tone", "Tone",
            juce::NormalisableRange<float>(200.0f, 5000.0f, 0.0f, 0.3f), 1000.0f));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            "diode_type", "Diode Type",
            juce::StringArray {"Silicon", "Germanium", "LED"}, 0));

        return { params.begin(), params.end() };
    }

    const juce::String getName() const override { return "RAT"; }
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }

private:
    float threshold = 0.7f;
    float asymmetry = 1.0f;
    float drive = 2.0f;

    juce::dsp::FirstOrderTPTFilter<float> preFilter;  // Lowpass @ 4kHz
    juce::dsp::FirstOrderTPTFilter<float> toneFilter; // Lowpass, user-adjustable

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RATDistortionPedal)
};
```

#### Octave Pitch Shifter

**Option A: Time-Domain (Simpler, Lower Quality)**
```cpp
class OctavePedal : public juce::AudioProcessor
{
public:
    OctavePedal()
        : juce::AudioProcessor(BusesProperties()
            .withInput("Input", juce::AudioChannelSet::mono(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        sr = sampleRate;
        delayLine.prepare({sampleRate, static_cast<juce::uint32>(samplesPerBlock), 1});
        delayLine.setDelay(sampleRate / 100.0f); // 10ms for tracking
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        // Simple pitch follower (monophonic)
        // For polyphonic octave, use FFT-based approach

        float* input = buffer.getReadPointer(0);
        float* outputL = buffer.getWritePointer(0);
        float* outputR = buffer.getWritePointer(1);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float dry = input[sample];

            // Zero-crossing detector for pitch tracking
            if ((dry > 0 && lastSample < 0) || (dry < 0 && lastSample > 0))
            {
                // Zero crossing detected
                // ... track period ...
            }

            // Square wave at 1/2 frequency (octave down)
            float octaveOut = std::sin(octavePhase * juce::MathConstants<float>::twoPi);
            octavePhase += phaseIncrement;  // 0.5x frequency

            // Mix
            outputL[sample] = dry * (1.0f - mix) + octaveOut * mix;
            outputR[sample] = outputL[sample];
        }
    }

private:
    double sr = 48000.0;
    float octavePhase = 0.0f;
    float phaseIncrement = 0.0f;
    float lastSample = 0.0f;
    float mix = 0.5f;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine;
};
```

**Option B: FFT-Based (Higher Quality, More Complex)**
- Use JUCE `dsp::FFT` for spectral processing
- Shift all frequency bins down by octave
- More CPU-intensive, better quality

**Recommendation:** Start with time-domain (simpler), upgrade to FFT if needed

#### Phaser (JUCE Built-in)

```cpp
juce::dsp::Phaser<float> phaser;
phaser.setRate(0.5f);          // Hz
phaser.setDepth(0.7f);         // 0-1
phaser.setFeedback(0.5f);      // -1 to 1
phaser.setMix(1.0f);           // Dry/wet
```

#### Reverb (JUCE Built-in)

```cpp
juce::dsp::Reverb reverb;
juce::dsp::Reverb::Parameters reverbParams;
reverbParams.roomSize = 0.5f;
reverbParams.damping = 0.3f;
reverbParams.wetLevel = 0.3f;
reverbParams.dryLevel = 0.7f;
reverb.setParameters(reverbParams);
```

### 9.4 Pedal Routing Configuration

```cpp
struct PedalRoutingConfig
{
    juce::Array<int> order = {0, 1, 2, 3, 4, 5, 6, 7};  // Default order
    juce::StringArray pedalTypes = {
        "compressor", "octave", "overdrive", "distortion",
        "rat", "phaser", "reverb", "empty"
    };
};

// User can reconfigure:
// routing.order = {6, 2, 3, 4, 5, 1, 0, 7};  // Custom order
```

---

## 10. TDD Implementation Plan

### 10.1 Test Structure

```
tests/dsp/
└── NatureAetherStringTests.cpp
```

### 10.2 Test Categories

**Category 1: String Model Tests**
```cpp
beginTest("WaveguideString initialization");
{
    WaveguideString string;
    string.prepare(48000.0);
    expect(string.getDelayLength() > 0);
}

beginTest("WaveguideString pitch tracking");
{
    WaveguideString string;
    string.prepare(48000.0);
    string.setFrequency(440.0f);
    expect(std::abs(string.getDelayLength() - (48000.0 / 440.0)) < 1.0);
}

beginTest("WaveguideString produces audio");
{
    WaveguideString string;
    string.prepare(48000.0);

    juce::AudioBuffer<float> exciter(1, 100);
    // Fill with noise burst
    for (int i = 0; i < 100; ++i)
        exciter.setSample(0, i, random.nextFloat() * 2.0f - 1.0f);

    string.excite(exciter, 1.0f);

    float output = string.processSample();
    expect(output != 0.0f);
}
```

**Category 2: Bridge Coupling Tests**
```cpp
beginTest("BridgeCoupling energy transfer");
{
    BridgeCoupling bridge;
    bridge.setCoupling(0.5f);

    float stringOutput = 1.0f;
    float reflected = bridge.processString(stringOutput);
    float bridgeEnergy = bridge.getBridgeEnergy();

    expect(bridgeEnergy > 0.0f);
    expect(reflected < stringOutput);  // Some energy lost to bridge
}

beginTest("BridgeCoupling stability");
{
    BridgeCoupling bridge;
    bridge.setCoupling(0.9f);  // High coupling

    float maxOutput = 0.0f;
    for (int i = 0; i < 1000; ++i)
    {
        float reflected = bridge.processString(1.0f);
        maxOutput = juce::jmax(maxOutput, reflected);
    }

    expect(maxOutput < 10.0f);  // Should not explode
}
```

**Category 3: Articulation FSM Tests**
```cpp
beginTest("ArticulationFSM crossfade");
{
    ArticulationStateMachine fsm;
    fsm.prepare(48000.0);
    fsm.setCrossfadeTime(0.1);  // 100ms

    fsm.setArticulation(ArticulationType::BOW);
    float output = fsm.processSample();

    expect(fsm.isInTransition());
    expect(fsm.getCrossfadeProgress() < 1.0f);
}

beginTest("ArticulationFSM generates different outputs");
{
    ArticulationStateMachine fsm;
    fsm.prepare(48000.0);

    fsm.setArticulation(ArticulationType::NORMAL);
    float normalOutput = fsm.processSample();

    fsm.setArticulation(ArticulationType::BOW);
    float bowOutput = fsm.processSample();

    // Outputs should be different
    expect(std::abs(normalOutput - bowOutput) > 0.001f);
}
```

**Category 4: Body Resonator Tests**
```cpp
beginTest("ModalBodyResonator frequency response");
{
    ModalBodyResonator body;
    body.prepare(48000.0);
    body.loadGuitarBodyPreset();

    // Impulse input
    float output = body.processSample(1.0f);

    expect(output > 0.0f);  // Should respond
}

beginTest("ModalBodyResonator resonance control");
{
    ModalBodyResonator body;
    body.prepare(48000.0);
    body.loadGuitarBodyPreset();

    body.setResonance(0.0f);
    float outputLow = body.processSample(1.0f);

    body.setResonance(1.0f);
    float outputHigh = body.processSample(1.0f);

    expect(outputHigh > outputLow);
}
```

**Category 5: Pedalboard Tests**
```cpp
beginTest("Pedalboard order configuration");
{
    Pedalboard pedalboard;
    pedalboard.prepare(48000.0, 512);

    juce::Array<int> newOrder = {6, 2, 3, 4, 5, 1, 0, 7};
    pedalboard.setPedalOrder(newOrder);

    // Verify order changed
    expect(pedalboard.getPedalOrder()[0] == 6);
}

beginTest("Pedalboard bypass");
{
    Pedalboard pedalboard;
    pedalboard.prepare(48000.0, 512);

    pedalboard.enablePedal(0, false);  // Bypass compressor

    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();

    float inputLevel = 0.5f;
    for (int i = 0; i < 512; ++i)
    {
        buffer.setSample(0, i, inputLevel);
        buffer.setSample(1, i, inputLevel);
    }

    pedalboard.processBlock(buffer);

    // Output should match input (no compression)
    float maxOutput = buffer.getMagnitude(0, 0, 512);
    expect(std::abs(maxOutput - inputLevel) < 0.01f);
}
```

**Category 6: Integration Tests**
```cpp
beginTest("Complete signal path");
{
    StringDSP dsp;
    dsp.prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    // Note-on
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

    dsp.processBlock(buffer, midi);

    // Should produce output
    float maxSample = buffer.getMagnitude(0, 0, 512);
    expect(maxSample > 0.0f);
}
```

### 10.3 Test Execution Plan

**Phase 1: String Model (Week 1)**
- WaveguideString tests (20 tests)
- BridgeCoupling tests (15 tests)
- ArticulationFSM tests (20 tests)

**Phase 2: Body & Pedalboard (Week 2)**
- ModalBodyResonator tests (15 tests)
- Individual pedal tests (10 tests each × 8 pedals = 80 tests)
- Pedalboard routing tests (15 tests)

**Phase 3: Integration (Week 3)**
- Complete signal path tests (20 tests)
- Preset system tests (10 tests)
- Parameter automation tests (10 tests)

**Total: ~205 tests**

---

## 11. FFI Bridge Design

### 11.1 C Bridge Header

```cpp
// nature_aether_string_ffi.h

extern "C" {

//==============================================================================
// Lifecycle
//==============================================================================

typedef void* NatureAetherStringHandle;

NatureAetherStringHandle nature_aether_string_create();
void nature_aether_string_destroy(NatureAetherStringHandle handle);

void nature_aether_string_prepare(NatureAetherStringHandle handle,
                                      double sampleRate,
                                      int samplesPerBlock);

//==============================================================================
// Audio Processing
//==============================================================================

void nature_aether_string_process(NatureAetherStringHandle handle,
                                      float** channels,
                                      int numChannels,
                                      int numSamples,
                                      const unsigned char* midiData,
                                      int midiDataSize);

//==============================================================================
// Articulation Control
//==============================================================================

void nature_aether_string_set_articulation(NatureAetherStringHandle handle,
                                               int articulationType);  // 0-5

int nature_aether_string_get_articulation(NatureAetherStringHandle handle);

void nature_aether_string_set_crossfade_time(NatureAetherStringHandle handle,
                                                 double timeMs);

//==============================================================================
// String Parameters
//==============================================================================

void nature_aether_string_set_string_frequency(NatureAetherStringHandle handle,
                                                   int stringIndex,  // 0-5
                                                   float frequency);

float nature_aether_string_get_string_frequency(NatureAetherStringHandle handle,
                                                    int stringIndex);

void nature_aether_string_set_string_damping(NatureAetherStringHandle handle,
                                                 int stringIndex,
                                                 float damping);  // 0-1

//==============================================================================
// Bridge Coupling
//==============================================================================

void nature_aether_string_set_bridge_coupling(NatureAetherStringHandle handle,
                                                  float coupling);  // 0-1

float nature_aether_string_get_bridge_coupling(NatureAetherStringHandle handle);

//==============================================================================
// Body Resonator
//==============================================================================

void nature_aether_string_set_body_resonance(NatureAetherStringHandle handle,
                                                 float resonance);  // 0-1

void nature_aether_string_load_body_preset(NatureAetherStringHandle handle,
                                               int presetIndex);  // 0 = guitar, etc.

//==============================================================================
// Pedalboard Control
//==============================================================================

void nature_aether_string_enable_pedal(NatureAetherStringHandle handle,
                                          int pedalIndex,  // 0-7
                                          bool enable);

bool nature_aether_string_is_pedal_enabled(NatureAetherStringHandle handle,
                                               int pedalIndex);

void nature_aether_string_set_pedal_parameter(NatureAetherStringHandle handle,
                                                  int pedalIndex,
                                                  const char* parameterId,
                                                  float value);

float nature_aether_string_get_pedal_parameter(NatureAetherStringHandle handle,
                                                   int pedalIndex,
                                                   const char* parameterId);

void nature_aether_string_set_pedal_order(NatureAetherStringHandle handle,
                                              const int* orderArray,
                                              int arraySize);

//==============================================================================
// Preset System
//==============================================================================

char* nature_aether_string_save_preset(NatureAetherStringHandle handle);
bool nature_aether_string_load_preset(NatureAetherStringHandle handle,
                                          const char* jsonData);

bool nature_aether_string_validate_preset(NatureAetherStringHandle handle,
                                              const char* jsonData);

int nature_aether_string_get_num_factory_presets(NatureAetherStringHandle handle);
const char* nature_aether_string_get_factory_preset_name(NatureAetherStringHandle handle,
                                                              int index);
bool nature_aether_string_load_factory_preset(NatureAetherStringHandle handle,
                                                  int index);

void nature_aether_string_free_string(char* str);

} // extern "C"
```

### 11.2 Swift Integration Example

```swift
import Foundation

class NatureAetherStringBridge {
    private var instance: OpaquePointer?

    init(sampleRate: Double = 48000.0, bufferSize: Int = 512) {
        instance = nature_aether_string_create()
        nature_aether_string_prepare(instance, sampleRate, Int32(bufferSize))

        // Load guitar body preset
        nature_aether_string_load_body_preset(instance, 0)
    }

    deinit {
        if let inst = instance {
            nature_aether_string_destroy(inst)
        }
    }

    // Articulation control
    func setArticulation(_ type: ArticulationType) {
        nature_aether_string_set_articulation(instance, type.rawValue)
    }

    func setCrossfadeTime(_ timeMs: Double) {
        nature_aether_string_set_crossfade_time(instance, timeMs)
    }

    // String control
    func setStringFrequency(_ index: Int, frequency: Float) {
        nature_aether_string_set_string_frequency(instance, Int32(index), frequency)
    }

    // Bridge coupling
    func setBridgeCoupling(_ coupling: Float) {
        nature_aether_string_set_bridge_coupling(instance, coupling)
    }

    // Body resonator
    func setBodyResonance(_ resonance: Float) {
        nature_aether_string_set_body_resonance(instance, resonance)
    }

    // Pedalboard
    func enablePedal(_ index: Int, enabled: Bool) {
        nature_aether_string_enable_pedal(instance, Int32(index), enabled)
    }

    func setPedalParameter(_ pedalIndex: Int, parameterId: String, value: Float) {
        parameterId.withCString { cString in
            nature_aether_string_set_pedal_parameter(instance, Int32(pedalIndex), cString, value)
        }
    }

    func setPedalOrder(_ order: [Int]) {
        order.withUnsafeBufferPointer { buffer in
            nature_aether_string_set_pedal_order(instance, buffer.baseAddress, Int32(order.count))
        }
    }

    // Audio processing
    func processAudio(_ output: UnsafeMutablePointer<Float>,
                     frameCount: Int,
                     midi: Data?) -> Bool {
        guard let inst = instance else { return false }

        var channels = [UnsafeMutablePointer<Float>?](repeating: nil, count: 2)
        channels[0] = output
        channels[1] = output.advanced(by: Int(frameCount))

        if let midi = midi {
            return midi.withUnsafeBytes { bytes in
                nature_aether_string_process(
                    inst,
                    &channels,
                    2,
                    Int32(frameCount),
                    bytes.baseAddress?.assumingMemoryBound(to: UInt8.self),
                    Int32(midi.count)
                )
            }
        } else {
            nature_aether_string_process(inst, &channels, 2, Int32(frameCount), nil, 0)
            return true
        }
    }

    // Presets
    func savePreset() -> String? {
        guard let cString = nature_aether_string_save_preset(instance) else { return nil }
        defer { nature_aether_string_free_string(cString) }
        return String(cString: cString)
    }

    func loadPreset(_ jsonData: String) -> Bool {
        return jsonData.withCString { cString in
            nature_aether_string_load_preset(instance, cString)
        }
    }
}

enum ArticulationType: Int {
    case bow = 0
    case pick
    case scrape
    case harmonic
    case tremolo
    case normal
}
```

---

## 12. Testing Strategy

### 12.1 Unit Tests (TDD)

**Test Coverage Goals:**
- String model: > 90% coverage
- Bridge coupling: > 90% coverage
- Articulation FSM: > 90% coverage
- Body resonator: > 90% coverage
- Pedalboard: > 85% coverage
- Integration: > 80% coverage

### 12.2 Performance Tests

```cpp
beginTest("CPU performance");
{
    StringDSP dsp;
    dsp.prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    // Trigger 6 strings (full chord)
    for (int note : {64, 59, 55, 50, 45, 40})
    {
        midi.addEvent(juce::MidiMessage::noteOn(1, note, 0.8f), 0);
    }

    // Measure CPU time
    auto start = juce::Time::getHighResolutionTicks();

    for (int i = 0; i < 1000; ++i)
    {
        dsp.processBlock(buffer, midi);
    }

    auto end = juce::Time::getHighResolutionTicks();
    double elapsed = juce::Time::highResolutionTicksToSeconds(end - start);

    double cpuPercent = (elapsed / (1000.0 * 512.0 / 48000.0)) * 100.0;

    expect(cpuPercent < 20.0);  // < 20% CPU target
}
```

### 12.3 Real-World Validation Tests

**Test 1: Guitar Range**
```cpp
// Test all guitar frets (E2 to E6)
for (int midiNote = 40; midiNote <= 88; ++midiNote)
{
    midi.addEvent(juce::MidiMessage::noteOn(1, midiNote, 0.8f), 0);
    dsp.processBlock(buffer, midi);

    float output = buffer.getMagnitude(0, 0, 512);
    expect(output > 0.0f);  // Should produce sound
}
```

**Test 2: Articulation Transitions**
```cpp
// Test all articulation transitions
for (int from = 0; from < 6; ++from)
{
    for (int to = 0; to < 6; ++to)
    {
        fsm.setArticulation(static_cast<ArticulationType>(from));
        fsm.setArticulation(static_cast<ArticulationType>(to));

        // Process through crossfade
        for (int i = 0; i < 4800; ++i)  // 100ms at 48kHz
        {
            float output = fsm.processSample();
            expect(output != 0.0f);  // Should not glitch
        }
    }
}
```

**Test 3: Pedalboard Stability**
```cpp
// Enable all pedals
for (int i = 0; i < 8; ++i)
{
    pedalboard.enablePedal(i, true);
}

// Process for extended period
juce::AudioBuffer<float> buffer(2, 512);
buffer.clear();

for (int i = 0; i < 10000; ++i)
{
    // Fill with noise
    for (int sample = 0; sample < 512; ++sample)
    {
        float noise = random.nextFloat() * 2.0f - 1.0f;
        buffer.setSample(0, sample, noise);
        buffer.setSample(1, sample, noise);
    }

    pedalboard.processBlock(buffer);

    // Check for explosion
    float maxSample = buffer.getMagnitude(0, 0, 512);
    expect(maxSample < 100.0f);  // Should not explode
}
```

### 12.4 Audio Quality Tests

**Test 1: Frequency Response**
```cpp
// Sweep tone from 20Hz to 20kHz
for (float freq = 20.0f; freq <= 20000.0f; freq *= 1.01f)
{
    // Generate sine wave
    for (int sample = 0; sample < 512; ++sample)
    {
        float sine = std::sin(2.0f * juce::MathConstants<float>::pi * freq * sample / 48000.0f);
        buffer.setSample(0, sample, sine);
    }

    dsp.processBlock(buffer, midi);

    // Measure output level
    float level = buffer.getMagnitude(0, 0, 512);

    // Store for frequency response plot
    frequencyResponse.add({freq, level});
}
```

**Test 2: Harmonic Content**
```cpp
// Pluck low E (82.4 Hz)
midi.addEvent(juce::MidiMessage::noteOn(1, 40, 0.8f), 0);
dsp.processBlock(buffer, midi);

// Analyze harmonics using FFT
juce::dsp::FFT fft(10);
fft.performFrequencyOnlyForwardTransform(buffer.getReadPointer(0));

// Verify harmonic series
expect(peakFrequency ≈ 82.4);   // Fundamental
expect(harmonic2 ≈ 164.8);      // 2nd harmonic
expect(harmonic3 ≈ 247.2);      // 3rd harmonic
```

---

## 13. File-by-File Implementation Checklist

### Phase 1: Core DSP (Week 1-2)

**File 1: `include/dsp/StringDSP.h`**
- [ ] Class declaration (inherits from juce::AudioProcessor)
- [ ] Parameter layout declaration
- [ ] Voice structure (6 voices for 6 strings)
- [ ] Articulation state machine
- [ ] Bridge coupling
- [ ] Modal body resonator
- [ ] Pedalboard architecture

**Estimated Lines:** ~800
**Estimated Time:** 12 hours

---

**File 2: `src/dsp/StringDSP.cpp`**
- [ ] Constructor/destructor
- [ ] prepareToPlay() - Initialize all DSP components
- [ ] releaseResources() - Clean up
- [ ] processBlock() - Main audio processing
- [ ] Voice allocation/stealing
- [ ] Render voice (waveguide string)
- [ ] Bridge coupling processing
- [ ] Body resonator processing
- [ ] Pedalboard processing
- [ ] Parameter smoothing
- [ ] Preset system (JSON)

**Estimated Lines:** ~2,500
**Estimated Time:** 35 hours

---

**File 3: `include/dsp/WaveguideString.h`** (Internal Component)
- [ ] WaveguideString class
- [ ] Fractional delay line
- [ ] Stiffness filter (allpass)
- [ ] Damping filter (lowpass)
- [ ] Bridge coupling calculation
- [ ] Frequency tracking

**Estimated Lines:** ~200
**Estimated Time:** 6 hours

---

**File 4: `src/dsp/WaveguideString.cpp`**
- [ ] Constructor/initialization
- [ ] prepare() - Initialize delay line and filters
- [ ] excite() - Fill delay line with exciter signal
- [ ] processSample() - Waveguide processing
- [ ] setFrequency() - Update delay line length
- [ ] Stability checks

**Estimated Lines:** ~400
**Estimated Time:** 10 hours

---

**File 5: `include/dsp/ArticulationStateMachine.h`**
- [ ] ArticulationType enum
- [ ] ExciterGenerator class
- [ ] ArticulationStateMachine class
- [ ] Crossfade logic

**Estimated Lines:** ~250
**Estimated Time:** 6 hours

---

**File 6: `src/dsp/ArticulationStateMachine.cpp`**
- [ ] Exciter generation (BOW, PICK, SCRAPE, HARMONIC, TREMOLO, NORMAL)
- [ ] Crossfade processing (equal-power)
- [ ] State transition logic
- [ ] Parameter smoothing

**Estimated Lines:** ~600
**Estimated Time:** 12 hours

---

**File 7: `include/dsp/ModalBodyResonator.h`**
- [ ] ModalFilter structure
- [ ] ModalBodyResonator class
- [ ] Preset body modes (guitar, violin, etc.)

**Estimated Lines:** ~150
**Estimated Time:** 4 hours

---

**File 8: `src/dsp/ModalBodyResonator.cpp`**
- [ ] Modal filter processing
- [ ] Modal bank summing
- [ ] Body preset loading
- [ ] Resonance control

**Estimated Lines:** ~300
**Estimated Time:** 6 hours

---

### Phase 2: Pedalboard (Week 3)

**File 9: `include/dsp/Pedalboard.h`**
- [ ] PedalSlot structure
- [ ] Pedalboard class
- [ ] Pedal ordering
- [ ] Bypass switching

**Estimated Lines:** ~200
**Estimated Time:** 5 hours

---

**File 10: `src/dsp/Pedalboard.cpp`**
- [ ] Pedal initialization
- [ ] Process chain (in order)
- [ ] Order reconfiguration
- [ ] Parameter routing

**Estimated Lines:** ~400
**Estimated Time:** 8 hours

---

**File 11: `include/dsp/CompressorPedal.h`** (JUCE Built-in)
- [ ] Wrapper class for juce::dsp::Compressor

**Estimated Lines:** ~100
**Estimated Time:** 2 hours

---

**File 12: `src/dsp/CompressorPedal.cpp`**
- [ ] Process block using JUCE compressor
- [ ] Parameter exposure

**Estimated Lines:** ~150
**Estimated Time:** 2 hours

---

**File 13: `include/dsp/OctavePedal.h`**
- [ ] OctavePedal class (time-domain or FFT)

**Estimated Lines:** ~100
**Estimated Time:** 3 hours

---

**File 14: `src/dsp/OctavePedal.cpp`**
- [ ] Pitch tracking
- [ ] Octave-down generation
- [ ] Mix control

**Estimated Lines:** ~300
**Estimated Time:** 8 hours

---

**File 15: `include/dsp/RATDistortionPedal.h`**
- [ ] RATDistortionPedal class
- [ ] Diode type enum

**Estimated Lines:** ~120
**Estimated Time:** 3 hours

---

**File 16: `src/dsp/RATDistortionPedal.cpp`**
- [ ] Asymmetric diode clipping
- [ ] Pre-filter and tone filter
- [ ] Switchable diodes (Si/Ge/LED)

**Estimated Lines:** ~350
**Estimated Time:** 8 hours

---

**File 17-20: Other Pedals** (Overdrive, Distortion, Phaser, Reverb)
- [ ] Similar structure to CompressorPedal
- [ ] Use JUCE built-in effects where possible

**Estimated Lines:** ~800 total
**Estimated Time:** 15 hours total

---

### Phase 3: FFI Bridge (Week 4)

**File 21: `include/ffi/NatureAetherStringFFI.h`**
- [ ] C bridge function declarations
- [ ] Lifecycle functions (create, destroy, prepare)
- [ ] Audio processing (process)
- [ ] Articulation control
- [ ] String parameters
- [ ] Bridge coupling
- [ ] Body resonator
- [ ] Pedalboard control
- [ ] Preset system

**Estimated Lines:** ~300
**Estimated Time:** 5 hours

---

**File 22: `src/ffi/NatureAetherStringFFI.cpp`**
- [ ] C bridge implementations
- [ ] Exception handling
- [ ] String memory management
- [ ] Parameter ID mapping

**Estimated Lines:** ~900
**Estimated Time:** 12 hours

---

### Phase 4: Testing (Week 5)

**File 23: `tests/dsp/NatureAetherStringTests.cpp`**
- [ ] String model tests (20 tests)
- [ ] Bridge coupling tests (15 tests)
- [ ] Articulation FSM tests (20 tests)
- [ ] Body resonator tests (15 tests)
- [ ] Pedalboard tests (115 tests)
- [ ] Integration tests (20 tests)
- [ ] Performance tests (5 tests)
- [ ] Audio quality tests (5 tests)

**Estimated Lines:** ~2,500
**Estimated Time:** 30 hours

---

### Phase 5: Presets (Week 6)

**File 24: `presets/NatureAetherString/`**
- [ ] Init Acoustic (10 presets)
- [ ] Init Electric (10 presets)
- [ ] Articulation presets (6 presets)
- [ ] Pedalboard presets (10 presets)
- [ ] Experimental (5 presets)

**Estimated Total:** 41 presets
**Estimated Time:** 15 hours

---

## Summary

### Total Implementation Metrics

| Metric | Value |
|--------|-------|
| **Total Files** | 24 files |
| **Total Code Lines** | ~10,820 lines |
| **Total Test Lines** | ~2,500 lines |
| **Total Presets** | 41 presets |
| **Estimated Time** | 80-120 hours (2-3 weeks full-time) |
| **Test Coverage Target** | > 85% |
| **CPU Budget** | < 20% on M1/M2 |
| **Latency Target** | < 10ms |

### Critical Success Factors

1. **Thread Safety:** Use `std::atomic<float>` for all cross-thread parameters
2. **Realtime Safety:** NO allocations in `processBlock()`
3. **Stability:** Use `std::tanh()` on all feedback paths
4. **TDD:** Write tests BEFORE implementation
5. **Performance:** Profile early, optimize hot paths
6. **Authenticity:** Tune modal frequencies to real measurements
7. **Usability:** Clear parameter names, sensible defaults

### Risk Mitigation

**Risk 1: Bridge Coupling Instability**
- **Mitigation:** Use `std::tanh()` on all feedback, limit coupling to < 0.7
- **Test:** High-coupling stress test (Section 12.3)

**Risk 2: CPU Overbudget**
- **Mitigation:** Use JUCE built-in effects, optimize modal filter count
- **Test:** Performance test (Section 12.2)

**Risk 3: Articulation Glitches**
- **Mitigation:** Equal-power crossfade, both generators active during transition
- **Test:** All articulation transitions (Section 12.3)

**Risk 4: Pedalboard Ordering Bugs**
- **Mitigation:** Simple array-based ordering, clear UI feedback
- **Test:** Pedalboard routing tests (Section 10.2)

---

## References

### Academic Papers
1. Smith, J. "Physical Audio Signal Processing - Waveguide Synthesis" (CCRMA)
2. Jaffe, D. & Smith, J. "Performance Expression in Commuted Waveguide Synthesis" (ICMC 1995)
3. Desvages, C. "Physical Modelling of the Bowed String" (PhD Thesis 2018)
4. "Circuit Based Classical Guitar Model" (ScienceDirect 2015)
5. Karplus, K. & Strong, A. "Digital Synthesis of Plucked String and Drum Timbres" (1983)

### JUCE Documentation
1. JUCE DSP Module Reference (docs.juce.com)
2. Tutorial: "Create a string model with delay lines"
3. Tutorial: "Introduction to DSP"
4. Tutorial: "Add distortion through waveshaping"

### Community Resources
1. Nathan Ho: "Exploring Modal Synthesis" (nathan.ho.name)
2. The Wolf Sound: "FM Synthesis Explained" (thewolfsound.com)
3. LEVEL2_RESEARCH_BEST_PRACTICES.md (existing codebase)
4. COMPLETE_APPLETV_HANDOFF.md (existing codebase)

### GitHub Repositories
1. github.com/odoare/StrinGO (waveguide synthesis)
2. github.com/juandagilc/Audio-Effects (pedalboard effects)
3. github.com/ff00ff/Delay-Audio-Plugin (Karplus-Strong implementation)

---

**Document Status:** ✅ COMPLETE
**Next Phase:** Implementation (TDD Phase 1)
**Priority:** HIGH (Critical path for Nature Marco Instrument Family)

**Approved by:** DSP Engineer
**Date:** 2025-12-25

---

**Implementation Ready: YES**
**Research Confidence: HIGH**
**Technical Feasibility: CONFIRMED**
**Production Timeline: 2-3 weeks**
