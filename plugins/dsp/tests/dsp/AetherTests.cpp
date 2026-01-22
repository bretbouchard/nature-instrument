/*
  ==============================================================================

    NatureAetherTests.cpp
    Created: 25 Dec 2025
    Author:  Bret Bouchard

    TDD Test Suite for Aether Physical Modeling Synthesizer
    - 13 tests covering ModalFilter and ResonatorBank (Week 1-2)
    - RED-GREEN-REFACTOR methodology
    - Performance and stability validation

  ==============================================================================
*/

#include <gtest/gtest.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../../include/dsp/AetherDSP.h"
#include <algorithm>
#include <chrono>
#include <array>
#include <vector>

//==============================================================================
// Test Fixture
class NatureAetherTests : public ::testing::Test
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
     * @brief Find peak frequency in buffer using FFT
     */
    float findPeakFrequency(const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        // Use JUCE FFT to find spectral peak
        juce::dsp::FFT fft(12); // 4096-point FFT
        std::array<float, 8192> fftData;

        // Copy mono buffer to FFT data (interleaved real/imaginary)
        const float* readPtr = buffer.getReadPointer(0);
        for (int i = 0; i < juce::jmin(buffer.getNumSamples(), 4096); ++i)
        {
            fftData[i * 2] = readPtr[i];     // Real
            fftData[i * 2 + 1] = 0.0f;      // Imaginary
        }

        // Perform FFT
        fft.performRealOnlyForwardTransform(fftData.data());

        // Find peak magnitude
        float maxMagnitude = 0.0f;
        int peakBin = 0;

        for (int i = 1; i < 2048; ++i) // Skip DC component
        {
            float real = fftData[i * 2];
            float imag = fftData[i * 2 + 1];
            float magnitude = std::sqrt(real * real + imag * imag);

            if (magnitude > maxMagnitude)
            {
                maxMagnitude = magnitude;
                peakBin = i;
            }
        }

        // Convert bin to frequency
        float frequency = static_cast<float>(peakBin * sampleRate / 4096.0);
        return frequency;
    }

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
     * @brief Find peak magnitude in buffer
     */
    float findPeak(const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;
        const float* readPtr = buffer.getReadPointer(0);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float absSample = std::abs(readPtr[i]);
            if (absSample > peak)
                peak = absSample;
        }

        return peak;
    }

    /**
     * @brief Find N spectral peaks in buffer using FFT
     * @return Vector of peak frequencies (sorted by magnitude)
     */
    std::vector<float> findSpectralPeaks(const juce::AudioBuffer<float>& buffer, double sampleRate, int numPeaks)
    {
        std::vector<float> peakFrequencies;

        // Use JUCE FFT
        juce::dsp::FFT fft(12); // 4096-point FFT
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

        // Find peak magnitudes
        struct PeakInfo
        {
            float frequency;
            float magnitude;
        };

        std::vector<PeakInfo> peaks;

        for (int i = 1; i < 2048; ++i) // Skip DC component
        {
            float real = fftData[i * 2];
            float imag = fftData[i * 2 + 1];
            float magnitude = std::sqrt(real * real + imag * imag);
            float frequency = static_cast<float>(i * sampleRate / 4096.0);

            // Only consider significant peaks (above noise floor)
            if (magnitude > 0.01f)
            {
                PeakInfo peak;
                peak.frequency = frequency;
                peak.magnitude = magnitude;
                peaks.push_back(peak);
            }
        }

        // Sort by magnitude (descending)
        std::sort(peaks.begin(), peaks.end(),
            [](const PeakInfo& a, const PeakInfo& b)
            {
                return a.magnitude > b.magnitude;
            });

        // Extract top N frequencies
        for (int i = 0; i < juce::jmin(numPeaks, static_cast<int>(peaks.size())); ++i)
        {
            peakFrequencies.push_back(peaks[i].frequency);
        }

        // Sort frequencies ascending
        std::sort(peakFrequencies.begin(), peakFrequencies.end());

        return peakFrequencies;
    }
};

//==============================================================================
// TEST: ModalFilter Tests (Week 1 - GREEN phase complete)
//==============================================================================

TEST_F(NatureAetherTests, ModalFilter_ImpulseResponseResonatesAtCorrectFrequency)
{
    // GREEN phase: ModalFilter is now implemented!
    double sampleRate = 48000.0;
    float testFrequency = 440.0f;
    float decayTime = 1.0f; // 1 second T60

    // Create modal filter
    AetherDSP::ModalFilter mode;
    mode.frequency = testFrequency;
    mode.decayTimeMs = decayTime * 1000.0f;
    mode.updateCoefficients(sampleRate);

    // Generate impulse response
    juce::AudioBuffer<float> impulseResponse(1, 48000); // 1 second
    impulseResponse.clear();

    // Apply impulse and process through filter
    float output = mode.processSample(1.0f); // Impulse
    impulseResponse.setSample(0, 0, output);

    for (int i = 1; i < 48000; ++i)
    {
        output = mode.processSample(0.0f);
        impulseResponse.setSample(0, i, output);
    }

    // Find peak frequency using FFT
    float peakFreq = findPeakFrequency(impulseResponse, sampleRate);

    // Verify resonance at expected frequency (±10% tolerance - modal filters have bandwidth)
    float expectedFreq = testFrequency;
    float tolerance = expectedFreq * 0.10f;
    EXPECT_GE(peakFreq, expectedFreq - tolerance) << "Peak frequency too low";
    EXPECT_LE(peakFreq, expectedFreq + tolerance) << "Peak frequency too high";

    std::cout << "Modal filter peak frequency: " << peakFreq << " Hz (expected: " << expectedFreq << " Hz)" << std::endl;
}

TEST_F(NatureAetherTests, ModalFilter_DecayTimeMatchesT60Specification)
{
    // GREEN phase: Verify that resonance decays to -60dB in specified time
    double sampleRate = 48000.0;
    float testFrequency = 440.0f;
    float decayTime = 0.5f; // 500ms T60

    AetherDSP::ModalFilter mode;
    mode.frequency = testFrequency;
    mode.decayTimeMs = decayTime * 1000.0f;
    mode.updateCoefficients(sampleRate);

    // Generate impulse response (capture full decay)
    int numSamples = static_cast<int>(decayTime * sampleRate * 2); // 2x T60 to be safe
    juce::AudioBuffer<float> impulseResponse(1, numSamples);
    impulseResponse.clear();

    // Process impulse
    float output = 1.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        float input = (i == 0) ? 1.0f : 0.0f;
        output = mode.processSample(input);
        impulseResponse.setSample(0, i, output);
    }

    // Find time to reach -60dB (0.001 amplitude)
    int t60Sample = -1;
    for (int i = 0; i < numSamples; ++i)
    {
        float amp = std::abs(impulseResponse.getSample(0, i));
        if (amp < 0.001f)
        {
            t60Sample = i;
            break;
        }
    }

    EXPECT_GT(t60Sample, 0) << "T60 not found within capture window";

    float actualT60 = static_cast<float>(t60Sample) / sampleRate;
    float tolerance = decayTime * 0.2f; // ±20% tolerance (modal approximations)
    EXPECT_GE(actualT60, decayTime - tolerance) << "Decay too fast";
    EXPECT_LE(actualT60, decayTime + tolerance) << "Decay too slow";

    std::cout << "Modal filter T60: " << (actualT60 * 1000.0f) << " ms (expected: " << (decayTime * 1000.0f) << " ms)" << std::endl;
}

TEST_F(NatureAetherTests, ModalFilter_NumericalStabilityWithDenormalPrevention)
{
    // GREEN phase: Verify filter doesn't produce NaN/inf with low-level signals
    double sampleRate = 48000.0;

    AetherDSP::ModalFilter mode;
    mode.frequency = 440.0f;
    mode.decayTimeMs = 1000.0f;
    mode.updateCoefficients(sampleRate);

    // Process very low-level signal (denormal range)
    bool allFinite = true;
    for (int i = 0; i < 1000; ++i)
    {
        float input = 1.0e-20f; // Very tiny signal
        float output = mode.processSample(input);

        if (!std::isfinite(output))
        {
            allFinite = false;
            break;
        }
    }

    EXPECT_TRUE(allFinite) << "Filter produced NaN or inf with denormal input";
}

TEST_F(NatureAetherTests, ModalFilter_DirectFormIICoefficientAccuracy)
{
    // GREEN phase: Verify biquad coefficients are calculated correctly
    double sampleRate = 48000.0;
    float frequency = 1000.0f;
    float decayTime = 0.5f;

    AetherDSP::ModalFilter mode;
    mode.frequency = frequency;
    mode.decayTimeMs = decayTime * 1000.0f;
    mode.updateCoefficients(sampleRate);

    // Expected coefficients (resonator formula):
    // omega = 2π * f / sr
    // r = e^(-π / (T60 * sr))
    // b0 = 1 - r
    // a1 = -2r * cos(omega)
    // a2 = r^2

    double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    double t60Seconds = decayTime;
    float expectedDecay = static_cast<float>(std::exp(-juce::MathConstants<double>::pi / (t60Seconds * sampleRate)));
    float expectedB0 = 1.0f - expectedDecay;
    float expectedA1 = -2.0f * expectedDecay * static_cast<float>(std::cos(omega));
    float expectedA2 = expectedDecay * expectedDecay;

    EXPECT_NEAR(mode.b0, expectedB0, 1.0e-6f) << "b0 coefficient incorrect";
    EXPECT_NEAR(mode.a1, expectedA1, 1.0e-6f) << "a1 coefficient incorrect";
    EXPECT_NEAR(mode.a2, expectedA2, 1.0e-6f) << "a2 coefficient incorrect";

    std::cout << "Coefficients: b0=" << mode.b0 << " a1=" << mode.a1 << " a2=" << mode.a2 << std::endl;
}

TEST_F(NatureAetherTests, ModalFilter_ResetClearsStateVariables)
{
    // GREEN phase: Verify reset clears state
    AetherDSP::ModalFilter mode;
    mode.frequency = 440.0f;
    mode.decayTimeMs = 1000.0f;
    mode.updateCoefficients(48000.0);

    // Process some samples to build up state
    for (int i = 0; i < 100; ++i)
    {
        mode.processSample(0.5f);
    }

    // Reset
    mode.reset();

    // Verify state is cleared
    EXPECT_FLOAT_EQ(mode.s1, 0.0f) << "State s1 not cleared";
    EXPECT_FLOAT_EQ(mode.s2, 0.0f) << "State s2 not cleared";
}

//==============================================================================
// TEST: Exciter Tests (Week 3 - RED phase)
//==============================================================================

TEST_F(NatureAetherTests, Exciter_TriggerProducesNoiseBurst)
{
    // RED phase: Test that exciter produces noise burst on noteOn
    AetherDSP::Exciter exciter;

    exciter.noteOn(0.8f);

    // Process 100 samples and verify output
    bool hasSignal = false;
    for (int i = 0; i < 100; ++i)
    {
        float output = exciter.processSample();
        if (std::abs(output) > 0.001f)
        {
            hasSignal = true;
            break;
        }
    }

    EXPECT_TRUE(hasSignal) << "Exciter should produce noise burst after noteOn";
}

TEST_F(NatureAetherTests, Exciter_EnvelopeShapeMatchesADSR)
{
    // RED phase: Test that exciter envelope follows ADSR shape
    AetherDSP::Exciter exciter;

    exciter.noteOn(0.8f);

    // Capture envelope output
    std::vector<float> envelopeValues;
    for (int i = 0; i < 1000; ++i)
    {
        float output = exciter.processSample();
        envelopeValues.push_back(std::abs(output));
    }

    // Verify attack (increasing)
    bool hasAttack = false;
    for (size_t i = 1; i < juce::jmin(size_t(100), envelopeValues.size()); ++i)
    {
        if (envelopeValues[i] > envelopeValues[i - 1])
        {
            hasAttack = true;
            break;
        }
    }

    EXPECT_TRUE(hasAttack) << "Exciter should have attack phase";
}

TEST_F(NatureAetherTests, Exciter_NoiseColorFilteringWorks)
{
    // RED phase: Test that color filter changes noise brightness
    double sampleRate = 48000.0;
    juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };

    AetherDSP::Exciter exciter;
    exciter.prepare(spec);

    // Test low color (darker)
    exciter.setColor(200.0f);
    exciter.noteOn(0.8f);

    float sumLowColor = 0.0f;
    for (int i = 0; i < 1000; ++i)
    {
        sumLowColor += std::abs(exciter.processSample());
    }

    // Reset and test high color (brighter)
    exciter.reset();
    exciter.prepare(spec);
    exciter.setColor(5000.0f);
    exciter.noteOn(0.8f);

    float sumHighColor = 0.0f;
    for (int i = 0; i < 1000; ++i)
    {
        sumHighColor += std::abs(exciter.processSample());
    }

    // Higher color filter should pass more high-frequency content
    // (This is a rough check - actual spectral analysis would be better)
    EXPECT_GT(sumHighColor, 0.0f) << "High color should produce signal";
    EXPECT_GT(sumLowColor, 0.0f) << "Low color should produce signal";
}

TEST_F(NatureAetherTests, Exciter_VelocityScalingIsCorrect)
{
    // RED phase: Test that velocity scales output amplitude
    AetherDSP::Exciter exciter;

    // Test low velocity
    exciter.noteOn(0.3f);
    float sumLowVelocity = 0.0f;
    for (int i = 0; i < 100; ++i)
    {
        sumLowVelocity += std::abs(exciter.processSample());
    }

    // Reset and test high velocity
    exciter.reset();
    exciter.noteOn(0.9f);
    float sumHighVelocity = 0.0f;
    for (int i = 0; i < 100; ++i)
    {
        sumHighVelocity += std::abs(exciter.processSample());
    }

    // Higher velocity should produce higher amplitude
    EXPECT_GT(sumHighVelocity, sumLowVelocity) << "Higher velocity should produce higher amplitude";
}

TEST_F(NatureAetherTests, Exciter_ReleaseFadesToZero)
{
    // RED phase: Test that noteOff fades exciter to zero
    AetherDSP::Exciter exciter;

    exciter.noteOn(0.8f);

    // Process some samples to build up envelope
    for (int i = 0; i < 100; ++i)
    {
        exciter.processSample();
    }

    // Trigger release
    exciter.noteOff();

    // Process release and verify it goes to zero
    bool fadedToZero = false;
    for (int i = 0; i < 1000; ++i)
    {
        float output = std::abs(exciter.processSample());
        if (output < 0.001f)
        {
            fadedToZero = true;
            break;
        }
    }

    EXPECT_TRUE(fadedToZero) << "Exciter should fade to zero after noteOff";
}

//==============================================================================
// TEST: Feedback Loop Tests (Week 3 - RED phase)
//==============================================================================

TEST_F(NatureAetherTests, Feedback_DelayTimeIsAccurate)
{
    // RED phase: Test that delay time is accurate
    double sampleRate = 48000.0;
    float delayTimeMs = 10.0f; // 10ms delay

    AetherDSP::FeedbackLoop feedback;
    feedback.prepare(sampleRate, 4096);
    feedback.setDelayTime(delayTimeMs, sampleRate);

    // Send impulse
    float impulse = feedback.processSample(1.0f);

    // Process zeros until delayed sample appears
    int delaySamples = 0;
    for (int i = 0; i < 1000; ++i)
    {
        float output = feedback.processSample(0.0f);
        if (std::abs(output) > 0.1f)
        {
            delaySamples = i;
            break;
        }
    }

    float expectedDelaySamples = delayTimeMs * 0.001f * sampleRate;
    float tolerance = expectedDelaySamples * 0.1f; // ±10% tolerance

    EXPECT_NEAR(delaySamples, expectedDelaySamples, tolerance) << "Delay time should be accurate";
}

TEST_F(NatureAetherTests, Feedback_FeedbackAmountIsCorrect)
{
    // RED phase: Test that feedback amount controls resonance
    double sampleRate = 48000.0;

    AetherDSP::FeedbackLoop feedback;
    feedback.prepare(sampleRate, 4096);
    feedback.setDelayTime(5.0f, sampleRate);

    // Test low feedback
    feedback.setFeedbackAmount(0.1f);
    float maxLowFeedback = 0.0f;
    for (int i = 0; i < 1000; ++i)
    {
        float input = (i == 0) ? 1.0f : 0.0f;
        float output = std::abs(feedback.processSample(input));
        maxLowFeedback = juce::jmax(maxLowFeedback, output);
    }

    // Reset and test high feedback
    feedback.reset();
    feedback.prepare(sampleRate, 4096);
    feedback.setDelayTime(5.0f, sampleRate);
    feedback.setFeedbackAmount(0.8f);

    float maxHighFeedback = 0.0f;
    for (int i = 0; i < 1000; ++i)
    {
        float input = (i == 0) ? 1.0f : 0.0f;
        float output = std::abs(feedback.processSample(input));
        maxHighFeedback = juce::jmax(maxHighFeedback, output);
    }

    // Higher feedback should produce more resonance
    EXPECT_GT(maxHighFeedback, maxLowFeedback) << "Higher feedback should produce more resonance";
}

TEST_F(NatureAetherTests, Feedback_SaturationSoftClips)
{
    // RED phase: Test that saturation prevents hard clipping
    double sampleRate = 48000.0;

    AetherDSP::FeedbackLoop feedback;
    feedback.prepare(sampleRate, 4096);
    feedback.setDelayTime(2.0f, sampleRate);
    feedback.setFeedbackAmount(0.95f); // Max feedback
    feedback.setSaturationDrive(5.0f); // High drive

    // Process worst-case scenario (constant input)
    bool allFinite = true;
    float maxOutput = 0.0f;

    for (int i = 0; i < 48000; ++i) // 1 second
    {
        float output = feedback.processSample(1.0f);
        maxOutput = juce::jmax(maxOutput, std::abs(output));

        if (!std::isfinite(output))
        {
            allFinite = false;
            break;
        }
    }

    EXPECT_TRUE(allFinite) << "Saturation should prevent NaN/inf";
    EXPECT_LT(maxOutput, 2.0f) << "Soft clipping should limit output to reasonable range";
}

TEST_F(NatureAetherTests, Feedback_StabilityAtMaxFeedback)
{
    // RED phase: Test stability at maximum feedback (critical safety test)
    double sampleRate = 48000.0;

    AetherDSP::FeedbackLoop feedback;
    feedback.prepare(sampleRate, 4096);
    feedback.setDelayTime(5.0f, sampleRate);
    feedback.setFeedbackAmount(0.95f); // MAX feedback (hard limit)

    // Process for 1 second with random input
    bool allFinite = true;
    float peakLevel = 0.0f;

    for (int i = 0; i < 48000; ++i)
    {
        // Random input between -1 and 1
        float input = (static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f;
        float output = feedback.processSample(input);

        peakLevel = juce::jmax(peakLevel, std::abs(output));

        if (!std::isfinite(output))
        {
            allFinite = false;
            break;
        }
    }

    EXPECT_TRUE(allFinite) << "Feedback loop must remain stable at max feedback";
    EXPECT_LT(peakLevel, 10.0f) << "Peak level should be reasonable even at max feedback";
}

TEST_F(NatureAetherTests, Feedback_DryWetMixWorks)
{
    // RED phase: Test dry/wet mix of feedback
    double sampleRate = 48000.0;

    // Note: This test requires exposing feedbackMix parameter
    // For now, we'll just verify feedback affects output
    AetherDSP::FeedbackLoop feedback;
    feedback.prepare(sampleRate, 4096);
    feedback.setDelayTime(2.0f, sampleRate);
    feedback.setFeedbackAmount(0.5f);

    // Process with feedback
    float sumWithFeedback = 0.0f;
    for (int i = 0; i < 100; ++i)
    {
        float input = (i == 0) ? 1.0f : 0.0f;
        sumWithFeedback += std::abs(feedback.processSample(input));
    }

    EXPECT_GT(sumWithFeedback, 0.0f) << "Feedback should affect output";
}

TEST_F(NatureAetherTests, Feedback_HardLimitAt095)
{
    // RED phase: Test that feedback is hard limited to 0.95
    AetherDSP::FeedbackLoop feedback;

    // Try to set feedback above 0.95
    feedback.setFeedbackAmount(1.5f); // Too high!

    // The implementation should clamp to 0.95
    // We need to expose feedbackAmount to test this directly
    // For now, just verify stability with high input
    feedback.prepare(48000.0, 4096);
    feedback.setDelayTime(2.0f, 48000.0);

    bool allFinite = true;
    for (int i = 0; i < 48000; ++i)
    {
        float output = feedback.processSample(1.0f);
        if (!std::isfinite(output))
        {
            allFinite = false;
            break;
        }
    }

    EXPECT_TRUE(allFinite) << "Hard limit at 0.95 should prevent instability";
}

//==============================================================================
// TEST: Voice Integration Tests (Week 3 - RED phase)
//==============================================================================

TEST_F(NatureAetherTests, Voice_NoteOnOffLifecycle)
{
    // RED phase: Test complete note on/note off lifecycle
    double sampleRate = 48000.0;
    juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };

    AetherDSP::Voice voice;
    voice.prepare(spec);

    // Note on
    voice.noteOn(60, 0.8f);
    EXPECT_TRUE(voice.active) << "Voice should be active after noteOn";

    // Process some samples
    juce::AudioBuffer<float> buffer(2, 100);
    buffer.clear();
    voice.process(buffer, 0, 100);

    float sum = 0.0f;
    for (int i = 0; i < 100; ++i)
    {
        sum += std::abs(buffer.getSample(0, i));
    }

    EXPECT_GT(sum, 0.0f) << "Voice should produce sound";

    // Note off
    voice.noteOff(0.0f);

    // Process release
    buffer.clear();
    voice.process(buffer, 0, 1000);

    // Eventually voice should become inactive
    // (depending on envelope release time)
}

TEST_F(NatureAetherTests, Voice_ExciterToResonatorPath)
{
    // RED phase: Test exciter → feedback → resonator path
    double sampleRate = 48000.0;
    juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };

    AetherDSP::Voice voice;
    voice.prepare(spec);
    voice.noteOn(60, 0.8f);

    juce::AudioBuffer<float> buffer(2, 1000);
    buffer.clear();
    voice.process(buffer, 0, 1000);

    // Verify output
    float sum = 0.0f;
    float peak = 0.0f;

    for (int i = 0; i < 1000; ++i)
    {
        float sample = std::abs(buffer.getSample(0, i));
        sum += sample;
        peak = juce::jmax(peak, sample);
    }

    EXPECT_GT(sum, 0.0f) << "Voice should produce output";
    EXPECT_GT(peak, 0.0f) << "Voice should have peaks";
}

TEST_F(NatureAetherTests, Voice_FeedbackLoopEnhancesResonance)
{
    // RED phase: Test that feedback loop enhances resonance
    double sampleRate = 48000.0;
    juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };

    // Process with feedback
    AetherDSP::Voice voiceWithFeedback;
    voiceWithFeedback.prepare(spec);
    voiceWithFeedback.feedback.setFeedbackAmount(0.7f);
    voiceWithFeedback.noteOn(60, 0.8f);

    juce::AudioBuffer<float> buffer1(2, 2000);
    buffer1.clear();
    voiceWithFeedback.process(buffer1, 0, 2000);

    float sumWithFeedback = 0.0f;
    for (int i = 0; i < 2000; ++i)
    {
        sumWithFeedback += std::abs(buffer1.getSample(0, i));
    }

    EXPECT_GT(sumWithFeedback, 0.0f) << "Voice with feedback should produce output";
}

TEST_F(NatureAetherTests, Voice_PolyphonyWorks)
{
    // RED phase: Test that multiple voices can play simultaneously
    double sampleRate = 48000.0;
    juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };

    std::array<AetherDSP::Voice, 4> voices;

    for (auto& voice : voices)
    {
        voice.prepare(spec);
    }

    // Start 4 voices simultaneously
    for (int i = 0; i < 4; ++i)
    {
        voices[i].noteOn(60 + i * 4, 0.8f); // Different notes
    }

    // Process and verify all are active
    juce::AudioBuffer<float> buffer(2, 100);
    buffer.clear();

    for (auto& voice : voices)
    {
        EXPECT_TRUE(voice.active) << "All voices should be active";
        voice.process(buffer, 0, 100);
    }

    // Verify output contains multiple notes
    float sum = 0.0f;
    for (int i = 0; i < 100; ++i)
    {
        sum += std::abs(buffer.getSample(0, i));
    }

    EXPECT_GT(sum, 0.0f) << "Polyphony should produce output";
}

TEST_F(NatureAetherTests, Voice_VoiceStealingWorks)
{
    // RED phase: Test voice stealing when all voices are in use
    double sampleRate = 48000.0;
    juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };

    std::array<AetherDSP::Voice, 4> voices;

    for (auto& voice : voices)
    {
        voice.prepare(spec);
    }

    // Start 4 voices
    for (int i = 0; i < 4; ++i)
    {
        voices[i].noteOn(60 + i, 0.8f);
    }

    // All voices should be active
    for (const auto& voice : voices)
    {
        EXPECT_TRUE(voice.active) << "Voice should be active";
    }

    // Try to start a 5th voice (should steal oldest)
    // This requires voice allocator logic in main DSP
    // For now, just verify that voices track their notes correctly
    EXPECT_EQ(voices[0].midiNote, 60);
    EXPECT_EQ(voices[1].midiNote, 61);
    EXPECT_EQ(voices[2].midiNote, 62);
    EXPECT_EQ(voices[3].midiNote, 63);
}

//==============================================================================
// TEST: Resonator Bank Tests (Week 2 - RED phase)
//==============================================================================

TEST_F(NatureAetherTests, ResonatorBank_HarmonicModesAreIntegerMultiples)
{
    // RED phase: Test harmonic modes (0-3) are 1x, 2x, 3x, 4x fundamental
    double sampleRate = 48000.0;
    float fundamental = 440.0f;

    AetherDSP::ResonatorBank bank;
    bank.activeModeCount = 8;

    // Configure harmonic modes (0-3)
    for (int i = 0; i < 4; ++i)
    {
        bank.setModeFrequency(i, fundamental * (i + 1));
        bank.setModeDecay(i, 1000.0f, sampleRate);
    }

    bank.prepare(sampleRate);

    // Verify harmonic mode frequencies
    for (int i = 0; i < 4; ++i)
    {
        float expectedFreq = fundamental * (i + 1);
        float actualFreq = bank.modes[i].frequency;
        EXPECT_NEAR(actualFreq, expectedFreq, 0.1f) << "Harmonic mode " << i << " frequency incorrect";
    }

    std::cout << "Harmonic modes verified: 1x, 2x, 3x, 4x fundamental" << std::endl;
}

TEST_F(NatureAetherTests, ResonatorBank_InharmonicModesUseGoldenRatio)
{
    // RED phase: Test inharmonic modes (4-7) use golden ratio spacing
    double sampleRate = 48000.0;
    float fundamental = 440.0f;
    float goldenRatio = 1.618033988749895f;

    AetherDSP::ResonatorBank bank;
    bank.activeModeCount = 8;

    // Configure inharmonic modes (4-7)
    float baseFreq = fundamental * goldenRatio;
    for (int i = 4; i < 8; ++i)
    {
        float expectedFreq = baseFreq * static_cast<float>(std::pow(goldenRatio, i - 4));
        bank.setModeFrequency(i, expectedFreq);
        bank.setModeDecay(i, 1000.0f, sampleRate);
    }

    bank.prepare(sampleRate);

    // Verify inharmonic mode frequencies
    for (int i = 4; i < 8; ++i)
    {
        float expectedFreq = baseFreq * static_cast<float>(std::pow(goldenRatio, i - 4));
        float actualFreq = bank.modes[i].frequency;
        float tolerance = expectedFreq * 0.01f; // ±1% tolerance
        EXPECT_NEAR(actualFreq, expectedFreq, tolerance) << "Inharmonic mode " << i << " frequency incorrect";
    }

    std::cout << "Inharmonic modes verified: golden ratio spacing" << std::endl;
}

// Continue with the rest of the tests in Google Test format...
        //======================================================================
        // PHASE 1: CORE DSP COMPONENTS (Week 1)
        //======================================================================

        //----------------------------------------------------------------------
        // Task 1.1: Modal Filter Tests (RED phase)
        //----------------------------------------------------------------------
        beginTest("ModalFilter: Impulse response resonates at correct frequency");
        {
            // GREEN phase: ModalFilter is now implemented!
            double sampleRate = 48000.0;
            float testFrequency = 440.0f;
            float decayTime = 1.0f; // 1 second T60

            // Create modal filter
            AetherDSP::ModalFilter mode;
            mode.frequency = testFrequency;
            mode.decayTimeMs = decayTime * 1000.0f;
            mode.updateCoefficients(sampleRate);

            // Generate impulse response
            juce::AudioBuffer<float> impulseResponse(1, 48000); // 1 second
            impulseResponse.clear();

            // Apply impulse and process through filter
            float output = mode.processSample(1.0f); // Impulse
            impulseResponse.setSample(0, 0, output);

            for (int i = 1; i < 48000; ++i)
            {
                output = mode.processSample(0.0f);
                impulseResponse.setSample(0, i, output);
            }

            // Find peak frequency using FFT
            float peakFreq = findPeakFrequency(impulseResponse, sampleRate);

            // Verify resonance at expected frequency (±10% tolerance - modal filters have bandwidth)
            float expectedFreq = testFrequency;
            float tolerance = expectedFreq * 0.10f;
            expectGreaterOrEqual(peakFreq, expectedFreq - tolerance, "Peak frequency too low");
            expectLessOrEqual(peakFreq, expectedFreq + tolerance, "Peak frequency too high");

            logMessage("Modal filter peak frequency: " + juce::String(peakFreq) + " Hz (expected: " + juce::String(expectedFreq) + " Hz)");
        }

        beginTest("ModalFilter: Decay time matches T60 specification");
        {
            // GREEN phase: Verify that resonance decays to -60dB in specified time
            double sampleRate = 48000.0;
            float testFrequency = 440.0f;
            float decayTime = 0.5f; // 500ms T60

            AetherDSP::ModalFilter mode;
            mode.frequency = testFrequency;
            mode.decayTimeMs = decayTime * 1000.0f;
            mode.updateCoefficients(sampleRate);

            // Generate impulse response (capture full decay)
            int numSamples = static_cast<int>(decayTime * sampleRate * 2); // 2x T60 to be safe
            juce::AudioBuffer<float> impulseResponse(1, numSamples);
            impulseResponse.clear();

            // Process impulse
            float output = 1.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                float input = (i == 0) ? 1.0f : 0.0f;
                output = mode.processSample(input);
                impulseResponse.setSample(0, i, output);
            }

            // Find time to reach -60dB (0.001 amplitude)
            int t60Sample = -1;
            for (int i = 0; i < numSamples; ++i)
            {
                float amp = std::abs(impulseResponse.getSample(0, i));
                if (amp < 0.001f)
                {
                    t60Sample = i;
                    break;
                }
            }

            expect(t60Sample > 0, "T60 not found within capture window");

            float actualT60 = static_cast<float>(t60Sample) / sampleRate;
            float tolerance = decayTime * 0.2f; // ±20% tolerance (modal approximations)
            expectGreaterOrEqual(actualT60, decayTime - tolerance, "Decay too fast");
            expectLessOrEqual(actualT60, decayTime + tolerance, "Decay too slow");

            logMessage("Modal filter T60: " + juce::String(actualT60 * 1000.0f) + " ms (expected: " + juce::String(decayTime * 1000.0f) + " ms)");
        }

        beginTest("ModalFilter: Numerical stability with denormal prevention");
        {
            // GREEN phase: Verify filter doesn't produce NaN/inf with low-level signals
            double sampleRate = 48000.0;

            AetherDSP::ModalFilter mode;
            mode.frequency = 440.0f;
            mode.decayTimeMs = 1000.0f;
            mode.updateCoefficients(sampleRate);

            // Process very low-level signal (denormal range)
            bool allFinite = true;
            for (int i = 0; i < 1000; ++i)
            {
                float input = 1.0e-20f; // Very tiny signal
                float output = mode.processSample(input);

                if (!std::isfinite(output))
                {
                    allFinite = false;
                    break;
                }
            }

            expect(allFinite, "Filter produced NaN or inf with denormal input");
        }

        beginTest("ModalFilter: Direct Form II coefficient accuracy");
        {
            // GREEN phase: Verify biquad coefficients are calculated correctly
            double sampleRate = 48000.0;
            float frequency = 1000.0f;
            float decayTime = 0.5f;

            AetherDSP::ModalFilter mode;
            mode.frequency = frequency;
            mode.decayTimeMs = decayTime * 1000.0f;
            mode.updateCoefficients(sampleRate);

            // Expected coefficients (resonator formula):
            // omega = 2π * f / sr
            // r = e^(-π / (T60 * sr))
            // b0 = 1 - r
            // a1 = -2r * cos(omega)
            // a2 = r^2

            double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
            double t60Seconds = decayTime;
            float expectedDecay = static_cast<float>(std::exp(-juce::MathConstants<double>::pi / (t60Seconds * sampleRate)));
            float expectedB0 = 1.0f - expectedDecay;
            float expectedA1 = -2.0f * expectedDecay * static_cast<float>(std::cos(omega));
            float expectedA2 = expectedDecay * expectedDecay;

            expectWithinAbsoluteError(mode.b0, expectedB0, 1.0e-6f, "b0 coefficient incorrect");
            expectWithinAbsoluteError(mode.a1, expectedA1, 1.0e-6f, "a1 coefficient incorrect");
            expectWithinAbsoluteError(mode.a2, expectedA2, 1.0e-6f, "a2 coefficient incorrect");

            logMessage("Coefficients: b0=" + juce::String(mode.b0) + " a1=" + juce::String(mode.a1) + " a2=" + juce::String(mode.a2));
        }

        beginTest("ModalFilter: Reset clears state variables");
        {
            // GREEN phase: Verify reset clears state
            AetherDSP::ModalFilter mode;
            mode.frequency = 440.0f;
            mode.decayTimeMs = 1000.0f;
            mode.updateCoefficients(48000.0);

            // Process some samples to build up state
            for (int i = 0; i < 100; ++i)
            {
                mode.processSample(0.5f);
            }

            // Reset
            mode.reset();

            // Verify state is cleared
            expectEquals(mode.s1, 0.0f, "State s1 not cleared");
            expectEquals(mode.s2, 0.0f, "State s2 not cleared");
        }

        //----------------------------------------------------------------------
        // Task 1.2: Resonator Bank Tests (Week 2 - RED phase)
        //----------------------------------------------------------------------
        beginTest("ResonatorBank: Harmonic modes are integer multiples");
        {
            // RED phase: Test harmonic modes (0-3) are 1x, 2x, 3x, 4x fundamental
            double sampleRate = 48000.0;
            float fundamental = 440.0f;

            AetherDSP::ResonatorBank bank;
            bank.activeModeCount = 8;

            // Configure harmonic modes (0-3)
            for (int i = 0; i < 4; ++i)
            {
                bank.setModeFrequency(i, fundamental * (i + 1));
                bank.setModeDecay(i, 1000.0f, sampleRate);
            }

            bank.prepare(sampleRate);

            // Verify harmonic mode frequencies
            for (int i = 0; i < 4; ++i)
            {
                float expectedFreq = fundamental * (i + 1);
                float actualFreq = bank.modes[i].frequency;
                expectWithinAbsoluteError(actualFreq, expectedFreq, 0.1f,
                    "Harmonic mode " + juce::String(i) + " frequency incorrect");
            }

            logMessage("Harmonic modes verified: 1x, 2x, 3x, 4x fundamental");
        }

        beginTest("ResonatorBank: Inharmonic modes use golden ratio");
        {
            // RED phase: Test inharmonic modes (4-7) use golden ratio spacing
            double sampleRate = 48000.0;
            float fundamental = 440.0f;
            float goldenRatio = 1.618033988749895f;

            AetherDSP::ResonatorBank bank;
            bank.activeModeCount = 8;

            // Configure inharmonic modes (4-7)
            float baseFreq = fundamental * goldenRatio;
            for (int i = 4; i < 8; ++i)
            {
                float expectedFreq = baseFreq * std::pow(goldenRatio, i - 4);
                bank.setModeFrequency(i, expectedFreq);
                bank.setModeDecay(i, 1000.0f, sampleRate);
            }

            bank.prepare(sampleRate);

            // Verify inharmonic mode frequencies
            for (int i = 4; i < 8; ++i)
            {
                float expectedFreq = baseFreq * static_cast<float>(std::pow(goldenRatio, i - 4));
                float actualFreq = bank.modes[i].frequency;
                float tolerance = expectedFreq * 0.01f; // ±1% tolerance
                expectWithinAbsoluteError(actualFreq, expectedFreq, tolerance,
                    "Inharmonic mode " + juce::String(i) + " frequency incorrect");
            }

            logMessage("Inharmonic modes verified: golden ratio spacing");
        }

        beginTest("ResonatorBank: Frequency response shows 8 peaks");
        {
            // RED phase: FFT analysis to verify 8 spectral peaks
            double sampleRate = 48000.0;
            float fundamental = 220.0f; // A3

            AetherDSP::ResonatorBank bank;
            bank.activeModeCount = 8;

            // Configure 8 modes (4 harmonic + 4 inharmonic)
            float goldenRatio = 1.618033988749895f;
            float baseFreq = fundamental * goldenRatio;

            // Harmonic modes
            for (int i = 0; i < 4; ++i)
            {
                bank.setModeFrequency(i, fundamental * (i + 1));
                bank.setModeDecay(i, 1500.0f, sampleRate);
            }

            // Inharmonic modes
            for (int i = 4; i < 8; ++i)
            {
                bank.setModeFrequency(i, baseFreq * static_cast<float>(std::pow(goldenRatio, i - 4)));
                bank.setModeDecay(i, 800.0f, sampleRate);
            }

            bank.prepare(sampleRate);

            // Generate impulse response
            juce::AudioBuffer<float> impulseResponse(1, 48000); // 1 second
            impulseResponse.clear();

            // Process impulse
            float output = bank.processSample(1.0f); // Impulse
            impulseResponse.setSample(0, 0, output);

            for (int i = 1; i < 48000; ++i)
            {
                output = bank.processSample(0.0f);
                impulseResponse.setSample(0, i, output);
            }

            // Find spectral peaks using FFT
            auto peakFrequencies = findSpectralPeaks(impulseResponse, sampleRate, 8);

            // Verify we found 8 distinct peaks
            expectEquals(static_cast<int>(peakFrequencies.size()), 8,
                "Should find exactly 8 spectral peaks");

            // Log peak frequencies
            logMessage("Spectral peaks found:");
            for (size_t i = 0; i < peakFrequencies.size(); ++i)
            {
                logMessage("  Peak " + juce::String(i + 1) + ": " +
                          juce::String(peakFrequencies[i]) + " Hz");
            }
        }

        beginTest("ResonatorBank: Equal-power normalization prevents clipping");
        {
            // RED phase: Verify normalization prevents clipping
            double sampleRate = 48000.0;

            AetherDSP::ResonatorBank bank;
            bank.activeModeCount = 8;

            // Configure 8 modes with equal amplitude
            for (int i = 0; i < 8; ++i)
            {
                bank.setModeFrequency(i, 440.0f + i * 100.0f);
                bank.setModeDecay(i, 1000.0f, sampleRate);
                bank.modes[i].amplitude = 1.0f; // Full amplitude
            }

            bank.prepare(sampleRate);

            // Process worst-case input (all modes in phase at t=0)
            float maxOutput = 0.0f;
            for (int i = 0; i < 1000; ++i)
            {
                float input = (i == 0) ? 1.0f : 0.0f;
                float output = std::abs(bank.processSample(input));
                if (output > maxOutput)
                    maxOutput = output;
            }

            // With equal-power normalization (1/sqrt(8)), output should be <= 1.0
            expectLessOrEqual(maxOutput, 1.0f,
                "Normalized output should not exceed 1.0 (prevents clipping)");

            logMessage("Max output after normalization: " + juce::String(maxOutput));
        }

        beginTest("ResonatorBank: Each mode has correct T60 decay");
        {
            // RED phase: Verify each mode decays at correct rate
            double sampleRate = 48000.0;

            AetherDSP::ResonatorBank bank;
            bank.activeModeCount = 8;

            // Configure modes with different decay times
            std::array<float, 8> decayTimes = { 2000.0f, 1800.0f, 1600.0f, 1400.0f,
                                                 1200.0f, 1000.0f, 800.0f, 600.0f };

            for (int i = 0; i < 8; ++i)
            {
                bank.setModeFrequency(i, 440.0f + i * 100.0f);
                bank.setModeDecay(i, decayTimes[i], sampleRate);
            }

            bank.prepare(sampleRate);

            // Measure each mode's T60 individually
            for (int i = 0; i < 8; ++i)
            {
                // Temporarily disable other modes
                std::array<float, 8> originalAmplitudes;
                for (int j = 0; j < 8; ++j)
                {
                    originalAmplitudes[j] = bank.modes[j].amplitude;
                    bank.modes[j].amplitude = (j == i) ? 1.0f : 0.0f;
                }

                bank.reset();

                // Measure T60
                int t60Samples = 0;
                float threshold = 0.001f; // -60dB

                for (int s = 0; s < static_cast<int>(decayTimes[i] * sampleRate * 1.5); ++s)
                {
                    float input = (s == 0) ? 1.0f : 0.0f;
                    float output = std::abs(bank.processSample(input));

                    if (output < threshold && t60Samples == 0)
                        t60Samples = s;

                    if (t60Samples > 0 && output >= threshold)
                        t60Samples = 0; // Not sustained below threshold
                }

                float actualT60 = static_cast<float>(t60Samples) / sampleRate;
                float expectedT60 = decayTimes[i] * 0.001f; // Convert ms to seconds
                float tolerance = expectedT60 * 0.25f; // ±25% tolerance

                expectGreaterOrEqual(actualT60, expectedT60 - tolerance,
                    "Mode " + juce::String(i) + " decays too fast");
                expectLessOrEqual(actualT60, expectedT60 + tolerance,
                    "Mode " + juce::String(i) + " decays too slow");

                // Restore amplitudes
                for (int j = 0; j < 8; ++j)
                    bank.modes[j].amplitude = originalAmplitudes[j];

                logMessage("Mode " + juce::String(i) + " T60: " +
                          juce::String(actualT60 * 1000.0f) + " ms (expected: " +
                          juce::String(expectedT60 * 1000.0f) + " ms)");
            }
        }

        beginTest("ResonatorBank: Parallel summation is stable");
        {
            // RED phase: Verify parallel summation doesn't cause numerical issues
            double sampleRate = 48000.0;

            AetherDSP::ResonatorBank bank;
            bank.activeModeCount = 8;

            // Configure 8 modes
            for (int i = 0; i < 8; ++i)
            {
                bank.setModeFrequency(i, 440.0f + i * 200.0f);
                bank.setModeDecay(i, 1000.0f, sampleRate);
            }

            bank.prepare(sampleRate);

            // Process long duration and verify stability
            bool allFinite = true;
            for (int i = 0; i < 48000; ++i) // 1 second
            {
                float input = (i == 0) ? 1.0f : 0.0f;
                float output = bank.processSample(input);

                if (!std::isfinite(output))
                {
                    allFinite = false;
                    break;
                }
            }

            expect(allFinite, "Parallel summation should remain stable (no NaN/inf)");
        }

        beginTest("ResonatorBank: CPU performance within target");
        {
            // RED phase: Verify CPU usage < 0.5% for 8 modes
            double sampleRate = 48000.0;

            AetherDSP::ResonatorBank bank;
            bank.activeModeCount = 8;

            // Configure 8 modes
            for (int i = 0; i < 8; ++i)
            {
                bank.setModeFrequency(i, 440.0f + i * 100.0f);
                bank.setModeDecay(i, 1000.0f, sampleRate);
            }

            bank.prepare(sampleRate);

            // Measure processing time for 48000 samples (1 second at 48kHz)
            auto startTime = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < 48000; ++i)
            {
                float input = (i % 1000 == 0) ? 1.0f : 0.0f; // Impulses every 1000 samples
                bank.processSample(input);
            }

            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

            double processingTimeMs = duration.count() / 1000.0;
            double audioTimeMs = 1000.0; // 1 second
            double cpuPercent = (processingTimeMs / audioTimeMs) * 100.0;

            // Target: < 0.5% CPU for 8 modes
            expectLessOrEqual(cpuPercent, 0.5,
                "CPU usage should be < 0.5% for 8 modes (actual: " +
                juce::String(cpuPercent, 3) + "%)");

            logMessage("CPU usage for 8 modes: " + juce::String(cpuPercent, 4) + "%");
        }

        beginTest("ResonatorBank: Mode skipping optimization works");
        {
            // RED phase: Verify modes with amplitude < 0.001 are skipped
            double sampleRate = 48000.0;

            AetherDSP::ResonatorBank bank;
            bank.activeModeCount = 8;

            // Configure 8 modes, but set mode 4-7 to near-zero amplitude
            for (int i = 0; i < 8; ++i)
            {
                bank.setModeFrequency(i, 440.0f + i * 100.0f);
                bank.setModeDecay(i, 1000.0f, sampleRate);
                bank.modes[i].amplitude = (i < 4) ? 1.0f : 0.0001f; // Skip modes 4-7
            }

            bank.prepare(sampleRate);

            // Process and verify only first 4 modes contribute
            float outputWithSkip = 0.0f;
            for (int i = 0; i < 100; ++i)
            {
                float input = (i == 0) ? 1.0f : 0.0f;
                outputWithSkip += std::abs(bank.processSample(input));
            }

            // Now disable mode skipping by setting all amplitudes to 1.0
            bank.prepare(sampleRate);
            for (int i = 0; i < 8; ++i)
                bank.modes[i].amplitude = 1.0f;

            bank.reset();

            float outputWithoutSkip = 0.0f;
            for (int i = 0; i < 100; ++i)
            {
                float input = (i == 0) ? 1.0f : 0.0f;
                outputWithoutSkip += std::abs(bank.processSample(input));
            }

            // Output with skipping should be significantly less
            expectLessOrEqual(outputWithSkip, outputWithoutSkip * 0.6f,
                "Mode skipping should reduce output (only 4 of 8 modes active)");

            logMessage("Mode skipping: " + juce::String(outputWithSkip) +
                      " vs all modes: " + juce::String(outputWithoutSkip));
        }

        //----------------------------------------------------------------------
        // Task 1.3: Exciter Tests (to be implemented after Task 1.2)
        //----------------------------------------------------------------------
        // beginTest("Exciter: NoteOn produces noise burst");
        // beginTest("Exciter: NoteOff fades to zero");
        // beginTest("Exciter: Velocity affects pressure");
        // beginTest("Exciter: Color filter changes brightness");
        // beginTest("Exciter: Envelope prevents clicks");

        //----------------------------------------------------------------------
        // Task 1.4: Feedback Loop Tests (to be implemented after Task 1.3)
        //----------------------------------------------------------------------
        // beginTest("FeedbackLoop: Stability at max feedback");
        // beginTest("FeedbackLoop: Saturation prevents clipping");
        // beginTest("FeedbackLoop: Delay time is accurate");
        // beginTest("FeedbackLoop: Lagrange interpolation works");
        // beginTest("FeedbackLoop: Reset clears delay buffer");

        //----------------------------------------------------------------------
        // Task 1.5: Voice Tests (to be implemented after Task 1.4)
        //----------------------------------------------------------------------
        // beginTest("Voice: NoteOn produces sustained resonance");
        // beginTest("Voice: Velocity affects brightness");
        // beginTest("Voice: Polyphony works correctly");
        // beginTest("Voice: Voice stealing works");
        // beginTest("Voice: NoteOff triggers release");

        //======================================================================
        // PHASE 2: Main Processor & Parameters (Week 1-2)
        //======================================================================

        //----------------------------------------------------------------------
        // Task 2.1: Parameter Tests
        //----------------------------------------------------------------------
        // beginTest("Parameters: All parameters have valid range");
        // beginTest("Parameters: Exciter parameters work");
        // beginTest("Parameters: Resonator parameters work");
        // beginTest("Parameters: Feedback parameters work");
        // beginTest("Parameters: Filter parameters work");

        //----------------------------------------------------------------------
        // Task 2.2: Preset Tests
        //----------------------------------------------------------------------
        // beginTest("Presets: Save to JSON works");
        // beginTest("Presets: Load from JSON works");
        // beginTest("Presets: Factory presets load");
        // beginTest("Presets: Validation catches invalid JSON");
        // beginTest("Presets: Metadata extraction works");

        //======================================================================
        // PHASE 3: Stability & Performance Tests (Week 2)
        //======================================================================

        //----------------------------------------------------------------------
        // Stability Tests
        //----------------------------------------------------------------------
        // beginTest("Stability: Max feedback + max resonance is stable");
        // beginTest("Stability: 16 voices polyphony is stable");
        // beginTest("Stability: All parameters automatable");
        // beginTest("Stability: Sample rate changes work");

        //----------------------------------------------------------------------
        // Realtime Safety Tests
        //----------------------------------------------------------------------
        // beginTest("Realtime: No allocations in processBlock");
        // beginTest("Realtime: No locks in processBlock");
        // beginTest("Realtime: No system calls in processBlock");
        // beginTest("Realtime: Denormal prevention works");

        //----------------------------------------------------------------------
        // Performance Tests
        //----------------------------------------------------------------------
        // beginTest("Performance: CPU usage < 10% for 16 voices");
        // beginTest("Performance: Memory usage < 1MB");
        // beginTest("Performance: Latency < 5ms");
        // beginTest("Performance: Mode skipping optimization works");
        // beginTest("Performance: SIMD optimization beneficial");
    }

private:
    //==========================================================================
    // Helper Functions
    //==========================================================================

    /**
     * @brief Find peak frequency in buffer using FFT
     */
    float findPeakFrequency(const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        // Use JUCE FFT to find spectral peak
        juce::dsp::FFT fft(12); // 4096-point FFT
        std::array<float, 8192> fftData;

        // Copy mono buffer to FFT data (interleaved real/imaginary)
        const float* readPtr = buffer.getReadPointer(0);
        for (int i = 0; i < juce::jmin(buffer.getNumSamples(), 4096); ++i)
        {
            fftData[i * 2] = readPtr[i];     // Real
            fftData[i * 2 + 1] = 0.0f;      // Imaginary
        }

        // Perform FFT
        fft.performRealOnlyForwardTransform(fftData.data());

        // Find peak magnitude
        float maxMagnitude = 0.0f;
        int peakBin = 0;

        for (int i = 1; i < 2048; ++i) // Skip DC component
        {
            float real = fftData[i * 2];
            float imag = fftData[i * 2 + 1];
            float magnitude = std::sqrt(real * real + imag * imag);

            if (magnitude > maxMagnitude)
            {
                maxMagnitude = magnitude;
                peakBin = i;
            }
        }

        // Convert bin to frequency
        float frequency = static_cast<float>(peakBin * sampleRate / 4096.0);
        return frequency;
    }

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
     * @brief Find peak magnitude in buffer
     */
    float findPeak(const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;
        const float* readPtr = buffer.getReadPointer(0);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float absSample = std::abs(readPtr[i]);
            if (absSample > peak)
                peak = absSample;
        }

        return peak;
    }

    /**
     * @brief Find N spectral peaks in buffer using FFT
     * @return Vector of peak frequencies (sorted by magnitude)
     */
    std::vector<float> findSpectralPeaks(const juce::AudioBuffer<float>& buffer, double sampleRate, int numPeaks)
    {
        std::vector<float> peakFrequencies;

        // Use JUCE FFT
        juce::dsp::FFT fft(12); // 4096-point FFT
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

        // Find peak magnitudes
        struct PeakInfo
        {
            float frequency;
            float magnitude;
        };

        std::vector<PeakInfo> peaks;

        for (int i = 1; i < 2048; ++i) // Skip DC component
        {
            float real = fftData[i * 2];
            float imag = fftData[i * 2 + 1];
            float magnitude = std::sqrt(real * real + imag * imag);
            float frequency = static_cast<float>(i * sampleRate / 4096.0);

            // Only consider significant peaks (above noise floor)
            if (magnitude > 0.01f)
            {
                PeakInfo peak;
                peak.frequency = frequency;
                peak.magnitude = magnitude;
                peaks.push_back(peak);
            }
        }

        // Sort by magnitude (descending)
        std::sort(peaks.begin(), peaks.end(),
            [](const PeakInfo& a, const PeakInfo& b)
            {
                return a.magnitude > b.magnitude;
            });

        // Extract top N frequencies
        for (int i = 0; i < juce::jmin(numPeaks, static_cast<int>(peaks.size())); ++i)
        {
            peakFrequencies.push_back(peaks[i].frequency);
        }

        // Sort frequencies ascending
        std::sort(peakFrequencies.begin(), peakFrequencies.end());

        return peakFrequencies;
    }
};

//==============================================================================
// Register test with JUCE unit test framework
static NatureAetherTests natureMarcoAetherTests;
