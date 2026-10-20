# Abyssal Roadmap

## Done — v0.1 (October 2026)

### Build & infrastructure
- [x] JUCE 8.0.12 + CMake project, VST3 and Standalone targets
- [x] NeuralAmpModelerCore fetched and pinned to a known commit (built as an object library so its model-type registration survives linking)
- [x] `AbyssalRender` offline test tool: renders a synthesized DI riff through every factory preset, writes WAVs, and fails on NaNs or silence
- [x] DSP engine separated from the plugin wrapper (`Source/DSP/Engine`) so it can be tested headless
- [x] Verified at 44.1, 48 and 96 kHz: levels match within about 0.7 dB; about 3% of one core per preset at 48 kHz

### Pre-amp pedals
- [x] Noise gate: hysteresis, hold, adjustable release
- [x] Octaver: -1 and -2 octave voices plus dry blend
- [x] Drop tune: ±12 semitones with mix
- [x] Compressor: soft knee, attack/release, makeup gain
- [x] Overdrive: TS-style mid boost with a low cut for tightening
- [x] 9-band graphic EQ (63 Hz – 16 kHz) with level
- [x] Click-free bypass crossfades on every pedal

### Amp
- [x] Three channels (Clean / Crunch / Lead): cascaded asymmetric triode stages, coupling and Miller filters between stages, fizz filtering
- [x] 4x oversampling around the nonlinear stages
- [x] Tone stack (Bass / Middle / Treble) and power amp (Presence / Depth / Master) with supply sag
- [x] Bright and Tight switches
- [x] NAM capture loader: loads off the audio thread, swaps lock-free, resamples when the host rate differs from the capture's, normalizes loudness
- [x] Crossfade when switching between the DSP amp and a NAM capture

### Cabinet
- [x] Two IR slots, each with level, pan and mic position
- [x] Three synthesized built-in cabs: Modern 4x12 V, British 4x12 G, Open Back 2x12
- [x] User IR loading (.wav / .aif / .flac)
- [x] Sample-rate compensation for normalized IRs

### Post FX
- [x] Parametric EQ: 24 dB/oct low and high cuts plus 4 peaking bands
- [x] Stereo chorus and flanger
- [x] Delay: tempo sync, ping-pong, filtered feedback
- [x] Reverb: size, damping, pre-delay, width, low-cut on the tail

### UI
- [x] Custom-drawn look: metal knobs with value arcs, stompbox enclosures with LEDs, amp head with tolex and grill
- [x] Signal-chain navigator showing what's active in each section
- [x] Pages: Pre FX, Amp, Cab (draggable mics), EQ (draggable curve, scroll for Q), Post FX
- [x] Input/output meters

### Presets & state
- [x] 10 factory presets, also exposed to the host as programs
- [x] Save/load user presets (`Documents\Abyssal\Presets\*.abyssal`)
- [x] Session state remembers NAM and IR file paths

---

## Next — v0.2: playability
- [ ] **Tune by ear with a real guitar.** The voicings were set by measurement only.
- [ ] Separate knob settings for each amp channel (currently shared)
- [ ] Built-in tuner (with output mute)
- [ ] Gate key options: detect from the input, apply after the amp
- [ ] Input level calibration for NAM captures (match interface headroom)
- [ ] Report the extra latency from the NAM resampler and pitch effects to the host
- [ ] Resizable / scalable UI (100–200%)
- [ ] Undo/redo for parameter changes

## v0.3: tone depth
- [ ] Fourth amp voicing: a tight, saturated modern lead ("Abyss" channel)
- [ ] Passive tone stack with interacting controls, replacing the independent biquads
- [ ] Power amp voicing switch (EL34 / 6L6 style)
- [ ] Better pitch shifter for the octaver and drop tune (polyphonic, lower latency)
- [ ] More built-in cabs and mics (dynamic, ribbon, condenser) with mic distance control
- [ ] IR time alignment between the two cab slots (phase)
- [ ] Optional tone controls after a NAM capture

## v0.4: more FX
- [ ] Phaser, tremolo, wah (auto and expression)
- [ ] Pitch-shift whammy pedal
- [ ] Shimmer and plate reverb modes
- [ ] Tape and analog delay modes, plus tap tempo
- [ ] Reorderable pedals (drag on the board)
- [ ] Doubler for stereo width on rhythm parts

## v1.0: release
- [ ] MIDI control: program change, CC learn for footswitches
- [ ] Preset browser with tags, favourites and search
- [ ] Installer (VST3 path copy) and code signing
- [ ] AU / CLAP formats; macOS build in CI
- [ ] GitHub Actions: build plus `AbyssalRender` regression check on every push
- [ ] User manual
