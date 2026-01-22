/*
  ==============================================================================

    NatureFFI.h
    Created: 26 Dec 2025
    Author:  Bret Bouchard

    C bridge interface for NatureDSP - FFI layer for Swift/tvOS

    This file provides a C API wrapper around the C++ NatureDSP class,
    enabling integration with Swift on tvOS and other platforms.

    Key Features:
    - Opaque handle pattern for C++ object hiding
    - Macro controls (8 macros)
    - Modulation matrix (16 slots)
    - JSON preset system
    - Factory preset support

  ==============================================================================
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//==============================================================================
// Opaque Types (C-compatible handles)
//==============================================================================

/**
 * @brief Opaque handle to NatureDSP instance
 *
 * This type is used to pass C++ object pointers through C API boundary.
 * Actual definition is in the .cpp file.
 */
typedef struct NatureDSPInstance NatureDSPInstance;

//==============================================================================
// Modulation Matrix Types
//==============================================================================

/**
 * @brief Modulation curve types
 */
typedef enum NatureModulationCurve
{
    NATURE_MOD_CURVE_LINEAR = 0,              ///< Linear response
    NATURE_MOD_CURVE_POSITIVE_EXP = 1,        ///< Positive exponential
    NATURE_MOD_CURVE_NEGATIVE_EXP = 2,        ///< Negative exponential
    NATURE_MOD_CURVE_SINE = 3                 ///< Sine curve
} NatureModulationCurve;

/**
 * @brief Modulation source types
 */
typedef enum NatureModulationSource
{
    NATURE_MOD_SOURCE_LFO1 = 0,               ///< LFO 1
    NATURE_MOD_SOURCE_LFO2 = 1,               ///< LFO 2
    NATURE_MOD_SOURCE_LFO3 = 2,               ///< LFO 3
    NATURE_MOD_SOURCE_LFO4 = 3,               ///< LFO 4
    NATURE_MOD_SOURCE_ENV1 = 4,               ///< Envelope 1
    NATURE_MOD_SOURCE_ENV2 = 5,               ///< Envelope 2
    NATURE_MOD_SOURCE_MACRO1 = 6,             ///< Macro control 1
    NATURE_MOD_SOURCE_MACRO2 = 7,             ///< Macro control 2
    NATURE_MOD_SOURCE_MACRO3 = 8,             ///< Macro control 3
    NATURE_MOD_SOURCE_MACRO4 = 9,             ///< Macro control 4
    NATURE_MOD_SOURCE_MACRO5 = 10,            ///< Macro control 5
    NATURE_MOD_SOURCE_MACRO6 = 11,            ///< Macro control 6
    NATURE_MOD_SOURCE_MACRO7 = 12,            ///< Macro control 7
    NATURE_MOD_SOURCE_MACRO8 = 13,            ///< Macro control 8
    NATURE_MOD_SOURCE_VELOCITY = 14,          ///< Note velocity
    NATURE_MOD_SOURCE_AFTERTOUCH = 15,        ///< Channel aftertouch
    NATURE_MOD_SOURCE_MODWHEEL = 16,          ///< Modulation wheel
    NATURE_MOD_SOURCE_PITCHBEND = 17          ///< Pitch bend
} NatureModulationSource;

//==============================================================================
// Lifecycle Functions
//==============================================================================

/**
 * @brief Create a new NatureDSP instance
 * @return Handle to the new instance, or NULL on failure
 */
NatureDSPInstance* nature_create(void);

/**
 * @brief Destroy a NatureDSP instance
 * @param instance Handle to the instance to destroy
 */
void nature_destroy(NatureDSPInstance* instance);

/**
 * @brief Initialize the synth for playback
 * @param instance Handle to the synth instance
 * @param sampleRate Sample rate in Hz
 * @param samplesPerBlock Maximum samples per block
 * @return true on success, false on failure
 */
bool nature_initialize(NatureDSPInstance* instance, double sampleRate, int samplesPerBlock);

//==============================================================================
// Audio Processing Functions
//==============================================================================

/**
 * @brief Process a block of audio
 * @param instance Handle to the synth instance
 * @param output Output audio buffer (interleaved stereo)
 * @param numSamples Number of samples to process
 * @param midiData MIDI message data
 * @param midiSize Size of MIDI data in bytes
 */
void nature_process(NatureDSPInstance* instance,
                        float* output,
                        int numSamples,
                        const uint8_t* midiData,
                        int midiSize);

/**
 * @brief Process a block of audio with MIDI buffer
 * @param instance Handle to the synth instance
 * @param output Output audio buffer (interleaved stereo)
 * @param numSamples Number of samples to process
 * @param midiMessages Array of MIDI messages
 * @param midiSizes Array of MIDI message sizes
 * @param numMessages Number of MIDI messages
 */
void nature_process_midi_buffer(NatureDSPInstance* instance,
                                    float* output,
                                    int numSamples,
                                    const uint8_t* midiMessages,
                                    const int* midiSizes,
                                    int numMessages);

//==============================================================================
// Parameter Control Functions
//==============================================================================

/**
 * @brief Get the number of parameters
 * @param instance Handle to the synth instance
 * @return Number of parameters
 */
int nature_get_parameter_count(NatureDSPInstance* instance);

/**
 * @brief Get parameter ID by index
 * @param instance Handle to the synth instance
 * @param index Parameter index (0-based)
 * @param idBuffer Buffer to receive parameter ID
 * @param idBufferSize Size of ID buffer
 * @return true on success, false on failure
 */
bool nature_get_parameter_id(NatureDSPInstance* instance,
                                int index,
                                char* idBuffer,
                                int idBufferSize);

/**
 * @brief Get parameter value
 * @param instance Handle to the synth instance
 * @param parameterId Parameter ID (null-terminated string)
 * @return Current parameter value (0.0 to 1.0)
 */
float nature_get_parameter_value(NatureDSPInstance* instance, const char* parameterId);

/**
 * @brief Set parameter value
 * @param instance Handle to the synth instance
 * @param parameterId Parameter ID (null-terminated string)
 * @param value New parameter value (0.0 to 1.0)
 * @return true on success, false on failure
 */
bool nature_set_parameter_value(NatureDSPInstance* instance,
                                  const char* parameterId,
                                  float value);

/**
 * @brief Get parameter name
 * @param instance Handle to the synth instance
 * @param parameterId Parameter ID (null-terminated string)
 * @param nameBuffer Buffer to receive parameter name
 * @param nameBufferSize Size of name buffer
 * @return true on success, false on failure
 */
bool nature_get_parameter_name(NatureDSPInstance* instance,
                                  const char* parameterId,
                                  char* nameBuffer,
                                  int nameBufferSize);

//==============================================================================
// Macro Control Functions (Nature Marco Specific)
//==============================================================================

/**
 * @brief Set macro control value
 * @param instance Handle to the synth instance
 * @param macroIndex Macro index (0-7)
 * @param value Macro value (0.0 to 1.0)
 * @return true on success, false on failure
 */
bool nature_set_macro(NatureDSPInstance* instance, int macroIndex, float value);

/**
 * @brief Get macro control value
 * @param instance Handle to the synth instance
 * @param macroIndex Macro index (0-7)
 * @return Current macro value (0.0 to 1.0)
 */
float nature_get_macro(NatureDSPInstance* instance, int macroIndex);

/**
 * @brief Get number of macros
 * @param instance Handle to the synth instance
 * @return Number of macro controls (always 8)
 */
int nature_get_macro_count(NatureDSPInstance* instance);

//==============================================================================
// Modulation Matrix Functions (Nature Marco Specific)
//==============================================================================

/**
 * @brief Set modulation routing
 * @param instance Handle to the synth instance
 * @param slot Modulation slot (0-15)
 * @param source Source type (see NatureModulationSource)
 * @param destination Parameter ID (null-terminated string)
 * @param amount Modulation amount (-1.0 to 1.0)
 * @param curve Modulation curve type
 * @return true on success, false on failure
 */
bool nature_set_modulation(NatureDSPInstance* instance,
                               int slot,
                               NatureModulationSource source,
                               const char* destination,
                               float amount,
                               NatureModulationCurve curve);

/**
 * @brief Clear modulation slot
 * @param instance Handle to the synth instance
 * @param slot Modulation slot (0-15)
 * @return true on success, false on failure
 */
bool nature_clear_modulation(NatureDSPInstance* instance, int slot);

/**
 * @brief Get modulation slot info
 * @param instance Handle to the synth instance
 * @param slot Modulation slot (0-15)
 * @param source Output: source type
 * @param destination Buffer to receive destination parameter ID
 * @param destSize Size of destination buffer
 * @param amount Output: modulation amount
 * @param curve Output: curve type
 * @return true if slot is active, false otherwise
 */
bool nature_get_modulation(NatureDSPInstance* instance,
                               int slot,
                               NatureModulationSource* source,
                               char* destination,
                               int destSize,
                               float* amount,
                               NatureModulationCurve* curve);

/**
 * @brief Clear all modulation slots
 * @param instance Handle to the synth instance
 */
void nature_clear_all_modulation(NatureDSPInstance* instance);

/**
 * @brief Get number of modulation slots
 * @param instance Handle to the synth instance
 * @return Number of modulation slots (always 16)
 */
int nature_get_modulation_slot_count(NatureDSPInstance* instance);

//==============================================================================
// Preset Functions
//==============================================================================

/**
 * @brief Save current state to JSON
 * @param instance Handle to the synth instance
 * @param jsonBuffer Buffer to receive JSON string
 * @param jsonBufferSize Size of JSON buffer
 * @return Number of bytes written, or -1 on failure
 */
int nature_save_preset(NatureDSPInstance* instance,
                          char* jsonBuffer,
                          int jsonBufferSize);

/**
 * @brief Load state from JSON
 * @param instance Handle to the synth instance
 * @param jsonData JSON preset data (null-terminated string)
 * @return true on success, false on failure
 */
bool nature_load_preset(NatureDSPInstance* instance, const char* jsonData);

/**
 * @brief Validate preset JSON
 * @param instance Handle to the synth instance
 * @param jsonData JSON preset data (null-terminated string)
 * @return true if valid, false otherwise
 */
bool nature_validate_preset(NatureDSPInstance* instance, const char* jsonData);

/**
 * @brief Get preset metadata
 * @param instance Handle to the synth instance
 * @param jsonData JSON preset data (null-terminated string)
 * @param nameBuffer Buffer to receive preset name
 * @param nameBufferSize Size of name buffer
 * @param authorBuffer Buffer to receive author name
 * @param authorBufferSize Size of author buffer
 * @param categoryBuffer Buffer to receive preset category
 * @param categoryBufferSize Size of category buffer
 * @param descriptionBuffer Buffer to receive preset description
 * @param descriptionBufferSize Size of description buffer
 * @return true on success, false on failure
 */
bool nature_get_preset_info(NatureDSPInstance* instance,
                               const char* jsonData,
                               char* nameBuffer,
                               int nameBufferSize,
                               char* authorBuffer,
                               int authorBufferSize,
                               char* categoryBuffer,
                               int categoryBufferSize,
                               char* descriptionBuffer,
                               int descriptionBufferSize);

//==============================================================================
// Factory Presets Functions
//==============================================================================

/**
 * @brief Get the number of factory presets
 * @param instance Handle to the synth instance
 * @return Number of factory presets
 */
int nature_get_factory_preset_count(NatureDSPInstance* instance);

/**
 * @brief Get factory preset name by index
 * @param instance Handle to the synth instance
 * @param index Preset index (0-based)
 * @param nameBuffer Buffer to receive preset name
 * @param nameBufferSize Size of name buffer
 * @return true on success, false on failure
 */
bool nature_get_factory_preset_name(NatureDSPInstance* instance,
                                       int index,
                                       char* nameBuffer,
                                       int nameBufferSize);

/**
 * @brief Load factory preset by index
 * @param instance Handle to the synth instance
 * @param index Preset index (0-based)
 * @return true on success, false on failure
 */
bool nature_load_factory_preset(NatureDSPInstance* instance, int index);

//==============================================================================
// Utility Functions
//==============================================================================

/**
 * @brief Get synth version string
 * @return Version string (e.g., "1.0.0")
 */
const char* nature_get_version(void);

/**
 * @brief Get last error message
 * @param instance Handle to the synth instance
 * @return Error message string, or NULL if no error
 */
const char* nature_get_last_error(NatureDSPInstance* instance);

/**
 * @brief Clear last error message
 * @param instance Handle to the synth instance
 */
void nature_clear_last_error(NatureDSPInstance* instance);

/**
 * @brief Reset synth to default state
 * @param instance Handle to the synth instance
 */
void nature_reset(NatureDSPInstance* instance);

/**
 * @brief Get current active voice count
 * @param instance Handle to the synth instance
 * @return Number of active voices
 */
int nature_get_active_voice_count(NatureDSPInstance* instance);

/**
 * @brief Get synth latency in samples
 * @param instance Handle to the synth instance
 * @return Latency in samples
 */
int nature_get_latency(NatureDSPInstance* instance);

#ifdef __cplusplus
}
#endif
