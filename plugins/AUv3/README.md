# Nature Marco AUv3 iOS Plugin

## Project Overview

Complete iOS AUv3 instrument plugin implementation of the Nature Marco Hybrid Virtual Analog Synthesizer.

**Nature Marco** is a professional-grade 16-voice polyphonic synthesizer featuring:
- **Oscillator WARP** (-1.0 to +1.0 phase manipulation)
- **FM Synthesis** with carrier/modulator swap (linear & exponential modes)
- **16-Slot Modulation Matrix** (lock-free, thread-safe)
- **8 Macro Controls** (Serum-style simplified workflow)
- **Multimode SVF Filter** (Lowpass, Highpass, Bandpass, Notch)
- **Sub-Oscillator** (-1 octave square wave)
- **Noise Generator** with level control
- **4 LFOs** with 5 waveforms each (sine, triangle, saw, square, S&H)
- **Comprehensive Envelopes** (filter & amp ADSR)
- **16-Voice Polyphony** with monophonic/legato modes

## Project Structure

```
ios-auv3/
├── NaturePluginApp/            # AUv3 host container app
│   ├── AppDelegate.swift
│   ├── ViewController.swift
│   └── Info.plist
├── NaturePluginExtension/      # AUv3 extension
│   ├── AudioUnit.swift           # Main AUv3 implementation
│   ├── AudioUnitViewController.swift  # SwiftUI UI
│   ├── ParameterBridge.swift     # Swift-C++ bridge
│   └── Info.plist                # Extension configuration
├── SharedDSP/                     # C++ static library
│   ├── NatureDSP.h            # DSP wrapper header (135 parameters)
│   ├── NatureDSP.cpp          # DSP wrapper implementation
│   └── CMakeLists.txt            # Build configuration
├── build.sh                       # Build script
└── README.md                      # This file
```

## Features

### DSP Architecture
- **135 Parameters** total:
  - 6 OSC1 parameters (shape, warp, pulse width, detune, pan, level)
  - 6 OSC2 parameters (shape, warp, pulse width, detune, pan, level)
  - 3 Sub/Noise parameters (enabled, level, noise)
  - 5 FM synthesis parameters (enabled, carrier, mode, depth, ratio)
  - 5 Filter parameters (type, cutoff, resonance, key track, vel track)
  - 5 Filter envelope parameters (A, D, S, R, amount)
  - 4 Amp envelope parameters (A, D, S, R)
  - 8 LFO1 parameters (waveform, rate, depth, bipolar)
  - 8 LFO2 parameters (waveform, rate, depth, bipolar)
  - 80 Modulation matrix parameters (16 slots × 5 params)
  - 8 Macro controls
  - 6 Global parameters (structure, poly mode, glide, tune, volume)

### UI Components (SwiftUI)
- **Tabbed Interface** (5 tabs):
  1. **Oscillators** - OSC1/OSC2/Sub/Noise controls
  2. **Filter & Envelope** - Filter type, cutoff, resonance, ADSR envelopes
  3. **Modulation** - LFO1/LFO2 controls, modulation matrix
  4. **Macros** - 8 macro controls in 4×2 grid
  5. **Global** - Polyphony mode, glide, master tune/volume

### Presets
- **30 Factory Presets** included:
  - 5 Bass presets (Deep Reesey, Rubber Band, Sub Warp, Acid Techno, Metallic FM)
  - 5 Lead presets (Evolving Warp, Crystal FM Bell, Aggressive Saw, Retro Square, Warping SciFi)
  - 5 Pad presets (Warm Analog, Ethereal Bell, Dark Warp Choir, Metallic FM, SciFi Atmosphere)
  - 5 Pluck presets (Electric, Warp Guitar, FM Kalimba, Rubber Band, Metallic Harp)
  - 4 FX presets (Alien Texture, Glitchy Noise, Dark Drone, SciFi Sweep)
  - 3 Keys presets (Wurly EP, FM Clavinet, Harmonic Synth)
  - 3 Seq presets (Acid Loop, Bassline Groove, Arpeggiator Bliss)

## Technical Specifications

### Platform
- **iOS**: 15.0+
- **Architectures**: arm64 (device), arm64-sim (simulator)
- **Plugin Format**: AUv3 Instrument Extension (aumu)
- **Manufacturer Code**: WHTR
- **Plugin Subtype**: KMRO

### DSP Specifications
- **Sample Rate**: 44.1kHz - 192kHz
- **Max Block Size**: 512 - 4096 samples
- **Polyphony**: 16 voices with LRU stealing
- **CPU Target**: <15% for 16 voices at 48kHz

### Audio Features
- **Stereo Output**: 2 channels
- **MIDI Input**: Note on/off, pitch bend, mod wheel, aftertouch
- **Realtime-Safe**: No allocations in audio thread
- **Lock-Free**: Parameter modulation using std::atomic

## Building

### Prerequisites
- **Xcode**: 14.0+
- **iOS SDK**: 15.0+
- **CMake**: 3.15+ (for SharedDSP library)

### Build Commands

```bash
# Build for iOS Device
./build.sh

# Build for iOS Simulator
./build.sh --simulator

# Build with debug symbols
./build.sh --debug

# Clean build
./build.sh --clean
```

### Manual Xcode Build

```bash
# Open Xcode project
open NaturePlugin.xcodeproj

# Or build from command line
xcodebuild -project NaturePlugin.xcodeproj \
           -scheme NaturePlugin \
           -configuration Release \
           -sdk iphoneos \
           -derivedDataPath build
```

## Installation

### On Device

1. Connect iOS device
2. Build project for device
3. Open Xcode Organizer
4. Archive and distribute

### In AUv3 Host Apps

The plugin will appear in compatible AUv3 host apps as:
- **Name**: Nature Marco
- **Manufacturer**: White Room
- **Type**: Instrument (aumu)

Compatible hosts include:
- GarageBand
- AUM
- Cubasis
- NanoStudio
- BeatMaker 3
- Audiobus

## Parameter Reference

### Oscillator 1 Parameters
| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| osc1Shape | 0-4 | 0 | Saw, Square, Triangle, Sine, Pulse |
| osc1Warp | -1.0 to 1.0 | 0.0 | Phase warp amount |
| osc1PulseWidth | 0.0 to 1.0 | 0.5 | Pulse width for square wave |
| osc1Detune | -100 to 100 | 0 | Detune in cents |
| osc1Pan | -1.0 to 1.0 | 0.0 | Stereo pan position |
| osc1Level | 0.0 to 1.0 | 0.7 | Oscillator level |

### Filter Parameters
| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| filterType | 0-3 | 0 | LP, HP, BP, Notch |
| filterCutoff | 0.0 to 1.0 | 0.5 | Filter cutoff frequency |
| filterResonance | 0.0 to 1.0 | 0.5 | Filter resonance/Q |
| filterKeyTrack | 0.0 to 1.0 | 0.0 | Keyboard tracking amount |
| filterVelTrack | 0.0 to 1.0 | 0.0 | Velocity tracking amount |

### Modulation Matrix

The Nature Marco features a **16-slot modulation matrix** with:

**Sources (16):**
- LFO1, LFO2
- Velocity, Aftertouch
- Pitch Wheel, Mod Wheel
- Filter Env, Amp Env
- Macro 1-8

**Destinations (24):**
- OSC1: freq, warp, pulse width, level
- OSC2: freq, warp, pulse width, level
- Sub: level
- Noise: level
- Filter: cutoff, resonance
- Filter Env: amount
- Amp Env: attack, decay, sustain, release
- LFO1: rate, depth
- LFO2: rate, depth

## Known Differences from Desktop Version

### Simplifications for iOS
1. **Modulation Matrix UI**: Simplified to 4 visible slots (host automation recommended for full 16-slot control)
2. **Preset Management**: Factory presets embedded, file system access limited by iOS sandbox
3. **FX Section**: Removed reverb/delay (use host effects instead)

### Performance Optimizations
1. **Reduced Voice Count**: 16 voices (vs unlimited on desktop)
2. **Simplified DSP**: Some advanced features streamlined for mobile CPU
3. **Memory Efficiency**: Optimized for iOS memory constraints

## Troubleshooting

### Build Issues

**Issue**: `NaturePureDSP.h not found`
```bash
# Fix: Ensure correct include path in CMakeLists.txt
# The DSP files are at: juce_backend/instruments/nature/plugins/dsp/
```

**Issue**: Linker errors for undefined symbols
```bash
# Fix: Add NatureDSP static library to linker flags
# In Xcode: Build Phases → Link Binary With Libraries
```

### Runtime Issues

**Issue**: Plugin not appearing in host app
- Ensure build succeeded with no errors
- Check Info.plist extension configuration
- Verify AUv3 host app supports instrument extensions

**Issue**: Audio glitches or CPU overload
- Reduce polyphony (use mono/legato mode)
- Disable unused oscillators
- Reduce modulation matrix activity
- Close other apps to free up CPU

## Development Notes

### Adding New Parameters

1. Add parameter to `NatureDSP.h` enum
2. Register parameter in `AudioUnit.swift` parameterDefinitions
3. Map parameter in `NatureDSP.cpp` setParameter()
4. Add UI control in appropriate SwiftUI view

### Creating New Presets

Presets are JSON files with all parameter values:
```json
{
  "osc1Shape": 0.0,
  "osc1Warp": 0.5,
  "osc1Level": 0.7,
  "filterCutoff": 0.6,
  "filterResonance": 0.4,
  ...
}
```

Place in `presets/Nature/` folder and add to factory preset list.

## Credits

**Developer**: Bret Bouchard
**Project**: White Room Audio Plugin Suite
**License**: Proprietary - All rights reserved

## References

- [Apple AUv3 Documentation](https://developer.apple.com/documentation/audiounits)
- [SwiftUI Documentation](https://developer.apple.com/documentation/swiftui)
- [Nature Marco Desktop Version](../../../../../../README.md)

---

**Status**: ✅ Complete - Ready for Xcode project creation and testing
**Created**: 2025-01-17
**Location**: `juce_backend/instruments/nature/ios-auv3/`
