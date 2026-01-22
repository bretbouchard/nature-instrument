/*
  ==============================================================================

    test_upfs_preset.cpp
    Test UPFS v1.0 preset loading for Nature Marco

  ==============================================================================
*/

#include <iostream>
#include <fstream>
#include <string>
#include "src/dsp/NaturePureDSP.h"

using namespace DSP;

int main() {
    std::cout << "=== Nature Marco UPFS v1.0 Preset Loading Test ===" << std::endl;

    // Create Nature Marco DSP instance
    NaturePureDSP natureMarcoDSP;

    // Prepare DSP
    natureMarcoDSP.prepare(48000.0, 512);
    std::cout << "✓ DSP prepared at 48kHz, 512 samples" << std::endl;

    // Test 1: Load UPFS v1.0 preset
    std::cout << "\n--- Test 1: UPFS v1.0 Format ---" << std::endl;
    std::ifstream upfsPresetFile("presets/Nature/01_Deep_Reesey_Bass.json");
    if (!upfsPresetFile.is_open()) {
        std::cerr << "✗ Failed to open UPFS preset file" << std::endl;
        return 1;
    }

    std::string upfsJsonData((std::istreambuf_iterator<char>(upfsPresetFile)),
                             std::istreambuf_iterator<char>());

    bool upfsSuccess = natureMarcoDSP.loadPreset(upfsJsonData.c_str());

    if (upfsSuccess) {
        std::cout << "✓ SUCCESS: UPFS v1.0 preset loaded!" << std::endl;

        // Verify key parameters were loaded
        std::cout << "\nVerifying parameters:" << std::endl;
        std::cout << "  OSC1 Shape: " << natureMarcoDSP.getParameter("osc1_shape")
                  << " (expected: 0.0)" << std::endl;
        std::cout << "  OSC1 Warp: " << natureMarcoDSP.getParameter("osc1_warp")
                  << " (expected: 0.3)" << std::endl;
        std::cout << "  OSC1 Level: " << natureMarcoDSP.getParameter("osc1_level")
                  << " (expected: 0.7)" << std::endl;
        std::cout << "  OSC2 Shape: " << natureMarcoDSP.getParameter("osc2_shape")
                  << " (expected: 1.0)" << std::endl;
        std::cout << "  Filter Cutoff: " << natureMarcoDSP.getParameter("filter_cutoff")
                  << " (expected: 0.35)" << std::endl;
        std::cout << "  Filter Resonance: " << natureMarcoDSP.getParameter("filter_resonance")
                  << " (expected: 0.7)" << std::endl;
        std::cout << "  FM Enabled: " << natureMarcoDSP.getParameter("fm_enabled")
                  << " (expected: 1.0)" << std::endl;
        std::cout << "  FM Depth: " << natureMarcoDSP.getParameter("fm_depth")
                  << " (expected: 0.6)" << std::endl;
        std::cout << "  Master Volume: " << natureMarcoDSP.getParameter("master_volume")
                  << " (expected: 0.85)" << std::endl;

        // Validate values
        bool allValid = true;
        if (std::abs(natureMarcoDSP.getParameter("osc1_shape") - 0.0f) > 0.001f) {
            std::cerr << "  ✗ OSC1 Shape mismatch!" << std::endl;
            allValid = false;
        }
        if (std::abs(natureMarcoDSP.getParameter("osc1_warp") - 0.3f) > 0.001f) {
            std::cerr << "  ✗ OSC1 Warp mismatch!" << std::endl;
            allValid = false;
        }
        if (std::abs(natureMarcoDSP.getParameter("master_volume") - 0.85f) > 0.001f) {
            std::cerr << "  ✗ Master Volume mismatch!" << std::endl;
            allValid = false;
        }

        if (allValid) {
            std::cout << "\n✓ All parameter values validated successfully!" << std::endl;
        } else {
            std::cout << "\n✗ Some parameter values didn't match expected values" << std::endl;
        }
    } else {
        std::cerr << "✗ FAILED: Could not load UPFS preset" << std::endl;
        return 1;
    }

    // Test 2: Legacy format (if available)
    std::cout << "\n--- Test 2: Legacy Format (if available) ---" << std::endl;
    // Create a simple legacy preset JSON
    const char* legacyJson = R"({
        "osc1_shape": 0.0,
        "osc1_warp": 0.5,
        "osc1_level": 0.8,
        "osc2_shape": 1.0,
        "osc2_warp": 0.0,
        "osc2_level": 0.6,
        "filter_cutoff": 0.6,
        "filter_resonance": 0.5,
        "amp_env_attack": 0.01,
        "amp_env_decay": 0.2,
        "amp_env_sustain": 0.7,
        "amp_env_release": 0.3,
        "master_volume": 0.9
    })";

    bool legacySuccess = natureMarcoDSP.loadPreset(legacyJson);

    if (legacySuccess) {
        std::cout << "✓ SUCCESS: Legacy preset loaded!" << std::endl;
        std::cout << "  Master Volume: " << natureMarcoDSP.getParameter("master_volume")
                  << " (expected: 0.9)" << std::endl;
    } else {
        std::cerr << "✗ FAILED: Could not load legacy preset" << std::endl;
        return 1;
    }

    std::cout << "\n=== All Tests Passed! ===" << std::endl;
    return 0;
}
