//
//  NatureDSP.cpp
//  SharedDSP
//
//  C++ wrapper implementation for Nature Marco synthesizer DSP
//

#include "NatureDSP.h"
#include "plugins/dsp/include/dsp/NaturePureDSP.h"
#include <cstring>
#include <cmath>
#include <array>

// Factory preset names (30 presets)
static const char* factoryPresetNames[] = {
    "Deep Reesey Bass",
    "Rubber Band Bass",
    "Sub Warp Foundation",
    "Acid Techno Bass",
    "Metallic FM Bass",
    "Evolving Warp Lead",
    "Crystal FM Bell",
    "Aggressive Saw Lead",
    "Retro Square Lead",
    "Warping SciFi Lead",
    "Warm Analog Pad",
    "Ethereal Bell Pad",
    "Dark Warp Choir",
    "Metallic FM Pad",
    "SciFi Atmosphere",
    "Electric Pluck",
    "Warp Guitar",
    "FM Kalimba",
    "Rubber Band Pluck",
    "Metallic Harp",
    "Alien Texture",
    "Glitchy Noise",
    "Dark Drone",
    "SciFi Sweep",
    "Wurly Electric Piano",
    "FM Clavinet",
    "Harmonic Synth",
    "Acid Loop",
    "Bassline Groove",
    "Arpeggiator Bliss"
};

class NatureDSP::Impl {
public:
    DSP::NaturePureDSP dsp;
    double sampleRate = 48000.0;
    int blockSize = 512;

    // Parameter cache (135 parameters)
    std::array<float, 136> parameterCache;

    Impl() {
        // Initialize parameter cache with defaults
        parameterCache.fill(0.5f);

        // OSC1 defaults
        parameterCache[PARAM_OSC1_LEVEL] = 0.7f;
        parameterCache[PARAM_OSC1_PULSE_WIDTH] = 0.5f;

        // OSC2 defaults
        parameterCache[PARAM_OSC2_LEVEL] = 0.5f;
        parameterCache[PARAM_OSC2_PULSE_WIDTH] = 0.5f;

        // Sub defaults
        parameterCache[PARAM_SUB_ENABLED] = 1.0f;
        parameterCache[PARAM_SUB_LEVEL] = 0.3f;

        // Filter defaults
        parameterCache[PARAM_FILTER_CUTOFF] = 0.5f;
        parameterCache[PARAM_FILTER_RESONANCE] = 0.5f;

        // Envelope defaults
        parameterCache[PARAM_FILTER_ENV_ATTACK] = 0.01f;
        parameterCache[PARAM_FILTER_ENV_DECAY] = 0.1f;
        parameterCache[PARAM_FILTER_ENV_SUSTAIN] = 0.5f;
        parameterCache[PARAM_FILTER_ENV_RELEASE] = 0.2f;

        parameterCache[PARAM_AMP_ENV_ATTACK] = 0.005f;
        parameterCache[PARAM_AMP_ENV_DECAY] = 0.1f;
        parameterCache[PARAM_AMP_ENV_SUSTAIN] = 0.6f;
        parameterCache[PARAM_AMP_ENV_RELEASE] = 0.2f;

        // LFO defaults
        parameterCache[PARAM_LFO1_RATE] = 5.0f;
        parameterCache[PARAM_LFO1_DEPTH] = 0.5f;
        parameterCache[PARAM_LFO1_BIPOLAR] = 1.0f;

        parameterCache[PARAM_LFO2_RATE] = 3.0f;
        parameterCache[PARAM_LFO2_DEPTH] = 0.5f;
        parameterCache[PARAM_LFO2_BIPOLAR] = 1.0f;

        // Macros defaults
        for (int i = 0; i < 8; i++) {
            parameterCache[PARAM_MACRO1_VALUE + i] = 0.5f;
        }

        // Global defaults
        parameterCache[PARAM_STRUCTURE] = 0.5f;
        parameterCache[PARAM_GLIDE_TIME] = 0.1f;
        parameterCache[PARAM_MASTER_VOLUME] = 3.0f; // Reduced to prevent clipping
    }

    void syncParametersToDSP() {
        // OSC1
        dsp.setParameter("osc1Shape", parameterCache[PARAM_OSC1_SHAPE]);
        dsp.setParameter("osc1Warp", parameterCache[PARAM_OSC1_WARP]);
        dsp.setParameter("osc1PulseWidth", parameterCache[PARAM_OSC1_PULSE_WIDTH]);
        dsp.setParameter("osc1Detune", parameterCache[PARAM_OSC1_DETUNE]);
        dsp.setParameter("osc1Pan", parameterCache[PARAM_OSC1_PAN]);
        dsp.setParameter("osc1Level", parameterCache[PARAM_OSC1_LEVEL]);

        // OSC2
        dsp.setParameter("osc2Shape", parameterCache[PARAM_OSC2_SHAPE]);
        dsp.setParameter("osc2Warp", parameterCache[PARAM_OSC2_WARP]);
        dsp.setParameter("osc2PulseWidth", parameterCache[PARAM_OSC2_PULSE_WIDTH]);
        dsp.setParameter("osc2Detune", parameterCache[PARAM_OSC2_DETUNE]);
        dsp.setParameter("osc2Pan", parameterCache[PARAM_OSC2_PAN]);
        dsp.setParameter("osc2Level", parameterCache[PARAM_OSC2_LEVEL]);

        // Sub & Noise
        dsp.setParameter("subEnabled", parameterCache[PARAM_SUB_ENABLED]);
        dsp.setParameter("subLevel", parameterCache[PARAM_SUB_LEVEL]);
        dsp.setParameter("noiseLevel", parameterCache[PARAM_NOISE_LEVEL]);

        // FM
        dsp.setParameter("fmEnabled", parameterCache[PARAM_FM_ENABLED]);
        dsp.setParameter("fmCarrierOsc", parameterCache[PARAM_FM_CARRIER_OSC]);
        dsp.setParameter("fmMode", parameterCache[PARAM_FM_MODE]);
        dsp.setParameter("fmDepth", parameterCache[PARAM_FM_DEPTH]);
        dsp.setParameter("fmModulatorRatio", parameterCache[PARAM_FM_MODULATOR_RATIO]);

        // Filter
        dsp.setParameter("filterType", parameterCache[PARAM_FILTER_TYPE]);
        dsp.setParameter("filterCutoff", parameterCache[PARAM_FILTER_CUTOFF]);
        dsp.setParameter("filterResonance", parameterCache[PARAM_FILTER_RESONANCE]);
        dsp.setParameter("filterKeyTrack", parameterCache[PARAM_FILTER_KEY_TRACK]);
        dsp.setParameter("filterVelTrack", parameterCache[PARAM_FILTER_VEL_TRACK]);

        // Filter Envelope
        dsp.setParameter("filterEnvAttack", parameterCache[PARAM_FILTER_ENV_ATTACK]);
        dsp.setParameter("filterEnvDecay", parameterCache[PARAM_FILTER_ENV_DECAY]);
        dsp.setParameter("filterEnvSustain", parameterCache[PARAM_FILTER_ENV_SUSTAIN]);
        dsp.setParameter("filterEnvRelease", parameterCache[PARAM_FILTER_ENV_RELEASE]);
        dsp.setParameter("filterEnvAmount", parameterCache[PARAM_FILTER_ENV_AMOUNT]);

        // Amp Envelope
        dsp.setParameter("ampEnvAttack", parameterCache[PARAM_AMP_ENV_ATTACK]);
        dsp.setParameter("ampEnvDecay", parameterCache[PARAM_AMP_ENV_DECAY]);
        dsp.setParameter("ampEnvSustain", parameterCache[PARAM_AMP_ENV_SUSTAIN]);
        dsp.setParameter("ampEnvRelease", parameterCache[PARAM_AMP_ENV_RELEASE]);

        // LFO1
        dsp.setParameter("lfo1Waveform", parameterCache[PARAM_LFO1_WAVEFORM]);
        dsp.setParameter("lfo1Rate", parameterCache[PARAM_LFO1_RATE]);
        dsp.setParameter("lfo1Depth", parameterCache[PARAM_LFO1_DEPTH]);
        dsp.setParameter("lfo1Bipolar", parameterCache[PARAM_LFO1_BIPOLAR]);

        // LFO2
        dsp.setParameter("lfo2Waveform", parameterCache[PARAM_LFO2_WAVEFORM]);
        dsp.setParameter("lfo2Rate", parameterCache[PARAM_LFO2_RATE]);
        dsp.setParameter("lfo2Depth", parameterCache[PARAM_LFO2_DEPTH]);
        dsp.setParameter("lfo2Bipolar", parameterCache[PARAM_LFO2_BIPOLAR]);

        // Modulation Matrix (16 slots)
        for (int slot = 0; slot < 16; slot++) {
            int baseAddr = PARAM_MOD0_SOURCE + (slot * 5);
            char sourceID[32], destID[32];
            snprintf(sourceID, sizeof(sourceID), "modSource%d", slot);
            snprintf(destID, sizeof(destID), "modDestination%d", slot);

            dsp.setParameter(sourceID, parameterCache[baseAddr]);
            dsp.setParameter(destID, parameterCache[baseAddr + 1]);
            // Amount, bipolar, curve handled internally by DSP
        }

        // Macros
        for (int i = 0; i < 8; i++) {
            char macroID[32];
            snprintf(macroID, sizeof(macroID), "macroValue%d", i);
            dsp.setParameter(macroID, parameterCache[PARAM_MACRO1_VALUE + i]);
        }

        // Global
        dsp.setParameter("structure", parameterCache[PARAM_STRUCTURE]);
        dsp.setParameter("polyMode", parameterCache[PARAM_POLY_MODE]);
        dsp.setParameter("glideEnabled", parameterCache[PARAM_GLIDE_ENABLED]);
        dsp.setParameter("glideTime", parameterCache[PARAM_GLIDE_TIME]);
        dsp.setParameter("masterTune", parameterCache[PARAM_MASTER_TUNE]);
        dsp.setParameter("masterVolume", parameterCache[PARAM_MASTER_VOLUME]);
    }
};

NatureDSP::NatureDSP() : impl(std::make_unique<Impl>()) {}

NatureDSP::~NatureDSP() = default;

void NatureDSP::initialize(double sampleRate, int maximumFramesToRender) {
    impl->sampleRate = sampleRate;
    impl->blockSize = maximumFramesToRender;
    impl->dsp.prepare(sampleRate, maximumFramesToRender);
    impl->syncParametersToDSP();
}

void NatureDSP::process(AUAudioFrameCount frameCount,
                          AudioBufferList *outputBufferList,
                          const AUEventSampleTime *timestamp,
                          AUAudioFrameCount inputBusNumber) {
    if (outputBufferList->mNumberBuffers < 1) return;

    // Get output buffer pointers
    float *leftChannel = static_cast<float *>(outputBufferList->mBuffers[0].mData);
    float *rightChannel = (outputBufferList->mNumberBuffers > 1)
                          ? static_cast<float *>(outputBufferList->mBuffers[1].mData)
                          : leftChannel;

    // Create temporary stereo buffer for DSP
    float *stereoBuffer[2] = { leftChannel, rightChannel };

    // Process audio
    impl->dsp.process(stereoBuffer, 2, static_cast<int>(frameCount));
}

void NatureDSP::setParameter(AUParameterAddress address, float value) {
    if (address >= PARAM_COUNT) return;

    impl->parameterCache[address] = value;

    // Map parameter address to DSP parameter ID
    const char *paramID = nullptr;

    switch (address) {
        // OSC1
        case PARAM_OSC1_SHAPE: paramID = "osc1Shape"; break;
        case PARAM_OSC1_WARP: paramID = "osc1Warp"; break;
        case PARAM_OSC1_PULSE_WIDTH: paramID = "osc1PulseWidth"; break;
        case PARAM_OSC1_DETUNE: paramID = "osc1Detune"; break;
        case PARAM_OSC1_PAN: paramID = "osc1Pan"; break;
        case PARAM_OSC1_LEVEL: paramID = "osc1Level"; break;

        // OSC2
        case PARAM_OSC2_SHAPE: paramID = "osc2Shape"; break;
        case PARAM_OSC2_WARP: paramID = "osc2Warp"; break;
        case PARAM_OSC2_PULSE_WIDTH: paramID = "osc2PulseWidth"; break;
        case PARAM_OSC2_DETUNE: paramID = "osc2Detune"; break;
        case PARAM_OSC2_PAN: paramID = "osc2Pan"; break;
        case PARAM_OSC2_LEVEL: paramID = "osc2Level"; break;

        // Sub & Noise
        case PARAM_SUB_ENABLED: paramID = "subEnabled"; break;
        case PARAM_SUB_LEVEL: paramID = "subLevel"; break;
        case PARAM_NOISE_LEVEL: paramID = "noiseLevel"; break;

        // FM
        case PARAM_FM_ENABLED: paramID = "fmEnabled"; break;
        case PARAM_FM_CARRIER_OSC: paramID = "fmCarrierOsc"; break;
        case PARAM_FM_MODE: paramID = "fmMode"; break;
        case PARAM_FM_DEPTH: paramID = "fmDepth"; break;
        case PARAM_FM_MODULATOR_RATIO: paramID = "fmModulatorRatio"; break;

        // Filter
        case PARAM_FILTER_TYPE: paramID = "filterType"; break;
        case PARAM_FILTER_CUTOFF: paramID = "filterCutoff"; break;
        case PARAM_FILTER_RESONANCE: paramID = "filterResonance"; break;
        case PARAM_FILTER_KEY_TRACK: paramID = "filterKeyTrack"; break;
        case PARAM_FILTER_VEL_TRACK: paramID = "filterVelTrack"; break;

        // Filter Envelope
        case PARAM_FILTER_ENV_ATTACK: paramID = "filterEnvAttack"; break;
        case PARAM_FILTER_ENV_DECAY: paramID = "filterEnvDecay"; break;
        case PARAM_FILTER_ENV_SUSTAIN: paramID = "filterEnvSustain"; break;
        case PARAM_FILTER_ENV_RELEASE: paramID = "filterEnvRelease"; break;
        case PARAM_FILTER_ENV_AMOUNT: paramID = "filterEnvAmount"; break;

        // Amp Envelope
        case PARAM_AMP_ENV_ATTACK: paramID = "ampEnvAttack"; break;
        case PARAM_AMP_ENV_DECAY: paramID = "ampEnvDecay"; break;
        case PARAM_AMP_ENV_SUSTAIN: paramID = "ampEnvSustain"; break;
        case PARAM_AMP_ENV_RELEASE: paramID = "ampEnvRelease"; break;

        // LFO1
        case PARAM_LFO1_WAVEFORM: paramID = "lfo1Waveform"; break;
        case PARAM_LFO1_RATE: paramID = "lfo1Rate"; break;
        case PARAM_LFO1_DEPTH: paramID = "lfo1Depth"; break;
        case PARAM_LFO1_BIPOLAR: paramID = "lfo1Bipolar"; break;

        // LFO2
        case PARAM_LFO2_WAVEFORM: paramID = "lfo2Waveform"; break;
        case PARAM_LFO2_RATE: paramID = "lfo2Rate"; break;
        case PARAM_LFO2_DEPTH: paramID = "lfo2Depth"; break;
        case PARAM_LFO2_BIPOLAR: paramID = "lfo2Bipolar"; break;

        // Global
        case PARAM_STRUCTURE: paramID = "structure"; break;
        case PARAM_POLY_MODE: paramID = "polyMode"; break;
        case PARAM_GLIDE_ENABLED: paramID = "glideEnabled"; break;
        case PARAM_GLIDE_TIME: paramID = "glideTime"; break;
        case PARAM_MASTER_TUNE: paramID = "masterTune"; break;
        case PARAM_MASTER_VOLUME: paramID = "masterVolume"; break;

        default:
            // Modulation matrix and macros handled separately
            break;
    }

    if (paramID) {
        impl->dsp.setParameter(paramID, value);
    }
}

float NatureDSP::getParameter(AUParameterAddress address) const {
    if (address >= PARAM_COUNT) return 0.0f;
    return impl->parameterCache[address];
}

void NatureDSP::handleMIDIEvent(const uint8_t *message, uint8_t messageSize) {
    if (messageSize < 3) return;

    uint8_t status = message[0];
    uint8_t data1 = message[1];
    uint8_t data2 = message[2];

    // Create scheduled event
    DSP::ScheduledEvent event;
    event.sampleOffset = 0;
    event.type = DSP::EventType::UNKNOWN;

    // Parse MIDI message
    if (status >= 0x80 && status <= 0x8F) {
        // Note Off
        event.type = DSP::EventType::NOTE_OFF;
        event.note = data1;
        event.velocity = data2 / 127.0f;
    } else if (status >= 0x90 && status <= 0x9F) {
        // Note On
        event.type = (data2 > 0) ? DSP::EventType::NOTE_ON : DSP::EventType::NOTE_OFF;
        event.note = data1;
        event.velocity = data2 / 127.0f;
    } else if (status >= 0xE0 && status <= 0xEF) {
        // Pitch Bend
        event.type = DSP::EventType::PITCH_BEND;
        int value = (data2 << 7) | data1;
        event.pitchBend = (value - 8192) / 8192.0;
    }

    if (event.type != DSP::EventType::UNKNOWN) {
        impl->dsp.handleEvent(event);
    }
}

void NatureDSP::setState(const char *stateData) {
    impl->dsp.loadPreset(stateData);
}

const char *NatureDSP::getState() const {
    static char jsonBuffer[4096];
    if (impl->dsp.savePreset(jsonBuffer, sizeof(jsonBuffer))) {
        return jsonBuffer;
    }
    return "{}";
}

int NatureDSP::getFactoryPresetCount() const {
    return sizeof(factoryPresetNames) / sizeof(factoryPresetNames[0]);
}

const char *NatureDSP::getFactoryPresetName(int index) const {
    if (index >= 0 && index < getFactoryPresetCount()) {
        return factoryPresetNames[index];
    }
    return "Unknown";
}

void NatureDSP::loadFactoryPreset(int index) {
    // Load from presets/Nature folder
    // This requires file system access which may be limited in AUv3 sandbox
    // For now, we'll implement this as a no-op and let the host app handle presets
}
