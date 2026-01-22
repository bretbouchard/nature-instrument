# Microtonal Tuning Support Implementation Plan

**Feature:** Custom scales and tunings beyond 12-TET
**Status:** Planned
**Priority:** Medium
**Estimated Complexity:** Medium

## Overview

Microtonal support allows giant instruments to use:
- Non-standard scales (19-TET, 31-TET, just intonation, etc.)
- Custom Scala files (.scl)
- Historical temperaments (meantone, well temperament, etc.)
- Experimental scales (partials, spectral scales)

## Benefits for Giant Instruments

1. **Historical Authenticity**: Use authentic tunings for period instruments
2. **Cinematic Tension**: Microtonal dissonance for tension
3. **Harmonic Richness**: Just intonation for cleaner harmonies
4. **Experimental Sounds**: Novel scales for unique textures

## Technical Implementation

### 1. Tuning System Architecture

```cpp
enum class TuningSystem
{
    EqualTemperament,      // 12-TET, 19-TET, etc.
    JustIntonation,        // Pure ratios
    Meantone,              // Historical temperaments
    CustomScale,           // User-defined
    ScalaFile              // Loaded from .scl
};

struct MicrotonalTuning
{
    TuningSystem system = TuningSystem::EqualTemperament;
    int divisions = 12;              // For equal temperaments
    float rootFrequency = 440.0f;    // A4 = 440Hz
    int rootNote = 69;               // MIDI note for root

    // Interval list (for custom scales)
    std::vector<float> intervals;    // In ratio from root
    std::vector<std::string> intervalNames;

    // Scala file
    std::string scalaFilename;
    std::string scaleName;

    // Calculate frequency for MIDI note
    float midiToFrequency(int midiNote) const;
};
```

### 2. Scala File Parser

```cpp
class ScalaFileLoader
{
public:
    struct ScaleData
    {
        std::string name;
        std::string description;
        std::vector<float> intervals;  // In cents
    };

    // Parse .scl file format
    static ScaleData loadScalaFile(const std::string& path);

    // Convert cents to ratio
    static float centsToRatio(float cents);

    // Built-in scales
    static ScaleData getJustIntonationScale();
    static ScaleData getMeantoneScale();
    static ScaleData getPythagoreanScale();
    static ScaleData get19TETScale();
    static ScaleData get31TETScale();
    static ScaleData getPartialsScale();  // Harmonic series
};
```

### 3. Frequency Calculation

```cpp
float MicrotonalTuning::midiToFrequency(int midiNote) const
{
    float semitones = midiNote - rootNote;

    switch (system)
    {
        case TuningSystem::EqualTemperament:
        {
            // Equal temperament with custom divisions
            float octaveRatio = 2.0f;
            float stepRatio = std::pow(octaveRatio, 1.0f / divisions);
            float ratio = std::pow(stepRatio, semitones);
            return rootFrequency * ratio;
        }

        case TuningSystem::JustIntonation:
        case TuningSystem::CustomScale:
        case TuningSystem::ScalaFile:
        {
            // Map to intervals
            int octave = static_cast<int>(semitones / divisions);
            int step = static_cast<int>(semitones) % divisions;

            if (step >= 0 && step < static_cast<int>(intervals.size()))
            {
                float ratio = intervals[step];
                ratio *= std::pow(2.0f, octave);
                return rootFrequency * ratio;
            }
            return rootFrequency;
        }

        default:
            return 440.0f * std::pow(2.0f, semitones / 12.0f);
    }
}
```

### 4. Preset Integration

Add microtonal settings to preset JSON:

```json
{
  "tuning": {
    "enabled": false,
    "system": "equal_temperament",
    "divisions": 12,
    "root_frequency": 440.0,
    "root_note": 69,
    "scala_file": "",
    "intervals": [],
    "name": "12-TET"
  }
}
```

### 5. Built-in Scales Library

```cpp
namespace MicrotonalScales
{
    // Standard equal temperaments
    MicrotonalTuning get12TET();
    MicrotonalTuning get19TET();
    MicrotonalTuning get24TET();     // Quarter tones
    MicrotonalTuning get31TET();
    MicrotonalTuning get36TET();     // Third tones
    MicrotonalTuning get48TET();     // Eighth tones
    MicrotonalTuning get72TET();     // Sixth tones

    // Just intonation
    MicrotonalTuning getJustIntonation5Limit();
    MicrotonalTuning getJustIntonation7Limit();
    MicrotonalTuning getJustIntonationHarmonic();

    // Historical
    MicrotonalTuning getMeantoneQuarterComma();
    MicrotonalTuning getWerckmeisterIII();
    MicrotonalTuning getVallotti();
    MicrotonalTuning getYoungLambert();

    // Experimental
    MicrotonalTuning getPartials();        // Harmonic series
    MicrotonalTuning getSpectral();        // Spectral scale
    MicrotonalTuning getWilsonBohlenPierce();
}
```

## Implementation Steps

### Phase 1: Core Tuning System (2-3 days)

1. **MicrotonalTuning Class**
   - Implement all tuning systems
   - MIDI to frequency calculation
   - Preset save/load

2. **Scala File Parser**
   - Parse .scl file format
   - Error handling for invalid files
   - Built-in scale library

### Phase 2: Instrument Integration (1-2 days)

1. **Add Tuning to Each Instrument**
   - Add MicrotonalTuning member to each DSP class
   - Replace midiToFrequency() calls
   - Update parameter handling

2. **Scale Parameter**
   - Add "tuning" parameter to plugins
   - UI selector for tuning systems
   - Root frequency control

### Phase 3: Preset Browser Integration (1 day)

1. **Tuning Browser**
   - List built-in scales
   - Load .scl files
   - Preview scales

2. **Preset Compatibility**
   - Old presets default to 12-TET
   - Migration path for microtonal presets

### Phase 4: Testing (1 day)

1. **Unit Tests**
   - Frequency calculation accuracy
   - Scala file parsing
   - All built-in scales

2. **Integration Tests**
   - Each instrument with each tuning
   - Preset save/load
   - MPE + microtonal compatibility

## File Changes

**New Files:**
- `include/dsp/MicrotonalTuning.h`
- `src/dsp/MicrotonalTuning.cpp`
- `include/dsp/ScalaFileLoader.h`
- `src/dsp/ScalaFileLoader.cpp`

**Modified Files:**
- All giant instrument implementations - Add MicrotonalTuning member
- Preset save/load - Handle tuning parameters

## Scala File Format Example

```
! Example.scl
Example scale
12
!               cents
100.00000
200.00000
300.00000
400.00000
500.00000
600.00000
700.00000
800.00000
900.00000
1000.00000
1100.00000
2/1
```

## Common Scala Libraries

Include built-in access to:
- [Scala Archive](http://www.huygens-fokker.org/scala/) (4000+ scales)
- Just intonation scales
- Historical temperaments
- Experimental/xenharmonic scales

## CPU Impact

- **Negligible**: Frequency calculation is cached per note
- **No audio path changes**: Only frequency determination
- **Estimated overhead**: < 1% CPU

## Compatibility

- **Backward Compatible**: Default is 12-TET
- **MPE Compatible**: Microtonal + MPE works together
- **DAW Integration**: Tuning is internal, no DAW changes needed

## Use Cases for Giant Instruments

### Giant Strings
- Just intonation for cleaner harmonies
- Meantone for historical authenticity
- Quarter tones for glissando effects

### Giant Drums
- Just intonation for tuned tom arrays
- Microtonal gong/bell overtones

### Giant Voice
- Just intonation for chant harmonies
- Experimental scales for alien character

### Giant Horns
- Historical temperaments for authentic period sound
- Quarter tones for Arabic/Maqaam scales

### Giant Percussion
- Just intonation for harmonic series resonance
- Partial scales for harmonic percussion

## Success Criteria

1. All built-in scales work correctly
2. Scala files load from disk
3. Each instrument responds to microtonal tuning
4. Preset system saves/loads tuning
5. No audio artifacts when changing scales
6. Documentation for users

---

**Status:** Ready for implementation
**Dependencies:** None (can proceed independently)
**Blocks:** Nothing
