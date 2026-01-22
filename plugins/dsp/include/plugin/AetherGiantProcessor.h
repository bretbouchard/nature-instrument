/*
  ==============================================================================

   AetherGiantProcessor.h
   VST3/AU Plugin Processor for Aether Giant Instruments

   Provides:
   - All 5 giant instruments (switchable)
   - Parameter automation
   - Preset loading/saving
   - MIDI/MPE support
   - State management for DAW projects

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/InstrumentDSP.h"
#include <memory>
#include <array>

//==============================================================================
/**
 * Giant instrument type selector
 */
enum class GiantInstrumentType
{
    GiantStrings,      // Aether Giant Strings
    GiantDrums,         // Giant Drums
    GiantVoice,         // Giant Voice/Roar
    GiantHorns,         // Giant Horns
    GiantPercussion     // Giant Percussion
};

//==============================================================================
/**
 * Audio Processor for Aether Giant Instruments
 *
 * Implements both VST3 and AU formats via JUCE
 */
class AetherGiantProcessor : public juce::AudioProcessor
#if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
#endif
{
public:
    //==============================================================================
    AetherGiantProcessor();
    ~AetherGiantProcessor() override;

    //==============================================================================
    // juce::AudioProcessor implementation
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Aether Giant"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    //==============================================================================
    // Programs (presets)
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    // Parameters
    int getNumParameters() override;
    float getParameter(int index) override;
    void setParameter(int index, float value) override;
    const juce::String getParameterName(int index) override;
    const juce::String getParameterText(int index) override;

    // Parameter shortcuts (for VST3/AU automation)
    void setInstrumentType(GiantInstrumentType type);
    GiantInstrumentType getInstrumentType() const { return instrumentType; }

    //==============================================================================
    // State
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Preset management
    bool loadPresetFromFile(const juce::File& presetFile);
    bool savePresetToFile(const juce::File& presetFile);
    void refreshPresetList();

    //==============================================================================
    // MPE Support
    bool isMPEEnabled() const { return mpeEnabled; }
    void setMPEEnabled(bool enabled);

    //==============================================================================
    // Info
    const juce::String getInputChannelName(int channelIndex) const override;
    const juce::String getOutputChannelName(int channelIndex) const override;
    bool isInputChannelStereoPair(int index) const override;
    bool isOutputChannelStereoPair(int index) const override;

    bool supportsMPE() const override { return true; }
    bool supportsDoublePrecisionProcessing() const override { return false; }

    //==============================================================================
    // Playhead
    juce::AudioPlayHead::CurrentPositionInfo getLastPositionInfo() const
    {
        return positionInfo;
    }

protected:
    juce::AudioPlayHead::CurrentPositionInfo positionInfo;

private:
    //==============================================================================
    // Current instrument DSP instance
    std::unique_ptr<DSP::InstrumentDSP> currentInstrument;
    GiantInstrumentType instrumentType = GiantInstrumentType::GiantStrings;

    // MPE state
    bool mpeEnabled = false;

    // Preset management
    juce::File presetsFolder;
    juce::StringArray presetNames;
    int currentProgramIndex = 0;

    // Critical section for DSP access
    juce::CriticalSection dspLock;

    //==============================================================================
    // Factory functions to create instruments
    std::unique_ptr<DSP::InstrumentDSP> createInstrument(GiantInstrumentType type);
    void switchInstrument(GiantInstrumentType type);

    // MIDI processing
    void processMIDI(juce::MidiBuffer& midiMessages,
                     std::vector<DSP::ScheduledEvent>& events);
    void midiMessageToEvent(const juce::MidiMessage& msg,
                           DSP::ScheduledEvent& event,
                           double sampleRate);

    // Preset scanning
    void scanPresetsFolder();
    juce::File getPresetsFolder() const;

    //==============================================================================
    // Parameter definitions
    enum ParameterIndex
    {
        // Common giant parameters
        ScaleMeters = 0,
        MassBias,
        AirLoss,
        TransientSlowing,
        Force,
        Speed,
        ContactArea,
        Roughness,
        MasterVolume,

        // Instrument selector
        InstrumentType,

        // MPE enable
        MPEEnabled,

        TotalNumParameters
    };

    // Parameter ranges
    struct ParameterInfo
    {
        const char* name;
        float minValue;
        float maxValue;
        float defaultValue;
        const char* label;
    };

    static const ParameterInfo parameterInfos[TotalNumParameters];

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AetherGiantProcessor)
};

//==============================================================================
/**
 * Plug-in description for JUCE wrapper
 */
struct AetherGiantPluginInfo
{
    static const char* getName()        { return "Aether Giant"; }
    static const char* getDescription() { return "Giant-scale physical modeling instruments"; }
    static const char* getManufacturer() { return "Nature Marco"; }
    static const char* getVersion()     { return "1.0.0"; }
    static int getVersionHex()          { return 0x00010000; }

    static const char* getCategory()    { return "Instrument"; }
};
