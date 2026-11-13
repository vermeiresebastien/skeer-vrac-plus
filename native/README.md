# SKEER VRAC+ — Native VST3 / Standalone

**One sound. One drum kit.** The SKEER-branded native edition turns any recording into a
sixteen-pad instrument with sixteen drum models, per-pad morphing, eight stereo outputs and a
host-synced 32-step sequencer with pattern chaining, polymeter, parameter locks and MIDI export.

![SKEER VRAC+ native editor](shots/editor-light.png)

**SEQUENCER page**

![SKEER VRAC+ step sequencer](shots/sequencer-light.png)

**Step inspector — nudge, flams and parameter locks**

![SKEER VRAC+ step inspector](shots/sequencer-inspector.png)

**SKEER Sample Vault page**

![SKEER VRAC+ Sample Vault browser](shots/vault-light.png)

**Dark mode**

![SKEER VRAC+ dark editor](shots/editor-dark.png)

![SKEER VRAC+ dark sequencer](shots/sequencer-dark.png)

![SKEER VRAC+ dark step inspector](shots/sequencer-inspector-dark.png)

![SKEER VRAC+ dark Sample Vault](shots/vault-dark.png)

---

## Status

| | |
| --- | --- |
| Formats | VST3 instrument and standalone, x64 Windows |
| Framework | JUCE 8.0.4, C++20 |
| Vendor / bundle | SKEER / `com.skeer.vracplus` |
| Host requirements | VST3 host with MIDI input and multi-output support |
| Sample Vault link | Optional read-only SKEER Sample Vault index (auto-detected) |
| Branding | SKEER logo, IBM Plex Sans / IBM Plex Mono, Barlow Condensed, SKEER palette |
| Theme | Light and dark SKEER skins, switched from the header and remembered per user |
| Build toolchain | Visual Studio 18 2026 / MSVC 14.51, CMake 3.22+ |
| Installed plugin | `%LOCALAPPDATA%\Programs\Common\VST3\VRAC+.vst3` |
| Installed standalone | `%LOCALAPPDATA%\Programs\VRACPlus\VRAC+.exe` |
| Build output | `build\VRACPlus_artefacts\Release\VST3\VRAC+.vst3` |
| Packaged release | `artifact\VRACPlus-1.0.0-Windows-x64.zip` |
| Test suite | 60 engine/state checks, editor layout snapshots, VST3 host load |

The plugin bundle is self-contained (factory audio and SKEER brand assets are embedded, no
external resource folder). The build is about **24 MB**. The VST3 session identity is preserved
from earlier VRAC+ builds, so existing host sessions resolve to this SKEER-branded build; the
vendor display name is updated to SKEER. The interface supports a light studio theme and a dark
low-light theme.

---

## Build

Requirements:

- Windows 10/11 x64
- Visual Studio 18 2026 with the Desktop C++ workload
- CMake 3.22 or newer
- JUCE 8.0.4 in `native/external/JUCE` (a local source checkout; CMake clones it if missing)

From the repository root:

```powershell
cmake -S native -B native/build -G "Visual Studio 18 2026" -A x64
cmake --build native/build --config Release --target VRACPlus_VST3 VRACPlus_Standalone --parallel 2
```

If you are already inside `native/`:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --target VRACPlus_VST3 VRACPlus_Standalone --parallel 2
```

The VST3 bundle is written to:

```text
native/build/VRACPlus_artefacts/Release/VST3/VRAC+.vst3
```

and the standalone application to:

```text
native/build/VRACPlus_artefacts/Release/Standalone/VRAC+.exe
```

`COPY_PLUGIN_AFTER_BUILD` is deliberately disabled, so the build never writes to a system
VST3 folder by itself.

### Install

System-wide (the terminal must be elevated):

```powershell
Copy-Item "native\build\VRACPlus_artefacts\Release\VST3\VRAC+.vst3" `
          "$env:ProgramFiles\Common Files\VST3\" -Recurse -Force
```

Per-user (no elevation; this is what is currently installed):

```powershell
$dest = "$env:LOCALAPPDATA\Programs\Common\VST3"
New-Item -ItemType Directory -Force $dest | Out-Null
Copy-Item "native\build\VRACPlus_artefacts\Release\VST3\VRAC+.vst3" $dest -Recurse -Force
```

Then rescan VST3 plugins in your DAW. If your DAW does not scan the per-user folder by default,
add `%LOCALAPPDATA%\Programs\Common\VST3` to its plugin paths. If the previous build is still
open in a DAW, close the DAW before replacing the `.vst3` bundle and rescan after restarting it.

Standalone:

```powershell
Copy-Item "native\build\VRACPlus_artefacts\Release\Standalone\VRAC+.exe" `
          "$env:LOCALAPPDATA\Programs\VRACPlus\VRAC+.exe" -Force
```

---

## Test

```powershell
# from the repository root
ctest --test-dir native/build -C Release --output-on-failure
```

Individual tests:

```powershell
native\build\VRACPlusSmokeTest_artefacts\Release\VRACPlusSmokeTest.exe
native\build\VRACPlusLayoutTest_artefacts\Release\VRACPlusLayoutTest.exe
native\build\VRACPlusHostTest_artefacts\Release\VRACPlusHostTest.exe
```

The host test asks JUCE's VST3 hosting path to scan and instantiate the built bundle, checks all
347 exposed parameters, accepts an eight-stereo-output bus layout, processes MIDI note 36,
checks the output is finite and non-silent, and round-trips the plugin state.

The layout test writes light and dark snapshots for the kit, sequencer, step inspector and
Sample Vault pages, opens the step inspector through the real UI path and verifies that no child
control falls outside its parent.

`VRACPlusSmokeTest` covers:

- factory source loading and slicing
- all sixteen models producing audio
- all eight stereo output buses
- pan, velocity response, mute and solo gating
- sample-accurate MIDI note triggering
- factory presets and full state round-trip
- undo/redo and randomisation
- MIDI CC learn and learned-parameter movement
- sidechain-input recording and recording adoption
- custom-sample state round-trip
- internal step-sequencer playback, host-synced transport, sequencer state and sequence undo
- step nudge, flams, per-lane polymeter, song-mode chaining and parameter locks
- extended sequencer state round-trip, factory/user pattern presets and MIDI export
- SKEER Sample Vault index query and indexed-sample loading
- randomizer, Euclidean, parseable MIDI file and CPU-headroom checks

Current result: **60/60 passed**, with the engine using about **5% of one core** while
continuously triggering pads at 48 kHz / 512 samples.

---

## Using it

### Load a sound

- **LOAD** — WAV, AIFF, MP3, FLAC, OGG or M4A. You can also drag a supported audio file
  anywhere onto the plugin window.
- **LIBRARY** — six embedded factory worlds: Foley Trash, Night Kitchen, Body Talk, Iron Works,
  Short Circuit and Dust Loop.
- **REC** — records the optional sidechain input bus (up to two minutes at 48 kHz; the cap is a
  48 MB stereo buffer). Enable the sidechain input bus in your host to use this. Stop recording
  and the take is adopted automatically as the new source.
- Waveform — drag the orange marker to set the selected pad's start position.
- **AUTO SLICE** — transient detection sensitivity. **HITS** shows the detected count.
- **< > RND** — step to the previous, next or a random detected hit.
- **+ / DEL** — add a slice at the current marker or delete the nearest slice.

### Theme

The **DARK / LIGHT** button in the header switches the complete SKEER interface between the warm
ivory studio skin and the low-light charcoal skin. The choice is stored under
`%APPDATA%\VRACPlus\theme.txt` and is applied the next time the plugin editor opens.

### Play

- Click a pad; the click position sets velocity. **Shift** = accent, **Alt** = ghost.
- MIDI notes **C1–D#2** (MIDI 36–51).
- Computer keyboard: `1 2 3 4 / Q W E R / A S D F / Z X C V`.
- **SPACE** or **STOP** cuts all voices.
- **AUTO SELECT** follows keyboard and MIDI notes with the pad editor selection.

### Edit a pad

Each pad starts as a default kit role. The editor owns every pad parameter:

| Block | Parameters |
| --- | --- |
| Morph | Original level, Drumify blend, Drumified level |
| Main | Pitch, Length, Velocity amount, Pan, Volume, Tone Shift |
| Routing | Choke group 0–4, one of eight stereo outputs |
| Model Bank | 4 families × 4 models = 16 choices |
| Sculpt | Warp, Impact, Instability, Super |
| EQ | Low, Mid, High |
| Pad tools | Play, Copy, Paste, Reset |

Click a model to audition it immediately. **COPY / PASTE** move a pad's design to another pad
without changing its start position.

### Sequencer page

Use the **KIT / SEQUENCER / VAULT** tabs in the header. The sequencer runs alongside the drum
engine and follows host transport when available.

- **Patterns A–D** — four independent patterns with per-pattern lane mute/solo.
- **Transport** — follows the host playhead and tempo; **PLAY** can preview on the internal
  clock when the host is stopped. **HOST** toggles host sync.
- **Length / division / swing** — 1–32 steps, 1/8, 1/8T, 1/16, 1/16T or 1/32, plus swing.
- **Colour-coded steps** — active steps use their drum-family colour: kick coral orange, snare
  electric blue, hats mint teal and clap violet. The playhead and transport state use the green
  playback colour.
- **Draw** — click or drag to paint steps.
- **Velocity / Probability / Ratchet** — select a lane, choose an edit mode and drag vertically.
  Active steps keep their full family colour, with velocity as a slim bottom bar, probability as
  a dot and ratchets as an `x2`–`x4` count.
- **Nudge** — use the NUDGE mode and drag vertically for ±50% of a step of micro-timing, or
  right-click any step and use the inspector.
- **Flams** — use FLAM mode or the inspector for up to two grace hits before a step.
- **Parameter locks** — right-click a step, enable **PARAMETER LOCK**, and lock pitch, volume,
  pan, tone and length for that individual hit.
- **Song mode / chain** — enable **SONG** and click the eight chain slots to cycle them through
  patterns A–D. Playback advances one slot per pattern-length.
- **Polymeter** — the **LANE LOOP** menu sets the selected lane to GLOBAL or its own loop length
  (1–32). Grid cells repeat visually when a lane runs shorter than the pattern.
- **Lane M/S** — mute or solo one sequencer row without changing the kit's pad mute/solo.
- **Random** — light/medium/busy grooves, mutate, humanise, accent downbeats, shift left/right,
  randomise the selected lane, or generate 3/5/7-pulse Euclidean rhythms.
- **Pattern presets** — the **PAT PRESET** menu loads six factory grooves or saves/loads
  `.vracpattern` files.
- **Copy / Paste / Clear** — pattern-level operations. All edits participate in undo/redo.
- **MIDI** — export a Standard MIDI File of the current pattern or song chain, or start a MIDI
  file drag into a host that accepts external file drops.
- **Keyboard** — `SPACE` starts/stops internal preview; `G` generates a random groove.

The sequence, chain, nudge/flams and parameter locks are saved with the host state and
`.vracpreset` files.

### SKEER Sample Vault page

VRAC+ can read the **SKEER Sample Vault** index directly. It opens the vault's local SQLite
index read-only, so browsing and filtering do not decode or analyse every file again, and no
files are modified.

- **Automatic connection** — finds the installed vault database, the installer registry, a
  saved custom path, or a development checkout beside VRAC+. If the index is elsewhere, use
  **LOCATE DB** once; the path is remembered.
- **Browse** — search filenames, folders and indexed tags; filter by family, type, collection
  or favourites; sort by name, recency, duration or BPM.
- **Indexed detail** — family/type/BPM/key, duration, sample rate, channels, tags, notes,
  source path and the waveform generated by the vault's analysis.
- **Load** — select a sample and press **LOAD SAMPLE** or double-click it. VRAC+ loads the
  original source file into the current kit and returns to the KIT page. Samples that are
  still queued for vault analysis are shown and can still be loaded.
- **Refresh** — rereads the index after the vault has analysed more samples or imported new
  folders.

The vault remains the owner of its database; VRAC+ only reads it.

### Global, presets and MIDI

- **GLOBAL** — fine tune, global Drumify macro, master volume, transpose, length scale, tone,
  DJ-style filter and stereo width.
- **Preset menu** — six factory kits: BIG CAT, NIGHT KITCHEN, BODY TALK, IRON WORKS, SHORT
  CIRCUIT, DUST LOOP.
- **SAVE / LOAD** — `.vracpreset` files stored under `%APPDATA%\VRACPlus\Presets`. The host
  session also stores the complete kit, slice map, MIDI mappings and — for user/recorded
  samples — the sample PCM itself.
- **UNDO / REDO** — 50 steps of parameter, slice, pad-start and selection changes. Source changes
  and factory presets intentionally reset the undo history.
- **MIDI learn** — right-click any knob, choose **MIDI Learn**, then move a MIDI CC. CC mappings
  are saved with the plugin state. Use the same menu to cancel learn.

### Outputs

The plugin exposes eight stereo output buses plus an optional stereo sidechain input. In a DAW,
output 1 is the main bus and outputs 2–8 can be enabled for per-pad routing. If the host only
enables the main output, inactive buses are folded into output 1 with a soft clip.

---

## Architecture

```text
Source/
  Params.*              parameter IDs, defaults, factory model metadata
  Presets.*             six factory kits
  Sequencer.*           4-pattern/32-step model, song chain, micro-timing, flams,
                        parameter locks, per-lane polymeter, presets and MIDI export
  VaultLibrary.*        read-only SKEER Sample Vault SQLite index discovery and queries
  PluginProcessor.*     APVTS, sample/slice state, 32-voice engine, 8-bus host output,
                        sequencer scheduling, recording, MIDI learn, undo/redo, state
  PluginEditor.*        resizable editor at base 1260×840, scaled via transform,
                        light/dark theme switching
  Engine/
    SampleData.*        embedded/user sample loading, 24-bit WAV helpers
    Slicer.*            RMS-flux transient detector
    Dsp.h               RBJ biquads and master filter/tone/width chain
    VoiceEngine.*       32-voice fixed pool, 16 model synthesizers, stereo bus rendering
  UI/
    Theme / LookAndFeel / Controls / WaveformDisplay / PadGrid / PadsPanel /
    SourcePanel / PadEditorPanel / HeaderBar / FooterBar / SequencerGrid / SequencerPanel /
    VaultPanel
Tests/
  SmokeTest.cpp         60 audio/state checks
  LayoutTest.cpp        editor bounds + PNG snapshots
  HostTest.cpp          scans and loads the built VST3 through JUCE's host path
```

- 32 fixed voices; no audio-thread allocation or locking.
- A shared sample pointer is published atomically, so a source swap never interrupts a sounding
  voice.
- MIDI note events split the block at their sample position, so timing is sample-accurate.
- The sequencer uses per-pad step history, so micro-timed events can cross audio block boundaries
  without being lost or doubled.
- UI triggers use a lock-free FIFO; MIDI CC learn uses a lock-free FIFO and an AsyncUpdater.
- Audio is folded to the main output only when the host does not expose the aux buses.
- The project builds with the static MSVC runtime.

---

## Limitations / notes

- VST3 and standalone only; no AU/AAX.
- Recording uses the optional sidechain input bus. If a host does not expose it, load a file
  instead.
- Multi-output routing requires the host to enable the additional output buses.
- Native preset files include custom-sample PCM; they can be several MB for long recordings.
- The native build does not include the browser edition's WAV/ZIP export; render or bounce from
  the DAW instead.
- MIDI pattern export writes notes, velocity, nudge, flams and ratchets. Probability rolls and
  parameter locks are performance features and are not encoded in the MIDI file.
- Song mode uses eight chain slots, one pattern-length each. Parameter locks currently cover
  pitch, volume, pan, tone and length.
- The Sample Vault link is read-only and uses the Windows SQLite engine. A queued or
  unanalysed vault entry can still be browsed and loaded, but only analysed entries have
  metadata, tags and indexed waveform previews. Loading a sample into VRAC+ still performs the
  instrument's own slice detection on that one sample.
- The factory library is original procedural material generated for this project. The real
  VRAC+ library was produced with foley artist Olivier Thys; this project is a tribute with its
  own recipes.

## Credits / disclaimer

SKEER wordmark and brand direction are used for the SKEER instrument line. IBM Plex Sans, IBM
Plex Mono and Barlow Condensed are bundled under the SIL Open Font License. JUCE is used under
its applicable license.

VRAC+ is an independent interpretation inspired by VRAC+ by Beatsurfing. Not affiliated with or
endorsed by Beatsurfing / Druw Audio. The factory sound library is original procedural material
generated for this project. The browser edition lives at the repository root and is documented
in the root `README.md`.
