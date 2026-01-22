# Nature Marco Preset Parameter Reference

Quick reference for all parameters used in Nature Marco factory presets.

## Oscillator Parameters

### OSC1 (Main Oscillator)

| Parameter ID | Range | Description |
|-------------|-------|-------------|
| `osc1_shape` | 0-4 | Waveform: 0=Saw, 1=Square, 2=Tri, 3=Sine, 4=Pulse |
| `osc1_warp` | -1.0 to 1.0 | Phase warp amount (-1=dark, +1=bright) |
| `osc1_pulse_width` | 0-1 | Pulse width (for pulse waveform) |
| `osc1_detune` | -100 to 100 | Detune in cents |
| `osc1_pan` | -1 to 1 | Stereo pan (-1=left, +1=right) |
| `osc1_level` | 0-1 | Oscillator mix level |

### OSC2 (Secondary Oscillator)

| Parameter ID | Range | Description |
|-------------|-------|-------------|
| `osc2_shape` | 0-4 | Waveform: 0=Saw, 1=Square, 2=Tri, 3=Sine, 4=Pulse |
| `osc2_warp` | -1.0 to 1.0 | Phase warp amount |
| `osc2_pulse_width` | 0-1 | Pulse width |
| `osc2_detune` | -100 to 100 | Detune in cents |
| `osc2_pan` | -1 to 1 | Stereo pan |
| `osc2_level` | 0-1 | Oscillator mix level |

### Sub-Oscillator

| Parameter ID | Range | Description |
|-------------|-------|-------------|
| `sub_enabled` | 0-1 | Enable sub-oscillator (-1 octave square) |
| `sub_level` | 0-1 | Sub-oscillator mix level |

### Noise Generator

| Parameter ID | Range | Description |
|-------------|-------|-------------|
| `noise_level` | 0-1 | White noise mix level |

## FM Synthesis Parameters

| Parameter ID | Range | Description |
|-------------|-------|-------------|
| `fm_enabled` | 0-1 | Enable FM synthesis |
| `fm_carrier_osc` | 0-1 | Carrier oscillator: 0=OSC1, 1=OSC2 |
| `fm_mode` | 0-1 | FM mode: 0=Linear, 1=Exponential |
| `fm_depth` | 0-1 | FM modulation amount |
| `fm_modulator_ratio` | 0.5-10 | Modulator frequency ratio |

## Filter Parameters

### Main Filter

| Parameter ID | Range | Description |
|-------------|-------|-------------|
| `filter_type` | 0-3 | Type: 0=LP, 1=HP, 2=BP, 3=Notch |
| `filter_cutoff` | 0-1 | Normalized cutoff frequency (20Hz-20kHz) |
| `filter_resonance` | 0-1 | Filter resonance/Q |
| `filter_key_track` | 0-1 | Keyboard tracking amount |
| `filter_vel_track` | 0-1 | Velocity tracking amount |

### Filter Envelope

| Parameter ID | Range | Description |
|-------------|-------|-------------|
| `filter_env_attack` | 0-1 | Attack time (0-1s) |
| `filter_env_decay` | 0-2 | Decay time (0-2s) |
| `filter_env_sustain` | 0-1 | Sustain level |
| `filter_env_release` | 0-2 | Release time (0-2s) |
| `filter_env_amount` | -1 to 1 | Envelope modulation depth |

## Amplifier Envelope

| Parameter ID | Range | Description |
|-------------|-------|-------------|
| `amp_env_attack` | 0-1 | Attack time (0-1s) |
| `amp_env_decay` | 0-2 | Decay time (0-2s) |
| `amp_env_sustain` | 0-1 | Sustain level |
| `amp_env_release` | 0-3 | Release time (0-3s) |

## LFO Parameters

### LFO1

| Parameter ID | Range | Description |
|-------------|-------|-------------|
| `lfo1_waveform` | 0-4 | Waveform: 0=Sine, 1=Tri, 2=Saw, 3=Square, 4=S&H |
| `lfo1_rate` | 0.1-20 | LFO frequency in Hz |
| `lfo1_depth` | 0-1 | Modulation depth |
| `lfo1_bipolar` | 0-1 | 0=Unipolar, 1=Bipolar |

### LFO2

| Parameter ID | Range | Description |
|-------------|-------|-------------|
| `lfo2_waveform` | 0-4 | Waveform: 0=Sine, 1=Tri, 2=Saw, 3=Square, 4=S&H |
| `lfo2_rate` | 0.1-20 | LFO frequency in Hz |
| `lfo2_depth` | 0-1 | Modulation depth |
| `lfo2_bipolar` | 0-1 | 0=Unipolar, 1=Bipolar |

## Modulation Matrix Parameters

For each slot (0-15):

| Parameter ID | Range | Description |
|-------------|-------|-------------|
| `mod_N_source` | 0-17 | Source: 0-3=LFO1-4, 4-5=ENV1-2, 6-13=Macro1-8, 14=Velocity, 15=Aftertouch, 16=Modwheel |
| `mod_N_destination` | 0-19 | Destination: 0-4=OSC1 params, 5-9=OSC2 params, 10=Sub, 11-12=Filter, 13-15=FM, 16-17=LFO1, 18-19=LFO2 |
| `mod_N_amount` | -1 to 1 | Modulation depth |
| `mod_N_bipolar` | 0-1 | 0=Unipolar, 1=Bipolar |
| `mod_N_curve` | 0-1 | 0=Linear, 1=Exponential |

## Macro Parameters

For each macro (0-7):

| Parameter ID | Range | Description |
|-------------|-------|-------------|
| `macro_N_value` | 0-1 | Macro control value |

## Global Parameters

| Parameter ID | Range | Description |
|-------------|-------|-------------|
| `poly_mode` | 0-2 | Polyphony: 0=Poly, 1=Mono, 2=Legato |
| `glide_enabled` | 0-1 | Enable portamento |
| `glide_time` | 0-1 | Glide time in seconds |
| `master_tune` | -12 to 12 | Master tune in semitones |
| `master_volume` | 0-1 | Master output level |

## Modulation Source IDs

| ID | Source | Description |
|----|--------|-------------|
| 0 | LFO1 | Low-frequency oscillator 1 |
| 1 | LFO2 | Low-frequency oscillator 2 |
| 2 | LFO3 | Low-frequency oscillator 3 |
| 3 | LFO4 | Low-frequency oscillator 4 |
| 4 | ENV1 | Envelope 1 (filter) |
| 5 | ENV2 | Envelope 2 (amp) |
| 6 | Macro1 | Macro control 1 |
| 7 | Macro2 | Macro control 2 |
| 8 | Macro3 | Macro control 3 |
| 9 | Macro4 | Macro control 4 |
| 10 | Macro5 | Macro control 5 |
| 11 | Macro6 | Macro control 6 |
| 12 | Macro7 | Macro control 7 |
| 13 | Macro8 | Macro control 8 |
| 14 | Velocity | Note velocity |
| 15 | Aftertouch | Channel aftertouch |
| 16 | ModWheel | Modulation wheel |
| 17 | PitchBend | Pitch bend wheel |

## Modulation Destination IDs

| ID | Destination | Description |
|----|-------------|-------------|
| 0 | OSC1_FREQ | Oscillator 1 frequency |
| 1 | OSC1_PULSE_WIDTH | Oscillator 1 pulse width |
| 2 | OSC1_WARP | Oscillator 1 warp amount |
| 3 | OSC1_LEVEL | Oscillator 1 level |
| 4 | OSC1_PAN | Oscillator 1 pan |
| 5 | OSC2_FREQ | Oscillator 2 frequency |
| 6 | OSC2_PULSE_WIDTH | Oscillator 2 pulse width |
| 7 | OSC2_WARP | Oscillator 2 warp amount |
| 8 | OSC2_LEVEL | Oscillator 2 level |
| 9 | OSC2_PAN | Oscillator 2 pan |
| 10 | SUB_LEVEL | Sub-oscillator level |
| 11 | FILTER_CUTOFF | Filter cutoff frequency |
| 12 | FILTER_RESONANCE | Filter resonance |
| 13 | FM_DEPTH | FM modulation depth |
| 14 | FM_RATIO | FM modulator ratio |
| 15 | FM_ENV_AMOUNT | FM envelope amount |
| 16 | LFO1_RATE | LFO 1 rate |
| 17 | LFO1_DEPTH | LFO 1 depth |
| 18 | LFO2_RATE | LFO 2 rate |
| 19 | LFO2_DEPTH | LFO 2 depth |

## Common Preset Patterns

### Bass Presets
- High filter resonance (0.6-0.9)
- Fast envelope attack (0.001-0.01)
- Short-medium decay (0.2-0.5)
- Sub-oscillator enabled
- Monophonic or poly mode

### Lead Presets
- Medium filter resonance (0.4-0.7)
- Fast attack (0.001-0.1)
- Medium sustain (0.5-0.8)
- Velocity sensitivity
- Aftertouch control

### Pad Presets
- Low filter resonance (0.1-0.4)
- Slow attack (0.5-2.0)
- High sustain (0.7-0.9)
- Long release (1.5-3.0)
- Slow LFO modulation

### Pluck Presets
- High filter resonance (0.6-0.8)
- Fast attack (0.001)
- Short decay (0.15-0.6)
- Low sustain (0.1-0.3)
- Velocity → envelope

### FX Presets
- Extreme warp values
- Complex modulation
- Long envelopes
- Experimental LFO settings
- Unusual waveform combinations

### Keys Presets
- Medium resonance (0.2-0.5)
- Fast attack (0.001-0.05)
- Medium decay (0.4-0.6)
- Medium sustain (0.3-0.5)
- Velocity sensitivity

### Seq Presets
- Fast LFO rates (4-8 Hz)
- High modulation amounts
- Monophonic mode
- Glide enabled
- Short envelopes

---

For full preset descriptions and usage tips, see `README.md`.
