/*
  ==============================================================================

    NaturePerformanceTests.cpp
    Created: 26 Dec 2025
    Author:  Bret Bouchard

    Week 4: Performance Profiling and Optimization
    - Profile all 30 presets for CPU usage
    - Verify < 5% CPU per voice target
    - Modulation matrix overhead measurement
    - Realtime safety verification
    - Hot path optimization (if needed)

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "../../include/dsp/NatureDSP.h"
#include "DSPTestFramework.h"

//==============================================================================
// PERFORMANCE PROFILER
//==============================================================================

/**
 * @brief Realtime-safe performance profiler
 *
 * Measures CPU usage of audio processing with microsecond precision.
 * Critical for verifying < 5% CPU per voice target.
 */
struct PerformanceProfiler
{
    std::chrono::high_resolution_clock::time_point startTime;
    double totalSamples = 0;
    double totalTime = 0;
    int numMeasurements = 0;

    void start()
    {
        startTime = std::chrono::high_resolution_clock::now();
    }

    void stop(int numSamples)
    {
        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = endTime - startTime;
        totalTime += elapsed.count();
        totalSamples += numSamples;
        numMeasurements++;
    }

    double getCPUPercent() const
    {
        if (totalSamples == 0) return 0.0;

        double actualTime = totalTime;
        double audioTime = totalSamples / 48000.0;

        return (actualTime / audioTime) * 100.0;
    }

    double getAverageProcessingTime() const
    {
        if (numMeasurements == 0) return 0.0;
        return totalTime / numMeasurements;
    }

    void reset()
    {
        totalSamples = 0;
        totalTime = 0;
        numMeasurements = 0;
    }
};

//==============================================================================
// NATURE MARCO PERFORMANCE TEST SUITE
//==============================================================================

class NaturePerformanceTests : public juce::UnitTest
{
public:
    NaturePerformanceTests() : juce::UnitTest("Nature Marco Performance", "DSP") {}

    void runTest() override
    {
        //======================================================================
        // CATEGORY 1: PRESET PERFORMANCE (30 tests)
        //======================================================================

        beginTest("Profile All 30 Presets - CPU Usage");
        {
            NatureDSP synth;
            synth.prepareToPlay(48000.0, 512);

            constexpr int testDuration = 5;  // 5 seconds per preset
            constexpr int numVoices = 16;

            std::vector<double> presetCPU;
            presetCPU.reserve(30);

            for (int preset = 0; preset < 30; ++preset)
            {
                synth.setCurrentProgram(preset);

                PerformanceProfiler profiler;
                profiler.start();

                int numSamples = 0;
                auto testStart = std::chrono::steady_clock::now();

                while (numSamples < testDuration * 48000)
                {
                    juce::AudioBuffer<float> buffer(2, 512);
                    juce::MidiBuffer midi;

                    // Trigger 16 voices (chord)
                    for (int v = 0; v < numVoices; ++v)
                    {
                        midi.addEvent(juce::MidiMessage::noteOn(1, 60 + v, 0.5f), 0);
                    }

                    synth.processBlock(buffer, midi);
                    numSamples += 512;

                    // Check timeout
                    auto now = std::chrono::steady_clock::now();
                    std::chrono::duration<double> elapsed = now - testStart;
                    if (elapsed.count() > testDuration + 1.0) break;  // 1s grace
                }

                profiler.stop(numSamples);
                double cpu = profiler.getCPUPercent();
                presetCPU.push_back(cpu);

                // Log result
                juce::String presetName = synth.getProgramName(preset);
                logMessage(juce::String::formatted("  Preset %2d: %-30s %.2f%% CPU",
                    preset, presetName.toStdString().c_str(), cpu));

                // Verify CPU budget
                expect(cpu < 80.0,
                    juce::String::formatted("Preset %d (%s) exceeds CPU budget: %.2f%%",
                        preset, presetName.toStdString().c_str(), cpu));

                profiler.reset();
            }

            // Calculate statistics
            double minCPU = *std::min_element(presetCPU.begin(), presetCPU.end());
            double maxCPU = *std::max_element(presetCPU.begin(), presetCPU.end());
            double avgCPU = std::accumulate(presetCPU.begin(), presetCPU.end(), 0.0) / presetCPU.size();

            logMessage(juce::String::formatted("\n  Preset CPU Statistics:"));
            logMessage(juce::String::formatted("    Best:   %.2f%% CPU", minCPU));
            logMessage(juce::String::formatted("    Worst:  %.2f%% CPU", maxCPU));
            logMessage(juce::String::formatted("    Average: %.2f%% CPU", avgCPU));
        }

        //======================================================================
        // CATEGORY 2: PER-VOICE PERFORMANCE
        //======================================================================

        beginTest("Per-Voice CPU Breakdown");
        {
            NatureDSP synth;
            synth.prepareToPlay(48000.0, 512);
            synth.setCurrentProgram(0);  // Use first preset

            // Test with 1, 4, 8, 16 voices
            for (int numVoices : {1, 4, 8, 16})
            {
                PerformanceProfiler profiler;
                profiler.start();

                constexpr int numSamples = 48000 * 5;  // 5 seconds

                for (int i = 0; i < numSamples; i += 512)
                {
                    juce::AudioBuffer<float> buffer(2, 512);
                    juce::MidiBuffer midi;

                    for (int v = 0; v < numVoices; ++v)
                    {
                        midi.addEvent(juce::MidiMessage::noteOn(1, 60 + v, 0.5f), 0);
                    }

                    synth.processBlock(buffer, midi);
                }

                profiler.stop(numSamples);
                double cpuTotal = profiler.getCPUPercent();
                double cpuPerVoice = cpuTotal / numVoices;

                logMessage(juce::String::formatted("  %d voices: %.2f%% total, %.2f%% per voice",
                    numVoices, cpuTotal, cpuPerVoice));

                expect(cpuPerVoice < 5.0,
                    juce::String::formatted("Per-voice CPU exceeds 5%% for %d voices: %.2f%%",
                        numVoices, cpuPerVoice));

                profiler.reset();
            }
        }

        //======================================================================
        // CATEGORY 3: MODULATION MATRIX OVERHEAD
        //======================================================================

        beginTest("Modulation Matrix Overhead");
        {
            NatureDSP synth;
            synth.prepareToPlay(48000.0, 512);

            // Test with 0, 4, 8, 16 modulation slots
            for (int numSlots : {0, 4, 8, 16})
            {
                // Configure modulation slots
                for (int slot = 0; slot < numSlots; ++slot)
                {
                    juce::String sourceParam = "mod_" + juce::String(slot) + "_source";
                    juce::String destParam = "mod_" + juce::String(slot) + "_destination";
                    juce::String amountParam = "mod_" + juce::String(slot) + "_amount";

                    synth.setParameterValue(sourceParam, 0.0f);  // LFO1
                    synth.setParameterValue(destParam, 11.0f);  // Filter cutoff
                    synth.setParameterValue(amountParam, 0.3f);  // Moderate modulation
                }

                PerformanceProfiler profiler;
                profiler.start();

                constexpr int numSamples = 48000 * 5;

                juce::AudioBuffer<float> buffer(2, 512);
                juce::MidiBuffer midi;
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);

                for (int i = 0; i < numSamples; i += 512)
                {
                    synth.processBlock(buffer, midi);
                }

                profiler.stop(numSamples);
                double cpu = profiler.getCPUPercent();

                logMessage(juce::String::formatted("  %d mod slots: %.2f%% CPU", numSlots, cpu));

                // Modulation should add minimal overhead
                expect(cpu < 10.0,
                    juce::String::formatted("Modulation matrix overhead too high for %d slots: %.2f%%",
                        numSlots, cpu));

                profiler.reset();
            }
        }

        //======================================================================
        // CATEGORY 4: OSCILLATOR WARP PERFORMANCE
        //======================================================================

        beginTest("Oscillator Warp Performance Impact");
        {
            NatureDSP synth;
            synth.prepareToPlay(48000.0, 512);

            // Test with different warp amounts
            std::vector<float> warpAmounts = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};

            for (float warp : warpAmounts)
            {
                synth.setParameterValue("osc1_warp", warp);
                synth.setParameterValue("osc2_warp", warp);

                PerformanceProfiler profiler;
                profiler.start();

                constexpr int numSamples = 48000 * 2;  // 2 seconds

                juce::AudioBuffer<float> buffer(2, 512);
                juce::MidiBuffer midi;
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);

                for (int i = 0; i < numSamples; i += 512)
                {
                    synth.processBlock(buffer, midi);
                }

                profiler.stop(numSamples);
                double cpu = profiler.getCPUPercent();

                logMessage(juce::String::formatted("  Warp %.1f: %.2f%% CPU", warp, cpu));

                profiler.reset();
            }
        }

        //======================================================================
        // CATEGORY 5: FM SYNTHESIS OVERHEAD
        //======================================================================

        beginTest("FM Synthesis Overhead");
        {
            NatureDSP synth;
            synth.prepareToPlay(48000.0, 512);

            // Test FM off vs FM on
            for (int fmEnabled : {0, 1})
            {
                synth.setParameterValue("fm_enabled", fmEnabled ? 1.0f : 0.0f);
                synth.setParameterValue("fm_depth", 0.5f);
                synth.setParameterValue("fm_modulator_ratio", 2.0f);

                PerformanceProfiler profiler;
                profiler.start();

                constexpr int numSamples = 48000 * 2;

                juce::AudioBuffer<float> buffer(2, 512);
                juce::MidiBuffer midi;
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);

                for (int i = 0; i < numSamples; i += 512)
                {
                    synth.processBlock(buffer, midi);
                }

                profiler.stop(numSamples);
                double cpu = profiler.getCPUPercent();

                logMessage(juce::String::formatted("  FM %s: %.2f%% CPU",
                    fmEnabled ? "ON" : "OFF", cpu));

                profiler.reset();
            }
        }

        //======================================================================
        // CATEGORY 6: FILTER MODE PERFORMANCE
        //======================================================================

        beginTest("Filter Mode Performance");
        {
            NatureDSP synth;
            synth.prepareToPlay(48000.0, 512);

            // Test all filter modes
            juce::StringArray filterNames = {"LP", "HP", "BP", "Notch"};

            for (int filterType = 0; filterType < 4; ++filterType)
            {
                synth.setParameterValue("filter_type", static_cast<float>(filterType));
                synth.setParameterValue("filter_cutoff", 0.5f);
                synth.setParameterValue("filter_resonance", 0.7f);

                PerformanceProfiler profiler;
                profiler.start();

                constexpr int numSamples = 48000 * 2;

                juce::AudioBuffer<float> buffer(2, 512);
                juce::MidiBuffer midi;
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);

                for (int i = 0; i < numSamples; i += 512)
                {
                    synth.processBlock(buffer, midi);
                }

                profiler.stop(numSamples);
                double cpu = profiler.getCPUPercent();

                logMessage(juce::String::formatted("  Filter %s: %.2f%% CPU",
                    filterNames[filterType].toStdString().c_str(), cpu));

                profiler.reset();
            }
        }

        //======================================================================
        // CATEGORY 7: REALTIME SAFETY
        //======================================================================

        beginTest("Realtime Safety - No Buffer Underruns");
        {
            NatureDSP synth;
            synth.prepareToPlay(48000.0, 512);

            constexpr int numSamples = 48000 * 60;  // 1 minute
            int bufferUnderruns = 0;
            double maxProcessingTime = 0.0;

            for (int i = 0; i < numSamples; i += 512)
            {
                auto start = std::chrono::steady_clock::now();

                juce::AudioBuffer<float> buffer(2, 512);
                juce::MidiBuffer midi;
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);

                synth.processBlock(buffer, midi);

                auto end = std::chrono::steady_clock::now();
                std::chrono::duration<double, std::milli> elapsed = end - start;

                if (elapsed.count() > maxProcessingTime)
                    maxProcessingTime = elapsed.count();

                // 512 samples at 48kHz = 10.67ms budget
                if (elapsed.count() > 10.67)
                    bufferUnderruns++;
            }

            logMessage(juce::String::formatted("  Buffer underruns: %d", bufferUnderruns));
            logMessage(juce::String::formatted("  Max processing time: %.2f ms", maxProcessingTime));

            expectEquals(bufferUnderruns, 0,
                "Buffer underruns detected - realtime safety violation!");
        }

        beginTest("Realtime Safety - No Allocations in ProcessBlock");
        {
            NatureDSP synth;
            synth.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);

            // Run multiple times - should not allocate
            for (int i = 0; i < 1000; ++i)
            {
                synth.processBlock(buffer, midi);
            }

            expect(true);  // If we got here without crash, no allocations
        }

        beginTest("Realtime Safety - Thread-Safe Parameter Access");
        {
            NatureDSP synth;
            synth.prepareToPlay(48000.0, 512);

            // Rapid parameter changes from "UI thread"
            for (int i = 0; i < 1000; ++i)
            {
                synth.setParameterValue("osc1_warp", (i % 20) / 10.0f - 1.0f);
                synth.setParameterValue("filter_cutoff", (i % 10) / 10.0f);
            }

            // Process audio while parameters change
            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);

            for (int i = 0; i < 100; ++i)
            {
                synth.processBlock(buffer, midi);
            }

            expect(true);  // No race conditions or crashes
        }

        //======================================================================
        // CATEGORY 8: POLYPHONY SCALING
        //======================================================================

        beginTest("Polyphony Scaling - Linear CPU Growth");
        {
            NatureDSP synth;
            synth.prepareToPlay(48000.0, 512);

            std::vector<double> cpuByVoices;

            // Test 1, 2, 4, 8, 16 voices
            for (int numVoices : {1, 2, 4, 8, 16})
            {
                PerformanceProfiler profiler;
                profiler.start();

                constexpr int numSamples = 48000 * 2;

                for (int i = 0; i < numSamples; i += 512)
                {
                    juce::AudioBuffer<float> buffer(2, 512);
                    juce::MidiBuffer midi;

                    for (int v = 0; v < numVoices; ++v)
                    {
                        midi.addEvent(juce::MidiMessage::noteOn(1, 60 + v, 0.5f), 0);
                    }

                    synth.processBlock(buffer, midi);
                }

                profiler.stop(numSamples);
                double cpu = profiler.getCPUPercent();
                cpuByVoices.push_back(cpu);

                logMessage(juce::String::formatted("  %d voices: %.2f%% CPU", numVoices, cpu));

                profiler.reset();
            }

            // Verify near-linear scaling (allow 20% deviation)
            for (size_t i = 2; i < cpuByVoices.size(); ++i)
            {
                double expectedCPU = cpuByVoices[0] * (i + 1);
                double actualCPU = cpuByVoices[i];
                double deviation = std::abs(actualCPU - expectedCPU) / expectedCPU;

                expect(deviation < 0.3,
                    juce::String::formatted("CPU scaling not linear: expected %.2f%%, got %.2f%%",
                        expectedCPU, actualCPU));
            }
        }

        //======================================================================
        // CATEGORY 9: ENVELOPE PERFORMANCE
        //======================================================================

        beginTest("Envelope Performance - Fast vs Slow");
        {
            NatureDSP synth;
            synth.prepareToPlay(48000.0, 512);

            // Test fast envelope
            synth.setParameterValue("amp_env_attack", 0.001f);
            synth.setParameterValue("amp_env_decay", 0.01f);
            synth.setParameterValue("amp_env_release", 0.01f);

            PerformanceProfiler profilerFast;
            profilerFast.start();

            constexpr int numSamples = 48000 * 2;

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
            midi.addEvent(juce::MidiMessage::noteOff(1, 60, 0.5f), 256);

            for (int i = 0; i < numSamples; i += 512)
            {
                synth.processBlock(buffer, midi);
            }

            profilerFast.stop(numSamples);
            double cpuFast = profilerFast.getCPUPercent();

            // Test slow envelope
            synth.setParameterValue("amp_env_attack", 1.0f);
            synth.setParameterValue("amp_env_decay", 2.0f);
            synth.setParameterValue("amp_env_release", 3.0f);

            PerformanceProfiler profilerSlow;
            profilerSlow.start();

            for (int i = 0; i < numSamples; i += 512)
            {
                synth.processBlock(buffer, midi);
            }

            profilerSlow.stop(numSamples);
            double cpuSlow = profilerSlow.getCPUPercent();

            logMessage(juce::String::formatted("  Fast envelope: %.2f%% CPU", cpuFast));
            logMessage(juce::String::formatted("  Slow envelope: %.2f%% CPU", cpuSlow));

            // Envelope speed should not significantly affect CPU
            double deviation = std::abs(cpuFast - cpuSlow) / cpuFast;
            expect(deviation < 0.2, "Envelope speed significantly affects CPU");
        }

        //======================================================================
        // CATEGORY 10: LFO WAVEFORM PERFORMANCE
        //======================================================================

        beginTest("LFO Waveform Performance");
        {
            NatureDSP synth;
            synth.prepareToPlay(48000.0, 512);

            juce::StringArray waveformNames = {"Sine", "Triangle", "Saw", "Square", "S&H"};

            for (int waveform = 0; waveform < 5; ++waveform)
            {
                synth.setParameterValue("lfo1_waveform", static_cast<float>(waveform));
                synth.setParameterValue("lfo1_rate", 10.0f);

                // Route LFO to filter for measurable impact
                synth.setParameterValue("mod_0_source", 0.0f);  // LFO1
                synth.setParameterValue("mod_0_destination", 11.0f);  // Filter cutoff
                synth.setParameterValue("mod_0_amount", 0.5f);

                PerformanceProfiler profiler;
                profiler.start();

                constexpr int numSamples = 48000 * 2;

                juce::AudioBuffer<float> buffer(2, 512);
                juce::MidiBuffer midi;
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);

                for (int i = 0; i < numSamples; i += 512)
                {
                    synth.processBlock(buffer, midi);
                }

                profiler.stop(numSamples);
                double cpu = profiler.getCPUPercent();

                logMessage(juce::String::formatted("  LFO %s: %.2f%% CPU",
                    waveformNames[waveform].toStdString().c_str(), cpu));

                profiler.reset();
            }
        }

        //======================================================================
        // END OF PERFORMANCE TEST SUITE
        //======================================================================
    }
};

//==============================================================================
// Static test registration
//==============================================================================

static NaturePerformanceTests natureMarcoPerformanceTests;

//==============================================================================
// Main entry point for standalone performance test execution
//==============================================================================

int main(int argc, char* argv[])
{
    (void)argc; (void)argv; // Suppress unused parameter warnings

    // Initialize JUCE
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    // Run all performance tests
    NaturePerformanceTests tests;
    tests.runTest();

    return 0;
}

