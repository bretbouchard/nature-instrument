/*
  Aether Preset Loading Example

  This example demonstrates how to load and use factory presets
  in the Aether physical modeling synthesizer.
*/

#include "../include/dsp/AetherDSP.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

class NatureAetherPresetLoader {
public:
    struct PresetParameters {
        // Exciter parameters
        float exciter_noise_color;
        float exciter_gain;
        float exciter_attack;
        float exciter_decay;
        float exciter_sustain;
        float exciter_release;

        // Resonator parameters
        float resonator_mode_count;
        float resonator_brightness;
        float resonator_decay;

        // Feedback parameters
        float feedback_amount;
        float feedback_delay_time;
        float feedback_saturation;
        float feedback_mix;

        // Filter parameters
        float filter_cutoff;
        float filter_resonance;

        // Amp envelope
        float amp_attack;
        float amp_decay;
        float amp_sustain;
        float amp_release;
    };

    // Load preset from JSON file
    static bool loadPreset(const std::string& presetPath,
                          AetherDSP& aether,
                          PresetParameters& params) {
        try {
            // Read JSON file
            std::ifstream file(presetPath);
            if (!file.is_open()) {
                std::cerr << "Failed to open preset: " << presetPath << std::endl;
                return false;
            }

            json presetJson;
            file >> presetJson;

            // Validate version
            if (!presetJson.contains("version")) {
                std::cerr << "Preset missing version field" << std::endl;
                return false;
            }

            // Extract parameters
            if (!presetJson.contains("parameters")) {
                std::cerr << "Preset missing parameters field" << std::endl;
                return false;
            }

            json parameters = presetJson["parameters"];

            // Load into struct
            params.exciter_noise_color = parameters["exciter_noise_color"];
            params.exciter_gain = parameters["exciter_gain"];
            params.exciter_attack = parameters["exciter_attack"];
            params.exciter_decay = parameters["exciter_decay"];
            params.exciter_sustain = parameters["exciter_sustain"];
            params.exciter_release = parameters["exciter_release"];

            params.resonator_mode_count = parameters["resonator_mode_count"];
            params.resonator_brightness = parameters["resonator_brightness"];
            params.resonator_decay = parameters["resonator_decay"];

            params.feedback_amount = parameters["feedback_amount"];
            params.feedback_delay_time = parameters["feedback_delay_time"];
            params.feedback_saturation = parameters["feedback_saturation"];
            params.feedback_mix = parameters["feedback_mix"];

            params.filter_cutoff = parameters["filter_cutoff"];
            params.filter_resonance = parameters["filter_resonance"];

            params.amp_attack = parameters["amp_attack"];
            params.amp_decay = parameters["amp_decay"];
            params.amp_sustain = parameters["amp_sustain"];
            params.amp_release = parameters["amp_release"];

            // Apply to Aether DSP
            applyPresetToDSP(params, aether);

            std::cout << "Loaded preset: " << presetJson["name"] << std::endl;
            std::cout << "Category: " << presetJson["category"] << std::endl;
            std::cout << "Description: " << presetJson["description"] << std::endl;

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Error loading preset: " << e.what() << std::endl;
            return false;
        }
    }

    // Apply preset parameters to DSP engine
    static void applyPresetToDSP(const PresetParameters& params,
                                 AetherDSP& aether) {
        // Exciter
        aether.setExciterNoiseColor(params.exciter_noise_color);
        aether.setExciterGain(params.exciter_gain);
        aether.setExciterAttack(params.exciter_attack);
        aether.setExciterDecay(params.exciter_decay);
        aether.setExciterSustain(params.exciter_sustain);
        aether.setExciterRelease(params.exciter_release);

        // Resonator
        aether.setResonatorModeCount(static_cast<int>(params.resonator_mode_count));
        aether.setResonatorBrightness(params.resonator_brightness);
        aether.setResonatorDecay(params.resonator_decay);

        // Feedback
        aether.setFeedbackAmount(params.feedback_amount);
        aether.setFeedbackDelayTime(params.feedback_delay_time);
        aether.setFeedbackSaturation(params.feedback_saturation);
        aether.setFeedbackMix(params.feedback_mix);

        // Filter
        aether.setFilterCutoff(params.filter_cutoff);
        aether.setFilterResonance(params.filter_resonance);

        // Amp envelope
        aether.setAmpAttack(params.amp_attack);
        aether.setAmpDecay(params.amp_decay);
        aether.setAmpSustain(params.amp_sustain);
        aether.setAmpRelease(params.amp_release);
    }

    // List all available presets
    static std::vector<std::string> getAvailablePresets(
        const std::string& presetsDir = "presets/NatureAether") {

        std::vector<std::string> presets;
        std::filesystem::path dir(presetsDir);

        if (!std::filesystem::exists(dir)) {
            std::cerr << "Presets directory not found: " << presetsDir << std::endl;
            return presets;
        }

        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".json") {
                presets.push_back(entry.path().string());
            }
        }

        // Sort alphabetically
        std::sort(presets.begin(), presets.end());

        return presets;
    }

    // Get preset metadata without loading full parameters
    static bool getPresetMetadata(const std::string& presetPath,
                                  std::string& name,
                                  std::string& category,
                                  std::string& description) {
        try {
            std::ifstream file(presetPath);
            if (!file.is_open()) return false;

            json presetJson;
            file >> presetJson;

            name = presetJson.value("name", "Unknown");
            category = presetJson.value("category", "Uncategorized");
            description = presetJson.value("description", "");

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Error reading preset metadata: " << e.what() << std::endl;
            return false;
        }
    }
};

/*
  Usage Example:

  ```cpp
  // Create DSP instance
  AetherDSP aether;
  aether.prepare(sampleRate, samplesPerBlock);

  // Load preset
  NatureAetherPresetLoader::PresetParameters params;
  bool success = NatureAetherPresetLoader::loadPreset(
      "presets/NatureAether/01_Ethereal_Atmosphere.json",
      aether,
      params
  );

  if (success) {
      // Play note
      aether.noteOn(midiNoteNumber, velocity);
  }

  // List all presets
  auto presets = NatureAetherPresetLoader::getAvailablePresets();
  for (const auto& presetPath : presets) {
      std::string name, category, description;
      if (NatureAetherPresetLoader::getPresetMetadata(
              presetPath, name, category, description)) {
          std::cout << name << " [" << category << "]" << std::endl;
      }
  }
  ```
*/
