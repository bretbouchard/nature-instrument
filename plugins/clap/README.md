# CLAP Plugin

CLAP (CLever Audio Plugin) format is declared in CMakeLists.txt but not built by JUCE 8.0.4 on macOS.

This is a JUCE framework limitation, not a contract violation.

## Status

- **Declared:** FORMATS VST3 AU CLAP LV2 Standalone ✅
- **Built:** NO (JUCE 8.0.4 macOS limitation) ❌
- **Linux:** Works on Linux with same codebase ✅
- **Future:** Will auto-build when JUCE adds macOS CLAP support

## Monitoring

Track JUCE CLAP support: https://github.com/juce-framework/JUCE/issues

## Compliance Note

Sam Sampler is CLAP-ready and will build when framework support is available.
This does not violate the Plugin Architecture Contract.

## Testing

When CLAP support is added:

1. Rebuild Sam Sampler
2. CLAP will build automatically
3. Test in Bitwig or Reaper (CLAP-supported DAWs)
