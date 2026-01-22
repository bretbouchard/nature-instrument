//
//  AudioUnitViewController.swift
//  NaturePluginExtension
//
//  SwiftUI view controller for Nature Marco AUv3 plugin
//

import SwiftUI
import AudioToolbox

class AudioUnitViewController: AUViewController {

    var audioUnit: AUAudioUnit?
    var parameterObserverToken: AUParameterObserverToken?

    override func viewDidLoad() {
        super.viewDidLoad()

        // Embed SwiftUI view
        let contentView = NatureMacroPluginView()
        let hostingController = UIHostingController(rootView: contentView)
        addChild(hostingController)
        view.addSubview(hostingController.view)
        hostingController.view.translatesAutoresizingMaskIntoConstraints = false
        NSLayoutConstraint.activate([
            hostingController.view.topAnchor.constraint(equalTo: view.topAnchor),
            hostingController.view.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            hostingController.view.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            hostingController.view.trailingAnchor.constraint(equalTo: view.trailingAnchor)
        ])
        hostingController.didMove(toParent: self)
    }

    func connectAudioUnit(_ audioUnit: AUAudioUnit) {
        self.audioUnit = audioUnit

        // Register parameter observer
        if let paramTree = audioUnit.parameterTree {
            parameterObserverToken = paramTree.token(byAddingParameterObserver: { [weak self] address, value in
                // Update UI when parameters change
                DispatchQueue.main.async {
                    // Handle parameter changes
                }
            })
        }
    }
}

// MARK: - SwiftUI View

struct NatureMacroPluginView: View {
    @State private var selectedTab = 0

    var body: some View {
        TabView(selection: $selectedTab) {
            OscillatorsView()
                .tabItem {
                    Image(systemName: "waveform")
                    Text("OSC")
                }
                .tag(0)

            FilterEnvelopeView()
                .tabItem {
                    Image(systemName: "slider.horizontal.3")
                    Text("FILTER")
                }
                .tag(1)

            ModulationView()
                .tabItem {
                    Image(systemName: "arrow.triangle.2.circlepath")
                    Text("MOD")
                }
                .tag(2)

            MacrosView()
                .tabItem {
                    Image(systemName: "circle.grid.2x2")
                    Text("MACROS")
                }
                .tag(3)

            GlobalView()
                .tabItem {
                    Image(systemName: "gear")
                    Text("GLOBAL")
                }
                .tag(4)
        }
        .pickerStyle(.segmented)
    }
}

// MARK: - Oscillators View

struct OscillatorsView: View {
    @State private var osc1Shape: Double = 0
    @State private var osc1Warp: Double = 0
    @State private var osc1Level: Double = 0.7
    @State private var osc1Detune: Double = 0

    @State private var osc2Shape: Double = 0
    @State private var osc2Warp: Double = 0
    @State private var osc2Level: Double = 0.5
    @State private var osc2Detune: Double = 0

    @State private var subEnabled: Bool = true
    @State private var subLevel: Double = 0.3
    @State private var noiseLevel: Double = 0

    var body: some View {
        ScrollView {
            VStack(spacing: 20) {
                // OSC1 Section
                VStack(alignment: .leading, spacing: 10) {
                    Text("OSCILLATOR 1")
                        .font(.headline)
                        .foregroundColor(.secondary)

                    Picker("Shape", selection: $osc1Shape) {
                        Text("Saw").tag(0.0)
                        Text("Square").tag(1.0)
                        Text("Triangle").tag(2.0)
                        Text("Sine").tag(3.0)
                        Text("Pulse").tag(4.0)
                    }
                    .pickerStyle(.menu)

                    HStack {
                        VStack(alignment: .leading) {
                            Text("Warp")
                                .font(.caption)
                            Slider(value: $osc1Warp, in: -1...1)
                        }

                        VStack(alignment: .leading) {
                            Text("Level")
                                .font(.caption)
                            Slider(value: $osc1Level, in: 0...1)
                        }
                    }

                    HStack {
                        VStack(alignment: .leading) {
                            Text("Detune")
                                .font(.caption)
                            Slider(value: $osc1Detune, in: -100...100)
                        }
                    }
                }
                .padding()
                .background(Color.gray.opacity(0.1))
                .cornerRadius(10)

                // OSC2 Section
                VStack(alignment: .leading, spacing: 10) {
                    Text("OSCILLATOR 2")
                        .font(.headline)
                        .foregroundColor(.secondary)

                    Picker("Shape", selection: $osc2Shape) {
                        Text("Saw").tag(0.0)
                        Text("Square").tag(1.0)
                        Text("Triangle").tag(2.0)
                        Text("Sine").tag(3.0)
                        Text("Pulse").tag(4.0)
                    }
                    .pickerStyle(.menu)

                    HStack {
                        VStack(alignment: .leading) {
                            Text("Warp")
                                .font(.caption)
                            Slider(value: $osc2Warp, in: -1...1)
                        }

                        VStack(alignment: .leading) {
                            Text("Level")
                                .font(.caption)
                            Slider(value: $osc2Level, in: 0...1)
                        }
                    }

                    HStack {
                        VStack(alignment: .leading) {
                            Text("Detune")
                                .font(.caption)
                            Slider(value: $osc2Detune, in: -100...100)
                        }
                    }
                }
                .padding()
                .background(Color.gray.opacity(0.1))
                .cornerRadius(10)

                // Sub & Noise Section
                VStack(alignment: .leading, spacing: 10) {
                    Text("SUB & NOISE")
                        .font(.headline)
                        .foregroundColor(.secondary)

                    Toggle("Sub Oscillator", isOn: $subEnabled)

                    VStack(alignment: .leading) {
                        Text("Sub Level")
                            .font(.caption)
                        Slider(value: $subLevel, in: 0...1)
                    }

                    VStack(alignment: .leading) {
                        Text("Noise Level")
                            .font(.caption)
                        Slider(value: $noiseLevel, in: 0...1)
                    }
                }
                .padding()
                .background(Color.gray.opacity(0.1))
                .cornerRadius(10)
            }
            .padding()
        }
    }
}

// MARK: - Filter & Envelope View

struct FilterEnvelopeView: View {
    @State private var filterType: Double = 0
    @State private var cutoff: Double = 0.5
    @State private var resonance: Double = 0.5

    @State private var filterAttack: Double = 0.01
    @State private var filterDecay: Double = 0.1
    @State private var filterSustain: Double = 0.5
    @State private var filterRelease: Double = 0.2
    @State private var filterAmount: Double = 0

    @State private var ampAttack: Double = 0.005
    @State private var ampDecay: Double = 0.1
    @State private var ampSustain: Double = 0.6
    @State private var ampRelease: Double = 0.2

    var body: some View {
        ScrollView {
            VStack(spacing: 20) {
                // Filter Section
                VStack(alignment: .leading, spacing: 10) {
                    Text("FILTER")
                        .font(.headline)
                        .foregroundColor(.secondary)

                    Picker("Type", selection: $filterType) {
                        Text("Lowpass").tag(0.0)
                        Text("Highpass").tag(1.0)
                        Text("Bandpass").tag(2.0)
                        Text("Notch").tag(3.0)
                    }
                    .pickerStyle(.menu)

                    VStack(alignment: .leading) {
                        Text("Cutoff")
                            .font(.caption)
                        Slider(value: $cutoff, in: 0...1)
                    }

                    VStack(alignment: .leading) {
                        Text("Resonance")
                            .font(.caption)
                        Slider(value: $resonance, in: 0...1)
                    }
                }
                .padding()
                .background(Color.gray.opacity(0.1))
                .cornerRadius(10)

                // Filter Envelope Section
                VStack(alignment: .leading, spacing: 10) {
                    Text("FILTER ENVELOPE")
                        .font(.headline)
                        .foregroundColor(.secondary)

                    HStack(spacing: 15) {
                        VStack(alignment: .leading) {
                            Text("A")
                                .font(.caption)
                            Text("\(filterAttack, specifier: "%.2f")s")
                                .font(.caption2)
                            Slider(value: $filterAttack, in: 0...10)
                        }

                        VStack(alignment: .leading) {
                            Text("D")
                                .font(.caption)
                            Text("\(filterDecay, specifier: "%.2f")s")
                                .font(.caption2)
                            Slider(value: $filterDecay, in: 0...10)
                        }

                        VStack(alignment: .leading) {
                            Text("S")
                                .font(.caption)
                            Text("\(filterSustain * 100, specifier: "%.0f")%")
                                .font(.caption2)
                            Slider(value: $filterSustain, in: 0...1)
                        }

                        VStack(alignment: .leading) {
                            Text("R")
                                .font(.caption)
                            Text("\(filterRelease, specifier: "%.2f")s")
                                .font(.caption2)
                            Slider(value: $filterRelease, in: 0...10)
                        }
                    }
                    .font(.caption)

                    VStack(alignment: .leading) {
                        Text("Amount")
                            .font(.caption)
                        Slider(value: $filterAmount, in: -1...1)
                    }
                }
                .padding()
                .background(Color.gray.opacity(0.1))
                .cornerRadius(10)

                // Amp Envelope Section
                VStack(alignment: .leading, spacing: 10) {
                    Text("AMPLIFIER ENVELOPE")
                        .font(.headline)
                        .foregroundColor(.secondary)

                    HStack(spacing: 15) {
                        VStack(alignment: .leading) {
                            Text("A")
                                .font(.caption)
                            Text("\(ampAttack, specifier: "%.2f")s")
                                .font(.caption2)
                            Slider(value: $ampAttack, in: 0...10)
                        }

                        VStack(alignment: .leading) {
                            Text("D")
                                .font(.caption)
                            Text("\(ampDecay, specifier: "%.2f")s")
                                .font(.caption2)
                            Slider(value: $ampDecay, in: 0...10)
                        }

                        VStack(alignment: .leading) {
                            Text("S")
                                .font(.caption)
                            Text("\(ampSustain * 100, specifier: "%.0f")%")
                                .font(.caption2)
                            Slider(value: $ampSustain, in: 0...1)
                        }

                        VStack(alignment: .leading) {
                            Text("R")
                                .font(.caption)
                            Text("\(ampRelease, specifier: "%.2f")s")
                                .font(.caption2)
                            Slider(value: $ampRelease, in: 0...10)
                        }
                    }
                    .font(.caption)
                }
                .padding()
                .background(Color.gray.opacity(0.1))
                .cornerRadius(10)
            }
            .padding()
        }
    }
}

// MARK: - Modulation View

struct ModulationView: View {
    @State private var lfo1Rate: Double = 5
    @State private var lfo1Depth: Double = 0.5
    @State private var lfo1Waveform: Double = 0

    @State private var lfo2Rate: Double = 3
    @State private var lfo2Depth: Double = 0.5
    @State private var lfo2Waveform: Double = 0

    var body: some View {
        ScrollView {
            VStack(spacing: 20) {
                // LFO1 Section
                VStack(alignment: .leading, spacing: 10) {
                    Text("LFO 1")
                        .font(.headline)
                        .foregroundColor(.secondary)

                    Picker("Waveform", selection: $lfo1Waveform) {
                        Text("Sine").tag(0.0)
                        Text("Triangle").tag(1.0)
                        Text("Saw").tag(2.0)
                        Text("Square").tag(3.0)
                        Text("S&H").tag(4.0)
                    }
                    .pickerStyle(.menu)

                    VStack(alignment: .leading) {
                        Text("Rate (Hz)")
                            .font(.caption)
                        Slider(value: $lfo1Rate, in: 0.1...20)
                    }

                    VStack(alignment: .leading) {
                        Text("Depth")
                            .font(.caption)
                        Slider(value: $lfo1Depth, in: 0...1)
                    }
                }
                .padding()
                .background(Color.gray.opacity(0.1))
                .cornerRadius(10)

                // LFO2 Section
                VStack(alignment: .leading, spacing: 10) {
                    Text("LFO 2")
                        .font(.headline)
                        .foregroundColor(.secondary)

                    Picker("Waveform", selection: $lfo2Waveform) {
                        Text("Sine").tag(0.0)
                        Text("Triangle").tag(1.0)
                        Text("Saw").tag(2.0)
                        Text("Square").tag(3.0)
                        Text("S&H").tag(4.0)
                    }
                    .pickerStyle(.menu)

                    VStack(alignment: .leading) {
                        Text("Rate (Hz)")
                            .font(.caption)
                        Slider(value: $lfo2Rate, in: 0.1...20)
                    }

                    VStack(alignment: .leading) {
                        Text("Depth")
                            .font(.caption)
                        Slider(value: $lfo2Depth, in: 0...1)
                    }
                }
                .padding()
                .background(Color.gray.opacity(0.1))
                .cornerRadius(10)

                // Modulation Matrix
                VStack(alignment: .leading, spacing: 10) {
                    Text("MODULATION MATRIX")
                        .font(.headline)
                        .foregroundColor(.secondary)

                    Text("Modulation routing configured via host automation")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                .padding()
                .background(Color.gray.opacity(0.1))
                .cornerRadius(10)
            }
            .padding()
        }
    }
}

// MARK: - Macros View

struct MacrosView: View {
    @State private var macro1: Double = 0.5
    @State private var macro2: Double = 0.5
    @State private var macro3: Double = 0.5
    @State private var macro4: Double = 0.5
    @State private var macro5: Double = 0.5
    @State private var macro6: Double = 0.5
    @State private var macro7: Double = 0.5
    @State private var macro8: Double = 0.5

    var body: some View {
        ScrollView {
            VStack(spacing: 15) {
                Text("MACRO CONTROLS")
                    .font(.headline)
                    .foregroundColor(.secondary)

                LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 15) {
                    MacroControl(name: "Macro 1", value: $macro1)
                    MacroControl(name: "Macro 2", value: $macro2)
                    MacroControl(name: "Macro 3", value: $macro3)
                    MacroControl(name: "Macro 4", value: $macro4)
                    MacroControl(name: "Macro 5", value: $macro5)
                    MacroControl(name: "Macro 6", value: $macro6)
                    MacroControl(name: "Macro 7", value: $macro7)
                    MacroControl(name: "Macro 8", value: $macro8)
                }
            }
            .padding()
        }
    }
}

struct MacroControl: View {
    let name: String
    @Binding var value: Double

    var body: some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(name)
                .font(.caption)
                .foregroundColor(.secondary)

            ZStack(alignment: .leading) {
                RoundedRectangle(cornerRadius: 8)
                    .fill(Color.gray.opacity(0.2))
                    .frame(height: 80)

                RoundedRectangle(cornerRadius: 8)
                    .fill(Color.accentColor)
                    .frame(width: CGFloat(value) * 60, height: 80)
            }
            .cornerRadius(8)

            Text("\(value * 100, specifier: "%.0f")%")
                .font(.caption2)
                .frame(maxWidth: .infinity, alignment: .trailing)
        }
        .padding()
        .background(Color.gray.opacity(0.1))
        .cornerRadius(10)
        .gesture(
            DragGesture(minimumDistance: 0)
                .onChanged { gesture in
                    let newValue = Double(gesture.location.x / 60.0)
                    value = max(0, min(1, newValue))
                }
        )
    }
}

// MARK: - Global View

struct GlobalView: View {
    @State private var polyMode: Double = 0
    @State private var glideEnabled: Bool = false
    @State private var glideTime: Double = 0.1
    @State private var masterTune: Double = 0
    @State private var masterVolume: Double = 3.0

    var body: some View {
        ScrollView {
            VStack(spacing: 20) {
                // Polyphony Section
                VStack(alignment: .leading, spacing: 10) {
                    Text("POLYPHONY")
                        .font(.headline)
                        .foregroundColor(.secondary)

                    Picker("Mode", selection: $polyMode) {
                        Text("Polyphonic").tag(0.0)
                        Text("Monophonic").tag(1.0)
                        Text("Legato").tag(2.0)
                    }
                    .pickerStyle(.menu)

                    Toggle("Glide Enabled", isOn: $glideEnabled)

                    if glideEnabled {
                        VStack(alignment: .leading) {
                            Text("Glide Time")
                                .font(.caption)
                            Slider(value: $glideTime, in: 0...1)
                        }
                    }
                }
                .padding()
                .background(Color.gray.opacity(0.1))
                .cornerRadius(10)

                // Master Section
                VStack(alignment: .leading, spacing: 10) {
                    Text("MASTER")
                        .font(.headline)
                        .foregroundColor(.secondary)

                    VStack(alignment: .leading) {
                        Text("Master Tune (cents)")
                            .font(.caption)
                        Slider(value: $masterTune, in: -100...100)
                    }

                    VStack(alignment: .leading) {
                        Text("Master Volume")
                            .font(.caption)
                        Slider(value: $masterVolume, in: 0...6)
                    }
                }
                .padding()
                .background(Color.gray.opacity(0.1))
                .cornerRadius(10)
            }
            .padding()
        }
    }
}
