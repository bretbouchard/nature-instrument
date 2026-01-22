# Nature Marco Implementation Research

**Research Date:** 2025-12-25
**Project:** Nature Marco Hybrid Virtual Analog Synthesizer
**Researcher:** Claude Code (DSP Engineer Specialist - Level 3 Deep Research)
**Status:** ✅ COMPLETE
**Recommendation:** Approach A - Pure JUCE DSP with Custom Oscillators

---

## Executive Summary

**Nature Marco** is a hybrid virtual analog synthesizer combining traditional subtractive synthesis with FM capabilities and oscillator warp. After comprehensive analysis of 3 implementation approaches, **Approach A (Pure JUCE DSP)** is recommended as the optimal solution.

**Key Recommendation:**
- Use `juce::dsp::StateVariableTPTFilter` for zero-delay feedback multimode filter
- Implement custom oscillators with PolyBLEP anti-aliasing for warp and FM
- Thread-safe modulation matrix using `std::atomic<float>` arrays
- Serum-style macro system with simplified parameter grouping
- 16-voice polyphony with monophonic/unison modes

**Implementation Complexity:** 60-80 hours
**Performance:** 2-5% CPU per voice at 48kHz (estimated)
**TDD Testability:** HIGH - All components testable in isolation

---

## Instrument Specifications

### User-Provided Requirements

**Oscillators:**
- 2 VA oscillators with waveforms: SAW, SQUARE, TRI, SINE, PULSE (PWM)
- Oscillator WARP: -1.0 (negative phase) to +1.0 (positive phase)
- FM mode: Carrier ↔ Modulator swap with linear vs exponential FM
- Sub-oscillator: -1 oct, square wave

**Mixer:**
- OSC1, OSC2, SUB levels + noise generator

**Filter:**
- SVF multimode filter: LP, HP, BP, NOTCH with cutoff/resonance
- Filter ENV: ADSR with amount control

**Amplifier:**
- Amp ENV: ADSR

**Modulation:**
- LFO: 5 waveforms, rate, depth, polarity
- Modulation Matrix: 16 slots (sources: LFO, ENVs, macros, velocity, aftertouch)
- Macro System: 8 macros (Serum-style simplified control)

**Global:**
- Poly mode (mono/legato/poly)
- Glide (portamento)
- Master tune
- Master volume

**Target:** 30 factory presets covering bass, leads, pads, FX

---

## Implementation Approaches Analyzed

### Approach A: Pure JUCE DSP with Custom Oscillators

#### Technical Design

**Architecture:**
```
MIDI Input → Voice Allocator → 16 Voices → Mixer → Filter → Amp → Output
                                  ↓
                            Modulation Matrix
                                  ↓
                          LFOs + Macros + Envelopes
```

**Core Components:**
1. **Custom Oscillator with Warp/FM**
   - PolyBLEP anti-aliased waveforms (SAW, SQUARE, TRI, SINE, PULSE)
   - Phase warp: `phase_warped = phase + (warp * sin(2π * phase))`
   - FM synthesis: Phase modulation with carrier/modulator swap
   - Linear vs exponential FM (selectable)

2. **State Variable TPT Filter**
   - Use `juce::dsp::StateVariableTPTFilter<float>` (zero-delay feedback)
   - Modes: LP, HP, BP, NOTCH
   - Cutoff: 20Hz - 20kHz (logarithmic)
   - Resonance: 0.0 - 1.0 (self-oscillation at high values)

3. **Modulation Matrix (16-slot)**
   - Lock-free atomic arrays for realtime-safe modulation
   - Sources: LFO1-2, Filter ENV, Amp ENV, Macro1-8, Velocity, Aftertouch
   - Destinations: All automatable parameters
   - Per-slot: amount, curve, bipolar/unipolar

4. **Macro System (8 macros)**
   - Simplified parameter grouping (Serum-style)
   - Each macro: name, value (0-1), list of destination parameters
   - Right-click "Learn to Macro" in UI (future feature)

5. **Voice Architecture**
   - 16-voice polyphony
   - Monophonic mode with last/low/high priority
   - Legato mode (retrigger envelopes optional)
   - Unison mode (2-8 voices per note with detune)

**DSP Algorithm Details:**

##### Oscillator Warp Algorithm
```cpp
class NatureOscillator
{
private:
    double phase = 0.0;
    double phaseIncrement = 0.0;

public:
    void setFrequency(float freqHz, double sampleRate)
    {
        phaseIncrement = freqHz / sampleRate;
    }

    void setWarp(float warpAmount) // -1.0 to 1.0
    {
        warp = warpAmount;
    }

    float processSample()
    {
        // Apply phase warp (negative or positive)
        float warpedPhase = phase + (warp * std::sin(phase * 2.0 * juce::MathConstants<double>::pi));

        // Generate selected waveform from warped phase
        float output = generateWaveform(warpedPhase);

        // Advance phase
        phase += phaseIncrement;
        if (phase >= 1.0) phase -= 1.0;

        return output;
    }

private:
    float warp = 0.0f;

    float generateWaveform(double phase)
    {
        switch (waveform)
        {
            case SAW:     return polyBlepSaw(phase);
            case SQUARE:  return polyBlepSquare(phase);
            case TRIANGLE: return polyBlepTriangle(phase);
            case SINE:    return std::sin(phase * 2.0 * juce::MathConstants<double>::pi);
            case PULSE:   return polyBlepPulse(phase, pulseWidth);
        }
    }

    // PolyBLEP implementations for anti-aliasing
    float polyBlep(double t, double dt) const;
    float polyBlepSaw(double phase) const;
    float polyBlepSquare(double phase) const;
    // ...
};
```

##### FM Synthesis Implementation
```cpp
class FMOperator
{
public:
    void setMode(bool isCarrier, bool linearFM)
    {
        this->isCarrier = isCarrier;
        this->linearFM = linearFM;
    }

    float processSample(float modulationInput)
    {
        if (isCarrier)
        {
            // Carrier: apply phase modulation from modulator
            double modulatedPhase = phase + modulationInput;

            float output = std::sin(modulatedPhase * 2.0 * juce::MathConstants<double>::pi);

            phase += phaseIncrement;
            if (phase >= 1.0) phase -= 1.0;

            return output;
        }
        else
        {
            // Modulator: generate output for carrier
            float output = std::sin(phase * 2.0 * juce::MathConstants<double>::pi);

            phase += phaseIncrement * (linearFM ? modulatorRatio : 1.0);
            if (phase >= 1.0) phase -= 1.0;

            return output * fmDepth;
        }
    }

private:
    bool isCarrier = false;
    bool linearFM = false;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    float fmDepth = 1.0f;
    float modulatorRatio = 1.0f;
};
```

##### Modulation Matrix Architecture
```cpp
class ModulationMatrix
{
public:
    void prepare(int numModulationSlots)
    {
        modulationAmounts.resize(numModulationSlots);
        for (auto& amount : modulationAmounts)
            amount.store(0.0f);
    }

    void updateModulationFromUI(int slot, float amount)
    {
        // Call from UI thread - NOT realtime-safe
        if (slot >= 0 && slot < modulationAmounts.size())
            modulationAmounts[slot].store(amount); // Atomic store
    }

    float getModulationValue(int slot) const
    {
        // Call from audio thread - realtime-safe
        if (slot >= 0 && slot < modulationAmounts.size())
            return modulationAmounts[slot].load(); // Atomic load
        return 0.0f;
    }

    float applyModulation(const ModulationRouting& routing, float baseValue)
    {
        float modValue = getModulationValue(routing.sourceIndex);
        float modAmount = routing.amount.load();

        if (routing.bipolar)
            return baseValue + (modValue * modAmount * routing.maxValue);
        else
            return baseValue + (std::abs(modValue) * modAmount * routing.maxValue);
    }

private:
    struct ModulationRouting
    {
        int sourceIndex = 0;
        int destinationIndex = 0;
        std::atomic<float> amount{0.0f};
        float maxValue = 1.0f;
        bool bipolar = false;
        int curveType = 0; // 0=linear, 1=exponential
    };

    std::vector<std::atomic<float>> modulationAmounts;
    std::array<ModulationRouting, 16> routings;
};
```

#### Pros

1. **Production-Ready**
   - All components use JUCE DSP module (battle-tested)
   - Zero-delay feedback filter (`StateVariableTPTFilter`) preserves analog character
   - Lock-free modulation ensures realtime safety

2. **Performance**
   - Estimated 2-5% CPU per voice at 48kHz
   - PolyBLEP oscillators provide high-quality anti-aliasing
   - No overhead from external dependencies

3. **Maintainability**
   - Single codebase, no external build tools
   - Consistent with existing instruments (NexSynthDSP, LocalGalDSP, SamSamplerDSP)
   - Easy to debug and profile

4. **TDD Testability**
   - All components testable in isolation
   - Existing test framework (`DSPTestFramework.h`) works perfectly
   - JSON preset system validates parameters

5. **Codebase Integration**
   - Follows established patterns from COMPLETE_APPLETV_HANDOFF.md
   - FFI bridge pattern proven to work with Swift/tvOS
   - Preset system compatible with existing instruments

#### Cons

1. **Custom Implementation Required**
   - Oscillator warp algorithm must be implemented from scratch
   - FM synthesis needs careful tuning for good sounds
   - PolyBLEP requires understanding of anti-aliasing techniques

2. **Filter Authenticity**
   - `StateVariableTPTFilter` is excellent but not circuit-modeled
   - No "Moog ladder" or "Oberheim SEM" filter character
   - May not satisfy analog purists

3. **Development Time**
   - 60-80 hours estimated (longest of the 3 approaches)
   - Requires careful tuning of warp and FM algorithms
   - 30 factory presets must be designed by hand

#### Implementation Complexity: MEDIUM-HIGH
**Estimated Hours:** 60-80 hours
- Core DSP: 30 hours
- Modulation matrix: 10 hours
- Macro system: 8 hours
- FFI bridge: 6 hours
- TDD tests: 12 hours
- Presets: 14 hours

#### Performance: EXCELLENT
**Estimated CPU Load:**
- Single voice: 2-3% CPU at 48kHz
- 16 voices polyphony: 32-48% CPU (typical usage)
- Oversampling (2x): +50% CPU overhead (optional)

#### TDD Testability: HIGH
**Test Coverage:**
- Oscillator waveforms (warp, FM, PWM)
- Filter modes and resonance
- Envelope stages (ADSR)
- Modulation matrix routing
- Macro system parameter linking
- Preset save/load/validate

---

### Approach B: Hybrid JUCE + Faust-Generated DSP Blocks

#### Technical Design

**Architecture:**
- Oscillators: JUCE `dsp::Oscillator<float>` (no PolyBLEP)
- Filter: Faust-generated state variable filter
- FM/Warp: Custom JUCE implementation (Faust not suitable)
- Modulation: JUCE-based (Faust not realtime-safe for dynamic routing)

**Faust Integration:**
```faust
// nature_filter.dsp
import("stdfaust.lib");

filter(freq, res, type) = select2(type,
    fi.resonbp(freq, res),    // Bandpass
    select2(type,
        fi.resonlp(freq, res), // Lowpass
        fi.resonhp(freq, res)  // Highpass
    )
);
```

#### Pros

1. **Rapid Prototyping**
   - Faust DSP code is concise (10-20 lines vs 200+ lines C++)
   - Filter design can be tested in Faust IDE
   - Quick iteration on filter topologies

2. **Academic Verification**
   - Faust compiler generates mathematically correct code
   - Can compare against textbook filter implementations
   - Useful for research/educational purposes

#### Cons

1. **Build Complexity**
   - Requires Faust compiler in build toolchain
   - Adds CMake complexity (`.dsp` → `.cpp` compilation step)
   - Not compatible with existing codebase workflow

2. **Limited Usefulness**
   - Faust cannot handle oscillator warp (requires custom phase accumulation)
   - Faust cannot handle modulation matrix (dynamic routing not supported)
   - Only filter benefits from Faust (minor part of architecture)

3. **Debugging Difficulty**
   - Generated C++ code is unreadable (5000+ lines from 20 lines Faust)
   - Cannot step through Faust code in debugger
   - Error messages reference generated code, not source

4. **No Performance Benefit**
   - Faust-generated code is not faster than hand-written C++
   - JUCE `StateVariableTPTFilter` is already highly optimized
   - SIMD optimizations available in JUCE DSP module

#### Implementation Complexity: HIGH
**Estimated Hours:** 70-90 hours
- Faust build system setup: 12 hours
- Filter DSP (Faust): 6 hours
- Oscillators (JUCE custom): 25 hours
- Modulation (JUCE): 10 hours
- Build toolchain debugging: 15 hours
- TDD tests: 12 hours
- Presets: 10 hours

#### Performance: GOOD
**Estimated CPU Load:**
- Single voice: 2-4% CPU at 48kHz (similar to Approach A)
- Filter overhead: negligible (Faust ≈ hand-written)

#### TDD Testability: MEDIUM
**Test Coverage:**
- Oscillators: HIGH (custom JUCE implementation)
- Filter: MEDIUM (generated code harder to test)
- Modulation: HIGH (JUCE-based)
- Build system: LOW (Faust integration fragile)

---

### Approach C: White-Box Analog Circuit Modeling

#### Technical Design

**Architecture:**
- Circuit-modelled components (Moog ladder, VCA, etc.)
- Zero-delay feedback filters (from research papers)
- Numerical integration (trapezoidal, backward Euler)

**Example: Moog Ladder Filter Model**
```cpp
class MoogLadderFilter
{
private:
    // 4 transistors = 4 lowpass stages
    double s1 = 0.0, s2 = 0.0, s3 = 0.0, s4 = 0.0;

public:
    void setCutoff(float freqHz, double sampleRate)
    {
        // Calculate circuit parameters from frequency
        double g = std::tan(juce::MathConstants<double>::pi * freqHz / sampleRate);
        // ... circuit coefficient calculation
    }

    float processSample(float input)
    {
        // Newton-Raphson iteration for zero-delay feedback
        double y = input;
        for (int iter = 0; iter < 4; ++iter)
        {
            // Solve implicit equation for feedback
            // ... complex nonlinear algebra
        }

        // Update state variables
        // ... trapezoidal integration

        return static_cast<float>(y);
    }
};
```

#### Pros

1. **Authentic Sound**
   - Circuit modeling captures analog non-linearities
   - Filter resonance "blooms" correctly
   - Oscillator drift and saturation modeled naturally

2. **Academic Interest**
   - Publishable research (novel contributions)
   - Demonstrates advanced DSP knowledge
   - Differentiation from other synths

#### Cons

1. **Extreme Complexity**
   - Requires deep understanding of circuit theory
   - Numerical stability issues common
   - One bug ruins audio (clicks, explosions)

2. **Development Time**
   - 120-160 hours estimated (2x longer than Approach A)
   - Most time spent debugging, not adding features
   - High risk of abandonment

3. **Performance**
   - Iterative solvers (Newton-Raphson) are CPU-intensive
   - Estimated 5-8% CPU per voice (vs 2-3% for Approach A)
   - May require downsampling for realtime performance

4. **Overkill for Nature Marco**
   - User requirements don't demand circuit modeling
   - "Virtual analog" usually means "sounds like analog", not "models circuits"
   - Existing instruments (NexSynthDSP, LocalGalDSP) don't use circuit modeling

5. **TDD Nightmare**
   - Numerical tolerances difficult to predict
   - Test failures may be due to floating-point precision, not bugs
   - Hard to validate "correctness" of circuit models

#### Implementation Complexity: VERY HIGH
**Estimated Hours:** 120-160 hours
- Circuit model research: 20 hours
- Filter implementation: 40 hours
- Numerical solver debugging: 30 hours
- Oscillator modeling: 20 hours
- TDD tests: 15 hours
- Presets: 15 hours

#### Performance: POOR
**Estimated CPU Load:**
- Single voice: 5-8% CPU at 48kHz (2-3x Approach A)
- 16 voices polyphony: 80-128% CPU (may exceed realtime budget)
- Requires optimizations (SIMD, downsampling) to be usable

#### TDD Testability: LOW
**Test Coverage:**
- Numerical stability: MEDIUM (hard to predict edge cases)
- Sound quality: LOW (subjective)
- Performance regressions: HIGH (CPU spikes)
- Modulation system: HIGH (still JUCE-based)

---

## Recommended Approach: Approach A (Pure JUCE DSP)

### Justification

**Why Approach A is the best choice for Nature Marco:**

1. **Fits User Requirements**
   - User requested "virtual analog" (not "circuit model")
   - Oscillator warp is achievable with custom phase manipulation
   - FM synthesis is straightforward with phase modulation
   - All specs (SVF filter, 16-slot mod matrix, macros) are achievable

2. **Consistent with Existing Codebase**
   - NexSynthDSP, LocalGalDSP, SamSamplerDSP all use JUCE DSP module
   - FFI bridge pattern is proven to work
   - Preset system matches existing instruments
   - No build system changes required

3. **Balanced Trade-offs**
   - Performance: Excellent (2-5% CPU per voice)
   - Development time: Reasonable (60-80 hours)
   - Sound quality: High (PolyBLEP anti-aliasing)
   - Maintainability: High (standard JUCE patterns)

4. **TDD-Friendly**
   - All components testable in isolation
   - Existing test framework (`DSPTestFramework.h`) works perfectly
   - 100% test coverage achievable

5. **Future-Proof**
   - JUCE DSP module actively maintained
   - Easy to add new features (wavetables, effects, etc.)
   - Portable to other platforms (VST, AU, AAX)

### When to Choose Other Approaches

**Choose Approach B (Hybrid with Faust) if:**
- User specifically requests academic verification
- Filter design is primary research goal
- Team has Faust expertise

**Choose Approach C (Circuit Modeling) if:**
- User specifically requests "component-level modeling"
- Target audience is analog purists
- Project has unlimited budget and timeline
- Publication in academic journals is goal

---

## File-by-File Implementation Checklist

### Phase 1: DSP Layer (30 hours)

#### [ ] `include/dsp/NatureDSP.h` (4 hours)
**Description:** Main DSP class declaration
**Key Components:**
- `class NatureDSP : public juce::AudioProcessor`
- `struct Oscillator` - Custom oscillator with warp/FM
- `struct SubOscillator` - -1 octave square wave
- `struct Filter` - SVF multimode wrapper
- `struct Envelope` - ADSR with amount
- `struct Voice` - Polyphonic voice
- `class ModulationMatrix` - 16-slot routing
- `class MacroSystem` - 8 macros
- `juce::AudioProcessorValueTreeState parameters`
- Factory preset declarations (30 presets)

**Dependencies:**
- `<juce_audio_processors/juce_audio_processors.h>`
- `<juce_dsp/juce_dsp.h>`
- `../../tests/dsp/DSPTestFramework.h`

#### [ ] `src/dsp/NatureDSP.cpp` (26 hours)

**Subtasks:**

##### [ ] Constructor & Parameter Layout (4 hours)
- Initialize `AudioProcessorValueTreeState`
- Create 50+ parameters (osc1-2, sub, mixer, filter, envs, lfo, global)
- Implement `createParameterLayout()`

##### [ ] Oscillator Implementation (8 hours)
- `Oscillator::processSample()` - Main render loop
- `Oscillator::generateWaveform()` - 5 waveforms with PolyBLEP
- `Oscillator::setWarp()` - Phase warp: -1.0 to +1.0
- `Oscillator::setFMMode()` - Carrier/modulator swap
- `Oscillator::polyBlep()` - Anti-aliasing function
- Waveform generators:
  - `polyBlepSaw()` - Bandlimited sawtooth
  - `polyBlepSquare()` - Bandlimited square
  - `polyBlepTriangle()` - Bandlimited triangle
  - `polyBlepPulse()` - Bandlimited pulse with PWM

##### [ ] Sub-Oscillator Implementation (2 hours)
- Fixed square wave at -1 octave
- `SubOscillator::processSample()`
- Level control

##### [ ] Filter Implementation (4 hours)
- `Filter::prepare()` - Initialize `StateVariableTPTFilter`
- `Filter::setType()` - LP, HP, BP, NOTCH
- `Filter::process()` - Per-sample processing
- Filter envelope amount application
- Cutoff modulation handling

##### [ ] Envelope Implementation (2 hours)
- ADSR using `juce::ADSR`
- `Envelope::setParameters()`
- `Envelope::noteOn()/noteOff()`
- Envelope amount scaling

##### [ ] Voice Implementation (3 hours)
- `Voice::prepare()` - Initialize oscs, filter, env
- `Voice::render()` - Render mono voice to buffer
- Voice allocation logic (round-robin)
- Note-on/note-off handling

##### [ ] Modulation Matrix (3 hours)
- `ModulationMatrix::prepare()` - Allocate atomic arrays
- `ModulationMatrix::processLFOs()` - Update LFO outputs
- `ModulationMatrix::getModulationValue()` - Lock-free read
- `ModulationMatrix::applyModulation()` - Apply to destination
- Source enumeration: LFO1-2, FilterEnv, AmpEnv, Macro1-8, Velocity, Aftertouch
- Destination enumeration: All automatable parameters

##### [ ] Macro System (2 hours)
- `MacroSystem::setMacroValue()` - Update macro
- `MacroSystem::applyMacros()` - Apply to destinations
- `MacroSystem::learnParameter()` - Right-click learn (stub for UI)

##### [ ] Audio Processing Loop (3 hours)
- `NatureDSP::prepareToPlay()`
- `NatureDSP::processBlock()` - Main render
- MIDI message handling
- Voice mixing
- Global effects (if any)

##### [ ] Preset System (3 hours)
- `getPresetState()` - Serialize to JSON
- `setPresetState()` - Deserialize from JSON
- `validatePreset()` - Schema validation
- `getPresetInfo()` - Extract metadata
- Load 30 factory presets

### Phase 2: FFI Layer (8 hours)

#### [ ] `include/ffi/nature_ffi.h` (2 hours)
**Description:** C bridge interface header
**Key Functions:**
- Lifecycle: `nature_create()`, `nature_destroy()`, `nature_initialize()`
- Audio: `nature_process()`
- Parameters: `nature_get/set_parameter_value()`, `nature_get_parameter_count()`
- Modulation: `nature_set_modulation_slot()`, `nature_get_modulation_value()`
- Macros: `nature_set_macro_value()`, `nature_learn_macro()`
- Presets: `nature_save/load/validate_preset()`
- Factory Presets: `nature_get_factory_preset_count()`, `nature_load_factory_preset()`
- Utility: `nature_get_version()`, `nature_get_last_error()`

**Pattern:** Follow `LocalGalFFI.h` exactly (extern "C", opaque handles, const char* strings)

#### [ ] `src/ffi/nature_ffi.cpp` (6 hours)
**Description:** C bridge implementation
**Key Functions:**
- Opaque handle implementation: `struct NatureDSPInstance { NatureDSP* dsp; }`
- Lifecycle wrappers
- Parameter get/set with thread-safety
- Modulation matrix slot configuration
- Macro parameter linking
- Preset JSON marshaling (C++ JSON → C string buffer)
- Error handling with thread-local storage

**Implementation Pattern:** Copy from `LocalGalFFI.cpp` and adapt for Nature Marco

### Phase 3: Tests (12 hours)

#### [ ] `tests/dsp/NatureTests.cpp` (12 hours)
**Description:** TDD test suite using `DSPTestFramework`
**Test Structure:** 80-100 tests

**Test Categories:**

##### [ ] Initialization Tests (3 tests, 1 hour)
- `testConstruction()` - Verify constructor runs
- `testPrepareToPlay()` - Verify preparation succeeds
- `testDefaultParameters()` - Verify all parameters initialized

##### [ ] Oscillator Tests (15 tests, 2 hours)
- `testOscillatorSawtoothWaveform()` - Verify saw shape
- `testOscillatorSquareWaveform()` - Verify square shape
- `testOscillatorTriangleWaveform()` - Verify triangle shape
- `testOscillatorSineWaveform()` - Verify sine shape
- `testOscillatorPulseWaveform()` - Verify pulse shape
- `testOscillatorPulseWidthModulation()` - Verify PWM works
- `testOscillatorWarpNegative()` - Verify negative phase warp
- `testOscillatorWarpPositive()` - Verify positive phase warp
- `testOscillatorWarpExtreme()` - Verify -1.0 and +1.0 behavior
- `testOscillatorFMCMode()` - Verify FM carrier mode
- `testOscillatorFMModulatorMode()` - Verify FM modulator mode
- `testOscillatorFMSwap()` - Verify carrier/modulator swap
- `testOscillatorLinearFM()` - Verify linear FM
- `testOscillatorExponentialFM()` - Verify exponential FM
- `testOscillatorAntiAliasing()` - Verify no aliasing at high frequencies

##### [ ] Sub-Oscillator Tests (3 tests, 1 hour)
- `testSubOscillatorSquareWave()` - Verify square shape
- `testSubOscillatorMinusOneOctave()` - Verify -1 octave detune
- `testSubOscillatorLevel()` - Verify level control

##### [ ] Mixer Tests (5 tests, 1 hour)
- `testMixerOSC1Level()` - Verify OSC1 mixing
- `testMixerOSC2Level()` - Verify OSC2 mixing
- `testMixerSubLevel()` - Verify sub mixing
- `testMixerNoiseLevel()` - Verify noise mixing
- `testMixerStereoPanning()` - Verify stereo output

##### [ ] Filter Tests (10 tests, 1 hour)
- `testFilterLowpassMode()` - Verify LP response
- `testFilterHighpassMode()` - Verify HP response
- `testFilterBandpassMode()` - Verify BP response
- `testFilterNotchMode()` - Verify notch response
- `testFilterCutoffFrequency()` - Verify cutoff tracking
- `testFilterResonance()` - Verify Q control
- `testFilterResonanceSelfOscillation()` - Verify high Q behavior
- `testFilterEnvelopeAmount()` - Verify env modulation
- `testFilterModulation()` - Verify LFO modulation
- `testFilterKeyTracking()` - Verify key tracking (if implemented)

##### [ ] Envelope Tests (8 tests, 1 hour)
- `testEnvelopeAttack()` - Verify attack stage
- `testEnvelopeDecay()` - Verify decay stage
- `testEnvelopeSustain()` - Verify sustain level
- `testEnvelopeRelease()` - Verify release stage
- `testEnvelopeADSRShape()` - Verify full envelope
- `testEnvelopeRetrigger()` - Verify fast retrigger
- `testEnvelopeAmount()` - Verify amount scaling
- `testEnvelopeLoopMode()` - If loop mode implemented

##### [ ] LFO Tests (6 tests, 1 hour)
- `testLFOSineWaveform()` - Verify sine shape
- `testLFOTriangleWaveform()` - Verify triangle shape
- `testLFOSawtoothWaveform()` - Verify saw shape
- `testLFOSquareWaveform()` - Verify square shape
- `testLFOSampleAndHold()` - Verify S&H random
- `testLFORate()` - Verify rate control

##### [ ] Modulation Matrix Tests (12 tests, 2 hours)
- `testModulationMatrix16Slots()` - Verify 16 slots available
- `testModulationLFO1ToFilterCutoff()` - Verify LFO1 → filter
- `testModulationLFO2ToOscPitch()` - Verify LFO2 → pitch
- `testModulationEnvToFilterCutoff()` - Verify env → filter
- `testModulationVelocityToAmp()` - Verify velocity → amp
- `testModulationAftertouchToFilter()` - Verify aftertouch → filter
- `testModulationMacroToMultipleParams()` - Verify macro → multiple destinations
- `testModulationBipolarMode()` - Verify bipolar modulation
- `testModulationUnipolarMode()` - Verify unipolar modulation
- `testModulationCurveLinear()` - Verify linear curve
- `testModulationCurveExponential()` - Verify exponential curve
- `testModulationSlotDisable()` - Verify zero amount disables

##### [ ] Macro System Tests (8 tests, 1 hour)
- `testMacroCount8()` - Verify 8 macros
- `testMacroSetValue()` - Verify macro value setting
- `testMacroGetValue()` - Verify macro value reading
- `testMacroSingleDestination()` - Verify macro → single parameter
- `testMacroMultipleDestinations()` - Verify macro → multiple parameters
- `testMacroMinMaxScaling()` - Verify parameter range mapping
- `testMacroAmount()` - Verify per-parameter amount
- `testMacroPresetSaveLoad()` - Verify macro saved in preset

##### [ ] Voice Allocation Tests (6 tests, 1 hour)
- `testPolyphony16Voices()` - Verify 16 voices max
- `testVoiceAllocationRoundRobin()` - Verify round-robin
- `testVoiceStealOldest()` - Verify voice stealing
- `testMonophonicMode()` - Verify monophonic behavior
- `testLegatoMode()` - Verify legato mode
- `testGlidePortamento()` - Verify pitch glide

##### [ ] Preset System Tests (10 tests, 1 hour)
- `testPresetSaveToJSON()` - Verify serialization
- `testPresetLoadFromJSON()` - Verify deserialization
- `testPresetValidateCorrect()` - Verify validation accepts valid preset
- `testPresetValidateMissingParameter()` - Verify validation rejects missing param
- `testPresetValidateInvalidRange()` - Verify validation rejects out-of-range
- `testPresetGetInfo()` - Verify metadata extraction
- `testFactoryPresetCount()` - Verify 30 presets
- `testFactoryPresetLoadBass()` - Verify bass preset loads
- `testFactoryPresetLoadLead()` - Verify lead preset loads
- `testPresetAllParametersRestored()` - Verify all params restored

##### [ ] Integration Tests (8 tests, 1 hour)
- `testFullSignalPath()` - Verify MIDI → audio output
- `testPolyphonicChord()` - Verify 16-note chord
- `testNoteOnNoteOff()` - Verify envelope complete cycle
- `testPitchBend()` - Verify pitch bend range
- `testModulationWheel()` - Verify mod wheel routing
- `testSustainPedal()` - Verify sustain pedal behavior
- `testAllNotesOff()` - Verify panic button
- `testCPUPerformance()` - Verify CPU < 5% per voice

### Phase 4: Presets (14 hours)

#### [ ] `presets/Nature/Init_Bass.json` (20 min)
**Category:** Bass
**Starting Point:** Clean bass template
**Parameters:**
- OSC1: Saw, 0.0 warp, detune 0, level 0.7
- OSC2: Off
- Sub: On, level 0.5
- Filter: LP, cutoff 800Hz, resonance 0.3
- Filter ENV: Attack 10ms, decay 200ms, sustain 0.4, release 100ms, amount 0.5
- Amp ENV: Attack 5ms, decay 150ms, sustain 0.0, release 200ms
- Modulation: LFO1 → filter cutoff (slow, shallow)

#### [ ] `presets/Nature/Acid_Bass.json` (20 min)
**Category:** Bass
**Character:** Resonant, squelchy
**Parameters:**
- OSC1: Saw, warp +0.3, level 0.8
- Filter: HP, cutoff 1200Hz, resonance 0.9
- Filter ENV: High amount (0.8)
- Modulation: Velocity → filter resonance

#### [ ] `presets/Nature/FM_Sub.json` (20 min)
**Category:** Bass
**Character:** FM-style metallic bass
**Parameters:**
- OSC1: Sine, carrier mode
- OSC2: Sine, modulator mode, ratio 2.0, depth 0.7
- Filter: LP, cutoff 2000Hz, resonance 0.5
- Filter ENV: Amount 0.6

#### [ ] `presets/Nature/Deep_Bass.json` (20 min)
**Category:** Bass
**Character:** Sub-heavy, clean
**Parameters:**
- OSC1: Triangle, level 0.6
- Sub: Square, level 0.9
- Filter: LP, cutoff 400Hz, resonance 0.2
- Amp ENV: Slow attack (20ms), long release (400ms)

#### [ ] `presets/Nature/Saw_Lead.json` (20 min)
**Category:** Lead
**Character:** Classic sawtooth lead
**Parameters:**
- OSC1: Saw, level 0.8
- OSC2: Saw, detune +7 cents, level 0.6
- Filter: LP, cutoff 3000Hz, resonance 0.4
- Filter ENV: Amount 0.5
- Amp ENV: Fast attack, medium release

#### [ ] `presets/Nature/Square_Lead.json` (20 min)
**Category:** Lead
**Character:** Hollow square wave
**Parameters:**
- OSC1: Square, PWM 50%, level 0.7
- Filter: BP, cutoff 1500Hz, resonance 0.6
- Modulation: LFO1 → PWM (slow)

#### [ ] `presets/Nature/FM_Bell.json` (20 min)
**Category:** Lead
**Character:** Metallic FM bell
**Parameters:**
- OSC1: Sine, carrier
- OSC2: Sine, modulator, ratio 5.0, depth 0.9
- Filter: HP, cutoff 1000Hz, resonance 0.0
- Amp ENV: Fast attack, long decay (1.0s)

#### [ ] `presets/Nature/Warp_Lead.json` (20 min)
**Category:** Lead
**Character:** Experimental phase warp
**Parameters:**
- OSC1: Saw, warp +0.8, level 0.8
- Filter: LP, cutoff 4000Hz, resonance 0.7
- Modulation: Mod wheel → warp amount

#### [ ] `presets/Nature/Evolution_Pad.json` (20 min)
**Category:** Pad
**Character:** Slow-evolving pad
**Parameters:**
- OSC1: Saw, level 0.5
- OSC2: Triangle, detune -5 cents, level 0.5
- Filter: LP, cutoff 2000Hz, resonance 0.3
- Amp ENV: Attack 800ms, decay 500ms, sustain 0.7, release 1500ms
- Modulation: LFO2 → filter cutoff (very slow, 0.2Hz)

#### [ ] `presets/Nature/Space_Pad.json` (20 min)
**Category:** Pad
**Character:** Detuned, spacious
**Parameters:**
- OSC1: Saw, level 0.6
- OSC2: Saw, detune +15 cents, level 0.6
- Filter: LP, cutoff 1500Hz, resonance 0.4
- Amp ENV: Very slow attack (1.2s)
- Modulation: LFO1 → pan (stereo width)

#### [ ] `presets/NatureMacro/Warm_Pad.json` (20 min)
**Category:** Pad
**Character:** Warm, filtered
**Parameters:**
- OSC1: Triangle, level 0.7
- OSC2: Triangle, detune +3 cents, level 0.5
- Sub: Square, level 0.3
- Filter: LP, cutoff 800Hz, resonance 0.2
- Amp ENV: Attack 400ms, sustain 0.8

#### [ ] `presets/Nature/Pluck_Synth.json` (20 min)
**Category:** Keys
**Character:** Percussive pluck
**Parameters:**
- OSC1: Saw, level 0.8
- Filter: LP, cutoff 5000Hz, resonance 0.5
- Filter ENV: Attack 0ms, decay 300ms, amount 0.9
- Amp ENV: Attack 0ms, decay 400ms, sustain 0.0

#### [ ] `presets/Nature/Electric_Piano.json` (20 min)
**Category:** Keys
**Character:** FM electric piano
**Parameters:**
- OSC1: Sine, carrier
- OSC2: Sine, modulator, ratio 1.0, depth 0.5
- Filter: LP, cutoff 4000Hz, resonance 0.1
- Amp ENV: Fast attack, medium decay

#### [ ] `presets/Nature/Hard_Sync.json` (20 min)
**Category:** Keys
**Character:** Hard sync lead
**Parameters:**
- OSC1: Saw, level 1.0 (slave)
- OSC2: Saw, level 0.0 (master, sync source)
- Filter: LP, cutoff 3000Hz, resonance 0.6
- Note: If hard sync not implemented, use FM instead

#### [ ] `presets/Nature/Atmosphere_FX.json` (20 min)
**Category:** FX
**Character:** Evolving texture
**Parameters:**
- OSC1: Saw, warp -0.5, level 0.6
- OSC2: Triangle, level 0.4
- Filter: BP, cutoff 1200Hz, resonance 0.8
- Modulation: LFO1 → filter cutoff, LFO2 → warp

#### [ ] `presets/Nature/Alien_Texture.json` (20 min)
**Category:** FX
**Character:** Unusual, metallic
**Parameters:**
- OSC1: Pulse, PWM 70%, level 0.7
- OSC2: Saw, detune +50 cents, level 0.5
- Filter: Notch, cutoff 2000Hz, resonance 0.9
- Modulation: LFO1 → PWM, LFO2 → detune

#### [ ] `presets/Nature/Glitch_Lead.json` (20 min)
**Category:** Experimental
**Character:** Chaotic, rhythmic
**Parameters:**
- OSC1: Saw, warp +0.9, level 0.8
- Filter: Sample-and-hold on cutoff, resonance 0.7
- Modulation: LFO1 (S&H) → filter cutoff

#### [ ] `presets/Nature/Chaos_FM.json` (20 min)
**Category:** Experimental
**Character:** Unpredictable FM
**Parameters:**
- OSC1: Sine, carrier
- OSC2: Sine, modulator, ratio 13.0, depth 1.0
- Filter: LP, cutoff 5000Hz, resonance 0.0
- Modulation: Velocity → FM depth

#### [ ] Additional presets (12 more, 4 hours)
**Categories:** Keys (3), Pluck (2), Brass (2), Vocal (2), Percussive (3)

### Phase 5: Documentation & Integration (6 hours)

#### [ ] `docs/plans/NATURE_MARCO_IMPLEMENTATION.md` (2 hours)
**Description:** Developer implementation guide
**Sections:**
- Architecture overview
- DSP algorithm details
- Parameter reference
- Modulation matrix guide
- Preset design tips
- Performance optimization
- Troubleshooting

#### [ ] `docs/plans/NATURE_MARCO_PRESET_GUIDE.md` (1 hour)
**Description:** Sound design guide for Nature Marco
**Sections:**
- Bass programming techniques
- Lead synthesis strategies
- Pad design workflow
- FX sound creation
- Oscillator warp creative uses
- FM synthesis recipes

#### [ ] CMakeLists.txt Updates (1 hour)
**Changes:**
- Add `include/dsp/NatureDSP.h` to headers
- Add `src/dsp/NatureDSP.cpp` to sources
- Add `include/ffi/nature_ffi.h` to headers
- Add `src/ffi/nature_ffi.cpp` to sources
- Add `tests/dsp/NatureTests.cpp` to tests
- Create `NatureDSP` target (if separate build)

#### [ ] Build Verification (2 hours)
**Tasks:**
- Build with CMake (Debug and Release)
- Run all tests (verify 100% pass rate)
- Profile CPU usage (verify < 5% per voice)
- Test FFI bridge with simple C program
- Validate preset load/save

---

## TDD Test Strategy

### RED-GREEN-REFACTOR Workflow

#### Phase 1: RED (Write Failing Tests)
**Goal:** Define desired behavior before implementation
**Example:**
```cpp
beginTest("Oscillator Warp - Positive Phase");
{
    NatureDSP dsp;
    dsp.prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> buffer(1, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

    // Set positive warp
    dsp.setParameterValue("osc1_warp", 0.5f);
    dsp.setParameterValue("osc1_shape", 0.0f); // Saw

    dsp.processBlock(buffer, midi);

    // Verify output differs from non-warped saw
    float maxSample = buffer.getMagnitude(0, 0, 512);
    expect(maxSample > 0.0f); // RED phase - fails until warp implemented
}
```

#### Phase 2: GREEN (Make Tests Pass)
**Goal:** Minimal implementation to pass tests
**Example:**
```cpp
// In NatureOscillator::processSample()
float warpedPhase = phase + (warp * std::sin(phase * 2.0 * juce::MathConstants<double>::pi));
// Tests now pass
```

#### Phase 3: REFACTOR (Improve Code)
**Goal:** Clean up while keeping tests green
**Example:**
```cpp
// Extract to helper function
float NatureOscillator::applyWarp(double phase) const
{
    return phase + (warp * std::sin(phase * 2.0 * juce::MathConstants<double>::pi));
}
// Tests still pass, code is cleaner
```

### Key Test Cases

#### Oscillator Warp
- **Negative warp (-1.0 to 0.0):** Phase delayed, waveform "pulls back"
- **Positive warp (0.0 to +1.0):** Phase advanced, waveform "pushes forward"
- **Zero warp (0.0):** No phase modification
- **Anti-aliasing:** Verify no significant harmonics above Nyquist at 10kHz

#### FM Synthesis
- **Carrier mode:** Outputs modulated phase
- **Modulator mode:** Outputs pure sine * FM depth
- **Linear FM:** Frequency deviation = constant * modulator output
- **Exponential FM:** Frequency deviation = exp(modulator output)
- **Carrier/modulator swap:** Roles reversed, timbre changes

#### Modulation Matrix
- **16-slot capacity:** All slots independently configurable
- **Thread-safety:** Modulation amounts updated from UI without audio glitches
- **Bipolar vs unipolar:** Bipolar adds/subtracts, unipolar only adds
- **Curve types:** Linear (direct), exponential (logarithmic)

#### Filter
- **LP mode:** Low frequencies pass, high frequencies attenuated
- **HP mode:** High frequencies pass, low frequencies attenuated
- **BP mode:** Band of frequencies pass
- **Notch mode:** Band of frequencies attenuated
- **Resonance:** Q control increases peak at cutoff

#### Envelopes
- **Attack time:** Time to reach peak from note-on
- **Decay time:** Time to reach sustain from peak
- **Sustain level:** Held level while note depressed
- **Release time:** Time to reach zero from note-off

#### Preset System
- **JSON schema:** All required fields present
- **Parameter validation:** All values within min/max range
- **Metadata preservation:** Name, author, category intact
- **Factory presets:** All 30 presets load correctly

---

## Estimated Timeline

### Phase 1: DSP Implementation (30 hours)
- Week 1: Oscillators, sub-oscillator, mixer (15 hours)
- Week 1: Filter, envelopes, voice architecture (8 hours)
- Week 2: Modulation matrix, macro system (7 hours)

### Phase 2: FFI Bridge (8 hours)
- Week 2: FFI header, lifecycle functions (3 hours)
- Week 2: Parameter access, modulation, macros (3 hours)
- Week 2: Preset system, error handling (2 hours)

### Phase 3: TDD Tests (12 hours)
- Week 3: Oscillator, filter, envelope tests (5 hours)
- Week 3: Modulation, macro, voice tests (4 hours)
- Week 3: Preset, integration tests (3 hours)

### Phase 4: Presets (14 hours)
- Week 3: Bass presets (4) - 2 hours
- Week 4: Lead presets (4) - 2 hours
- Week 4: Pad presets (3) - 2 hours
- Week 4: Keys presets (4) - 2 hours
- Week 4: FX/experimental presets (6) - 3 hours
- Week 4: Remaining presets (9) - 3 hours

### Phase 5: Documentation & Integration (6 hours)
- Week 4: Implementation guide, preset guide (3 hours)
- Week 4: CMake updates, build verification (3 hours)

### Total: 70 hours (2 weeks full-time)

---

## Performance Optimization Strategies

### CPU Optimization
1. **PolyBLEP Tables:** Pre-compute PolyBLEP correction function in lookup table
2. **SIMD:** Use `juce::dsp::SIMDRegister<float>` for parallel voice processing
3. **Smoothing:** Use `juce::dsp::SmoothedValue<float>` for parameter transitions
4. **Oversampling:** Optional 2x oversampling for oscillators only (not entire signal path)

### Memory Optimization
1. **Voice Pool:** Pre-allocate 16 voices, avoid `new` in audio thread
2. **Lock-Free Ring Buffers:** For modulation matrix signal routing
3. **Parameter Smoothing:** Use `std::atomic<float>` instead of critical sections

### Realtime Safety
1. **No Allocations:** All memory allocated in `prepareToPlay()`
2. **No Locks:** Use atomics for parameter sharing
3. **No Exceptions:** Wrap audio thread code in try-catch (though should never throw)

---

## Risk Assessment

### Technical Risks

#### HIGH RISK: Oscillator Warp Sound Quality
**Issue:** Phase warp may not produce musically useful results
**Mitigation:**
- Prototype warp algorithm early (Week 1)
- Test with musician feedback
- Fall back to traditional detuning if warp fails

#### MEDIUM RISK: FM Synthesis Aliasing
**Issue:** FM produces strong harmonics, may alias above Nyquist
**Mitigation:**
- Use oversampling (2x or 4x) for FM operators
- Limit FM modulation index to safe range
- Implement anti-aliased FM (more complex)

#### LOW RISK: Modulation Matrix Performance
**Issue:** 16 modulation slots may be CPU-intensive
**Mitigation:**
- Use lock-free atomics (already planned)
- Pre-compute modulation routing (no per-sample string lookups)
- Profile early, optimize hot paths

### Project Risks

#### MEDIUM RISK: Preset Design Time
**Issue:** 30 presets may take longer than 14 hours
**Mitigation:**
- Start with 10 high-quality presets
- Use preset variations (tweak existing presets)
- Document preset design workflow

#### LOW RISK: FFI Bridge Bugs
**Issue:** C/C++ boundary may have memory leaks or crashes
**Mitigation:**
- Follow existing pattern from LocalGalFFI
- Use address sanitizer (ASan) during testing
- Comprehensive FFI unit tests

---

## Success Criteria

### Functional Requirements
- ✅ All user-specified features implemented
- ✅ 30 factory presets load without error
- ✅ 16-voice polyphony works at 48kHz
- ✅ Modulation matrix routes all sources to all destinations
- ✅ Macro system controls multiple parameters
- ✅ Preset save/load/validate works

### Quality Requirements
- ✅ 100% of TDD tests pass (80-100 tests)
- ✅ CPU usage < 5% per voice at 48kHz
- ✅ No audio artifacts (clicks, pops, zipper noise)
- ✅ Thread-safe (no race conditions, deadlocks)
- ✅ No memory leaks (verified with Valgrind/ASan)

### Integration Requirements
- ✅ FFI bridge compiles without warnings
- ✅ FFI bridge callable from C
- ✅ CMake builds on macOS, Linux, Windows (future)
- ✅ JSON presets compatible with preset system

### Sound Quality Requirements
- ✅ Oscillator waveforms sound "analog" (rich harmonics)
- ✅ Filter resonance sounds "musical" (smooth sweep)
- ✅ FM synthesis produces "bell-like" tones
- ✅ Oscillator warp produces interesting timbres
- ✅ Envelopes respond naturally (snappy attacks, smooth decays)

---

## Conclusion

**Approach A (Pure JUCE DSP)** is the recommended implementation strategy for Nature Marco. It balances sound quality, performance, development time, and maintainability while meeting all user requirements.

**Key Strengths:**
- Production-ready JUCE DSP module
- Zero-delay feedback filter (`StateVariableTPTFilter`)
- Lock-free modulation matrix (realtime-safe)
- Custom oscillators with PolyBLEP anti-aliasing
- Consistent with existing codebase patterns

**Estimated Timeline:** 70 hours (2 weeks full-time)
**Test Coverage:** 80-100 TDD tests (100% pass rate)
**CPU Performance:** 2-5% per voice at 48kHz
**Presets:** 30 factory presets

**Next Steps:**
1. Review and approve this research document
2. Create implementation plan from file-by-file checklist
3. Begin Phase 1: DSP implementation (oscillators first)
4. Follow TDD RED-GREEN-REFACTOR workflow
5. Complete all phases, then integrate with Swift/tvOS

---

**Document Status:** ✅ COMPLETE - READY FOR IMPLEMENTATION
**Recommendation Confidence:** HIGH - Approach A based on proven patterns from NexSynthDSP, LocalGalDSP, and SamSamplerDSP
**Approval Required:** User confirmation to proceed with Phase 1

---

**References:**
- LEVEL2_RESEARCH_BEST_PRACTICES.md (this codebase)
- COMPLETE_APPLETV_HANDOFF.md (this codebase)
- JUCE DSP Module Documentation (docs.juce.com)
- "Designing Audio Effect Plugins in C++" by Will Pirkle
- "Audio Effects: Theory, Implementation and Application" (DAFx)
- PolyBLEP paper: "Hard Sync Without Aliasing" (Kleimola)
