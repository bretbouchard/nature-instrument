/*
  ==============================================================================

   NaturePluginProcessor.cpp
   Implementation of VST3/AU Plugin Processor

  ==============================================================================
*/

#include "plugin/NaturePluginProcessor.h"
#include "dsp/AetherPureDSP.h"
#include <algorithm>
#include <cstring>

//==============================================================================
// Parameter info table
//==============================================================================
const NaturePluginProcessor::ParameterInfo NaturePluginProcessor::parameterInfos[] = {
    { "Master Volume",       0.0f,   1.0f,   0.8f,   "" },
    { "Damping",             0.9f,   1.0f,   0.996f, "" },
    { "Brightness",          0.0f,   1.0f,   0.5f,   "" },
    { "Stiffness",           0.0f,   0.5f,   0.0f,   "" },
    { "Dispersion",          0.0f,   1.0f,   0.5f,   "" },
    { "Sympathetic Coupling", 0.0f,  1.0f,   0.1f,   "" },
    { "Material",            0.0f,   3.0f,   1.0f,   "" },
    { "Body Preset",         0.0f,   2.0f,   0.0f,   "" },
};

//==============================================================================
// NaturePluginProcessor
//==============================================================================

NaturePluginProcessor::NaturePluginProcessor()
    : juce::AudioProcessor(BusesProperties()
                           .withInput("Input", juce::AudioChannelSet::stereo(), false)
                           .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // DSP will be prepared in prepareToPlay()
}

NaturePluginProcessor::~NaturePluginProcessor()
{
}

//==============================================================================
void NaturePluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ScopedLock lock(dspLock);
    dsp_.prepare(sampleRate, samplesPerBlock);
}

void NaturePluginProcessor::releaseResources()
{
    juce::ScopedLock lock(dspLock);
    dsp_.reset();
}

void NaturePluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedLock lock(dspLock);

    // Clear output
    buffer.clear();

    // Process MIDI to events
    std::vector<DSP::ScheduledEvent> events;
    processMIDI(midiMessages, events);

    // Handle events
    for (const auto& event : events)
    {
        dsp_.handleEvent(event);
    }

    // Process audio
    float* outputs[] = { buffer.getWritePointer(0), buffer.getWritePointer(1) };
    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    dsp_.process(outputs, numChannels, numSamples);
}

juce::AudioProcessorEditor* NaturePluginProcessor::createEditor()
{
    // Generic editor for pluginval testing
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
// Parameters
int NaturePluginProcessor::getNumParameters()
{
    return TotalNumParameters;
}

float NaturePluginProcessor::getParameter(int index)
{
    switch (index)
    {
        case MasterVolume:          return dsp_.getParameter("masterVolume");
        case Damping:               return dsp_.getParameter("damping");
        case Brightness:            return dsp_.getParameter("brightness");
        case Stiffness:             return dsp_.getParameter("stiffness");
        case Dispersion:            return dsp_.getParameter("dispersion");
        case SympatheticCoupling:   return dsp_.getParameter("sympatheticCoupling");
        case Material:              return dsp_.getParameter("material");
        case BodyPreset:            return static_cast<float>(dsp_.getParameter("bodyPreset"));
        default:                    return 0.0f;
    }
}

void NaturePluginProcessor::setParameter(int index, float value)
{
    switch (index)
    {
        case MasterVolume:
            dsp_.setParameter("masterVolume", value);
            break;
        case Damping:
            dsp_.setParameter("damping", value);
            break;
        case Brightness:
            dsp_.setParameter("brightness", value);
            break;
        case Stiffness:
            dsp_.setParameter("stiffness", value);
            break;
        case Dispersion:
            dsp_.setParameter("dispersion", value);
            break;
        case SympatheticCoupling:
            dsp_.setParameter("sympatheticCoupling", value);
            break;
        case Material:
            dsp_.setParameter("material", value);
            break;
        case BodyPreset:
            dsp_.setParameter("bodyPreset", value);
            break;
    }
}

const juce::String NaturePluginProcessor::getParameterName(int index)
{
    if (index >= 0 && index < TotalNumParameters)
        return parameterInfos[index].name;
    return {};
}

const juce::String NaturePluginProcessor::getParameterText(int index)
{
    float value = getParameter(index);
    return juce::String(value, 3);
}

//==============================================================================
// State Management
void NaturePluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Create state XML
    juce::XmlElement state("NatureState");

    // Save parameters
    juce::XmlElement* params = state.createNewChildElement("parameters");
    for (int i = 0; i < TotalNumParameters; ++i)
    {
        float value = getParameter(i);
        params->setAttribute(getParameterName(i), value);
    }

    // Copy to memory block
    copyXmlToBinary(state, destData);
}

void NaturePluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Parse state XML
    std::unique_ptr<juce::XmlElement> state(getXmlFromBinary(data, sizeInBytes));

    if (!state)
        return;

    // Restore parameters
    if (juce::XmlElement* params = state->getChildByName("parameters"))
    {
        for (int i = 0; i < TotalNumParameters; ++i)
        {
            float value = params->getDoubleAttribute(getParameterName(i),
                                                     getParameter(i));
            setParameter(i, value);
        }
    }
}

//==============================================================================
// Channel Info
const juce::String NaturePluginProcessor::getInputChannelName(int channelIndex) const
{
    switch (channelIndex)
    {
        case 0: return "Left";
        case 1: return "Right";
        default: return {};
    }
}

const juce::String NaturePluginProcessor::getOutputChannelName(int channelIndex) const
{
    switch (channelIndex)
    {
        case 0: return "Left";
        case 1: return "Right";
        default: return {};
    }
}

bool NaturePluginProcessor::isInputChannelStereoPair(int index) const
{
    return index == 0;
}

bool NaturePluginProcessor::isOutputChannelStereoPair(int index) const
{
    return index == 0;
}

//==============================================================================
// Private Methods
//==============================================================================

void NaturePluginProcessor::processMIDI(juce::MidiBuffer& midiMessages,
                                          std::vector<DSP::ScheduledEvent>& events)
{
    for (const auto& metadata : midiMessages)
    {
        const auto& msg = metadata.getMessage();
        double timestamp = msg.getTimeStamp();

        DSP::ScheduledEvent event;
        event.time = timestamp;
        event.sampleOffset = 0;

        // Convert MIDI message to scheduled event
        if (msg.isNoteOn())
        {
            event.type = DSP::ScheduledEvent::NOTE_ON;
            event.data.note.midiNote = msg.getNoteNumber();
            event.data.note.velocity = msg.getVelocity() / 127.0f;
            events.push_back(event);
        }
        else if (msg.isNoteOff())
        {
            event.type = DSP::ScheduledEvent::NOTE_OFF;
            event.data.note.midiNote = msg.getNoteNumber();
            event.data.note.velocity = 0.0f;
            events.push_back(event);
        }
        else if (msg.isAllNotesOff() || msg.isResetAllControllers())
        {
            event.type = DSP::ScheduledEvent::RESET;
            events.push_back(event);
        }
        else if (msg.isPitchWheel())
        {
            event.type = DSP::ScheduledEvent::PITCH_BEND;
            event.data.pitchBend.bendValue = msg.getPitchWheelValue() / 8192.0f - 1.0f;
            events.push_back(event);
        }
        else if (msg.isChannelPressure())
        {
            event.type = DSP::ScheduledEvent::CHANNEL_PRESSURE;
            event.data.channelPressure.pressure = msg.getChannelPressureValue() / 127.0f;
            events.push_back(event);
        }
    }
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NaturePluginProcessor();
}
