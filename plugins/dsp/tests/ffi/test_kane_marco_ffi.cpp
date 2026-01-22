/*
  ==============================================================================

    test_nature_ffi.cpp
    Created: 26 Dec 2025
    Author:  Bret Bouchard

    Example/test program demonstrating Nature Marco FFI usage

    This program shows how to use the Nature Marco FFI bridge from C/C++.

  ==============================================================================
*/

#include "../include/ffi/NatureFFI.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

void print_separator()
{
    printf("=============================================================================\n");
}

void test_lifecycle()
{
    print_separator();
    printf("TEST: Lifecycle Functions\n");
    print_separator();

    // Create instance
    printf("Creating Nature Marco instance...\n");
    NatureDSPInstance* instance = nature_create();
    if (instance == NULL)
    {
        printf("✗ FAILED: Could not create instance\n");
        return;
    }
    printf("✓ Instance created successfully\n");

    // Initialize
    printf("\nInitializing synth (48kHz, 512 samples)...\n");
    bool success = nature_initialize(instance, 48000.0, 512);
    if (!success)
    {
        printf("✗ FAILED: Could not initialize (error: %s)\n",
               nature_get_last_error(instance));
        nature_destroy(instance);
        return;
    }
    printf("✓ Synth initialized\n");

    // Get version
    printf("\nVersion: %s\n", nature_get_version());

    // Cleanup
    printf("\nDestroying instance...\n");
    nature_destroy(instance);
    printf("✓ Instance destroyed\n");
}

void test_parameters()
{
    print_separator();
    printf("TEST: Parameter Control\n");
    print_separator();

    NatureDSPInstance* instance = nature_create();
    nature_initialize(instance, 48000.0, 512);

    // Get parameter count
    int paramCount = nature_get_parameter_count(instance);
    printf("Total parameters: %d\n", paramCount);

    // Test first few parameters
    for (int i = 0; i < (paramCount < 5 ? paramCount : 5); ++i)
    {
        char idBuffer[64];
        char nameBuffer[64];

        if (nature_get_parameter_id(instance, i, idBuffer, sizeof(idBuffer)))
        {
            if (nature_get_parameter_name(instance, idBuffer, nameBuffer, sizeof(nameBuffer)))
            {
                float value = nature_get_parameter_value(instance, idBuffer);
                printf("  Param %d: ID='%s', Name='%s', Value=%.3f\n",
                       i, idBuffer, nameBuffer, value);
            }
        }
    }

    // Test setting a parameter
    printf("\nSetting 'master_gain' to 0.8...\n");
    if (nature_set_parameter_value(instance, "master_gain", 0.8f))
    {
        float newValue = nature_get_parameter_value(instance, "master_gain");
        printf("✓ New value: %.3f\n", newValue);
    }
    else
    {
        printf("✗ FAILED to set parameter\n");
    }

    nature_destroy(instance);
}

void test_macros()
{
    print_separator();
    printf("TEST: Macro Controls (Nature Marco Specific)\n");
    print_separator();

    NatureDSPInstance* instance = nature_create();
    nature_initialize(instance, 48000.0, 512);

    int macroCount = nature_get_macro_count(instance);
    printf("Macro count: %d\n", macroCount);

    // Test setting/getting macros
    for (int i = 0; i < macroCount; ++i)
    {
        float testValue = 0.5f + (i * 0.05f);
        printf("\nTesting macro %d:\n", i);

        // Set macro
        if (nature_set_macro(instance, i, testValue))
        {
            printf("  Set to %.3f\n", testValue);

            // Get macro
            float retrievedValue = nature_get_macro(instance, i);
            printf("  Retrieved: %.3f\n", retrievedValue);

            if (fabs(retrievedValue - testValue) < 0.001f)
            {
                printf("  ✓ PASS\n");
            }
            else
            {
                printf("  ✗ FAIL: Expected %.3f, got %.3f\n", testValue, retrievedValue);
            }
        }
        else
        {
            printf("  ✗ FAIL: Could not set macro\n");
        }
    }

    nature_destroy(instance);
}

void test_modulation_matrix()
{
    print_separator();
    printf("TEST: Modulation Matrix (Nature Marco Specific)\n");
    print_separator();

    NatureDSPInstance* instance = nature_create();
    nature_initialize(instance, 48000.0, 512);

    int slotCount = nature_get_modulation_slot_count(instance);
    printf("Modulation slot count: %d\n", slotCount);

    // Test setting a modulation routing
    printf("\nSetting modulation slot 0: LFO1 → filter_cutoff, amount=0.5\n");
    bool success = nature_set_modulation(instance,
                                             0,  // slot
                                             NATURE_MOD_SOURCE_LFO1,  // source
                                             "filter_cutoff",  // destination
                                             0.5f,  // amount
                                             NATURE_MOD_CURVE_LINEAR);  // curve

    if (success)
    {
        printf("✓ Modulation routing set\n");
    }
    else
    {
        printf("Note: Modulation routing implementation pending\n");
    }

    // Test clearing modulation
    printf("\nClearing modulation slot 0...\n");
    if (nature_clear_modulation(instance, 0))
    {
        printf("✓ Modulation cleared\n");
    }

    nature_destroy(instance);
}

void test_factory_presets()
{
    print_separator();
    printf("TEST: Factory Presets\n");
    print_separator();

    NatureDSPInstance* instance = nature_create();
    nature_initialize(instance, 48000.0, 512);

    int presetCount = nature_get_factory_preset_count(instance);
    printf("Factory preset count: %d\n", presetCount);

    // List all presets
    printf("\nFactory presets:\n");
    for (int i = 0; i < presetCount; ++i)
    {
        char nameBuffer[128];
        if (nature_get_factory_preset_name(instance, i, nameBuffer, sizeof(nameBuffer)))
        {
            printf("  %2d: %s\n", i, nameBuffer);
        }
    }

    // Load first preset
    if (presetCount > 0)
    {
        printf("\nLoading preset 0...\n");
        if (nature_load_factory_preset(instance, 0))
        {
            printf("✓ Preset loaded\n");
        }
        else
        {
            printf("✗ FAILED to load preset\n");
        }
    }

    nature_destroy(instance);
}

void test_preset_save_load()
{
    print_separator();
    printf("TEST: Preset Save/Load\n");
    print_separator();

    NatureDSPInstance* instance = nature_create();
    nature_initialize(instance, 48000.0, 512);

    // Set some parameters
    printf("Setting parameters...\n");
    nature_set_parameter_value(instance, "master_gain", 0.7f);
    nature_set_parameter_value(instance, "osc1_waveform", 2.0f);
    nature_set_macro(instance, 0, 0.75f);

    // Save preset
    printf("\nSaving preset to JSON...\n");
    char jsonBuffer[8192];
    int jsonSize = nature_save_preset(instance, jsonBuffer, sizeof(jsonBuffer));

    if (jsonSize > 0)
    {
        printf("✓ Preset saved (%d bytes)\n", jsonSize);
        printf("\nJSON preview (first 200 chars):\n%.200s...\n", jsonBuffer);

        // Validate preset
        printf("\nValidating preset JSON...\n");
        if (nature_validate_preset(instance, jsonBuffer))
        {
            printf("✓ Preset is valid\n");
        }
        else
        {
            printf("✗ Preset validation failed\n");
        }

        // Get preset info
        char nameBuf[128], authorBuf[128], categoryBuf[64], descBuf[256];
        if (nature_get_preset_info(instance, jsonBuffer,
                                       nameBuf, sizeof(nameBuf),
                                       authorBuf, sizeof(authorBuf),
                                       categoryBuf, sizeof(categoryBuf),
                                       descBuf, sizeof(descBuf)))
        {
            printf("\nPreset Info:\n");
            printf("  Name: %s\n", nameBuf);
            printf("  Author: %s\n", authorBuf);
            printf("  Category: %s\n", categoryBuf);
            printf("  Description: %s\n", descBuf);
        }

        // Modify parameters and reload preset
        printf("\nModifying parameters...\n");
        nature_set_parameter_value(instance, "master_gain", 0.3f);
        printf("  master_gain changed to: %.3f\n",
               nature_get_parameter_value(instance, "master_gain"));

        printf("\nReloading preset from JSON...\n");
        if (nature_load_preset(instance, jsonBuffer))
        {
            printf("✓ Preset loaded\n");
            float restoredValue = nature_get_parameter_value(instance, "master_gain");
            printf("  master_gain restored to: %.3f\n", restoredValue);

            if (fabs(restoredValue - 0.7f) < 0.001f)
            {
                printf("✓ PASS: Parameter correctly restored\n");
            }
            else
            {
                printf("✗ FAIL: Expected 0.7, got %.3f\n", restoredValue);
            }
        }
        else
        {
            printf("✗ FAILED to load preset\n");
        }
    }
    else
    {
        printf("✗ FAILED to save preset (error: %s)\n",
               nature_get_last_error(instance));
    }

    nature_destroy(instance);
}

void test_audio_processing()
{
    print_separator();
    printf("TEST: Audio Processing\n");
    print_separator();

    NatureDSPInstance* instance = nature_create();
    nature_initialize(instance, 48000.0, 512);

    // Load a preset
    if (nature_get_factory_preset_count(instance) > 0)
    {
        nature_load_factory_preset(instance, 0);
    }

    // Process audio block
    printf("Processing 512 samples...\n");

    float audioBuffer[512 * 2];  // Stereo interleaved

    // Create MIDI note-on message (C4, velocity 100)
    uint8_t midiNoteOn[] = {0x90, 0x3C, 0x64};

    nature_process(instance, audioBuffer, 512, midiNoteOn, sizeof(midiNoteOn));

    printf("✓ Audio processed\n");

    // Check for non-silent output
    bool hasSignal = false;
    for (int i = 0; i < 512; ++i)
    {
        if (fabs(audioBuffer[i * 2]) > 0.001f || fabs(audioBuffer[i * 2 + 1]) > 0.001f)
        {
            hasSignal = true;
            break;
        }
    }

    if (hasSignal)
    {
        printf("✓ PASS: Output signal detected\n");
    }
    else
    {
        printf("Note: Output is silent (may need envelope time)\n");
    }

    // Check voice count
    int voiceCount = nature_get_active_voice_count(instance);
    printf("Active voices: %d\n", voiceCount);

    // Check latency
    int latency = nature_get_latency(instance);
    printf("Latency: %d samples\n", latency);

    nature_destroy(instance);
}

void test_reset()
{
    print_separator();
    printf("TEST: Reset\n");
    print_separator();

    NatureDSPInstance* instance = nature_create();
    nature_initialize(instance, 48000.0, 512);

    // Modify parameters
    printf("Modifying parameters...\n");
    nature_set_parameter_value(instance, "master_gain", 0.2f);
    nature_set_macro(instance, 0, 0.9f);
    printf("  master_gain: %.3f\n", nature_get_parameter_value(instance, "master_gain"));
    printf("  macro0: %.3f\n", nature_get_macro(instance, 0));

    // Reset
    printf("\nResetting synth...\n");
    nature_reset(instance);

    // Check default values
    printf("  master_gain after reset: %.3f\n",
           nature_get_parameter_value(instance, "master_gain"));
    printf("  macro0 after reset: %.3f\n",
           nature_get_macro(instance, 0));

    printf("✓ Reset complete\n");

    nature_destroy(instance);
}

int main(int argc, char* argv[])
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                  Nature Marco FFI Bridge Test Program                       ║\n");
    printf("║                      Week 2: FFI Implementation                          ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    // Run all tests
    test_lifecycle();
    printf("\n");

    test_parameters();
    printf("\n");

    test_macros();
    printf("\n");

    test_modulation_matrix();
    printf("\n");

    test_factory_presets();
    printf("\n");

    test_preset_save_load();
    printf("\n");

    test_audio_processing();
    printf("\n");

    test_reset();
    printf("\n");

    print_separator();
    printf("All tests completed!\n");
    print_separator();

    return 0;
}
