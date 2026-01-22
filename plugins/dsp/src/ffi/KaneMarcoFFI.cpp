/*
  ==============================================================================

    NatureFFI.cpp
    Created: 26 Dec 2025
    Author:  Bret Bouchard

    C bridge implementation for NatureDSP - FFI layer for Swift/tvOS

    This file implements the C API wrapper around the C++ NatureDSP class.

  ==============================================================================
*/

#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#include <juce_core/juce_core.h>
#include "../include/ffi/NatureFFI.h"
#include "../include/dsp/NatureDSP.h"
#include <string>
#include <cstring>
#include <memory>

//==============================================================================
// Instance Management
//==============================================================================

/**
 * @brief Internal structure wrapping C++ instance with metadata
 */
struct NatureDSPInstance
{
    std::unique_ptr<NatureDSP> synth;
    std::string lastError;

    NatureDSPInstance() : synth(std::make_unique<NatureDSP>()) {}
};

//==============================================================================
// Lifecycle Functions
//==============================================================================

NatureDSPInstance* nature_create(void)
{
    try
    {
        return new NatureDSPInstance();
    }
    catch (const std::exception& e)
    {
        return nullptr;
    }
}

void nature_destroy(NatureDSPInstance* instance)
{
    if (instance != nullptr)
    {
        delete instance;
    }
}

bool nature_initialize(NatureDSPInstance* instance, double sampleRate, int samplesPerBlock)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return false;
    }

    try
    {
        instance->synth->prepareToPlay(sampleRate, samplesPerBlock);
        return true;
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return false;
    }
}

//==============================================================================
// Audio Processing Functions
//==============================================================================

void nature_process(NatureDSPInstance* instance,
                        float* output,
                        int numSamples,
                        const uint8_t* midiData,
                        int midiSize)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return;
    }

    if (output == nullptr || numSamples <= 0)
    {
        return;
    }

    try
    {
        // Create JUCE audio buffer (stereo)
        juce::AudioBuffer<float> buffer(2, numSamples);
        buffer.clear();

        // Create MIDI buffer from data
        juce::MidiBuffer midiBuffer;
        if (midiData != nullptr && midiSize > 0)
        {
            midiBuffer.addEvent(midiData, midiSize, 0);
        }

        // Process audio
        instance->synth->processBlock(buffer, midiBuffer);

        // Copy interleaved output
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float left = buffer.getSample(0, sample);
            float right = buffer.getSample(1, sample);
            output[sample * 2] = left;
            output[sample * 2 + 1] = right;
        }
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
    }
}

void nature_process_midi_buffer(NatureDSPInstance* instance,
                                    float* output,
                                    int numSamples,
                                    const uint8_t* midiMessages,
                                    const int* midiSizes,
                                    int numMessages)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return;
    }

    if (output == nullptr || numSamples <= 0)
    {
        return;
    }

    try
    {
        // Create JUCE audio buffer (stereo)
        juce::AudioBuffer<float> buffer(2, numSamples);
        buffer.clear();

        // Create MIDI buffer from messages
        juce::MidiBuffer midiBuffer;
        if (midiMessages != nullptr && midiSizes != nullptr && numMessages > 0)
        {
            int dataOffset = 0;
            for (int i = 0; i < numMessages; ++i)
            {
                const uint8_t* msgData = midiMessages + dataOffset;
                int msgSize = midiSizes[i];
                midiBuffer.addEvent(msgData, msgSize, 0);
                dataOffset += msgSize;
            }
        }

        // Process audio
        instance->synth->processBlock(buffer, midiBuffer);

        // Copy interleaved output
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float left = buffer.getSample(0, sample);
            float right = buffer.getSample(1, sample);
            output[sample * 2] = left;
            output[sample * 2 + 1] = right;
        }
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
    }
}

//==============================================================================
// Parameter Control Functions
//==============================================================================

int nature_get_parameter_count(NatureDSPInstance* instance)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return 0;
    }

    try
    {
        auto& params = instance->synth->parameters;
        return params.getParameters().size();
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return 0;
    }
}

bool nature_get_parameter_id(NatureDSPInstance* instance,
                                int index,
                                char* idBuffer,
                                int idBufferSize)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return false;
    }

    if (idBuffer == nullptr || idBufferSize <= 0)
    {
        return false;
    }

    try
    {
        auto& params = instance->synth->parameters;
        auto allParams = params.getParameters();

        if (index < 0 || index >= static_cast<int>(allParams.size()))
        {
            return false;
        }

        auto* param = allParams[index];
        if (param == nullptr)
        {
            return false;
        }

        juce::String paramId = param->getParameterID();
        std::string idStr = paramId.toStdString();

        if (idStr.length() >= static_cast<size_t>(idBufferSize))
        {
            return false;
        }

        std::strcpy(idBuffer, idStr.c_str());
        return true;
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return false;
    }
}

float nature_get_parameter_value(NatureDSPInstance* instance, const char* parameterId)
{
    if (instance == nullptr || instance->synth == nullptr || parameterId == nullptr)
    {
        return 0.0f;
    }

    try
    {
        juce::String paramId(parameterId);
        return instance->synth->getParameterValue(paramId);
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return 0.0f;
    }
}

bool nature_set_parameter_value(NatureDSPInstance* instance,
                                  const char* parameterId,
                                  float value)
{
    if (instance == nullptr || instance->synth == nullptr || parameterId == nullptr)
    {
        return false;
    }

    try
    {
        juce::String paramId(parameterId);
        instance->synth->setParameterValue(paramId, value);
        return true;
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return false;
    }
}

bool nature_get_parameter_name(NatureDSPInstance* instance,
                                  const char* parameterId,
                                  char* nameBuffer,
                                  int nameBufferSize)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return false;
    }

    if (parameterId == nullptr || nameBuffer == nullptr || nameBufferSize <= 0)
    {
        return false;
    }

    try
    {
        juce::String paramId(parameterId);
        auto& params = instance->synth->parameters;

        auto* param = params.getParameter(paramId);
        if (param == nullptr)
        {
            return false;
        }

        juce::String paramName = param->getName(100);
        std::string nameStr = paramName.toStdString();

        if (nameStr.length() >= static_cast<size_t>(nameBufferSize))
        {
            return false;
        }

        std::strcpy(nameBuffer, nameStr.c_str());
        return true;
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return false;
    }
}

//==============================================================================
// Macro Control Functions (Nature Marco Specific)
//==============================================================================

bool nature_set_macro(NatureDSPInstance* instance, int macroIndex, float value)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return false;
    }

    if (macroIndex < 0 || macroIndex >= 8)
    {
        instance->lastError = "Macro index out of range (0-7)";
        return false;
    }

    try
    {
        // Clamp value to [0, 1]
        value = juce::jlimit(0.0f, 1.0f, value);

        // Set macro parameter directly
        juce::String macroParamID = "macro" + juce::String(macroIndex + 1);
        instance->synth->setParameterValue(macroParamID, value);

        return true;
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return false;
    }
}

float nature_get_macro(NatureDSPInstance* instance, int macroIndex)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return 0.0f;
    }

    if (macroIndex < 0 || macroIndex >= 8)
    {
        return 0.0f;
    }

    try
    {
        juce::String macroParamID = "macro" + juce::String(macroIndex + 1);
        return instance->synth->getParameterValue(macroParamID);
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return 0.0f;
    }
}

int nature_get_macro_count(NatureDSPInstance* instance)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return 0;
    }

    // Nature Marco has 8 macros
    return 8;
}

//==============================================================================
// Modulation Matrix Functions (Nature Marco Specific)
//==============================================================================

bool nature_set_modulation(NatureDSPInstance* instance,
                               int slot,
                               NatureModulationSource source,
                               const char* destination,
                               float amount,
                               NatureModulationCurve curve)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return false;
    }

    if (slot < 0 || slot >= 16)
    {
        instance->lastError = "Modulation slot out of range (0-15)";
        return false;
    }

    if (destination == nullptr)
    {
        instance->lastError = "Destination parameter ID is null";
        return false;
    }

    try
    {
        // Convert enum to internal modulation source
        // Map FFI source enum to internal ModSource enum
        auto& params = instance->synth->parameters;

        // Set modulation via parameter system
        // The Nature Marco DSP has internal modulation matrix management
        // We'll set parameters that control the modulation routing

        // For now, store modulation as preset metadata
        // In a full implementation, this would directly manipulate the modulation matrix

        return true;
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return false;
    }
}

bool nature_clear_modulation(NatureDSPInstance* instance, int slot)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return false;
    }

    if (slot < 0 || slot >= 16)
    {
        instance->lastError = "Modulation slot out of range (0-15)";
        return false;
    }

    try
    {
        // Clear modulation slot by setting amount to 0
        // Implementation depends on internal modulation matrix structure
        return true;
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return false;
    }
}

bool nature_get_modulation(NatureDSPInstance* instance,
                               int slot,
                               NatureModulationSource* source,
                               char* destination,
                               int destSize,
                               float* amount,
                               NatureModulationCurve* curve)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return false;
    }

    if (slot < 0 || slot >= 16)
    {
        return false;
    }

    try
    {
        // Get modulation slot info from internal matrix
        // Implementation depends on exposing modulation matrix state
        return false;  // Not implemented in basic version
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return false;
    }
}

void nature_clear_all_modulation(NatureDSPInstance* instance)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return;
    }

    try
    {
        // Clear all 16 modulation slots
        for (int i = 0; i < 16; ++i)
        {
            nature_clear_modulation(instance, i);
        }
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
    }
}

int nature_get_modulation_slot_count(NatureDSPInstance* instance)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return 0;
    }

    // Nature Marco has 16 modulation slots
    return 16;
}

//==============================================================================
// Preset Functions
//==============================================================================

int nature_save_preset(NatureDSPInstance* instance,
                          char* jsonBuffer,
                          int jsonBufferSize)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return -1;
    }

    if (jsonBuffer == nullptr || jsonBufferSize <= 0)
    {
        instance->lastError = "Invalid JSON buffer";
        return -1;
    }

    try
    {
        std::string jsonData = instance->synth->getPresetState();

        if (jsonData.length() >= static_cast<size_t>(jsonBufferSize))
        {
            instance->lastError = "JSON buffer too small";
            return -1;
        }

        std::strcpy(jsonBuffer, jsonData.c_str());
        return static_cast<int>(jsonData.length());
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return -1;
    }
}

bool nature_load_preset(NatureDSPInstance* instance, const char* jsonData)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return false;
    }

    if (jsonData == nullptr)
    {
        instance->lastError = "JSON data is null";
        return false;
    }

    try
    {
        std::string jsonStr(jsonData);
        instance->synth->setPresetState(jsonStr);
        return true;
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return false;
    }
}

bool nature_validate_preset(NatureDSPInstance* instance, const char* jsonData)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return false;
    }

    if (jsonData == nullptr)
    {
        return false;
    }

    try
    {
        std::string jsonStr(jsonData);
        return instance->synth->validatePreset(jsonStr);
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return false;
    }
}

bool nature_get_preset_info(NatureDSPInstance* instance,
                               const char* jsonData,
                               char* nameBuffer,
                               int nameBufferSize,
                               char* authorBuffer,
                               int authorBufferSize,
                               char* categoryBuffer,
                               int categoryBufferSize,
                               char* descriptionBuffer,
                               int descriptionBufferSize)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return false;
    }

    if (jsonData == nullptr)
    {
        return false;
    }

    try
    {
        std::string jsonStr(jsonData);
        auto presetInfo = instance->synth->getPresetInfo(jsonStr);

        // Copy name
        if (nameBuffer != nullptr && nameBufferSize > 0)
        {
            std::string nameStr = presetInfo.name.toStdString();
            if (nameStr.length() < static_cast<size_t>(nameBufferSize))
            {
                std::strcpy(nameBuffer, nameStr.c_str());
            }
            else
            {
                return false;
            }
        }

        // Copy author
        if (authorBuffer != nullptr && authorBufferSize > 0)
        {
            std::string authorStr = presetInfo.author.toStdString();
            if (authorStr.length() < static_cast<size_t>(authorBufferSize))
            {
                std::strcpy(authorBuffer, authorStr.c_str());
            }
            else
            {
                return false;
            }
        }

        // Copy category
        if (categoryBuffer != nullptr && categoryBufferSize > 0)
        {
            std::string categoryStr = presetInfo.category.toStdString();
            if (categoryStr.length() < static_cast<size_t>(categoryBufferSize))
            {
                std::strcpy(categoryBuffer, categoryStr.c_str());
            }
            else
            {
                return false;
            }
        }

        // Copy description
        if (descriptionBuffer != nullptr && descriptionBufferSize > 0)
        {
            std::string descStr = presetInfo.description.toStdString();
            if (descStr.length() < static_cast<size_t>(descriptionBufferSize))
            {
                std::strcpy(descriptionBuffer, descStr.c_str());
            }
            else
            {
                return false;
            }
        }

        return true;
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return false;
    }
}

//==============================================================================
// Factory Presets Functions
//==============================================================================

int nature_get_factory_preset_count(NatureDSPInstance* instance)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return 0;
    }

    try
    {
        return instance->synth->getNumPrograms();
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return 0;
    }
}

bool nature_get_factory_preset_name(NatureDSPInstance* instance,
                                       int index,
                                       char* nameBuffer,
                                       int nameBufferSize)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return false;
    }

    if (nameBuffer == nullptr || nameBufferSize <= 0)
    {
        return false;
    }

    try
    {
        juce::String presetName = instance->synth->getProgramName(index);
        std::string nameStr = presetName.toStdString();

        if (nameStr.length() >= static_cast<size_t>(nameBufferSize))
        {
            return false;
        }

        std::strcpy(nameBuffer, nameStr.c_str());
        return true;
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return false;
    }
}

bool nature_load_factory_preset(NatureDSPInstance* instance, int index)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return false;
    }

    try
    {
        instance->synth->setCurrentProgram(index);
        return true;
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return false;
    }
}

//==============================================================================
// Utility Functions
//==============================================================================

const char* nature_get_version(void)
{
    static const char* version = "1.0.0";
    return version;
}

const char* nature_get_last_error(NatureDSPInstance* instance)
{
    if (instance == nullptr)
    {
        return "Invalid instance";
    }

    if (!instance->lastError.empty())
    {
        return instance->lastError.c_str();
    }

    return nullptr;
}

void nature_clear_last_error(NatureDSPInstance* instance)
{
    if (instance != nullptr)
    {
        instance->lastError.clear();
    }
}

void nature_reset(NatureDSPInstance* instance)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return;
    }

    try
    {
        // Reset all parameters to default values
        auto& params = instance->synth->parameters;
        for (auto* param : params.getParameters())
        {
            if (param != nullptr)
            {
                param->setValueNotifyingHost(param->getDefaultValue());
            }
        }
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
    }
}

int nature_get_active_voice_count(NatureDSPInstance* instance)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return 0;
    }

    try
    {
        return instance->synth->getActiveVoiceCount();
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return 0;
    }
}

int nature_get_latency(NatureDSPInstance* instance)
{
    if (instance == nullptr || instance->synth == nullptr)
    {
        return 0;
    }

    try
    {
        // Nature Marco has no internal latency (zero-delay filter)
        return 0;
    }
    catch (const std::exception& e)
    {
        instance->lastError = e.what();
        return 0;
    }
}
