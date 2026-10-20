# Abyssal — Metal Amp Suite

A high-gain guitar amp sim plugin (VST3 + Standalone) built with JUCE 8.

## Signal chain

```
IN → Noise Gate → Octaver → Drop Tune → Compressor → Overdrive → Graphic EQ
   → AMP (3-channel DSP model  or  NAM capture)
   → CAB (2 mic/IR slots: built-in cabs or your .wav IRs)
   → Parametric EQ → Chorus → Flanger → Delay → Reverb → OUT
```

Everything up to the cab is mono; the cab slots' pan controls make it stereo.

| Section | What's in it |
|---|---|
| **Pre FX** | Noise gate (hysteresis + hold), sub-octave (-1/-2), drop tune (±12 st), compressor, TS-style overdrive for tightening, 9-band graphic EQ |
| **Amp** | Clean / Crunch / Lead channels: cascaded asymmetric triode stages, 4x oversampled, interstage coupling/Miller filters, tone stack, power amp with sag, depth and presence. Bright and Tight switches. **Or** load any Neural Amp Modeler `.nam` capture (resampled automatically if the host rate differs from the capture's). |
| **Cab** | Two convolution slots, each with level, pan, and a draggable mic position. Three built-in synthesized cabs (Modern 4x12 V, British 4x12 G, Open Back 2x12) or load your own IR. |
| **EQ** | Post-cab parametric: 24 dB/oct low/high cut + 4 peaking bands, drag nodes on the graph, scroll for Q |
| **Post FX** | Stereo chorus, flanger, tempo-sync ping-pong delay, reverb with pre-delay |

Every control is an automatable host parameter. Pedal on/off switches crossfade so they don't click.

## Building (Windows)

Requirements: Visual Studio 2022/2026 with the C++ workload, Windows 10/11 SDK 10.0.22621 or newer, CMake 3.22+, Git.
JUCE and NeuralAmpModelerCore are downloaded automatically.

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --target Abyssal_VST3 Abyssal_Standalone
```

Outputs:

- `build/Abyssal_artefacts/Release/VST3/Abyssal.vst3` → copy to `C:\Program Files\Common Files\VST3\`
- `build/Abyssal_artefacts/Release/Standalone/Abyssal.exe` → runs directly (set your interface in Options → Audio settings)

## Offline test renderer

`AbyssalRender` runs a synthesized DI riff through every factory preset and writes WAVs:

```powershell
cmake --build build --config Release --target AbyssalRender
build\AbyssalRender_artefacts\Release\AbyssalRender.exe renders 48000 [path\to\model.nam]
```

It prints peak/RMS/CPU per preset and fails if any preset produces NaNs or silence.

## Presets

Factory presets are in the preset menu (and exposed to the host as programs). **Save** writes
`.abyssal` files to `Documents\Abyssal\Presets\`. Presets remember the NAM capture and IR file paths.

## Where to get captures and IRs

- NAM captures: [tone3000.com](https://www.tone3000.com) has thousands of free `.nam` files
- Cab IRs: any mono/stereo `.wav` IR works (many free packs are available online)

## Known limitations

- The pitch effects (octaver, drop tune) use a low-CPU delay-line shifter. They track best on single
  notes and palm-muted riffs; big chords can warble a bit. They also add roughly 15–30 ms of latency.
- In NAM mode the amp tone knobs are bypassed (the capture *is* the amp). Use the EQ page to shape it.
