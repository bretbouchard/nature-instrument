#include <iostream>
#include <fstream>
#include <string>
#include "src/dsp/StringPureDSP.h"

using namespace DSP;

int main() {
    // Create String DSP instance
    StringPureDSP stringDSP;
    
    // Prepare DSP
    stringDSP.prepare(48000.0, 512);
    
    // Load UPFS v1.0 preset
    std::ifstream presetFile("presets/String/01_Clean_Telecaster.json");
    if (!presetFile.is_open()) {
        std::cerr << "Failed to open preset file" << std::endl;
        return 1;
    }
    
    std::string jsonData((std::istreambuf_iterator<char>(presetFile)),
                         std::istreambuf_iterator<char>());
    
    // Test preset loading
    bool success = stringDSP.loadPreset(jsonData.c_str());
    
    if (success) {
        std::cout << "SUCCESS: UPFS v1.0 preset loaded successfully!" << std::endl;
        
        // Verify parameters were loaded
        std::cout << "String Damping: " << stringDSP.getParameter("string_damping") << std::endl;
        std::cout << "String Stiffness: " << stringDSP.getParameter("string_stiffness") << std::endl;
        std::cout << "String Brightness: " << stringDSP.getParameter("string_brightness") << std::endl;
        std::cout << "Bridge Coupling: " << stringDSP.getParameter("bridge_coupling") << std::endl;
        std::cout << "Body Resonance: " << stringDSP.getParameter("body_resonance") << std::endl;
        
        return 0;
    } else {
        std::cerr << "FAILED: Could not load preset" << std::endl;
        return 1;
    }
}
