# Nature Instrument

**Part of the White Room Instruments Collection**

Nature is a professional JUCE audio instrument plugin for macOS and iOS.

## Overview

This repository contains the complete implementation of the Nature instrument, including:

- **DSP Implementation**: Pure C++ audio synthesis code
- **JUCE Plugin**: AUv3, VST, and standalone formats
- **Presets**: Factory presets in UPFS v1.0 JSON format
- **Rendered Audio**: 48kHz, 24-bit stereo preview files
- **Tests**: Unit tests and integration tests
- **Documentation**: Complete technical documentation

## Quick Start

### Building from Source

```bash
# Clone repository
git clone git@github.com:bretbouchard/nature-instrument.git
cd nature-instrument

# Build with CMake
mkdir build && cd build
cmake ..
make -j8
```

### Using in White Room App

The instrument is automatically integrated into the White Room app through the instruments subsystem.

### Loading Presets

Presets are located in `presets/factory/` and follow the UPFS v1.0 specification:

```json
{
  "version": "1.0.0",
  "format": "UPFS",
  "name": "Preset Name",
  "instrument": "nature",
  "parameters": { ... }
}
```

## Project Structure

```
nature/
├── src/                  # DSP implementation source
├── include/              # Public headers
├── presets/              # UPFS JSON presets
├── presets/factory/      # Factory presets
├── plugins/              # JUCE plugin configurations
│   ├── AUv3/            # AUv3 plugin (macOS/iOS)
│   └── dsp/             # Pure DSP implementation
├── docs/                 # Documentation
├── tests/                # Unit tests
├── rendered/             # Rendered WAV preview files
├── CMakeLists.txt        # Build configuration
└── README.md             # This file
```

## Instrument Details

# Nature

Nature instrument for White Room.

## Structure

- `src/` - DSP implementation source code
- `include/` - Public headers
- `presets/` - UPFS JSON presets
- `presets/factory/` - Factory presets
- `plugins/AUv3/` - AUv3 plugin (macOS/iOS)
- `plugins/dsp/` - Pure DSP implementation
- `docs/` - Documentation
- `tests/` - Unit tests

## Presets

All presets use UPFS (Universal Preset Format Specification) v1.0 JSON format.

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## License

Copyright © 2025 Schillinger Ecosystem. All rights reserved.


## Preset Count

**Total Presets**: 0

## Development

### Dependencies

- JUCE 7.0+
- CMake 3.26+
- Xcode 14+ (macOS)
- iOS SDK 16+ (iOS)

### Testing

```bash
# Run unit tests
cd tests
./run_tests.sh
```

### Rendering Presets

Presets can be rendered to WAV files using the preset renderer:

```bash
cd /path/to/white_room/tools/preset_renderer
./render_instrument.sh nature
```

## Repository Information

- **Repository**: https://github.com/bretbouchard/nature-instrument
- **Part of**: White Room Instruments Collection
- **License**: PROPRIETARY - All rights reserved
- **Author**: Bret Bouchard
- **Created**: 2026-01-21

## Related Repositories

- [white_room](https://github.com/bretbouchard/white_room) - Main White Room app
- [juce_backend](https://github.com/bretbouchard/juce_backend) - JUCE backend infrastructure
- [preset_renderer](https://github.com/bretbouchard/preset_renderer) - Preset rendering tool

---

**White Room Instruments** - Professional audio instruments for creative music production.
