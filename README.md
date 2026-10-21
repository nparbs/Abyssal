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

**Rearranging:** every pedal can go before or after the amp, in any order. Drag a pedal on its board to
reorder it; drop it on **PRE FX** / **POST FX** in the chain bar to move it across the amp (hover there to
switch boards); or right-click a pedal. Mono pedals placed after the amp run per channel in stereo;
stereo effects placed before the amp are folded to mono. The order is saved with sessions and presets.

**Routing:** the header has an input selector (Left / Right / Mono Sum, for whichever interface input
carries your guitar) and an output mode (Stereo, or Mono for double-tracked hard-panned takes).

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
| `AbyssalRender --t3ktest` | TONE3000 PKCE, redirect handling and token endpoint |

## Presets

Click the preset name to open the browser: filter by category, search, and star favourites. Clicking or
arrowing through the list auditions presets immediately; `<` / `>` step through the current filter.
15 factory presets are level-matched (about -17.5 dBFS RMS) and also exposed to the host as programs.
**Save** writes `.abyssal` files to `Documents\Abyssal\Presets\`; they show up under **User**.
Presets remember the pedal order and the NAM capture / IR file paths. Presets never change the input
selector, input gain, output mode or bypass. Favourites are shared by every instance (stored in
`%APPDATA%\Abyssal\Abyssal.settings`).

## Using it in FL Studio and Reaper

- **Bypass:** the host's bypass button crossfades to the dry signal, delayed to match the plugin's latency.
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
- **Favorites / Downloaded / Created / Trending / Latest** list tones without leaving the plugin.
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

- The pitch effects (octaver, drop tune) use a low-CPU delay-line shifter. They track best on single
  notes and palm-muted riffs; big chords can warble a bit. They also add roughly 15–30 ms of latency.
- In NAM mode the amp tone knobs are bypassed (the capture *is* the amp). Use the EQ page to shape it.
