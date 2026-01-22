/*
  ==============================================================================

    NatureAdvancedPhysicsTests.cpp
    Created: January 9, 2026
    Author: Bret Bouchard

    Advanced Physical Modeling Tests for Aether
    - Per-Mode Q Calculation Tests
    - Dispersion Filter Tests
    - Sympathetic Coupling Tests
    - Bridge Impedance Tests
    - Material Preset Tests

  ==============================================================================
*/

#include <gtest/gtest.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../../include/dsp/AetherPureDSP.h"
#include <algorithm>
#include <chrono>
#include <array>
#include <vector>

//==============================================================================
// Test Fixture
class NatureAdvancedPhysicsTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Initialize before each test
    }

    void TearDown() override
    {
        // Cleanup after each test
    }

    //==========================================================================
    // Helper Functions
    //==========================================================================

    /**
     * @brief Calculate RMS of buffer
     */
    float calculateRMS(const juce::AudioBuffer<float>& buffer)
    {
        float sum = 0.0f;
        const float* readPtr = buffer.getReadPointer(0);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float sample = readPtr[i];
            sum += sample * sample;
        }

        return std::sqrt(sum / buffer.getNumSamples());
    }

    /**
     * @brief Find T60 (time to decay to -60dB)
     */
    float findT60(const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        float threshold = 0.001f;  // -60dB

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float amp = std::abs(buffer.getSample(0, i));
            if (amp < threshold)
            {
                return static_cast<float>(i) / sampleRate;
            }
        }

        return -1.0f;  // Never reached threshold
    }

    /**
     * @brief Measure frequency response using FFT
     */
    std::vector<std::pair<float, float>> analyzeSpectrum(const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        juce::dsp::FFT fft(12);  // 4096-point FFT
        std::array<float, 8192> fftData;

        // Copy mono buffer to FFT data
        const float* readPtr = buffer.getReadPointer(0);
        int fftSize = juce::jmin(buffer.getNumSamples(), 4096);

        for (int i = 0; i < fftSize; ++i)
        {
            fftData[i * 2] = readPtr[i];     // Real
            fftData[i * 2 + 1] = 0.0f;      // Imaginary
        }

        // Zero-pad if necessary
        for (int i = fftSize; i < 4096; ++i)
        {
            fftData[i * 2] = 0.0f;
            fftData[i * 2 + 1] = 0.0f;
        }

        // Perform FFT
        fft.performRealOnlyForwardTransform(fftData.data());

        // Extract spectrum
        std::vector<std::pair<float, float>> spectrum;
        for (int i = 1; i < 2048; ++i)  // Skip DC
        {
            float real = fftData[i * 2];
            float imag = fftData[i * 2 + 1];
            float magnitude = std::sqrt(real * real + imag * imag);
            float frequency = static_cast<float>(i * sampleRate / 4096.0);
            spectrum.push_back({frequency, magnitude});
        }

        return spectrum;
    }
};

//==============================================================================
// TEST: Per-Mode Q Calculation
//==============================================================================

TEST_F(NatureAdvancedPhysicsTests, PerModeQ_HigherFrequenciesDampFaster)
{
    // Verify that higher frequency modes have lower Q (damp faster)
    DSP::ModalFilter lowFreqMode;
    DSP::ModalFilter highFreqMode;

    lowFreqMode.frequency = 100.0f;
    lowFreqMode.decay = 1.0f;
    lowFreqMode.modeIndex = 0.0f;
    lowFreqMode.materialFactor = 1.0f;
    lowFreqMode.prepare(48000.0);

    highFreqMode.frequency = 2000.0f;
    highFreqMode.decay = 1.0f;
    highFreqMode.modeIndex = 0.0f;
    highFreqMode.materialFactor = 1.0f;
    highFreqMode.prepare(48000.0);

    // Higher frequency should have lower Q
    EXPECT_LT(highFreqMode.computedQ, lowFreqMode.computedQ)
        << "Higher frequency modes should damp faster (lower Q)";

    std::cout << "Low frequency Q (100 Hz): " << lowFreqMode.computedQ << std::endl;
    std::cout << "High frequency Q (2000 Hz): " << highFreqMode.computedQ << std::endl;
}

TEST_F(NatureAdvancedPhysicsTests, PerModeQ_HarmonicsDampFaster)
{
    // Verify that higher harmonics damp faster
    DSP::ModalFilter fundamental;
    DSP::ModalFilter harmonic;

    fundamental.frequency = 440.0f;
    fundamental.decay = 1.0f;
    fundamental.modeIndex = 0.0f;
    fundamental.materialFactor = 1.0f;
    fundamental.prepare(48000.0);

    harmonic.frequency = 440.0f;  // Same frequency
    harmonic.decay = 1.0f;
    harmonic.modeIndex = 3.0f;  // But it's the 3rd harmonic
    harmonic.materialFactor = 1.0f;
    harmonic.prepare(48000.0);

    // Higher harmonic should have lower Q
    EXPECT_LT(harmonic.computedQ, fundamental.computedQ)
        << "Higher harmonics should damp faster (lower Q)";

    std::cout << "Fundamental Q (mode 0): " << fundamental.computedQ << std::endl;
    std::cout << "Harmonic Q (mode 3): " << harmonic.computedQ << std::endl;
}

TEST_F(NatureAdvancedPhysicsTests, PerModeQ_MaterialAffectsBrightness)
{
    // Verify that material parameter affects Q
    DSP::ModalFilter softWood;
    DSP::ModalFilter metal;

    softWood.frequency = 440.0f;
    softWood.decay = 1.0f;
    softWood.modeIndex = 0.0f;
    softWood.materialFactor = 0.5f;  // Soft wood
    softWood.prepare(48000.0);

    metal.frequency = 440.0f;
    metal.decay = 1.0f;
    metal.modeIndex = 0.0f;
    metal.materialFactor = 1.5f;  // Metal
    metal.prepare(48000.0);

    // Metal should have higher Q (brighter)
    EXPECT_GT(metal.computedQ, softWood.computedQ)
        << "Metal strings should have higher Q (brighter)";

    std::cout << "Soft wood Q: " << softWood.computedQ << std::endl;
    std::cout << "Metal Q: " << metal.computedQ << std::endl;
}

TEST_F(NatureAdvancedPhysicsTests, PerModeQ_DecayProfilesAreRealistic)
{
    // Generate impulse responses and verify realistic decay profiles
    double sampleRate = 48000.0;

    std::array<DSP::ModalFilter, 4> modes;
    std::array<float, 4> frequencies = {220.0f, 440.0f, 880.0f, 1760.0f};

    for (size_t i = 0; i < modes.size(); ++i)
    {
        modes[i].frequency = frequencies[i];
        modes[i].decay = 1.0f;
        modes[i].modeIndex = static_cast<float>(i);
        modes[i].materialFactor = 1.0f;
        modes[i].prepare(sampleRate);
    }

    // Generate impulse responses and measure T60
    std::array<float, 4> t60Times;
    for (size_t i = 0; i < modes.size(); ++i)
    {
        juce::AudioBuffer<float> impulse(1, 48000);
        impulse.clear();

        float output = modes[i].processSample(1.0f);
        impulse.setSample(0, 0, output);

        for (int s = 1; s < 48000; ++s)
        {
            output = modes[i].processSample(0.0f);
            impulse.setSample(0, s, output);
        }

        t60Times[i] = findT60(impulse, sampleRate);
    }

    // Verify that higher frequencies have shorter T60
    for (size_t i = 1; i < t60Times.size(); ++i)
    {
        EXPECT_LT(t60Times[i], t60Times[i - 1])
            << "Higher frequency mode " << i << " should decay faster";
        std::cout << "Mode " << i << " (" << frequencies[i] << " Hz) T60: "
                  << (t60Times[i] * 1000.0f) << " ms" << std::endl;
    }
}

//==============================================================================
// TEST: Dispersion Filters
//==============================================================================

TEST_F(NatureAdvancedPhysicsTests, Dispersion_AffectsHighFrequencies)
{
    // Verify that dispersion filters affect high frequencies more than low
    double sampleRate = 48000.0;

    DSP::WaveguideString string;
    string.prepare(sampleRate);
    string.setFrequency(440.0f);
    string.setDispersion(0.0f);  // No dispersion

    // Generate response without dispersion
    float exciter[] = {1.0f, 0.5f, 0.0f};
    string.excite(exciter, 3, 0.8f);

    juce::AudioBuffer<float> bufferNoDisp(1, 10000);
    for (int i = 0; i < 10000; ++i)
    {
        bufferNoDisp.setSample(0, i, string.processSample());
    }

    // Reset and enable dispersion
    string.reset();
    string.prepare(sampleRate);
    string.setFrequency(440.0f);
    string.setDispersion(0.8f);  // High dispersion
    string.excite(exciter, 3, 0.8f);

    juce::AudioBuffer<float> bufferWithDisp(1, 10000);
    for (int i = 0; i < 10000; ++i)
    {
        bufferWithDisp.setSample(0, i, string.processSample());
    }

    // Analyze spectra
    auto spectrumNoDisp = analyzeSpectrum(bufferNoDisp, sampleRate);
    auto spectrumWithDisp = analyzeSpectrum(bufferWithDisp, sampleRate);

    // Find high-frequency energy ratio
    float highFreqEnergyNoDisp = 0.0f;
    float highFreqEnergyWithDisp = 0.0f;

    for (const auto& [freq, mag] : spectrumNoDisp)
    {
        if (freq > 2000.0f)
            highFreqEnergyNoDisp += mag;
    }

    for (const auto& [freq, mag] : spectrumWithDisp)
    {
        if (freq > 2000.0f)
            highFreqEnergyWithDisp += mag;
    }

    // Dispersion should affect high-frequency content
    EXPECT_NE(highFreqEnergyNoDisp, highFreqEnergyWithDisp)
        << "Dispersion should affect high-frequency content";

    std::cout << "High frequency energy (no dispersion): " << highFreqEnergyNoDisp << std::endl;
    std::cout << "High frequency energy (with dispersion): " << highFreqEnergyWithDisp << std::endl;
}

TEST_F(NatureAdvancedPhysicsTests, Dispersion_ParameterIsSmooth)
{
    // Verify that dispersion parameter changes smoothly
    double sampleRate = 48000.0;

    std::vector<float> dispersionValues = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    std::vector<float> rmsValues;

    for (float disp : dispersionValues)
    {
        DSP::WaveguideString string;
        string.prepare(sampleRate);
        string.setFrequency(440.0f);
        string.setDispersion(disp);

        float exciter[] = {1.0f, 0.0f};
        string.excite(exciter, 2, 0.8f);

        juce::AudioBuffer<float> buffer(1, 1000);
        for (int i = 0; i < 1000; ++i)
        {
            buffer.setSample(0, i, string.processSample());
        }

        rmsValues.push_back(calculateRMS(buffer));
    }

    // RMS should change smoothly with dispersion
    for (size_t i = 1; i < rmsValues.size(); ++i)
    {
        float difference = std::abs(rmsValues[i] - rmsValues[i - 1]);
        EXPECT_LT(difference, 0.1f)
            << "Dispersion parameter should change smoothly (large jump at value "
            << dispersionValues[i] << ")";
    }
}

//==============================================================================
// TEST: Sympathetic Coupling
//==============================================================================

TEST_F(NatureAdvancedPhysicsTests, Sympathetic_CouplingAffectsOutput)
{
    // Verify that sympathetic coupling affects output
    double sampleRate = 48000.0;

    DSP::WaveguideString string;
    string.prepare(sampleRate);
    string.setFrequency(440.0f);
    string.setSympatheticCoupling(0.0f);  // No coupling

    float exciter[] = {1.0f, 0.0f};
    string.excite(exciter, 2, 0.8f);

    juce::AudioBuffer<float> bufferNoCoupling(1, 1000);
    for (int i = 0; i < 1000; ++i)
    {
        bufferNoCoupling.setSample(0, i, string.processSample());
    }

    // Reset with coupling
    string.reset();
    string.prepare(sampleRate);
    string.setFrequency(440.0f);
    string.setSympatheticCoupling(0.5f);  // Moderate coupling
    string.excite(exciter, 2, 0.8f);

    juce::AudioBuffer<float> bufferWithCoupling(1, 1000);
    for (int i = 0; i < 1000; ++i)
    {
        bufferWithCoupling.setSample(0, i, string.processSample());
    }

    float rmsNoCoupling = calculateRMS(bufferNoCoupling);
    float rmsWithCoupling = calculateRMS(bufferWithCoupling);

    // Coupling should affect output
    EXPECT_NE(rmsNoCoupling, rmsWithCoupling)
        << "Sympathetic coupling should affect output";

    std::cout << "RMS (no coupling): " << rmsNoCoupling << std::endl;
    std::cout << "RMS (with coupling): " << rmsWithCoupling << std::endl;
}

//==============================================================================
// TEST: Bridge Impedance
//==============================================================================

TEST_F(NatureAdvancedPhysicsTests, BridgeImpedance_AffectsReflection)
{
    // Verify that bridge impedance affects reflection
    double sampleRate = 48000.0;

    // Thin string (low impedance)
    DSP::WaveguideString thinString;
    thinString.prepare(sampleRate);
    thinString.setFrequency(440.0f);
    thinString.setStringGauge(DSP::WaveguideString::StringGauge::Thin);

    float exciter[] = {1.0f, 0.0f};
    thinString.excite(exciter, 2, 0.8f);

    juce::AudioBuffer<float> bufferThin(1, 1000);
    for (int i = 0; i < 1000; ++i)
    {
        bufferThin.setSample(0, i, thinString.processSample());
    }

    // Thick string (high impedance)
    DSP::WaveguideString thickString;
    thickString.prepare(sampleRate);
    thickString.setFrequency(440.0f);
    thickString.setStringGauge(DSP::WaveguideString::StringGauge::Massive);
    thickString.excite(exciter, 2, 0.8f);

    juce::AudioBuffer<float> bufferThick(1, 1000);
    for (int i = 0; i < 1000; ++i)
    {
        bufferThick.setSample(0, i, thickString.processSample());
    }

    float rmsThin = calculateRMS(bufferThin);
    float rmsThick = calculateRMS(bufferThick);

    // Impedance should affect output
    EXPECT_NE(rmsThin, rmsThick)
        << "Bridge impedance should affect reflection";

    std::cout << "RMS (thin string, low impedance): " << rmsThin << std::endl;
    std::cout << "RMS (thick string, high impedance): " << rmsThick << std::endl;
}

//==============================================================================
// TEST: Material Presets
//==============================================================================

TEST_F(NatureAdvancedPhysicsTests, Material_GuitarPresetHasCorrectModes)
{
    // Verify guitar preset has correct mode frequencies
    DSP::ModalBodyResonator body;
    body.prepare(48000.0);
    body.loadGuitarBodyPreset();

    // Check fundamental frequency
    EXPECT_NEAR(body.getModeFrequency(0), 95.0f, 1.0f)
        << "Guitar fundamental should be ~95 Hz";

    // Check number of modes
    EXPECT_GE(body.getModeFrequency(7), 1000.0f)
        << "Guitar should have at least 8 modes";

    std::cout << "Guitar body modes:" << std::endl;
    for (int i = 0; i < 8; ++i)
    {
        std::cout << "  Mode " << i << ": " << body.getModeFrequency(i) << " Hz" << std::endl;
    }
}

TEST_F(NatureAdvancedPhysicsTests, Material_PianoPresetIsMoreResonant)
{
    // Verify piano preset is more resonant than guitar
    double sampleRate = 48000.0;

    DSP::ModalBodyResonator guitar;
    guitar.prepare(sampleRate);
    guitar.loadGuitarBodyPreset();

    DSP::ModalBodyResonator piano;
    piano.prepare(sampleRate);
    piano.loadPianoBodyPreset();

    // Excite both
    juce::AudioBuffer<float> guitarResponse(1, 48000);
    juce::AudioBuffer<float> pianoResponse(1, 48000);

    for (int i = 0; i < 48000; ++i)
    {
        float input = (i == 0) ? 1.0f : 0.0f;
        guitarResponse.setSample(0, i, guitar.processSample(input));
        pianoResponse.setSample(0, i, piano.processSample(input));
    }

    float guitarT60 = findT60(guitarResponse, sampleRate);
    float pianoT60 = findT60(pianoResponse, sampleRate);

    // Piano should have longer decay (higher Q)
    EXPECT_GT(pianoT60, guitarT60)
        << "Piano should be more resonant (longer T60)";

    std::cout << "Guitar T60: " << (guitarT60 * 1000.0f) << " ms" << std::endl;
    std::cout << "Piano T60: " << (pianoT60 * 1000.0f) << " ms" << std::endl;
}

TEST_F(NatureAdvancedPhysicsTests, Material_OrchestralStringIsBrightest)
{
    // Verify orchestral string preset is brightest (most metallic)
    double sampleRate = 48000.0;

    DSP::ModalBodyResonator orchestral;
    orchestral.prepare(sampleRate);
    orchestral.loadOrchestralStringPreset();

    // Check that it has higher frequency modes
    float highestMode = orchestral.getModeFrequency(7);
    EXPECT_GT(highestMode, 2000.0f)
        << "Orchestral strings should have high-frequency modes";

    std::cout << "Orchestral string highest mode: " << highestMode << " Hz" << std::endl;
}

//==============================================================================
// TEST: Integration Tests
//==============================================================================

TEST_F(NatureAdvancedPhysicsTests, Integration_AllFeaturesWorkTogether)
{
    // Verify that all features work together without issues
    double sampleRate = 48000.0;

    DSP::WaveguideString string;
    string.prepare(sampleRate);

    // Enable all features
    string.setFrequency(440.0f);
    string.setDispersion(0.7f);
    string.setSympatheticCoupling(0.3f);
    string.setStringGauge(DSP::WaveguideString::StringGauge::Normal);
    string.setDamping(0.996f);
    string.setBrightness(0.6f);
    string.setBridgeCoupling(0.4f);

    // Excite and process
    float exciter[] = {1.0f, 0.8f, 0.5f, 0.0f};
    string.excite(exciter, 4, 0.9f);

    juce::AudioBuffer<float> output(1, 10000);

    bool allFinite = true;
    for (int i = 0; i < 10000; ++i)
    {
        float sample = string.processSample();
        output.setSample(0, i, sample);

        if (!std::isfinite(sample))
        {
            allFinite = false;
            break;
        }
    }

    EXPECT_TRUE(allFinite)
        << "All features should work together without producing NaN/inf";

    float rms = calculateRMS(output);
    EXPECT_GT(rms, 0.0f)
        << "Output should have energy";

    std::cout << "Integration test RMS: " << rms << std::endl;
}

TEST_F(NatureAdvancedPhysicsTests, Performance_CPUUsageIsReasonable)
{
    // Verify that CPU usage is reasonable with all features enabled
    double sampleRate = 48000.0;

    DSP::WaveguideString string;
    string.prepare(sampleRate);
    string.setFrequency(440.0f);
    string.setDispersion(0.7f);
    string.setSympatheticCoupling(0.3f);

    float exciter[] = {1.0f, 0.0f};
    string.excite(exciter, 2, 0.8f);

    // Measure processing time
    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 48000; ++i)  // 1 second of audio
    {
        string.processSample();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    double processingTimeMs = duration.count() / 1000.0;
    double audioTimeMs = 1000.0;  // 1 second
    double cpuPercent = (processingTimeMs / audioTimeMs) * 100.0;

    // Target: < 1% CPU for single voice with all features
    EXPECT_LT(cpuPercent, 1.0)
        << "CPU usage should be < 1% for single voice (actual: " << cpuPercent << "%)";

    std::cout << "CPU usage with all features: " << cpuPercent << "%" << std::endl;
}
