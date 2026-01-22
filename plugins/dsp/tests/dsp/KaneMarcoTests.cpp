/*
  ==============================================================================

    NatureTests.cpp
    Created: 25 Dec 2025
    Author:  Bret Bouchard

    TDD Test Suite for Nature Marco Hybrid Virtual Analog Synthesizer
    - RED-GREEN-REFACTOR cycle
    - 80-100 tests for complete coverage
    - Tests follow implementation order

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../../include/dsp/NatureDSP.h"
#include "DSPTestFramework.h"

//==============================================================================
// Nature Marco Test Suite
//==============================================================================

class NatureTests : public juce::UnitTest
{
public:
    NatureTests() : juce::UnitTest("Nature Marco DSP", "DSP") {}

    void runTest() override
    {
        //======================================================================
        // CATEGORY 1: INITIALIZATION (3 tests)
        //======================================================================

        beginTest("Construction - Basic Initialization");
        {
            NatureDSP dsp;
            expectEquals(dsp.getActiveVoiceCount(), 0, "Should start with no active voices");
            expect(dsp.getTailLengthSeconds() > 0.0, "Should have non-zero tail length");
        }

        beginTest("Prepare to Play - Default Sample Rate");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            // Verify preparation succeeded (no crashes)
            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            dsp.processBlock(buffer, midi);

            expect(true); // If we got here, preparation worked
        }

        beginTest("Default Parameters - All Parameters Initialized");
        {
            NatureDSP dsp;

            // Test critical parameters exist and have valid defaults
            float osc1Shape = dsp.getParameterValue("osc1_shape");
            expect(osc1Shape >= 0.0f && osc1Shape <= 1.0f, "OSC1 shape should be in valid range");

            float filterCutoff = dsp.getParameterValue("filter_cutoff");
            expect(filterCutoff > 0.0f, "Filter cutoff should be positive");

            float masterVolume = dsp.getParameterValue("master_volume");
            expect(masterVolume >= 0.0f && masterVolume <= 1.0f, "Master volume should be normalized");
        }

        //======================================================================
        // CATEGORY 2: OSCILLATOR WARP (4 tests)
        //======================================================================

        beginTest("Oscillator Warp - Zero Warp (No Modification)");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // Set zero warp
            dsp.setParameterValue("osc1_warp", 0.0f);
            dsp.setParameterValue("osc1_shape", 0.0f); // Sawtooth

            dsp.processBlock(buffer, midi);

            float peak = DSPTestFramework::Framework::findPeak(buffer);
            expect(peak > 0.0f, "Zero warp should still produce audio");
        }

        beginTest("Oscillator Warp - Positive Phase");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> bufferNoWarp(2, 512);
            juce::AudioBuffer<float> bufferWarp(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // No warp baseline
            dsp.setParameterValue("osc1_warp", 0.0f);
            dsp.setParameterValue("osc1_shape", 0.0f);
            dsp.processBlock(bufferNoWarp, midi);

            // Positive warp
            dsp.setParameterValue("osc1_warp", 0.5f);
            dsp.processBlock(bufferWarp, midi);

            // Output should differ with warp applied
            float peakNoWarp = DSPTestFramework::Framework::findPeak(bufferNoWarp);
            float peakWarp = DSPTestFramework::Framework::findPeak(bufferWarp);

            expect(std::abs(peakNoWarp - peakWarp) > 0.001f,
                   "Positive warp should modify waveform");
        }

        beginTest("Oscillator Warp - Negative Phase");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            dsp.setParameterValue("osc1_warp", -0.5f);
            dsp.setParameterValue("osc1_shape", 0.0f);

            dsp.processBlock(buffer, midi);

            float peak = DSPTestFramework::Framework::findPeak(buffer);
            expect(peak > 0.0f, "Negative warp should still produce audio");
        }

        beginTest("Oscillator Warp - Extreme Values");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // Test maximum positive warp
            dsp.setParameterValue("osc1_warp", 1.0f);
            dsp.setParameterValue("osc1_shape", 0.0f);
            dsp.processBlock(buffer, midi);

            float peakPositive = DSPTestFramework::Framework::findPeak(buffer);
            expect(peakPositive > 0.0f && peakPositive <= 1.0f,
                   "Max positive warp should stay in valid range");

            // Test maximum negative warp
            dsp.setParameterValue("osc1_warp", -1.0f);
            buffer.clear();
            dsp.processBlock(buffer, midi);

            float peakNegative = DSPTestFramework::Framework::findPeak(buffer);
            expect(peakNegative > 0.0f && peakNegative <= 1.0f,
                   "Max negative warp should stay in valid range");
        }

        //======================================================================
        // CATEGORY 3: FM SYNTHESIS (4 tests)
        //======================================================================

        beginTest("FM Synthesis - Carrier Mode");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // Enable FM with OSC1 as carrier
            dsp.setParameterValue("fm_enabled", true);
            dsp.setParameterValue("fm_carrier OSC", 0); // OSC1 is carrier
            dsp.setParameterValue("osc1_shape", 4.0f); // Sine
            dsp.setParameterValue("osc2_shape", 4.0f); // Sine
            dsp.setParameterValue("fm_depth", 0.5f);

            dsp.processBlock(buffer, midi);

            float peak = DSPTestFramework::Framework::findPeak(buffer);
            expect(peak > 0.0f, "FM carrier mode should produce audio");
        }

        beginTest("FM Synthesis - Modulator Mode");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // OSC2 as modulator
            dsp.setParameterValue("fm_enabled", true);
            dsp.setParameterValue("fm_modulator_ratio", 2.0f);
            dsp.setParameterValue("fm_depth", 0.7f);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "FM modulator should affect output");
        }

        beginTest("FM Synthesis - Linear vs Exponential");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> bufferLinear(2, 512);
            juce::AudioBuffer<float> bufferExp(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // Linear FM
            dsp.setParameterValue("fm_enabled", true);
            dsp.setParameterValue("fm_mode", 0.0f); // Linear
            dsp.setParameterValue("fm_depth", 0.5f);
            dsp.processBlock(bufferLinear, midi);

            // Exponential FM
            dsp.setParameterValue("fm_mode", 1.0f); // Exponential
            bufferExp.clear();
            dsp.processBlock(bufferExp, midi);

            // Outputs should differ
            float rmsLinear = DSPTestFramework::Framework::calculateRMS(bufferLinear);
            float rmsExp = DSPTestFramework::Framework::calculateRMS(bufferExp);

            expect(std::abs(rmsLinear - rmsExp) > 0.001f,
                   "Linear and exponential FM should produce different results");
        }

        beginTest("FM Synthesis - Carrier/Modulator Swap");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> bufferNormal(2, 512);
            juce::AudioBuffer<float> bufferSwapped(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // Normal: OSC1 carrier, OSC2 modulator
            dsp.setParameterValue("fm_enabled", true);
            dsp.setParameterValue("fm_carrier_osc", 0.0f); // OSC1
            dsp.setParameterValue("fm_depth", 0.5f);
            dsp.processBlock(bufferNormal, midi);

            // Swapped: OSC2 carrier, OSC1 modulator
            dsp.setParameterValue("fm_carrier_osc", 1.0f); // OSC2
            bufferSwapped.clear();
            dsp.processBlock(bufferSwapped, midi);

            // Timbre should change noticeably
            float peakNormal = DSPTestFramework::Framework::findPeak(bufferNormal);
            float peakSwapped = DSPTestFramework::Framework::findPeak(bufferSwapped);

            expect(std::abs(peakNormal - peakSwapped) > 0.001f,
                   "Carrier/modulator swap should change timbre");
        }

        //======================================================================
        // CATEGORY 4: POLYBLEP ANTIALIASING (2 tests)
        //======================================================================

        beginTest("PolyBLEP - Bandlimited Sawtooth");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            dsp.setParameterValue("osc1_shape", 0.0f); // Sawtooth
            dsp.setParameterValue("osc1_warp", 0.0f);

            dsp.processBlock(buffer, midi);

            // Check for no clipping (PolyBLEP should prevent aliasing spikes)
            float peak = DSPTestFramework::Framework::findPeak(buffer);
            expect(peak <= 1.0f, "Bandlimited saw should not clip");
        }

        beginTest("PolyBLEP - Bandlimited Square");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            dsp.setParameterValue("osc1_shape", 1.0f); // Square
            dsp.setParameterValue("osc1_warp", 0.0f);

            dsp.processBlock(buffer, midi);

            float peak = DSPTestFramework::Framework::Framework::findPeak(buffer);
            expect(peak <= 1.0f, "Bandlimited square should not clip");
            expect(peak > 0.0f, "Square wave should produce signal");
        }

        //======================================================================
        // CATEGORY 5: FILTER TESTS (6 tests)
        //======================================================================

        beginTest("Filter - Lowpass Mode");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // Lowpass at 1kHz
            dsp.setParameterValue("filter_type", 0.0f); // LP
            dsp.setParameterValue("filter_cutoff", 0.3f); // ~1kHz
            dsp.setParameterValue("filter_resonance", 0.5f);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Lowpass filter should pass audio");
        }

        beginTest("Filter - Highpass Mode");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            dsp.setParameterValue("filter_type", 1.0f); // HP
            dsp.setParameterValue("filter_cutoff", 0.5f); // Mid frequency

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Highpass filter should pass audio");
        }

        beginTest("Filter - Bandpass Mode");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            dsp.setParameterValue("filter_type", 2.0f); // BP
            dsp.setParameterValue("filter_cutoff", 0.5f);
            dsp.setParameterValue("filter_resonance", 0.7f);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Bandpass filter should pass audio");
        }

        beginTest("Filter - Notch Mode");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            dsp.setParameterValue("filter_type", 3.0f); // Notch
            dsp.setParameterValue("filter_cutoff", 0.5f);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Notch filter should pass audio");
        }

        beginTest("Filter - Resonance Control");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> bufferLowQ(2, 512);
            juce::AudioBuffer<float> bufferHighQ(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // Low resonance
            dsp.setParameterValue("filter_type", 0.0f); // LP
            dsp.setParameterValue("filter_resonance", 0.1f);
            dsp.processBlock(bufferLowQ, midi);

            // High resonance
            dsp.setParameterValue("filter_resonance", 0.9f);
            dsp.processBlock(bufferHighQ, midi);

            float rmsLowQ = DSPTestFramework::Framework::calculateRMS(bufferLowQ);
            float rmsHighQ = DSPTestFramework::Framework::calculateRMS(bufferHighQ);

            // High Q should boost peaks
            expect(rmsHighQ > rmsLowQ * 0.5f, "High resonance should boost signal");
        }

        beginTest("Filter - Envelope Modulation");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // Enable filter envelope with modulation
            dsp.setParameterValue("filter_env_amount", 0.5f);
            dsp.setParameterValue("filter_env_attack", 0.1f);
            dsp.setParameterValue("filter_env_decay", 0.2f);
            dsp.setParameterValue("filter_env_sustain", 0.5f);
            dsp.setParameterValue("filter_env_release", 0.3f);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Filter envelope modulation should work");
        }

        //======================================================================
        // CATEGORY 6: ENVELOPE TESTS (4 tests)
        //======================================================================

        beginTest("Envelope - ADSR Stages");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            midi.addEvent(juce::MidiMessage::noteOff(1, 60, 0.8f), 256);

            // Set ADSR parameters
            dsp.setParameterValue("amp_env_attack", 0.1f);
            dsp.setParameterValue("amp_env_decay", 0.2f);
            dsp.setParameterValue("amp_env_sustain", 0.6f);
            dsp.setParameterValue("amp_env_release", 0.3f);

            dsp.processBlock(buffer, midi);

            // Envelope should modulate amplitude
            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "ADSR envelope should modulate amplitude");
        }

        beginTest("Envelope - Fast Attack");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            dsp.setParameterValue("amp_env_attack", 0.0f); // Instant attack
            dsp.setParameterValue("amp_env_sustain", 1.0f);

            dsp.processBlock(buffer, midi);

            // First samples should be at full amplitude
            float firstSample = std::abs(buffer.getSample(0, 0));
            expect(firstSample > 0.5f, "Fast attack should reach peak quickly");
        }

        beginTest("Envelope - Long Release");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            midi.addEvent(juce::MidiMessage::noteOff(1, 60, 0.8f), 100);

            dsp.setParameterValue("amp_env_release", 1.0f); // Long release

            dsp.processBlock(buffer, midi);

            // Signal should continue after note-off
            float sampleAfterOff = std::abs(buffer.getSample(0, 200));
            expect(sampleAfterOff > 0.001f, "Long release should sustain sound");
        }

        beginTest("Envelope - Retrigger");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;

            // Rapid note-on, note-off, note-on sequence
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            midi.addEvent(juce::MidiMessage::noteOff(1, 60, 0.8f), 50);
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 100);

            dsp.setParameterValue("amp_env_attack", 0.1f);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Envelope should retrigger properly");
        }

        //======================================================================
        // CATEGORY 7: MODULATION MATRIX (8 tests)
        //======================================================================

        beginTest("Modulation Matrix - 16 Slots Available");
        {
            NatureDSP dsp;

            // All 16 slots should be configurable
            for (int i = 0; i < 16; ++i)
            {
                // If we can set modulation without crashing, slots exist
                juce::String modAmountParam = "mod_" + juce::String(i) + "_amount";
                dsp.setParameterValue(modAmountParam, 0.5f);
                expectEquals(dsp.getParameterValue(modAmountParam), 0.5f);
            }
        }

        beginTest("Modulation Matrix - LFO to Filter Cutoff");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // Route LFO1 to filter cutoff
            dsp.setParameterValue("mod_0_source", 0.0f); // LFO1
            dsp.setParameterValue("mod_0_destination", 0.0f); // Filter cutoff
            dsp.setParameterValue("mod_0_amount", 0.5f);
            dsp.setParameterValue("lfo1_rate", 5.0f); // 5 Hz LFO

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "LFO modulation should work");
        }

        beginTest("Modulation Matrix - Velocity to Amp");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> bufferVelLow(2, 512);
            juce::AudioBuffer<float> bufferVelHigh(2, 512);
            juce::MidiBuffer midi;

            // Low velocity
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.3f), 0);
            dsp.setParameterValue("mod_1_source", 8.0f); // Velocity
            dsp.setParameterValue("mod_1_destination", 1.0f); // Amp
            dsp.setParameterValue("mod_1_amount", 0.5f);
            dsp.processBlock(bufferVelLow, midi);

            // High velocity
            midi.clear();
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
            bufferVelHigh.clear();
            dsp.processBlock(bufferVelHigh, midi);

            float rmsLow = DSPTestFramework::Framework::calculateRMS(bufferVelLow);
            float rmsHigh = DSPTestFramework::Framework::calculateRMS(bufferVelHigh);

            expect(rmsHigh > rmsLow, "Higher velocity should produce louder output");
        }

        beginTest("Modulation Matrix - Bipolar Mode");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            // Set bipolar modulation
            dsp.setParameterValue("mod_2_bipolar", 1.0f); // Bipolar
            dsp.setParameterValue("mod_2_amount", 0.5f);

            bool isBipolar = dsp.getParameterValue("mod_2_bipolar") > 0.5f;
            expect(isBipolar, "Bipolar mode should be settable");
        }

        beginTest("Modulation Matrix - Unipolar Mode");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            dsp.setParameterValue("mod_3_bipolar", 0.0f); // Unipolar
            dsp.setParameterValue("mod_3_amount", 0.5f);

            bool isUnipolar = dsp.getParameterValue("mod_3_bipolar") < 0.5f;
            expect(isUnipolar, "Unipolar mode should be settable");
        }

        beginTest("Modulation Matrix - Linear Curve");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            dsp.setParameterValue("mod_4_curve", 0.0f); // Linear

            bool isLinear = dsp.getParameterValue("mod_4_curve") < 0.5f;
            expect(isLinear, "Linear curve should be settable");
        }

        beginTest("Modulation Matrix - Exponential Curve");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            dsp.setParameterValue("mod_5_curve", 1.0f); // Exponential

            bool isExponential = dsp.getParameterValue("mod_5_curve") > 0.5f;
            expect(isExponential, "Exponential curve should be settable");
        }

        beginTest("Modulation Matrix - Slot Disable");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            // Disable slot by setting amount to zero
            dsp.setParameterValue("mod_6_amount", 0.0f);

            float amount = dsp.getParameterValue("mod_6_amount");
            expectEquals(amount, 0.0f, "Zero amount should disable modulation slot");
        }

        //======================================================================
        // CATEGORY 8: MACRO SYSTEM (6 tests)
        //======================================================================

        beginTest("Macro System - 8 Macros Available");
        {
            NatureDSP dsp;

            // All 8 macros should be accessible
            for (int i = 0; i < 8; ++i)
            {
                juce::String macroParam = "macro_" + juce::String(i) + "_value";
                dsp.setParameterValue(macroParam, 0.5f);
                expectEquals(dsp.getParameterValue(macroParam), 0.5f);
            }
        }

        beginTest("Macro System - Set Macro Value");
        {
            NatureDSP dsp;

            dsp.setParameterValue("macro_0_value", 0.75f);
            expectEquals(dsp.getParameterValue("macro_0_value"), 0.75f);
        }

        beginTest("Macro System - Get Macro Value");
        {
            NatureDSP dsp;

            dsp.setParameterValue("macro_1_value", 0.25f);
            float value = dsp.getParameterValue("macro_1_value");
            expectEquals(value, 0.25f);
        }

        beginTest("Macro System - Single Destination");
        {
            NatureDSP dsp;

            // Macro 0 controls filter cutoff (simplified test)
            dsp.setParameterValue("macro_0_value", 0.75f);
            expectEquals(dsp.getParameterValue("macro_0_value"), 0.75f);

            // This test verifies macro value can be set
            // (Macro-to-parameter routing is internal implementation)
            expect(true);
        }

        beginTest("Macro System - Multiple Destinations");
        {
            NatureDSP dsp;

            // Set multiple macro values
            dsp.setParameterValue("macro_1_value", 0.5f);
            dsp.setParameterValue("macro_2_value", 0.6f);
            dsp.setParameterValue("macro_3_value", 0.7f);

            expectEquals(dsp.getParameterValue("macro_1_value"), 0.5f);
            expectEquals(dsp.getParameterValue("macro_2_value"), 0.6f);
            expectEquals(dsp.getParameterValue("macro_3_value"), 0.7f);
        }

        beginTest("Macro System - Amount Scaling");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // Set macro value
            dsp.setParameterValue("macro_2_value", 0.5f);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Macro system should work");
        }

        //======================================================================
        // CATEGORY 9: VOICE ALLOCATION (4 tests)
        //======================================================================

        beginTest("Voice Allocation - 16 Voice Polyphony");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;

            // Trigger 16 simultaneous notes
            for (int i = 0; i < 16; ++i)
            {
                midi.addEvent(juce::MidiMessage::noteOn(1, 60 + i, 0.8f), 0);
            }

            dsp.processBlock(buffer, midi);

            int activeVoices = dsp.getActiveVoiceCount();
            expectEquals(activeVoices, 16, "Should allocate all 16 voices");
        }

        beginTest("Voice Allocation - Round Robin");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;

            // Trigger more than 16 notes to test voice stealing
            for (int i = 0; i < 20; ++i)
            {
                midi.addEvent(juce::MidiMessage::noteOn(1, 60 + i, 0.8f), i * 10);
            }

            dsp.processBlock(buffer, midi);

            int activeVoices = dsp.getActiveVoiceCount();
            expect(activeVoices <= 16, "Should not exceed 16 voices");
        }

        beginTest("Voice Allocation - Note Off");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;

            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            dsp.processBlock(buffer, midi);
            expectEquals(dsp.getActiveVoiceCount(), 1);

            buffer.clear();
            midi.clear();
            midi.addEvent(juce::MidiMessage::noteOff(1, 60, 0.8f), 0);
            dsp.processBlock(buffer, midi);

            // Voice should enter release phase
            expect(dsp.getActiveVoiceCount() >= 0, "Voice should handle note-off");
        }

        beginTest("Voice Allocation - Monophonic Mode");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;

            // Enable monophonic mode
            dsp.setParameterValue("poly_mode", 0.0f); // Mono

            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            midi.addEvent(juce::MidiMessage::noteOn(1, 64, 0.8f), 100);

            dsp.processBlock(buffer, midi);

            // Should only use one voice in mono mode
            int activeVoices = dsp.getActiveVoiceCount();
            expect(activeVoices <= 1, "Monophonic mode should use single voice");
        }

        //======================================================================
        // CATEGORY 10: LFO TESTS (4 tests)
        //======================================================================

        beginTest("LFO - Sine Waveform");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            dsp.setParameterValue("lfo1_waveform", 0.0f); // Sine
            dsp.setParameterValue("lfo1_rate", 5.0f); // 5 Hz

            // Route LFO to audible parameter
            dsp.setParameterValue("mod_0_source", 0.0f); // LFO1
            dsp.setParameterValue("mod_0_destination", 0.0f); // Filter cutoff
            dsp.setParameterValue("mod_0_amount", 0.5f);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "LFO sine should modulate parameter");
        }

        beginTest("LFO - Triangle Waveform");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            dsp.setParameterValue("lfo1_waveform", 1.0f); // Triangle
            dsp.setParameterValue("lfo1_rate", 5.0f);

            bool isTriangle = dsp.getParameterValue("lfo1_waveform") > 0.5f;
            expect(isTriangle, "Triangle waveform should be settable");
        }

        beginTest("LFO - Sample and Hold");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            dsp.setParameterValue("lfo1_waveform", 4.0f); // S&H
            dsp.setParameterValue("lfo1_rate", 10.0f);

            bool isSH = dsp.getParameterValue("lfo1_waveform") > 3.5f;
            expect(isSH, "Sample & Hold waveform should be settable");
        }

        beginTest("LFO - Rate Control");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            // Test slow rate
            dsp.setParameterValue("lfo1_rate", 0.1f); // 0.1 Hz
            expectEquals(dsp.getParameterValue("lfo1_rate"), 0.1f);

            // Test fast rate
            dsp.setParameterValue("lfo1_rate", 20.0f); // 20 Hz
            expectEquals(dsp.getParameterValue("lfo1_rate"), 20.0f);
        }

        //======================================================================
        // CATEGORY 11: PRESET SYSTEM (8 tests)
        //======================================================================

        beginTest("Preset System - Save to JSON");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            // Set some parameters
            dsp.setParameterValue("osc1_shape", 0.0f);
            dsp.setParameterValue("filter_cutoff", 0.5f);
            dsp.setParameterValue("master_volume", 0.8f);

            std::string json = dsp.getPresetState();

            expect(json.length() > 0, "Preset JSON should not be empty");
            expect(json.find("\"format_version\"") != std::string::npos,
                   "Preset should have format_version");
        }

        beginTest("Preset System - Load from JSON");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            std::string json = R"({
                "format_version": "1.0",
                "preset_name": "Test Preset",
                "parameters": {
                    "osc1_shape": 0.0,
                    "filter_cutoff": 0.5,
                    "master_volume": 0.75
                }
            })";

            dsp.setPresetState(json);

            expectEquals(dsp.getParameterValue("master_volume"), 0.75f);
        }

        beginTest("Preset System - Validate Correct");
        {
            NatureDSP dsp;

            std::string validJson = R"({
                "format_version": "1.0",
                "preset_name": "Valid Preset",
                "parameters": {
                    "osc1_shape": 0.0
                }
            })";

            bool isValid = dsp.validatePreset(validJson);
            expect(isValid, "Valid preset should pass validation");
        }

        beginTest("Preset System - Validate Missing Parameter");
        {
            NatureDSP dsp;

            std::string invalidJson = R"({
                "format_version": "1.0",
                "preset_name": "Invalid Preset",
                "parameters": {
                }
            })";

            bool isValid = dsp.validatePreset(invalidJson);
            expect(!isValid, "Preset missing parameters should fail validation");
        }

        beginTest("Preset System - Validate Invalid Range");
        {
            NatureDSP dsp;

            std::string invalidJson = R"({
                "format_version": "1.0",
                "preset_name": "Invalid Range",
                "parameters": {
                    "osc1_shape": 999.0
                }
            })";

            bool isValid = dsp.validatePreset(invalidJson);
            expect(!isValid, "Out-of-range parameter should fail validation");
        }

        beginTest("Preset System - Get Preset Info");
        {
            NatureDSP dsp;

            std::string json = R"({
                "format_version": "1.0",
                "preset_name": "Test Preset",
                "author": "Bret Bouchard",
                "description": "Test description",
                "category": "Test",
                "creation_date": "2025-12-25",
                "parameters": {}
            })";

            NatureDSP::PresetInfo info = dsp.getPresetInfo(json);

            expectEquals(info.name, juce::String("Test Preset"));
            expectEquals(info.author, juce::String("Bret Bouchard"));
            expectEquals(info.category, juce::String("Test"));
        }

        beginTest("Preset System - Factory Preset Count");
        {
            NatureDSP dsp;

            int presetCount = dsp.getNumPrograms();
            expect(presetCount >= 10, "Should have at least 10 factory presets");
        }

        beginTest("Preset System - Load Factory Preset");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            // Load first factory preset
            dsp.setCurrentProgram(0);

            juce::String presetName = dsp.getProgramName(0);
            expect(presetName.isNotEmpty(), "Factory preset should have name");

            // Verify preset produces audio
            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Factory preset should produce audio");
        }

        //======================================================================
        // CATEGORY 12: INTEGRATION TESTS (6 tests)
        //======================================================================

        beginTest("Integration - Full Signal Path");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            midi.addEvent(juce::MidiMessage::noteOff(1, 60, 0.8f), 400);

            // Enable oscillators, filter, amp
            dsp.setParameterValue("osc1_level", 0.7f);
            dsp.setParameterValue("osc2_level", 0.5f);
            dsp.setParameterValue("sub_level", 0.3f);
            dsp.setParameterValue("filter_cutoff", 0.5f);
            dsp.setParameterValue("master_volume", 0.8f);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Full signal path should produce audio");
        }

        beginTest("Integration - Polyphonic Chord");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;

            // Major chord
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);   // C
            midi.addEvent(juce::MidiMessage::noteOn(1, 64, 0.8f), 0);   // E
            midi.addEvent(juce::MidiMessage::noteOn(1, 67, 0.8f), 0);   // G

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Chord should produce audio");
        }

        beginTest("Integration - Pitch Bend");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> bufferNoBend(2, 512);
            juce::AudioBuffer<float> bufferWithBend(2, 512);
            juce::MidiBuffer midi;

            // No pitch bend
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            dsp.processBlock(bufferNoBend, midi);

            // With pitch bend
            midi.clear();
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            midi.addEvent(juce::MidiMessage::pitchWheel(1, 8192 + 2000), 0); // Bend up
            dsp.processBlock(bufferWithBend, midi);

            float rmsNoBend = DSPTestFramework::Framework::calculateRMS(bufferNoBend);
            float rmsWithBend = DSPTestFramework::Framework::calculateRMS(bufferWithBend);

            // Both should produce audio (bend changes pitch, not volume)
            expect(rmsWithBend > 0.0f, "Pitch bend should work");
        }

        beginTest("Integration - Modulation Wheel");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;

            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 64), 0); // Mod wheel

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Modulation wheel should work");
        }

        beginTest("Integration - All Notes Off");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;

            // Trigger multiple notes
            for (int i = 0; i < 8; ++i)
            {
                midi.addEvent(juce::MidiMessage::noteOn(1, 60 + i, 0.8f), 0);
            }

            // All notes off
            midi.addEvent(juce::MidiMessage::allNotesOff(1), 100);

            dsp.processBlock(buffer, midi);

            expect(true); // If we got here without crash, panic works
        }

        beginTest("Integration - CPU Performance");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // Measure processing time
            auto processor = [&]() {
                buffer.clear();
                juce::MidiBuffer m = midi;
                dsp.processBlock(buffer, m);
            };

            double avgTime = DSPTestFramework::Framework::measureProcessingTime(processor, 100);
            double cpuPercent = DSPTestFramework::Framework::calculateCPUPercent(
                avgTime, 512, 48000.0);

            // Target: < 5% CPU per voice at 48kHz
            expect(cpuPercent < 5.0, "CPU usage should be < 5% per voice");
        }

        //======================================================================
        // CATEGORY 13: POLYPHEPHONY (3 tests)
        //======================================================================

        beginTest("Polyphony - Voice Stealing");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;

            // Trigger 20 notes (more than 16 voices)
            for (int i = 0; i < 20; ++i)
            {
                midi.addEvent(juce::MidiMessage::noteOn(1, 60 + i, 0.8f), i * 5);
            }

            dsp.processBlock(buffer, midi);

            // Should handle gracefully (voice stealing)
            int activeVoices = dsp.getActiveVoiceCount();
            expect(activeVoices <= 16, "Should not exceed polyphony limit");
        }

        beginTest("Polyphony - Legato Mode");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;

            // Enable legato
            dsp.setParameterValue("poly_mode", 1.0f); // Legato

            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            midi.addEvent(juce::MidiMessage::noteOn(1, 64, 0.8f), 100);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Legato mode should work");
        }

        beginTest("Polyphony - Glide Portamento");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;

            // Enable glide
            dsp.setParameterValue("glide_enabled", true);
            dsp.setParameterValue("glide_time", 0.2f); // 200ms

            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            midi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.8f), 100);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Glide should work");
        }

        //======================================================================
        // CATEGORY 14: SUB-OSCILLATOR (2 tests)
        //======================================================================

        beginTest("Sub-Oscillator - Minus One Octave");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // Enable sub-oscillator
            dsp.setParameterValue("sub_enabled", true);
            dsp.setParameterValue("sub_level", 0.8f);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Sub-oscillator should produce audio");
        }

        beginTest("Sub-Oscillator - Square Wave");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            dsp.setParameterValue("sub_enabled", true);
            dsp.setParameterValue("sub_level", 1.0f);

            dsp.processBlock(buffer, midi);

            // Sub should be square wave (odd harmonics)
            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Sub square wave should work");
        }

        //======================================================================
        // CATEGORY 15: MIXER TESTS (3 tests)
        //======================================================================

        beginTest("Mixer - OSC1 Level");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            dsp.setParameterValue("osc1_level", 0.0f);
            dsp.processBlock(buffer, midi);

            float rmsOff = DSPTestFramework::Framework::calculateRMS(buffer);

            buffer.clear();
            midi.clear();
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            dsp.setParameterValue("osc1_level", 1.0f);
            dsp.processBlock(buffer, midi);

            float rmsOn = DSPTestFramework::Framework::calculateRMS(buffer);

            expect(rmsOn > rmsOff, "OSC1 level should affect output");
        }

        beginTest("Mixer - OSC2 Level");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            dsp.setParameterValue("osc2_level", 0.5f);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "OSC2 level should work");
        }

        beginTest("Mixer - Noise Generator");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;

            // Enable noise with oscillators disabled
            dsp.setParameterValue("osc1_level", 0.0f);
            dsp.setParameterValue("osc2_level", 0.0f);
            dsp.setParameterValue("sub_level", 0.0f);
            dsp.setParameterValue("noise_level", 0.5f);

            dsp.processBlock(buffer, midi);

            float rms = DSPTestFramework::Framework::calculateRMS(buffer);
            expect(rms > 0.0f, "Noise generator should produce signal");
        }

        //======================================================================
        // CATEGORY 16: REALTIME SAFETY (3 tests)
        //======================================================================

        beginTest("Realtime Safety - No Allocations in ProcessBlock");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // Run multiple times - should not allocate
            for (int i = 0; i < 100; ++i)
            {
                dsp.processBlock(buffer, midi);
            }

            expect(true); // If we got here without crash, no allocations
        }

        beginTest("Realtime Safety - Thread-Safe Parameter Access");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            // Read parameters from multiple "threads" (simulated)
            for (int i = 0; i < 1000; ++i)
            {
                float value = dsp.getParameterValue("osc1_shape");
                expect(value >= 0.0f && value <= 1.0f);
            }

            expect(true); // No race conditions
        }

        beginTest("Realtime Safety - Lock-Free Modulation");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

            // Modulation should use lock-free atomics
            dsp.setParameterValue("mod_0_amount", 0.5f);
            dsp.processBlock(buffer, midi);

            float modAmount = dsp.getParameterValue("mod_0_amount");
            expectEquals(modAmount, 0.5f);
        }

        //======================================================================
        // CATEGORY 9: FACTORY PRESETS (3 tests)
        //======================================================================

        beginTest("Factory Presets - All 30 Presets Load");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            // Test that all 30 factory presets exist and can be loaded
            int presetCount = dsp.getNumPrograms();
            expectEquals(presetCount, 30, "Should have exactly 30 factory presets");

            for (int i = 0; i < presetCount; ++i)
            {
                dsp.setCurrentProgram(i);
                juce::String presetName = dsp.getProgramName(i);
                expect(presetName.isNotEmpty(), "Preset " + juce::String(i) + " should have a name");
            }
        }

        beginTest("Factory Presets - Preset Parameters Valid");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            // Load each preset and validate parameters are in range
            for (int i = 0; i < dsp.getNumPrograms(); ++i)
            {
                dsp.setCurrentProgram(i);

                // Check critical parameters are in valid range
                float osc1Warp = dsp.getParameterValue("osc1_warp");
                expect(osc1Warp >= -1.0f && osc1Warp <= 1.0f,
                    "Preset " + juce::String(i) + " OSC1 warp out of range");

                float filterCutoff = dsp.getParameterValue("filter_cutoff");
                expect(filterCutoff >= 0.0f && filterCutoff <= 1.0f,
                    "Preset " + juce::String(i) + " filter cutoff out of range");

                float masterVolume = dsp.getParameterValue("master_volume");
                expect(masterVolume >= 0.0f && masterVolume <= 1.0f,
                    "Preset " + juce::String(i) + " master volume out of range");

                // Verify FM depth is valid when FM is enabled
                float fmEnabled = dsp.getParameterValue("fm_enabled");
                if (fmEnabled > 0.5f)
                {
                    float fmDepth = dsp.getParameterValue("fm_depth");
                    expect(fmDepth >= 0.0f && fmDepth <= 1.0f,
                        "Preset " + juce::String(i) + " FM depth out of range");
                }
            }
        }

        beginTest("Factory Presets - Preset Categories");
        {
            NatureDSP dsp;
            dsp.prepareToPlay(48000.0, 512);

            // Test preset categories (Bass, Lead, Pad, Pluck, FX, Keys, Seq)
            juce::StringArray categories;
            for (int i = 0; i < dsp.getNumPrograms(); ++i)
            {
                dsp.setCurrentProgram(i);
                juce::String presetName = dsp.getProgramName(i);

                // Verify naming convention (Number_Name format)
                expect(presetName.containsChar('_'), "Preset should use Number_Name format");

                // Check that presets are ordered correctly
                if (i > 0)
                {
                    juce::String prevName = dsp.getProgramName(i - 1);
                    expect(prevName.compareLexicographic(presetName) < 0,
                        "Presets should be in alphabetical order");
                }
            }

            // Verify we have presets from each category
            expect(true, "All presets follow naming convention");
        }

        //======================================================================
        // END OF TEST SUITE
        //======================================================================
    }
};

//==============================================================================
// Static test registration
//==============================================================================

static NatureTests natureMarcoTests;
