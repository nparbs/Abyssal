# Abyssal — Metal Amp Suite

A high-gain guitar amp sim plugin (VST3 + Standalone) built with JUCE 8.

## Signal chain

```
IN → Wah → Whammy → Noise Gate → Octaver → Drop Tune → Compressor → Overdrive → Graphic EQ
   → AMP (4-channel DSP model  or  NAM capture)
   → CAB (2 mic/IR slots: built-in cabs or your .wav IRs)
   → Parametric EQ → Phaser → Chorus → Flanger → Tremolo → Doubler → Delay → Reverb → OUT
```

Everything up to the cab is mono; the cab slots' pan controls make it stereo.

**Rearranging:** every pedal can go before or after the amp, in any order. Drag a pedal on its board to
reorder it; drop it on **PRE FX** / **POST FX** in the chain bar to move it across the amp (hover there to
switch boards); or right-click a pedal. Mono pedals placed after the amp run per channel in stereo;
stereo effects placed before the amp are folded to mono. The order is saved with sessions and presets.

**Routing:** the header has an input selector (Left / Right / Mono Sum, for whichever interface input
carries your guitar) and an output mode (Stereo, or Mono for double-tracked hard-panned takes).

| Section | What's in it |
|---|---|
| **Pre FX** | Wah (pedal or auto), whammy, noise gate (hysteresis + hold, optional key from the guitar input), sub-octave (-1/-2), drop tune (±12 st), compressor, TS-style overdrive for tightening, 9-band graphic EQ |
| **Amp** | Clean / Crunch / Lead / Abyss channels, each with its own knob settings: cascaded asymmetric triode stages, 4x oversampled, interstage coupling/Miller filters, a passive interactive tone stack, power amp with sag, depth and presence, EL34 / 6L6 power tubes. Bright and Tight switches. **Or** load any Neural Amp Modeler `.nam` capture (resampled automatically if the host rate differs from the capture's, with optional input calibration and tone controls after it). |
| **Cab** | Two convolution slots, each with a mic type (dynamic / ribbon / condenser), draggable mic position, mic distance, level, pan and polarity, and optional time alignment between them. Six built-in synthesized cabs (Modern 4x12 V, British 4x12 G, Open Back 2x12, Oversized 4x12 C, Vintage 4x12 25, Closed 2x12 V) or load your own IR. |
| **EQ** | Post-cab parametric: 24 dB/oct low/high cut + 4 peaking bands, drag nodes on the graph, scroll for Q |
| **Post FX** | Phaser (4/8 stage), stereo chorus, flanger, tremolo (sync, auto-pan), doubler, tempo-sync ping-pong delay (digital / tape / analog, tap tempo), reverb (hall / plate / shimmer) |

Every control is an automatable host parameter. Pedal on/off switches crossfade so they don't click.

**Amp channels:** each channel remembers its own Gain / tone / Presence / Depth / Master and Bright / Tight,
so switching channels is like stepping on a real channel switch. Projects and presets from v0.2 (one
shared set of knobs) load with those settings on whichever channel they used.

**Abyss channel:** a fourth voicing for tight, saturated modern leads and rhythm: more gain, steeper
low cuts between the stages so it stays tight, an upper-mid push into the clipping, and a tone stack
whose Bass knob works on the punch rather than the boom.

**Tone stack:** Bass / Middle / Treble are a model of the passive network real amps use, solved as a
circuit (Bassman-style on Clean, Plexi/800-style on Crunch, modern high-gain on Lead and Abyss). The
controls interact the way they do on an amp: there's a mid scoop with everything at noon, turning the
mids down also changes the bass and treble, and the Middle knob has a modest range. Each channel's
level is set for every knob at 5, so cranking the tone controls also makes it louder.

**Power tubes:** the **EL34 / 6L6** switch on the amp face. EL34s push the upper mids and break up
earlier with a little asymmetry; 6L6s are rounder in the mids, firmer in the lows and cleaner for longer.

**Tone after a capture:** in capture mode, **TONE** on the amp face turns Bass / Middle / Treble /
Presence into an EQ after the capture (5 = flat, +-10 dB).

**Mics:** each cab slot has a mic type: **Dynamic** (the classic close mic and the reference sound),
**Ribbon** (dark, smooth top and big lows, with a strong proximity effect) and **Condenser** (open,
extended top). **Distance** backs the mic off up to about 50 cm: less proximity bass, less air and a
floor reflection that gets stronger as the mic moves back. **ALIGN MICS** (on by default) lines the
two slots up in time, so IRs with different start offsets or mics at different distances don't cancel
each other; switch it off for the natural offsets and their phase colour. **POLARITY** flips a slot.

**More effects (v0.5):** every one of these can go before or after the amp like the other pedals.
- **Wah:** **Pedal** mode is a cry-baby style sweep (about 350 Hz – 2.2 kHz) on the **Pedal** knob:
  automate it, or map it to an expression pedal through your host. **Auto** follows your picking
  (**Sens** sets how far). **Peak** is the resonance.
- **Whammy:** shifts by the chosen interval (±4th, ±5th, ±1 or 2 octaves). **Pedal** is the treadle:
  0 = no shift, full = the whole interval, so automating it gives dive-bombs and octave sweeps. **Mix**
  below full turns it into a harmony. Uses the same splice-picking shifter as drop tune.
- **Phaser:** 4 or 8 all-pass stages with feedback. After the amp, the right side sweeps a quarter-cycle
  ahead of the left for width.
- **Tremolo:** **Shape** goes from a smooth sine to a hard chop; **SYNC** locks it to the host tempo;
  **PAN** (after the amp) makes the sides alternate.
- **Doubler:** blends each side with its own slightly late, drifting copy, so a single take sounds
  double-tracked. **Width** is how much, **Time** the offset, **Detune** the drift.
- **Delay modes:** **Tape** adds wow, flutter and a little saturation per repeat; **Analog** gives dark,
  softly saturating bucket-brigade repeats. **TAP** sets the time from your taps (and turns sync off).
- **Reverb modes:** **Plate** is a dense, bright plate (Dattorro-style tank); **Shimmer** feeds the hall's
  tail back an octave up (**Shimmer** sets how much).

**Gate key:** the gate's **INPUT KEY** switch makes it listen to your guitar as it comes in, instead of
the signal at the pedal. Combined with moving the gate after the amp, this is the classic "detect at the
guitar, gate after the amp" setup: it cuts the amp's hiss and hum without choking sustained notes.

**NAM input calibration:** many captures record the input level (dBu) they were made at. Turn on
**CALIBRATE** on the amp page and set **INTERFACE** to your interface's input level for a 0 dBFS signal
(see its specs; often listed as "max input level") and Abyssal adds the gain that makes the capture react
to your playing like the real amp did. It's a setting for your rig, so presets don't change it and new
instances start with your last values. Captures without a recorded level are left alone.

## Playing

- **Tuner:** **TUNER** in the header. Reads the selected input (works while bypassed), shows note, cents
  and frequency, and covers B0 up to the high frets, so drop-tuned 7/8-strings and basses read fine.
  **MUTE** silences the output while it's open (remembered); **- / +** sets the A4 reference (420–460 Hz).
- **Undo / redo:** the arrows in the header, or Ctrl+Z / Ctrl+Y (Ctrl+Shift+Z). Knob moves, switches,
  preset loads and pedal-order changes are all undoable. Host automation isn't recorded.
- **A/B compare:** **A** and **B** hold two complete sounds. Click to flip between them (B starts as a
  copy of A); right-click to copy the current sound to the other slot. Both are saved with the project.

## Building (Windows)

Requirements: Visual Studio 2022/2026 with the C++ workload, Windows 10/11 SDK 10.0.22621 or newer, CMake 3.22+, Git.
JUCE, NeuralAmpModelerCore and the Steinberg ASIO SDK are downloaded automatically
(turn ASIO off with `-DABYSSAL_ASIO=OFF`). The C++ runtime is linked statically, so the exe and VST3
run on any 64-bit Windows 10/11 machine without the Visual C++ Redistributable.

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --target Abyssal_VST3 Abyssal_Standalone
```

Outputs:

- `build/Abyssal_artefacts/Release/VST3/Abyssal.vst3` → copy to `C:\Program Files\Common Files\VST3\`
- `build/Abyssal_artefacts/Release/Standalone/Abyssal.exe` → a single self-contained exe

### Installer

With [Inno Setup 6](https://jrsoftware.org/isinfo.php) installed (`winget install JRSoftware.InnoSetup`):

```powershell
cmake --build build --config Release --target installer
```

This produces `build/installer/AbyssalSetup-1.0.0.exe`. It installs the standalone app (Start menu and optional
desktop shortcut) and/or the VST3 into `C:\Program Files\Common Files\VST3\`, and adds an uninstaller.

## Standalone app

- **Options → Audio/MIDI Settings:** choose **ASIO** and your interface's driver for the lowest latency
  (WASAPI and DirectSound are also available). Start with a 64–128 sample buffer.
- The input is live on first launch (JUCE's default feedback-protection mute is off for a guitar amp).
  If you're on laptop speakers with a mic, re-enable "Mute audio input" in the settings.
- Device, buffer size and the last sound are remembered between launches.

## Offline test renderer

`AbyssalRender` runs a synthesized DI riff through every factory preset and writes WAVs:

```powershell
cmake --build build --config Release --target AbyssalRender
build\AbyssalRender_artefacts\Release\AbyssalRender.exe renders 48000 [path\to\model.nam]
```

It prints peak/RMS/CPU per preset and fails if any preset produces NaNs or silence.

Other test modes:

| Command | Checks |
|---|---|
| `AbyssalRender --vst3test <Abyssal.vst3> <folder of .nam>` | Hosts the **built VST3** like a DAW and verifies every capture in the folder loads and plays inside the plugin |
| `AbyssalRender --libtest <folder of .nam>` | IR/NAM library import, de-duplication, stepping, removal |
| `AbyssalRender --namtest <model.nam>` | Background capture loading: prepare on a worker thread while the host changes rate, then install and play |
| `AbyssalRender --t3ktest` | TONE3000 PKCE, redirect handling and token endpoint |
| `AbyssalRender --dsptest` | Tuner accuracy (B0–E5 within 1 cent at 44.1/48/96 kHz), NAM resampler latency vs. what's reported, pitch shifter vs. the v0.3 one (on-pitch energy, warble, delay; shifting down and up), tone stack circuit behaviour and digital/analog match, cab alignment, reverb modes decaying at max settings, pedal-order upgrades, state migrations, gate keying |

## Presets

Click the preset name to open the browser: filter by category, search, and star favourites. Clicking or
arrowing through the list auditions presets immediately; `<` / `>` step through the current filter.
15 factory presets are level-matched (about -17.5 dBFS RMS) and also exposed to the host as programs.
**Save** opens a dialog for the name, a category (pick one or type your own) and notes, and writes an
`.abyssal` file to `Documents\Abyssal\Presets\`. User presets appear under their category and under
**User**; your own categories get their own filter. Notes show next to the name and are searchable.
Right-click a user preset in the browser to rename it, change its category or notes, delete it (to the
Recycle Bin) or show it in Explorer.
Presets remember the pedal order and the NAM capture / IR file paths. Presets never change the input
selector, input gain, output mode or bypass. Favourites are shared by every instance (stored in
`%APPDATA%\Abyssal\Abyssal.settings`).

## Using it in FL Studio and Reaper

- **Bypass:** the host's bypass button crossfades to the dry signal, delayed to match the plugin's latency.
- **Latency:** the reported latency follows the setup: the DSP amp's oversampling (a few samples), the
  NAM resampler when a capture's rate differs from the host's (about 35 samples), and drop tune while it's
  shifting (10 ms nominal), and the whammy when its mix is mostly wet. The octaver isn't compensated,
  because its dry signal would end up early.
- **Idle CPU:** with silent input, once the delay/reverb tails die out (below -100 dBFS for about 2.6 s),
  the plugin stops processing until signal returns. It plays nicely with FL's Smart Disable and Reaper's
  auto-bypass on silence.
- **Resizable UI:** drag the window corner (60%–200%). New instances open at the last size you used.
- **Project recall:** the host's program number is ignored when it's already the current one, so reopening
  a project never resets your sound to a factory preset.
- **FL Studio:** mixer inserts are stereo. If your guitar is on interface input 1 only, leave the input
  selector on **Left**. If FL feeds the same signal to both sides, any setting works.
- **Reaper:** for a mono-recorded guitar on a stereo track, leave the input on **Left** (or **Mono Sum**).
  Use output **Mono** and pan two takes hard left and right for a classic double-tracked rhythm.

## IR and capture libraries

Each cab slot and the amp's capture picker have a dropdown backed by a library folder:

| Library | Folder |
|---|---|
| Cab IRs (.wav / .aif / .flac) | `Documents\Abyssal\IRs\` |
| NAM captures (.nam) | `Documents\Abyssal\NAM\` |

- **Load file / Load folder:** loading an IR or capture copies it into the library, so it stays in the
  dropdown for every slot, project and preset. **Load folder** imports a whole pack, keeping its
  sub-folders, which then appear as submenus. The first file is loaded straight away. Files you drop
  into these folders yourself show up too.
- **‹ / ›** step through the current file's folder, for auditioning a pack quickly.
- **Remove from library** moves the file to the Recycle Bin; any slot using it falls back to a built-in cab.
- Identical files are never duplicated. Broken files are rejected before they're copied.

## Downloading from TONE3000

Abyssal can download NAM captures and cab IRs straight from [TONE3000](https://www.tone3000.com) into the
libraries above (`...\NAM\TONE3000\` and `...\IRs\TONE3000\`, one folder per tone with a credits file).

**One-time setup:** TONE3000 requires each app to identify itself with a free *publishable key*.
1. Sign in at tone3000.com and open **Settings → API**, then create a key.
2. In Abyssal, click **TONE3000...** (amp page, a cab slot, or the dropdown menus), paste the
   **publishable** key (`t3k_pub_...`) and click **SAVE KEY**. Never paste the secret key; Abyssal doesn't need it.
3. Click **CONNECT**. Your browser opens TONE3000's sign-in, and it returns you to Abyssal automatically.

**Getting tones:**
- **PICK A TONE ON TONE3000.COM** opens the catalog in your browser, filtered to NAM captures or IRs.
  Choose a tone and it appears in Abyssal.
- **Favorites / Downloaded / Created / Trending / Latest** list tones without leaving the plugin, with the
  creator's avatar. Long lists load 50 at a time (**LOAD MORE**).
- Click the star on a tone (or **FAVORITE** in its details) to add it to / remove it from your TONE3000 favorites.
- Click a model to download it and load it into the amp or cab slot the panel was opened from
  (**LOAD** once it's downloaded). **DOWNLOAD ALL STANDARD** grabs every standard-size model of the tone.

**How sign-in works:** OAuth 2.0 with PKCE in your normal browser. The redirect goes to a one-off local
server on `127.0.0.1`, and tokens are encrypted with Windows DPAPI in
`%APPDATA%\Abyssal\tone3000.settings`, readable only by your Windows user. **DISCONNECT** removes them.

**Free-tier limits:** TONE3000's free API tier (non-commercial use) doesn't include in-app keyword search,
which is why browsing goes through their website. Tones belong to their creators; check each tone's
license (shown in the panel and in the credits file) before using captures in released music.

## Where to get captures and IRs

- NAM captures: [tone3000.com](https://www.tone3000.com) has thousands of free `.nam` files
- Cab IRs: any mono/stereo `.wav` IR works (many free packs are available online)

## Licensing note

The ASIO SDK is dual-licensed by Steinberg (proprietary or GPLv3), and JUCE is AGPLv3 or commercial.
Personal use is fine. Before distributing builds publicly, pick compatible licenses (e.g. release the
source under GPLv3/AGPLv3, or get JUCE and Steinberg licences).

## Known limitations

- The pitch effects (octaver, drop tune) use a delay-line shifter that picks its splice points by
  correlation. Compared with v0.3 it keeps far more of the output on pitch on chords (about 99% vs
  85–95% in the test signals) and its delay follows the note: about 5–9 ms on single notes and ~15 ms
  on chords, instead of a fixed 17.5 ms (drop tune) / 27.5 ms (octaver). Fast pick attacks can still
  smear slightly. Drop tune reports 10 ms to the host, so on chords it lands ~5 ms late, on single notes
  a few ms early.
- The TONE3000 panel doesn't show TONE3000's official logo yet; their brand assets are a separate download.
- In NAM mode the amp's tone stack is bypassed (the capture *is* the amp); use **TONE** or the EQ page.
- The DSP amp's sound changed in v0.4 (passive tone stack, power tube voicing). Factory presets were
  re-levelled to their previous loudness, but saved user presets on the DSP amp will sound different.
