/*
  ==============================================================================

   AetherGiantProcessor.cpp
   Implementation of VST3/AU Plugin Processor

  ==============================================================================
*/

#include "plugin/AetherGiantProcessor.h"
#include "dsp/AetherGiantDrumsDSP.h"
#include "dsp/AetherGiantVoiceDSP.h"
#include "dsp/AetherGiantHornsDSP.h"
#include "dsp/AetherGiantPercussionDSP.h"
#include "dsp/StringDSP.h"
#include <algorithm>
#include <cstring>

//==============================================================================
// Parameter info table
//==============================================================================
const AetherGiantProcessor::ParameterInfo AetherGiantProcessor::parameterInfos[] = {
    // Common giant parameters
    { "Scale (m)",     0.1f,   100.0f,  1.0f,   "m" },
    { "Mass Bias",     0.0f,   1.0f,   0.5f,   "" },
    { "Air Loss",      0.0f,   1.0f,   0.3f,   "" },
    { "Transient Slowing", 0.0f, 1.0f,   0.5f,   "" },
    { "Force",        0.0f,   1.0f,   0.5f,   "" },
    { "Speed",        0.0f,   1.0f,   0.5f,   "" },
    { "Contact Area",  0.0f,   1.0f,   0.5f,   "" },
    { "Roughness",    0.0f,   1.0f,   0.3f,   "" },
    { "Master Volume", 0.0f,   1.0f,   0.8f,   "" },

    // Instrument selector
    { "Instrument",   0.0f,   4.0f,   0.0f,   "" },

    // MPE enable
    { "MPE Enabled",  0.0f,   1.0f,   0.0f,   "" }
};

//==============================================================================
// AetherGiantProcessor
//==============================================================================

AetherGiantProcessor::AetherGiantProcessor()
    : juce::AudioProcessor(BusesProperties()
                           .withInput("Input", juce::AudioChannelSet::stereo(), false)
                           .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Create initial instrument
    currentInstrument = createInstrument(instrumentType);

    // Scan presets folder
    scanPresetsFolder();
}

AetherGiantProcessor::~AetherGiantProcessor()
{
}

//==============================================================================
void AetherGiantProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ScopedLock lock(dspLock);

    if (currentInstrument)
    {
        currentInstrument->prepare(sampleRate, samplesPerBlock);
    }
}

void AetherGiantProcessor::releaseResources()
{
    juce::ScopedLock lock(dspLock);

    if (currentInstrument)
    {
        currentInstrument->reset();
    }
}

void AetherGiantProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ScopedLock lock(dspLock);

    // Get playhead info
    if (auto* playHead = getPlayHead())
        playHead->getCurrentPosition(positionInfo);

    // Clear output
    buffer.clear();

    if (!currentInstrument)
        return;

    // Process MIDI to events
    std::vector<DSP::ScheduledEvent> events;
    processMIDI(midiMessages, events);

    // Handle events
    for (const auto& event : events)
    {
        currentInstrument->handleEvent(event);
    }

    // Process audio
    float* outputs[] = { buffer.getWritePointer(0), buffer.getWritePointer(1) };
    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    currentInstrument->process(outputs, numChannels, numSamples);
}

juce::AudioProcessorEditor* AetherGiantProcessor::createEditor()
{
    // Editor will be implemented separately
    return nullptr;
}

//==============================================================================
// Programs (Presets)
int AetherGiantProcessor::getNumPrograms()
{
    return presetNames.size();
}

int AetherGiantProcessor::getCurrentProgram()
{
    return currentProgramIndex;
}

void AetherGiantProcessor::setCurrentProgram(int index)
{
    if (index >= 0 && index < presetNames.size())
    {
        currentProgramIndex = index;
        loadPresetFromFile(getPresetsFolder().getChildFile(presetNames[index]));
    }
}

const juce::String AetherGiantProcessor::getProgramName(int index)
{
    if (index >= 0 && index < presetNames.size())
        return presetNames[index];
    return {};
}

void AetherGiantProcessor::changeProgramName(int index, const juce::String& newName)
{
    // Program names come from preset files, not editable
    ignoreUnused(index, newName);
}

//==============================================================================
// Parameters
int AetherGiantProcessor::getNumParameters()
{
    return TotalNumParameters;
}

float AetherGiantProcessor::getParameter(int index)
{
    switch (index)
    {
        case ScaleMeters:    return currentInstrument->getParameter("scale_meters");
        case MassBias:        return currentInstrument->getParameter("mass_bias");
        case AirLoss:         return currentInstrument->getParameter("air_loss");
        case TransientSlowing: return currentInstrument->getParameter("transient_slowing");
        case Force:           return currentInstrument->getParameter("force");
        case Speed:           return currentInstrument->getParameter("speed");
        case ContactArea:     return currentInstrument->getParameter("contact_area");
        case Roughness:       return currentInstrument->getParameter("roughness");
        case MasterVolume:    return currentInstrument->getParameter("master_volume");
        case InstrumentType:  return static_cast<float>(instrumentType);
        case MPEEnabled:      return mpeEnabled ? 1.0f : 0.0f;
        default:              return 0.0f;
    }
}

void AetherGiantProcessor::setParameter(int index, float value)
{
    switch (index)
    {
        case ScaleMeters:
            currentInstrument->setParameter("scale_meters", value);
            break;
        case MassBias:
            currentInstrument->setParameter("mass_bias", value);
            break;
        case AirLoss:
            currentInstrument->setParameter("air_loss", value);
            break;
        case TransientSlowing:
            currentInstrument->setParameter("transient_slowing", value);
            break;
        case Force:
            currentInstrument->setParameter("force", value);
            break;
        case Speed:
            currentInstrument->setParameter("speed", value);
            break;
        case ContactArea:
            currentInstrument->setParameter("contact_area", value);
            break;
        case Roughness:
            currentInstrument->setParameter("roughness", value);
            break;
        case MasterVolume:
            currentInstrument->setParameter("master_volume", value);
            break;
        case InstrumentType:
            setInstrumentType(static_cast<GiantInstrumentType>(static_cast<int>(value)));
            break;
        case MPEEnabled:
            setMPEEnabled(value > 0.5f);
            break;
    }
}

const juce::String AetherGiantProcessor::getParameterName(int index)
{
    if (index >= 0 && index < TotalNumParameters)
        return parameterInfos[index].name;
    return {};
}

const juce::String AetherGiantProcessor::getParameterText(int index)
{
    float value = getParameter(index);

    if (index == InstrumentType)
    {
        switch (static_cast<GiantInstrumentType>(static_cast<int>(value)))
        {
            case GiantInstrumentType::GiantStrings:     return "Giant Strings";
            case GiantInstrumentType::GiantDrums:        return "Giant Drums";
            case GiantInstrumentType::GiantVoice:        return "Giant Voice";
            case GiantInstrumentType::GiantHorns:        return "Giant Horns";
            case GiantInstrumentType::GiantPercussion:   return "Giant Percussion";
        }
    }

    return juce::String(value, 2);
}

void AetherGiantProcessor::setInstrumentType(GiantInstrumentType type)
{
    if (type == instrumentType)
        return;

    switchInstrument(type);

    // Notify host of parameter change
    sendParameterChangeToListeners(InstrumentType);
}

//==============================================================================
// State Management
void AetherGiantProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Create state XML
    juce::XmlElement state("AetherGiantState");

    // Save instrument type
    state.setAttribute("instrument", static_cast<int>(instrumentType));

    // Save parameters
    juce::XmlElement* params = state.createNewChildElement("parameters");
    for (int i = 0; i < TotalNumParameters; ++i)
    {
        float value = getParameter(i);
        params->setAttribute(getParameterName(i), value);
    }

    // Save MPE state
    state.setAttribute("mpeEnabled", mpeEnabled);

    // Save current preset name if loaded
    if (currentProgramIndex >= 0 && currentProgramIndex < presetNames.size())
    {
        state.setAttribute("currentPreset", presetNames[currentProgramIndex]);
    }

    // Copy to memory block
    copyXmlToBinary(state, destData);
}

void AetherGiantProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Parse state XML
    std::unique_ptr<juce::XmlElement> state(getXmlFromBinary(data, sizeInBytes));

    if (!state)
        return;

    // Restore instrument type
    int instrumentInt = state->getIntAttribute("instrument", 0);
    setInstrumentType(static_cast<GiantInstrumentType>(instrumentInt));

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

    // Restore MPE state
    mpeEnabled = state->getBoolAttribute("mpeEnabled", false);

    // Load preset if specified
    juce::String presetName = state->getStringAttribute("currentPreset", "");
    if (!presetName.isEmpty())
    {
        int presetIndex = presetNames.indexOf(presetName);
        if (presetIndex >= 0)
            setCurrentProgram(presetIndex);
    }
}

//==============================================================================
// Preset Management
bool AetherGiantProcessor::loadPresetFromFile(const juce::File& presetFile)
{
    if (!presetFile.existsAsFile())
        return false;

    // Read preset file
    juce::String presetContent = presetFile.loadFileAsString();

    // Load preset into current instrument
    bool loaded = currentInstrument->loadPreset(presetContent.toRawUTF8());

    if (loaded)
    {
        // Update program index
        currentProgramIndex = presetNames.indexOf(presetFile.getFileName());
    }

    return loaded;
}

bool AetherGiantProcessor::savePresetToFile(const juce::File& presetFile)
{
    // Allocate buffer
    const int bufferSize = 65536;
    std::vector<char> buffer(bufferSize);

    // Save preset
    bool saved = currentInstrument->savePreset(buffer.data(), bufferSize);

    if (saved)
    {
        // Write to file
        presetFile.replaceWithData(buffer.data(), std::strlen(buffer.data()));
    }

    return saved;
}

void AetherGiantProcessor::refreshPresetList()
{
    scanPresetsFolder();
    updateHostDisplay();
}

//==============================================================================
// MPE Support
void AetherGiantProcessor::setMPEEnabled(bool enabled)
{
    mpeEnabled = enabled;

    // MPE is handled at the event level in processMIDI()
    // This flag just enables/disables MPE zone detection
}

//==============================================================================
// Channel Info
const juce::String AetherGiantProcessor::getInputChannelName(int channelIndex) const
{
    switch (channelIndex)
    {
        case 0: return "Left";
        case 1: return "Right";
        default: return {};
    }
}

const juce::String AetherGiantProcessor::getOutputChannelName(int channelIndex) const
{
    switch (channelIndex)
    {
        case 0: return "Left";
        case 1: return "Right";
        default: return {};
    }
}

bool AetherGiantProcessor::isInputChannelStereoPair(int index) const
{
    return index == 0;
}

bool AetherGiantProcessor::isOutputChannelStereoPair(int index) const
{
    return index == 0;
}

//==============================================================================
// Private Methods
//==============================================================================

std::unique_ptr<DSP::InstrumentDSP> AetherGiantProcessor::createInstrument(GiantInstrumentType type)
{
    switch (type)
    {
        case GiantInstrumentType::GiantStrings:
            return std::make_unique<DSP::StringPureDSP>();

        case GiantInstrumentType::GiantDrums:
            return std::make_unique<DSP::AetherGiantDrumsPureDSP>();

        case GiantInstrumentType::GiantVoice:
            return std::make_unique<DSP::AetherGiantVoicePureDSP>();

        case GiantInstrumentType::GiantHorns:
            return std::make_unique<DSP::AetherGiantHornsPureDSP>();

        case GiantInstrumentType::GiantPercussion:
            return std::make_unique<DSP::AetherGiantPercussionPureDSP>();

        default:
            return std::make_unique<DSP::StringPureDSP>();
    }
}

void AetherGiantProcessor::switchInstrument(GiantInstrumentType newType)
{
    if (newType == instrumentType)
        return;

    double sampleRate = getSampleRate();
    int blockSize = getBlockSize();

    // Create new instrument
    auto newInstrument = createInstrument(newType);

    // Prepare new instrument
    newInstrument->prepare(sampleRate, blockSize);

    // Swap
    {
        juce::ScopedLock lock(dspLock);
        currentInstrument = std::move(newInstrument);
        instrumentType = newType;
    }

    // Rescan presets for new instrument
    scanPresetsFolder();
}

void AetherGiantProcessor::processMIDI(juce::MidiBuffer& midiMessages,
                                       std::vector<DSP::ScheduledEvent>& events)
{
    double sampleRate = getSampleRate();

    for (const auto& metadata : midiMessages)
    {
        const auto* msg = metadata.getMessage();
        double timestamp = msg->getTimeStamp();

        DSP::ScheduledEvent event;

        // Convert MIDI message to scheduled event
        if (msg->isNoteOn())
        {
            event.type = DSP::ScheduledEventType::NoteOn;
            event.noteNumber = msg->getNoteNumber();
            event.velocity = msg->getVelocity() / 127.0f;

            // Add MPE data if enabled
            if (mpeEnabled)
            {
                // MPE data will be extracted from subsequent messages
                // and applied to the voice
            }

            event.timestamp = timestamp;
            events.push_back(event);
        }
        else if (msg->isNoteOff())
        {
            event.type = DSP::ScheduledEventType::NoteOff;
            event.noteNumber = msg->getNoteNumber();
            event.velocity = 0.0f;
            event.timestamp = timestamp;
            events.push_back(event);
        }
        else if (msg->isAllNotesOff())
        {
            event.type = DSP::ScheduledEventType::AllNotesOff;
            event.timestamp = timestamp;
            events.push_back(event);
        }
        else if (msg->isPitchBend())
        {
            // Pitch bend can be used for MPE or regular pitch bend
            if (mpeEnabled)
            {
                // MPE pitch bend is per-note, will be applied to active voices
            }
            else
            {
                // Global pitch bend
                // Apply to all active notes (future implementation)
            }
        }
        else if (msg->isChannelPressure())
        {
            // Channel pressure (mono-aftertouch)
            // Can be used for MPE or regular aftertouch
        }
        else if (msg->isControllerOfType(0x74))  // Timbre (CC 74)
        {
            // MPE timbre expression
        }
    }
}

void AetherGiantProcessor::midiMessageToEvent(const juce::MidiMessage& msg,
                                               DSP::ScheduledEvent& event,
                                               double sampleRate)
{
    // Convert specific MIDI messages to scheduled events
    if (msg->isNoteOn())
    {
        event.type = DSP::ScheduledEventType::NoteOn;
        event.noteNumber = msg->getNoteNumber();
        event.velocity = msg->getVelocity() / 127.0f;
        event.timestamp = msg->getTimeStamp();
    }
    else if (msg->isNoteOff())
    {
        event.type = DSP::ScheduledEventType::NoteOff;
        event.noteNumber = msg->getNoteNumber();
        event.velocity = 0.0f;
        event.timestamp = msg->getTimeStamp();
    }
    else if (msg->isAllNotesOff())
    {
        event.type = DSP::ScheduledEventType::AllNotesOff;
        event.timestamp = msg->getTimeStamp();
    }
}

juce::File AetherGiantProcessor::getPresetsFolder() const
{
    // Default to instrument-specific presets folder
    switch (instrumentType)
    {
        case GiantInstrumentType::GiantStrings:
            return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("AetherGiant/presets/NatureAetherString");

        case GiantInstrumentType::GiantDrums:
            return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("AetherGiant/presets/NatureAetherGiantDrums");

        case GiantInstrumentType::GiantVoice:
            return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("AetherGiant/presets/NatureAetherGiantVoice");

        case GiantInstrumentType::GiantHorns:
            return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("AetherGiant/presets/NatureAetherGiantHorns");

        case GiantInstrumentType::GiantPercussion:
            return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("AetherGiant/presets/NatureAetherGiantPercussion");

        default:
            return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("AetherGiant/presets");
    }
}

void AetherGiantProcessor::scanPresetsFolder()
{
    presetsFolder = getPresetsFolder();
    presetNames.clear();

    if (!presetsFolder.exists())
        return;

    // Find all JSON files in presets folder
    juce::Array<juce::File> presetFiles;
    presetsFolder.findChildFiles(presetFiles, false, "*.json");

    // Sort alphabetically
    presetFiles.sort();

    // Extract file names
    for (const auto& file : presetFiles)
    {
        presetNames.add(file.getFileName());
    }
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AetherGiantProcessor();
}
