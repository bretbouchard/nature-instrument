//
//  NatureDSP.h
//  SharedDSP
//
//  C++ wrapper for Nature Marco synthesizer DSP
//  Provides interface for AUv3 extension to access DSP functionality
//

#ifndef NatureDSP_h
#define NatureDSP_h

#include <AudioToolbox/AudioToolbox.h>
#include <memory>
#include <array>

// Parameter addresses for Nature Marco (60+ parameters)
enum NatureParameterAddress : AUParameterAddress {
    // OSC1 (0-5)
    PARAM_OSC1_SHAPE = 0,
    PARAM_OSC1_WARP,
    PARAM_OSC1_PULSE_WIDTH,
    PARAM_OSC1_DETUNE,
    PARAM_OSC1_PAN,
    PARAM_OSC1_LEVEL,

    // OSC2 (6-11)
    PARAM_OSC2_SHAPE = 6,
    PARAM_OSC2_WARP,
    PARAM_OSC2_PULSE_WIDTH,
    PARAM_OSC2_DETUNE,
    PARAM_OSC2_PAN,
    PARAM_OSC2_LEVEL,

    // Sub & Noise (12-13)
    PARAM_SUB_ENABLED = 12,
    PARAM_SUB_LEVEL,
    PARAM_NOISE_LEVEL,

    // FM Synthesis (15-19)
    PARAM_FM_ENABLED = 15,
    PARAM_FM_CARRIER_OSC,
    PARAM_FM_MODE,
    PARAM_FM_DEPTH,
    PARAM_FM_MODULATOR_RATIO,

    // Filter (20-24)
    PARAM_FILTER_TYPE = 20,
    PARAM_FILTER_CUTOFF,
    PARAM_FILTER_RESONANCE,
    PARAM_FILTER_KEY_TRACK,
    PARAM_FILTER_VEL_TRACK,

    // Filter Envelope (25-29)
    PARAM_FILTER_ENV_ATTACK = 25,
    PARAM_FILTER_ENV_DECAY,
    PARAM_FILTER_ENV_SUSTAIN,
    PARAM_FILTER_ENV_RELEASE,
    PARAM_FILTER_ENV_AMOUNT,

    // Amp Envelope (30-33)
    PARAM_AMP_ENV_ATTACK = 30,
    PARAM_AMP_ENV_DECAY,
    PARAM_AMP_ENV_SUSTAIN,
    PARAM_AMP_ENV_RELEASE,

    // LFO1 (34-37)
    PARAM_LFO1_WAVEFORM = 34,
    PARAM_LFO1_RATE,
    PARAM_LFO1_DEPTH,
    PARAM_LFO1_BIPOLAR,

    // LFO2 (38-41)
    PARAM_LFO2_WAVEFORM = 38,
    PARAM_LFO2_RATE,
    PARAM_LFO2_DEPTH,
    PARAM_LFO2_BIPOLAR,

    // Modulation Matrix (16 slots × 5 params = 80 params)
    // Slot 0: 42-46
    PARAM_MOD0_SOURCE = 42,
    PARAM_MOD0_DESTINATION,
    PARAM_MOD0_AMOUNT,
    PARAM_MOD0_BIPOLAR,
    PARAM_MOD0_CURVE,

    // Slot 1: 47-51
    PARAM_MOD1_SOURCE = 47,
    PARAM_MOD1_DESTINATION,
    PARAM_MOD1_AMOUNT,
    PARAM_MOD1_BIPOLAR,
    PARAM_MOD1_CURVE,

    // Slot 2: 52-56
    PARAM_MOD2_SOURCE = 52,
    PARAM_MOD2_DESTINATION,
    PARAM_MOD2_AMOUNT,
    PARAM_MOD2_BIPOLAR,
    PARAM_MOD2_CURVE,

    // Slot 3: 57-61
    PARAM_MOD3_SOURCE = 57,
    PARAM_MOD3_DESTINATION,
    PARAM_MOD3_AMOUNT,
    PARAM_MOD3_BIPOLAR,
    PARAM_MOD3_CURVE,

    // Slot 4: 62-66
    PARAM_MOD4_SOURCE = 62,
    PARAM_MOD4_DESTINATION,
    PARAM_MOD4_AMOUNT,
    PARAM_MOD4_BIPOLAR,
    PARAM_MOD4_CURVE,

    // Slot 5: 67-71
    PARAM_MOD5_SOURCE = 67,
    PARAM_MOD5_DESTINATION,
    PARAM_MOD5_AMOUNT,
    PARAM_MOD5_BIPOLAR,
    PARAM_MOD5_CURVE,

    // Slot 6: 72-76
    PARAM_MOD6_SOURCE = 72,
    PARAM_MOD6_DESTINATION,
    PARAM_MOD6_AMOUNT,
    PARAM_MOD6_BIPOLAR,
    PARAM_MOD6_CURVE,

    // Slot 7: 77-81
    PARAM_MOD7_SOURCE = 77,
    PARAM_MOD7_DESTINATION,
    PARAM_MOD7_AMOUNT,
    PARAM_MOD7_BIPOLAR,
    PARAM_MOD7_CURVE,

    // Macros (8 macros: 122-129)
    PARAM_MACRO1_VALUE = 122,
    PARAM_MACRO2_VALUE,
    PARAM_MACRO3_VALUE,
    PARAM_MACRO4_VALUE,
    PARAM_MACRO5_VALUE,
    PARAM_MACRO6_VALUE,
    PARAM_MACRO7_VALUE,
    PARAM_MACRO8_VALUE,

    // Global (130-134)
    PARAM_STRUCTURE = 130,
    PARAM_POLY_MODE,
    PARAM_GLIDE_ENABLED,
    PARAM_GLIDE_TIME,
    PARAM_MASTER_TUNE,
    PARAM_MASTER_VOLUME,

    // Total: 135 parameters
    PARAM_COUNT = 136
};

class NatureDSP {
public:
    NatureDSP();
    ~NatureDSP();

    // Initialization
    void initialize(double sampleRate, int maximumFramesToRender);

    // DSP Processing
    void process(AUAudioFrameCount frameCount,
                AudioBufferList *outputBufferList,
                const AUEventSampleTime *timestamp,
                AUAudioFrameCount inputBusNumber = 0);

    // Parameters (60+ parameters)
    void setParameter(AUParameterAddress address, float value);
    float getParameter(AUParameterAddress address) const;

    // MIDI
    void handleMIDIEvent(const uint8_t *message, uint8_t messageSize);

    // Presets (JSON-based)
    void setState(const char *stateData);
    const char *getState() const;

    // Factory presets
    int getFactoryPresetCount() const;
    const char *getFactoryPresetName(int index) const;
    void loadFactoryPreset(int index);

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

#endif /* NatureDSP_h */
