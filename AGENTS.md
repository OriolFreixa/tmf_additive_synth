# Agent Guide

This module implements additive-synthesis voice interceptors and harmonic collectors on top of `tmf_intercept_synth`. It also owns the FFT abstraction used for wavetable generation.

## Index-First Workflow
- Start with this index, then search for the exact collector, manager, FFT, or voice symbol before opening files.
- Prefer targeted commands such as `rg -n "VoiceInterceptorAdditiveSynth|collectHarmonics|markWavetable" AdditiveSynth tests`.
- Ignore `.github/`, `.git`, and generated build folders unless the task is specifically about CI, submodules, or generated output.
- For public API changes, check `tmf_additive_synth.h`, the relevant collector/manager header, and `tests/AdditiveSynthHarmonicCollectors.cpp` together.

Useful searches:
- `rg -n "class VoiceInterceptorAdditiveSynth|processBlock|collectHarmonics|readWaveTable" AdditiveSynth`
- `rg -n "HarmonicCollectorManager|getAudioParametersIds|getModTargetData|parameterChanged" AdditiveSynth tests`
- `rg -n "FourierTransform|transformRealInverse|TMF_ADDITIVE_SYNTH_HAS_IPP" FFT.h CMakeLists.txt tests`

## File Index
- `tmf_additive_synth.h`: public JUCE module header and umbrella include list for FFT, additive synth voice, collector managers, and example collectors.
- `tmf_additive_synth.cpp`, `tmf_additive_synth.mm`: JUCE module translation units. Usually no behavior changes live here.
- `CMakeLists.txt`: standalone and parent-project CMake integration, dependency on `tmf_intercept_synth`, optional Intel IPP configuration, test source discovery, and `tmf::additive_synth` alias.
- `FFT.h`: `FourierTransform` abstraction with IPP, Apple Accelerate, and portable fallback implementations.

Voice and managers:
- `AdditiveSynth/VoiceInterceptorAdditiveSynth.h`: generator interceptor that builds/refreshes wavetables, renders samples, handles pitch wheel movement, tracks collectors, and receives modulation target updates.
- `AdditiveSynth/VoiceInterceptorAdditiveSynthManager.h`: voice interceptor manager that owns collector managers, exposes parameter groups/IDs, forwards parameter changes, and wires collectors into active voices.
- `AdditiveSynth/AdditiveSynthHarmonicCollector.h`: base collector interface and shared parameter/modulation handling for level, order, and pan.
- `AdditiveSynth/AdditiveSynthHarmonicCollectorManager.h`: templated collector manager, parameter group creation, static ID helpers, mod target data, and per-voice collector storage.

Collector examples:
- `AdditiveSynth/CollectorExamples/HarmonicCollectorSine.h`: first-harmonic sine collector.
- `AdditiveSynth/CollectorExamples/HarmonicCollectorOctaves.h`: octave-based collector with low/high bounds and a specialized manager.
- `AdditiveSynth/CollectorExamples/HarmonicCollectorEnFifther.h`: fifth-based harmonic collector.

Tests:
- `tests/AdditiveSynthHarmonicCollectors.cpp`: collector small-table tolerance, FFT bounds/amplitude behavior, manager parameter/mod target exposure, parameter propagation, and additive voice rendering.

## Dependency Notes
- Depends on `tmf_intercept_synth` plus JUCE core/audio modules.
- Optional IPP support is disabled on Apple in CMake; Apple builds use Accelerate when available through `FFT.h`.
- Collector parameter IDs are also modulation target IDs. Treat ID changes as API changes because the modulation matrix depends on them.

## Build And Test
- From the repository root: `cmake -S . -B build`, `cmake --build build`, `ctest --test-dir build`.
- Standalone module builds are supported by `CMakeLists.txt`, but may fetch JUCE/Catch2 unless local dependency paths are provided.
- When changing FFT behavior, run/add assertions around `FourierTransform`; when changing collectors or voice rendering, update `tests/AdditiveSynthHarmonicCollectors.cpp`.
