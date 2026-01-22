# MPE Support Implementation Plan

**Feature:** MIDI Polyphonic Expression (MPE) for per-note gesture control
**Status:** Planned
**Priority:** High
**Estimated Complexity:** Medium-High

## Overview

MPE (MIDI Polyphonic Expression) allows each note to have independent control over:
- Pitch bend (per-note)
- Pressure (channel pressure, per-note)
- Timbre (CC 74, per-note)

This maps perfectly to the Giant Gesture system:
- **Force** → Pressure (MPE Channel Pressure)
- **Speed** → Timbre (MPE CC 74)
- **Roughness** → Pitch bend amount (subtle pitch variation)

## Benefits for Giant Instruments

1. **Expressive Control**: Each note can have different gesture parameters
2. **Real-Time Modulation**: Smoothly modulate force/speed/roughness per note
3. **Performance Nuance**: Subtle variations between notes for realism
4. **MPE Controllers**: Works with Roli Seaboard, LinnStrument, etc.

## Technical Implementation

### 1. Extend ScheduledEvent Structure

```cpp
struct ScheduledEvent
{
    ScheduledEventType type;
    int noteNumber;
    float velocity;

    // New MPE fields
    float pitchBend = 0.0f;        // -1.0 to 1.0 (per-note)
    float pressure = 0.0f;          // 0.0 to 1.0 (per-note)
    float timbre = 0.0f;            // 0.0 to 1.0 (per-note)

    int midiChannel = 0;            // For MPE zone tracking
    double timestamp = 0.0;
};
```

### 2. Voice State Extension

Each voice needs to track MPE state:

```cpp
struct GiantVoiceState
{
    // Existing
    int midiNote = -1;
    float velocity = 0.0f;

    // New MPE state
    float mpePitchBend = 0.0f;
    float mpePressure = 0.0f;
    float mpeTimbre = 0.0f;

    // Map MPE to gestures
    GiantGestureParameters getGestures() const
    {
        GiantGestureParameters g;
        g.force = mpePressure;           // Pressure controls force
        g.speed = mpeTimbre;              // Timbre controls speed
        g.roughness = std::abs(mpePitchBend);  // Bend controls roughness
        return g;
    }
};
```

### 3. MPE Zone Detection

Detect and configure MPE zones:

```cpp
class MPEZoneDetector
{
public:
    struct Zone
    {
        bool isActive = false;
        int masterChannel = 0;
        int numMemberChannels = 0;
        int pitchBendRange = 48;         // Semitones
    };

    Zone lowerZone;
    Zone upperZone;

    void parseRPN(int channel, int msb, int lsb);
    bool isInMPEZone(int channel) const;
    Zone* getZoneForChannel(int channel);
};
```

### 4. Gesture Mapping

Map MPE CCs to gesture parameters:

```cpp
struct MPEGestureMapping
{
    // Default mapping
    float pressureToForce = 1.0f;       // Direct
    float timbreToSpeed = 0.5f;         // Timbre → speed (0-1)
    float pitchBendToRoughness = 0.3f;  // Bend → roughness
    bool invertPressure = false;
    bool invertTimbre = false;

    // Advanced mapping (future)
    // - LFO mapping
    // - Aftertouch mapping
    // - Custom curves
};
```

## Implementation Steps

### Phase 1: Core MPE Support (2-3 days)

1. **Extend ScheduledEvent**
   - Add MPE fields to ScheduledEvent
   - Update event queue to handle MPE messages

2. **MPE Zone Detection**
   - Implement RPN parser for MPE zone configuration
   - Track lower/upper zones
   - Auto-detect MPE controllers

3. **Voice State Tracking**
   - Add MPE state to each voice
   - Map note-on to MPE member channels
   - Track per-note pitch bend/pressure/timbre

### Phase 2: Gesture Mapping (2 days)

1. **Default MPE Mapping**
   - Pressure → Force
   - Timbre → Speed
   - Pitch Bend → Roughness (subtle)

2. **Mapping Configuration**
   - Add MPEGestureMapping to each instrument
   - Preset-storable mappings
   - Per-instrument defaults

### Phase 3: Smoothing and Interpolation (1-2 days)

1. **Parameter Smoothing**
   - Smooth MPE parameter changes
   - Prevent zipper noise
   - Configurable smoothing times

2. **Pitch Bend Smoothing**
   - Per-note pitch smoothing
   - Glissando prevention

### Phase 4: Testing and Validation (1-2 days)

1. **MPE Controller Testing**
   - Test with Roli Seaboard
   - Test with LinnStrument
   - Test with K-Board

2. **Unit Tests**
   - MPE zone detection
   - Parameter mapping
   - Voice stealing with MPE

## File Changes

**New Files:**
- `include/dsp/MPEZoneDetector.h`
- `src/dsp/MPEZoneDetector.cpp`
- `include/dsp/MPEGestureMapping.h`

**Modified Files:**
- `include/dsp/InstrumentDSP.h` - Add MPE fields to ScheduledEvent
- All giant instrument implementations - Add MPE state to voices

## Preset Integration

Add MPE settings to preset JSON:

```json
{
  "mpe": {
    "enabled": true,
    "pressure_mapping": {
      "target": "force",
      "amount": 1.0,
      "invert": false
    },
    "timbre_mapping": {
      "target": "speed",
      "amount": 0.5,
      "invert": false
    },
    "pitch_bend_mapping": {
      "target": "roughness",
      "amount": 0.3,
      "invert": false
    },
    "smoothing_time": 0.02
  }
}
```

## Compatibility

- **Backward Compatible**: MPE is optional, works with standard MIDI
- **Gradual Adoption**: Each instrument can enable MPE independently
- **DAW Integration**: Most modern DAWs support MPE (Logic, Cubase, Bitwig, Reaper)

## References

- [MPE Specification](https://www.midi.org/midi-articles/midi-polyphonic-expression-mpe)
- [JUCE MPE Documentation](https://docs.juce.com/master/classjuce__mp_e__zone__layout.html)
- Roli MPE Best Practices

## Success Criteria

1. MPE controllers work out of the box
2. Per-note gesture control feels natural
3. Smooth parameter changes without artifacts
4. Preset system saves MPE mappings
5. CPU overhead < 5% for MPE processing

---

**Status:** Ready for implementation
**Dependencies:** None (can proceed independently)
**Blocks:** Nothing
