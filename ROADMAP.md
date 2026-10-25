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

## Done — v0.2 (October 2026)

- [x] Input selector (Left / Right / Mono Sum) and Stereo / Mono output mode
- [x] Rearrangeable chain: any pedal before or after the amp, by drag-and-drop or right-click menu;
      mono pedals run per channel post-amp, stereo FX fold to mono pre-amp; order saved in presets
- [x] Preset browser: categories, search, favourites (shared across instances), arrow-key audition,
      prev/next within the current filter
- [x] 15 level-matched core metal factory presets (Djent, Metalcore, Thrash, Death, Black, Doom, Nu Metal,
      Lead, Clean); some use custom pedal orders (e.g. EQ into a cranked OD, post-amp octaver)
- [x] Host bypass parameter with a latency-aligned dry crossfade
- [x] Idle detection: no processing on silent tracks once tails have decayed
- [x] Resizable / scalable UI (60–200%), remembered per project and as the default
- [x] Program-change guard so project recall in FL Studio / Reaper never resets the sound
- [x] Setup parameters (routing, input gain, bypass) kept when loading presets
- [x] Lighter UI repainting (cab view and chain bar repaint only on change)
- [x] Render tool: chain-order stress tests (all pedals on, sides swapped, all post-amp) and mono output

## Done — v0.2.1 (October 2026)

- [x] Self-contained standalone exe and VST3 (static C++ runtime, no redistributable needed)
- [x] ASIO driver support in the standalone app (Steinberg ASIO SDK fetched at configure time)
- [x] Custom standalone app class: input unmuted on first run
- [x] App icon
- [x] Inno Setup installer (standalone + VST3 components, shortcuts, uninstaller) via `--target installer`

## Done — v0.2.2 (October 2026)

- [x] IR library (`Documents\Abyssal\IRs`) and NAM capture library (`Documents\Abyssal\NAM`): loaded
      files are saved and listed in the dropdowns, with sub-folders as submenus
- [x] Load a whole folder of IRs or NAM captures (structure kept, first file loaded)
- [x] ‹ / › stepping through a pack, remove-from-library (to the Recycle Bin), open-folder
- [x] Tooltips enabled across the UI
- [x] `AbyssalRender --libtest`: 16-check self-test of the library (import, de-dupe, step, remove)

## Done — v0.2.3 (October 2026)

- [x] TONE3000 integration: browser sign-in (OAuth 2.0 + PKCE, loopback redirect, DPAPI token storage),
      pick a tone on tone3000.com, Favorites / Downloaded / Created / Trending / Latest lists,
      per-model download + load, download-all-standard, credits file per tone
- [x] `AbyssalRender --t3ktest`: PKCE, callback parsing, loopback server and live token-endpoint checks
- [x] THIRD_PARTY_NOTICES.md (TONE3000 example code is MIT)

---

## Done — v0.3: playability (October 2026)

- [x] Separate knob settings for each amp channel; v0.2 projects/presets migrate onto the channel they used
- [x] Built-in chromatic tuner (YIN, B0–E5 within 1 cent) with output mute and A4 reference
- [x] Gate key from the guitar input (pairs with a post-amp gate)
- [x] Input level calibration for NAM captures (interface dBu vs. the capture's recorded level)
- [x] Latency reporting follows the NAM resampler and drop tune; bypass dry path aligned to match
- [x] Preset save dialog with name / category / notes; user categories; rename / edit / delete from the browser
- [x] A/B compare snapshots, saved with the project
- [x] Load NAM captures on a background thread
- [x] Undo/redo for parameter changes, preset loads and pedal order (Ctrl+Z / Ctrl+Y)
- [x] TONE3000: creator avatars, favourite/unfavourite from the panel, paging for long lists
- [x] `AbyssalRender --dsptest`

Still open from v0.3:
- [ ] **Tune by ear with a real guitar.** The voicings were set by measurement only.
- [ ] TONE3000 logo artwork per their brand guidelines (needs their official asset pack)

## Done — v0.4: tone depth (October 2026)

- [x] Fourth amp voicing: "Abyss" channel (tight, saturated modern lead) with its own faceplate
- [x] Passive tone stack with interacting controls, solved from the circuit (nodal analysis → exact 3rd-order
      transfer function → bilinear transform), per-channel component values; factory presets re-levelled
- [x] Power amp voicing switch (EL34 / 6L6)
- [x] Better pitch shifter: correlation-chosen splice points. On-pitch energy ~99% on chords (was 85–95%),
      delay follows the note (5–9 ms single notes, ~15 ms chords; was a fixed 17.5 / 27.5 ms)
- [x] Three more built-in cabs, mic types (dynamic / ribbon / condenser) and mic distance per slot
- [x] IR time alignment between the two cab slots (onset-based), plus polarity invert
- [x] Optional tone controls after a NAM capture
- [x] State migration v4 (cab source indices), defaults for parameters missing from older states

Not done: a truly polyphonic shifter (phase vocoder) would cost more latency than v0.3 had, which the
"lower latency" goal rules out.

## Done — v0.5: more FX (October 2026)

- [x] Phaser (4/8 stage, feedback, stereo offset post-amp), tremolo (shape, tempo sync, auto-pan),
      wah (pedal position for automation / expression, and auto)
- [x] Whammy pedal (eight intervals, treadle position, harmony mix) on the splice-picking shifter, which
      now handles big up-shifts too (adaptive crossfade; octave up 18% → 99.8% on-pitch vs v0.3's shifter)
- [x] Plate (Dattorro-style tank) and shimmer (octave-up feedback) reverb modes; all modes decay at max settings
- [x] Tape and analog delay modes, plus tap tempo
- [x] Doubler for stereo width
- [x] Chain grows to 15 pedals + the amp block; older pedal orders pick up the new pedals in place;
      delay and reverb pedals are double-width; boards wrap to three rows when needed

## v1.0: release
- [ ] MIDI control: program change, CC learn for footswitches
- [ ] Preset tags (multiple per preset) (user-defined categories arrived in v0.3)
- [ ] Code signing for the installer and binaries
- [ ] AU / CLAP formats; macOS build in CI
- [ ] GitHub Actions: build plus `AbyssalRender` regression check on every push
- [ ] User manual
