# Jimmi Carcosa Fuzz (VST3)

A silicon fuzz based on the public description of the DOD *Carcosa* (manual and reviews), extended with more gain,
an octave section and a 3-band tone control. It is a behavioural model, **not** a schematic-exact copy, and is not
affiliated with DOD or Harman. The real circuit is not published, so the internals of this model are educated guesses.

## Signal flow
```
in -> Input trim -> Noise Gate -> [DEMHE: adds bass | HALI: cuts bass] -> [Octave down | up]
   -> stage 1 (BEFORE = gain) -> stage 2 (AFTER = bias: gating, sag, splatter)
   -> mid/treble emphasis -> BASS / MID / TREBLE -> HI-CUT -> OUTPUT -> Trim -> out
```

## Controls
| Control | What it does |
|---|---|
| Before | Amount of fuzz (stage 1 gain, 0 to +54 dB). Low = warm and thick, cleans up with the guitar volume. High = heavy, sustaining fuzz |
| After | Bias of the second stage. Left = loud, woody. Right = starved: gated, splattery, sagging, with broken vowel-like tones |
| Output | Level. With Before and After at zero the pedal works as a clean boost |
| Hi-Cut | Low-pass on the top end, from open (left) to dark (right) |
| Octave | Amount of octave added before the fuzz (0 = off) |
| Octave mode | **Down**: a sub-octave from a flip-flop divider (best on single notes, tracks the note and decays with it). **Up**: a full-wave rectifier octave-up, the same trick as the Octave control of the FZ-2 sim |
| Bass / Mid / Treble | +-12 dB at 100 Hz (shelf), 750 Hz (peak) and 3.5 kHz (shelf). Middle position is flat |
| Mode | **Demhe** adds bass and low-mid boost (for clean amps). **Hali** cuts bass (for dirty amps). Default is Demhe |
| ON | Footswitch, click-free crossfade. When off the pedal is bypassed and the gate is off too |
| Input / Trim | Host-side trims, -24 to +24 dB and -24 to +12 dB. Input sits before the gate |
| Noise Gate | Input gate, -80 (off) to -30 dBFS, default -60. A fuzz this strong amplifies interface and pickup hiss a lot, so keep the gate on or put the Guitar Gate before it (then set this one to Off) |

The octave-down divider works on one note at a time: chords and fast runs make it jump. Play single notes, low on the neck.
Fuzz reacts strongly to the guitar's volume knob and pickups: turn the guitar volume down to clean it up.
DSP runs at 4x oversampling (the plugin reports the oversampler and gate latency to the host).

## Build
Requires CMake 3.22+ and a C++17 compiler. JUCE 8.0.15 is downloaded automatically.
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Result: `build/JimmiCarcosaFuzz_artefacts/Release/VST3/Jimmi Carcosa Fuzz.vst3`

Install:
- Windows: copy the `.vst3` folder to `C:\Program Files\Common Files\VST3\`
- macOS: `~/Library/Audio/Plug-Ins/VST3/` (unsigned builds may need `xattr -cr` on the bundle)
- Linux: `~/.vst3/`

No compiler? Push this folder to a GitHub repository: the included workflow
(`.github/workflows/build.yml`) builds Windows, macOS and Linux VST3s and attaches them as downloadable artifacts.

Optional offline tests: `cmake -S . -B build -DJIMMICARCOSA_BUILD_TESTS=ON && cmake --build build --target dsp_test gate_test && ./build/dsp_test && ./build/gate_test`

JUCE is used under its AGPLv3/commercial licensing: if you distribute the plugin, the AGPL terms apply.
