//
//  AudioUnit.swift
//  NaturePluginExtension
//
//  Main AUv3 audio unit implementation for Nature Marco synthesizer
//

import AudioToolbox

@objc(AudioUnit)
public class AudioUnit: AUAudioUnit {

    var dsp: NatureDSPWrapper?
    var parameterTree: AUParameterTree!

    // Parameter definitions (135 parameters total)
    private let parameterDefinitions: [AUParameterIdentifier: (name: String, range: ClosedRange<Float>, unit: AUUnitParameterUnit, defaultValue: Float)] = [
        // OSC1 (0-5)
        "osc1Shape": ("OSC1 Shape", 0.0...4.0, .indexed, 0.0),
        "osc1Warp": ("OSC1 Warp", -1.0...1.0, .generic, 0.0),
        "osc1PulseWidth": ("OSC1 Pulse Width", 0.0...1.0, .generic, 0.5),
        "osc1Detune": ("OSC1 Detune", -100.0...100.0, .cents, 0.0),
        "osc1Pan": ("OSC1 Pan", -1.0...1.0, .generic, 0.0),
        "osc1Level": ("OSC1 Level", 0.0...1.0, .boolean, 0.7),

        // OSC2 (6-11)
        "osc2Shape": ("OSC2 Shape", 0.0...4.0, .indexed, 0.0),
        "osc2Warp": ("OSC2 Warp", -1.0...1.0, .generic, 0.0),
        "osc2PulseWidth": ("OSC2 Pulse Width", 0.0...1.0, .generic, 0.5),
        "osc2Detune": ("OSC2 Detune", -100.0...100.0, .cents, 0.0),
        "osc2Pan": ("OSC2 Pan", -1.0...1.0, .generic, 0.0),
        "osc2Level": ("OSC2 Level", 0.0...1.0, .boolean, 0.5),

        // Sub & Noise (12-14)
        "subEnabled": ("Sub Enabled", 0.0...1.0, .boolean, 1.0),
        "subLevel": ("Sub Level", 0.0...1.0, .generic, 0.3),
        "noiseLevel": ("Noise Level", 0.0...1.0, .generic, 0.0),

        // FM (15-19)
        "fmEnabled": ("FM Enabled", 0.0...1.0, .boolean, 0.0),
        "fmCarrierOsc": ("FM Carrier Osc", 0.0...1.0, .indexed, 0.0),
        "fmMode": ("FM Mode", 0.0...1.0, .indexed, 0.0),
        "fmDepth": ("FM Depth", 0.0...1.0, .generic, 0.0),
        "fmModulatorRatio": ("FM Modulator Ratio", 0.0...16.0, .generic, 1.0),

        // Filter (20-24)
        "filterType": ("Filter Type", 0.0...3.0, .indexed, 0.0),
        "filterCutoff": ("Filter Cutoff", 0.0...1.0, .hertz, 0.5),
        "filterResonance": ("Filter Resonance", 0.0...1.0, .generic, 0.5),
        "filterKeyTrack": ("Filter Key Track", 0.0...1.0, .generic, 0.0),
        "filterVelTrack": ("Filter Vel Track", 0.0...1.0, .generic, 0.0),

        // Filter Envelope (25-29)
        "filterEnvAttack": ("Filter Env Attack", 0.0...10.0, .seconds, 0.01),
        "filterEnvDecay": ("Filter Env Decay", 0.0...10.0, .seconds, 0.1),
        "filterEnvSustain": ("Filter Env Sustain", 0.0...1.0, .generic, 0.5),
        "filterEnvRelease": ("Filter Env Release", 0.0...10.0, .seconds, 0.2),
        "filterEnvAmount": ("Filter Env Amount", 0.0...1.0, .generic, 0.0),

        // Amp Envelope (30-33)
        "ampEnvAttack": ("Amp Env Attack", 0.0...10.0, .seconds, 0.005),
        "ampEnvDecay": ("Amp Env Decay", 0.0...10.0, .seconds, 0.1),
        "ampEnvSustain": ("Amp Env Sustain", 0.0...1.0, .generic, 0.6),
        "ampEnvRelease": ("Amp Env Release", 0.0...10.0, .seconds, 0.2),

        // LFO1 (34-37)
        "lfo1Waveform": ("LFO1 Waveform", 0.0...4.0, .indexed, 0.0),
        "lfo1Rate": ("LFO1 Rate", 0.1...20.0, .hertz, 5.0),
        "lfo1Depth": ("LFO1 Depth", 0.0...1.0, .generic, 0.5),
        "lfo1Bipolar": ("LFO1 Bipolar", 0.0...1.0, .boolean, 1.0),

        // LFO2 (38-41)
        "lfo2Waveform": ("LFO2 Waveform", 0.0...4.0, .indexed, 0.0),
        "lfo2Rate": ("LFO2 Rate", 0.1...20.0, .hertz, 3.0),
        "lfo2Depth": ("LFO2 Depth", 0.0...1.0, .generic, 0.5),
        "lfo2Bipolar": ("LFO2 Bipolar", 0.0...1.0, .boolean, 1.0),

        // Modulation Matrix (16 slots × 5 params)
        // Slot 0
        "mod0Source": ("Mod 0 Source", 0.0...15.0, .indexed, 0.0),
        "mod0Destination": ("Mod 0 Dest", 0.0...23.0, .indexed, 0.0),
        "mod0Amount": ("Mod 0 Amount", -1.0...1.0, .generic, 0.0),
        "mod0Bipolar": ("Mod 0 Bipolar", 0.0...1.0, .boolean, 1.0),
        "mod0Curve": ("Mod 0 Curve", 0.0...1.0, .indexed, 0.0),

        // Slot 1
        "mod1Source": ("Mod 1 Source", 0.0...15.0, .indexed, 0.0),
        "mod1Destination": ("Mod 1 Dest", 0.0...23.0, .indexed, 0.0),
        "mod1Amount": ("Mod 1 Amount", -1.0...1.0, .generic, 0.0),
        "mod1Bipolar": ("Mod 1 Bipolar", 0.0...1.0, .boolean, 1.0),
        "mod1Curve": ("Mod 1 Curve", 0.0...1.0, .indexed, 0.0),

        // Slot 2
        "mod2Source": ("Mod 2 Source", 0.0...15.0, .indexed, 0.0),
        "mod2Destination": ("Mod 2 Dest", 0.0...23.0, .indexed, 0.0),
        "mod2Amount": ("Mod 2 Amount", -1.0...1.0, .generic, 0.0),
        "mod2Bipolar": ("Mod 2 Bipolar", 0.0...1.0, .boolean, 1.0),
        "mod2Curve": ("Mod 2 Curve", 0.0...1.0, .indexed, 0.0),

        // Slot 3
        "mod3Source": ("Mod 3 Source", 0.0...15.0, .indexed, 0.0),
        "mod3Destination": ("Mod 3 Dest", 0.0...23.0, .indexed, 0.0),
        "mod3Amount": ("Mod 3 Amount", -1.0...1.0, .generic, 0.0),
        "mod3Bipolar": ("Mod 3 Bipolar", 0.0...1.0, .boolean, 1.0),
        "mod3Curve": ("Mod 3 Curve", 0.0...1.0, .indexed, 0.0),

        // Macros (122-129)
        "macro1Value": ("Macro 1", 0.0...1.0, .generic, 0.5),
        "macro2Value": ("Macro 2", 0.0...1.0, .generic, 0.5),
        "macro3Value": ("Macro 3", 0.0...1.0, .generic, 0.5),
        "macro4Value": ("Macro 4", 0.0...1.0, .generic, 0.5),
        "macro5Value": ("Macro 5", 0.0...1.0, .generic, 0.5),
        "macro6Value": ("Macro 6", 0.0...1.0, .generic, 0.5),
        "macro7Value": ("Macro 7", 0.0...1.0, .generic, 0.5),
        "macro8Value": ("Macro 8", 0.0...1.0, .generic, 0.5),

        // Global (130-135)
        "structure": ("Structure", 0.0...1.0, .generic, 0.5),
        "polyMode": ("Poly Mode", 0.0...2.0, .indexed, 0.0),
        "glideEnabled": ("Glide Enabled", 0.0...1.0, .boolean, 0.0),
        "glideTime": ("Glide Time", 0.0...1.0, .seconds, 0.1),
        "masterTune": ("Master Tune", -100.0...100.0, .cents, 0.0),
        "masterVolume": ("Master Volume", 0.0...6.0, .decibels, 3.0)
    ]

    public override init(componentDescription: AudioComponentDescription,
                         options: AudioComponentInstantiationOptions) throws {
        try super.init(componentDescription: componentDescription, options: options)

        // Initialize DSP wrapper
        dsp = NatureDSPWrapper()

        // Create parameter tree
        parameterTree = AUParameterTree()

        // Register all parameters
        for (identifier, info) in parameterDefinitions {
            let parameter = AUParameter(
                identifier: identifier,
                name: info.name,
                address: AUParameterAddress(identifier.hashValue),
                range: info.range,
                unit: info.unit,
                flags: [.flag_IsReadable, .flag_IsWritable, .flag_CanRamp]
            )
            parameter.value = info.defaultValue
            parameterTree.registerParameter(parameter)
        }

        // Default parameter values
        parameterTree.implementorValueProvider = { [weak self] address in
            guard let self = self else { return 0.0 }
            return self.dsp?.getParameter(forAddress: address) ?? 0.5
        }

        parameterTree.implementorStringFromValueProvider = { parameter, value in
            // Custom value formatting
            if let param = parameter {
                switch param.identifier {
                case "osc1Shape", "osc2Shape":
                    let shapes = ["Saw", "Square", "Triangle", "Sine", "Pulse"]
                    let index = Int(value)
                    return index < shapes.count ? shapes[index] : "Unknown"
                case "filterType":
                    let types = ["LP", "HP", "BP", "Notch"]
                    let index = Int(value)
                    return index < types.count ? types[index] : "Unknown"
                case "lfo1Waveform", "lfo2Waveform":
                    let waves = ["Sine", "Triangle", "Saw", "Square", "S&H"]
                    let index = Int(value)
                    return index < waves.count ? waves[index] : "Unknown"
                case "polyMode":
                    let modes = ["Poly", "Mono", "Legato"]
                    let index = Int(value)
                    return index < modes.count ? modes[index] : "Unknown"
                default:
                    return String(format: "%.2f", value)
                }
            }
            return String(format: "%.2f", value)
        }
    }

    public override func allocateRenderResources() throws {
        try super.allocateRenderResources()

        // Initialize DSP with current format
        if let dsp = dsp {
            let format = self.outputBusses[0].format
            dsp.initialize(withSampleRate: format.sampleRate,
                          maximumFramesToRender: Int32(self.maximumFramesToRender))
        }
    }

    public override func deallocateRenderResources() {
        super.deallocateRenderResources()
    }

    public override var internalRenderBlock: AUInternalRenderBlock {
        return { [weak self] timestamp, frameCount, outputBusNumber, outputBufferList, inputBusNumber, events in
            guard let self = self else {
                return kAudioUnitErr_InvalidProperty
            }

            // Process events (MIDI, parameters)
            for event in events {
                self.handleEvent(event)
            }

            // Render audio
            self.dsp?.process(frameCount: frameCount,
                            outputBufferList: UnsafeMutableAudioBufferListPointer(outputBufferList),
                            timestamp: timestamp)

            return noErr
        }
    }

    private func handleEvent(_ event: AURenderEvent) {
        switch event.head.eventType {
        case .MIDI:
            if let midiEvent = event.MIDI {
                self.handleMIDI(midiEvent)
            }
        case .parameter:
            if let parameterEvent = event.parameter {
                self.handleParameter(parameterEvent)
            }
        default:
            break
        }
    }

    private func handleMIDI(_ event: AUMIDIEvent) {
        var message = [UInt8](repeating: 0, count: Int(event.length))
        message.withMutableBufferPointer { buffer in
            if let baseAddress = buffer.baseAddress {
                event.getData(&baseAddress.pointee)
                self.dsp?.handleMIDIEvent(message, messageSize: UInt8(event.length))
            }
        }
    }

    private func handleParameter(_ event: AUParameterEvent) {
        dsp?.setParameter(event.parameterAddress, value: event.value)
    }
}
