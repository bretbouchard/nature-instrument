# Aether - Implementation Plan

**Status:** In Progress
**Start Date:** 2025-12-25
**Methodology:** TDD (Test-Driven Development)
**Approach:** Pure Modal Synthesis (32 modes per voice, 16-voice polyphony)

---

## Implementation Phases

### Phase 1: Core DSP Components (Week 1)

#### Task 1.1: Single Modal Filter (Direct Form II Biquad)
**File:** `include/dsp/AetherDSP.h` (partial)
**Test:** `tests/dsp/NatureAetherTests.cpp` (ModalFilter tests)

**RED:** Write failing test
- Impulse response at known frequency
- Verify resonant peak magnitude
- Check decay time matches T60

**GREEN:** Make test pass
- Implement Direct Form II biquad
- Calculate coefficients from frequency/decay
- Add denormal prevention

**REFACTOR:** Clean up
- Extract coefficient calculation
- Add inline documentation

---

#### Task 1.2: Resonator Bank (8 modes - MVP)
**File:** `include/dsp/AetherDSP.h` (partial)
**Test:** `tests/dsp/NatureAetherTests.cpp` (ResonatorBank tests)

**RED:** Write failing test
- Impulse response produces 8 spectral peaks
- Each mode at correct frequency
- Verify total output normalization

**GREEN:** Make test pass
- Implement 8-mode array
- Process all modes and sum
- Normalize by active mode count

**REFACTOR:** Clean up
- Optimize mode loop (skip inactive)
- Add frequency distribution strategy

---

#### Task 1.3: Exciter (Noise Burst with Envelope)
**File:** `include/dsp/AetherDSP.h` (partial)
**Test:** `tests/dsp/NatureAetherTests.cpp` (Exciter tests)

**RED:** Write failing test
- NoteOn produces signal burst
- NoteOff fades to zero
- Velocity affects output level

**GREEN:** Make test pass
- Generate filtered noise
- Implement ADSR envelope
- Map velocity to pressure

**REFACTOR:** Clean up
- Add exciter color filter (SVF)
- Smooth pressure transitions

---

#### Task 1.4: Feedback Loop with Delay Line
**File:** `include/dsp/AetherDSP.h` (partial)
**Test:** `tests/dsp/NatureAetherTests.cpp` (FeedbackLoop tests)

**RED:** Write failing test
- Stability test (max feedback, no clipping)
- Delay time changes correctly
- Saturation prevents runaway

**GREEN:** Make test pass
- Implement delay line with interpolation
- Add std::tanh() soft clipping
- Hard limit feedback to 0.95

**REFACTOR:** Clean up
- Optimize interpolation (Lagrange)
- Add leaky integrator option

---

#### Task 1.5: Complete Voice Structure
**File:** `include/dsp/AetherDSP.h` (Voice struct)
**Test:** `tests/dsp/NatureAetherTests.cpp` (Voice tests)

**RED:** Write failing test
- NoteOn → NoteOff produces sustained resonance
- Velocity affects brightness
- Polyphony (multiple voices)

**GREEN:** Make test pass
- Integrate Exciter + Resonator + Feedback
- Add SVF filter and envelope
- Implement voice lifecycle

**REFACTOR:** Clean up
- Optimize processing loop
- Add voice stealing

---

### Phase 2: Main Processor & Parameters (Week 1-2)

#### Task 2.1: Parameter Layout
**File:** `src/dsp/AetherDSP.cpp` (createParameterLayout)

**Parameters:**
- Exciter: attack, decay, sustain, release, color
- Resonator: modeCount, decay, inharmonicity, spread
- Feedback: amount, delayTime, saturationDrive
- Filter: cutoff, resonance, type, envAmount
- Envelope: attack, decay, sustain, release
- Global: masterGain, polyphony, glideTime

---

#### Task 2.2: Main Processor Loop
**File:** `src/dsp/AetherDSP.cpp` (processBlock)

**Implementation:**
- MIDI message handling
- Voice allocation
- Voice processing loop
- Master effects (reverb)
- Output gain

---

#### Task 2.3: Preset System
**File:** `src/dsp/AetherDSP.cpp` (getPresetState/setPresetState)

**Implementation:**
- JSON serialization
- Parameter validation
- Factory preset loading

---

### Phase 3: FFI Bridge (Week 2)

#### Task 3.1: C Interface Header
**File:** `include/ffi/nature_aether_ffi.h`

**Functions:**
- Lifecycle: create, destroy, initialize
- Processing: process
- Parameters: get/set
- Presets: load, save, validate

---

#### Task 3.2: FFI Implementation
**File:** `src/ffi/nature_aether_ffi.cpp`

**Implementation:**
- Exception handling
- String conversion
- JSON serialization
- Memory management

---

### Phase 4: Factory Presets (Week 2)

#### Task 4.1: Ambient Presets (5)
- Ethereal Pad
- Space Drone
- Cosmic Wind
- Deep Resonance
- Floating

---

#### Task 4.2: Metallic Presets (5)
- Crystal Bell
- Bronze Gong
- Singing Bowl
- Tubular Bells
- Wind Chimes

---

#### Task 4.3: Wooden Presets (5)
- Marimba Warmth
- Kalimba Dream
- Xylophone Spark
- Wood Block
- Bamboo

---

#### Task 4.4: Experimental Presets (5)
- Inharmonic Chaos
- Ghost Resonance
- Alien Texture
- Reverse Decay
- Feedback Storm

---

## Test Strategy

### Unit Tests (25 tests)
1. ModalFilter: 5 tests
2. ResonatorBank: 5 tests
3. Exciter: 5 tests
4. FeedbackLoop: 5 tests
5. Voice: 5 tests

### Integration Tests (15 tests)
6. Polyphony: 5 tests
7. Parameters: 5 tests
8. Presets: 5 tests

### System Tests (10 tests)
9. Stability: 3 tests
10. Performance: 3 tests
11. Realtime Safety: 4 tests

### Performance Tests (5 tests)
12. CPU usage: 2 tests
13. Memory usage: 2 tests
14. Latency: 1 test

**Total: 55 tests**

---

## Performance Targets

- CPU: < 10% for 16 voices @ 48kHz
- Memory: < 1MB total
- Latency: < 5ms algorithmic
- NO allocations in processBlock

---

## Current Progress

- [ ] Task 1.1: Single Modal Filter
- [ ] Task 1.2: Resonator Bank (8 modes)
- [ ] Task 1.3: Exciter
- [ ] Task 1.4: Feedback Loop
- [ ] Task 1.5: Complete Voice Structure

---

**Last Updated:** 2025-12-25
**Status:** Phase 1 - Task 1.1 (Modal Filter Implementation)
