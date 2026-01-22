/*
 * NaturePlugin.cpp
 *
 * JUCE AudioProcessor wrapper implementation for Nature instrument
 *
 * Created: January 19, 2026
 */

#include "NaturePlugin.h"

//==============================================================================
// Constructor/Destructor
//==============================================================================

NaturePlugin::NaturePlugin()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Create pureDSP instance
    dsp_ = std::make_unique<DSP::NatureDSP>();

    // Initialize parameters
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& paramInfo : parameterInfos_) {
        auto parameter = std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(paramInfo.paramId, 1),
            paramInfo.paramName,
            juce::NormalisableRange<float>(paramInfo.minValue, paramInfo.maxValue),
            paramInfo.defaultValue,
            paramInfo.label,
            juce::AudioProcessorParameter::Category::genericParameter
        );
        layout.add(std::move(parameter));
    }

    parameters_ = std::make_unique<juce::AudioProcessorValueTreeState>(
        *this, nullptr, "NatureParameters", std::move(layout)
    );

    // Register listener for parameter changes
    for (const auto& paramInfo : parameterInfos_) {
        parameters_->addParameterListener(paramInfo.paramId, this);
    }
}

NaturePlugin::~NaturePlugin()
{
    // Remove parameter listeners
    for (const auto& paramInfo : parameterInfos_) {
        parameters_->removeParameterListener(paramInfo.paramId, this);
    }
}

//==============================================================================
// AudioProcessor Interface
//==============================================================================

void NaturePlugin::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Prepare pureDSP
    if (!dsp_->prepare(sampleRate, samplesPerBlock)) {
        jassertfalse; // Failed to prepare DSP
    }
}

void NaturePlugin::releaseResources()
{
    // Reset pureDSP
    dsp_->reset();
}

void NaturePlugin::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    jassert(!isUsingDoublePrecision());

    // Get output buffer pointers
    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    // Ensure stereo output
    jassert(numChannels >= 2);

    float* outputs[2] = {
        buffer.getWritePointer(0),
        buffer.getWritePointer(1)
    };

    // Clear output buffers (pureDSP adds to output)
    buffer.clear();

    // Process MIDI messages
    processMIDI(midiMessages, numSamples);

    // Process audio through pureDSP
    dsp_->process(outputs, numChannels, numSamples);
}

void NaturePlugin::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // In bypass mode, just clear the output
    buffer.clear();
}

juce::AudioProcessorEditor* NaturePlugin::createEditor()
{
    // Create generic editor (can be replaced with custom UI later)
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
// Plugin State Management
//==============================================================================

void NaturePlugin::getStateInformation(juce::MemoryBlock& destData)
{
    // Get state from pureDSP (JSON format)
    constexpr int jsonBufferSize = 4096;
    char jsonBuffer[jsonBufferSize];

    if (dsp_->savePreset(jsonBuffer, jsonBufferSize)) {
        // Convert to XML
        juce::String jsonString(jsonBuffer);
        auto xml = std::make_unique<juce::XmlElement>("NaturePreset");
        xml->setAttribute("version", "1.0");
        xml->setAttribute("jsonData", jsonString);

        // Copy to MemoryBlock
        copyXmlToBinary(*xml, destData);
    }
}

void NaturePlugin::setStateInformation(const void* data, int sizeInBytes)
{
    // Convert from XML
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml != nullptr) {
        juce::String jsonString = xml->getStringAttribute("jsonData");

        // Load state into pureDSP
        if (!dsp_->loadPreset(jsonString.toStdString().c_str())) {
            jassertfalse; // Failed to load preset
        }

        // Update JUCE parameters
        for (const auto& paramInfo : parameterInfos_) {
            float value = dsp_->getParameter(paramInfo.paramId);
            auto* param = parameters_->getParameter(paramInfo.paramId);
            if (param) {
                param->setValueNotifyingHost(value);
            }
        }
    }
}

//==============================================================================
// Channel Information
//==============================================================================

const juce::String NaturePlugin::getInputChannelName(int channelIndex) const
{
    return "Input " + juce::String(channelIndex + 1);
}

const juce::String NaturePlugin::getOutputChannelName(int channelIndex) const
{
    return "Output " + juce::String(channelIndex + 1);
}

bool NaturePlugin::isInputChannelStereoPair(int index) const
{
    return true;
}

bool NaturePlugin::isOutputChannelStereoPair(int index) const
{
    return true;
}

//==============================================================================
// Parameter Changes
//==============================================================================

void NaturePlugin::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Update pureDSP parameter
    dsp_->setParameter(parameterID.toStdString().c_str(), newValue);
}

//==============================================================================
// MIDI Handling
//==============================================================================

void NaturePlugin::processMIDI(const juce::MidiBuffer& midiMessages, int numSamples)
{
    for (const auto& metadata : midiMessages) {
        const auto& midiMessage = metadata.getMessage();
        int sampleOffset = metadata.samplePosition;

        DSP::ScheduledEvent event;
        convertMIDIToEvent(midiMessage, event, sampleOffset);

        if (event.type != DSP::ScheduledEvent::Type::RESET) {
            dsp_->handleEvent(event);
        }
    }
}

void NaturePlugin::convertMIDIToEvent(const juce::MidiMessage& midiMessage,
                                      DSP::ScheduledEvent& event,
                                      int sampleOffset)
{
    event.time = 0.0;
    event.sampleOffset = static_cast<uint32_t>(sampleOffset);

    if (midiMessage.isNoteOn()) {
        event.type = DSP::ScheduledEvent::NOTE_ON;
        event.data.note.midiNote = midiMessage.getNoteNumber();
        event.data.note.velocity = midiMessage.getVelocity() / 127.0f;
    }
    else if (midiMessage.isNoteOff()) {
        event.type = DSP::ScheduledEvent::NOTE_OFF;
        event.data.note.midiNote = midiMessage.getNoteNumber();
        event.data.note.velocity = 0.0f;
    }
    else if (midiMessage.isPitchWheel()) {
        event.type = DSP::ScheduledEvent::PITCH_BEND;
        int pitchBendValue = midiMessage.getPitchWheelValue();
        event.data.pitchBend.bendValue = (pitchBendValue - 8192) / 8192.0f;
    }
    else if (midiMessage.isChannelPressure()) {
        event.type = DSP::ScheduledEvent::CHANNEL_PRESSURE;
        event.data.channelPressure.pressure = midiMessage.getChannelPressureValue() / 127.0f;
    }
    else if (midiMessage.isController()) {
        event.type = DSP::ScheduledEvent::CONTROL_CHANGE;
        event.data.controlChange.controllerNumber = midiMessage.getControllerNumber();
        event.data.controlChange.value = midiMessage.getControllerValue() / 127.0f;
    }
    else if (midiMessage.isProgramChange()) {
        event.type = DSP::ScheduledEvent::PROGRAM_CHANGE;
        event.data.programChange.programNumber = midiMessage.getProgramChangeNumber();
    }
    else if (midiMessage.isAllNotesOff() || midiMessage.isAllSoundOff()) {
        event.type = DSP::ScheduledEvent::RESET;
    }
    else {
        // Unknown MIDI message, convert to RESET
        event.type = DSP::ScheduledEvent::RESET;
    }
}

//==============================================================================
// This creates new instances of the plugin
//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NaturePlugin();
}
