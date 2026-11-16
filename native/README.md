# SKEER SliceForge — Native VST3 / Standalone

**One sound. One drum kit.** The SKEER-branded native edition turns any recording into a
sixteen-pad instrument with sixteen drum models, per-pad morphing, eight stereo outputs and a
host-synced 32-step sequencer with pattern chaining, polymeter, parameter locks and MIDI export.

![SKEER SliceForge native editor](shots/editor-light.png)

**SEQUENCER page**

![SKEER SliceForge step sequencer](shots/sequencer-light.png)

**Step inspector — nudge, flams and parameter locks**

![SKEER SliceForge step inspector](shots/sequencer-inspector.png)

**SKEER Sample Vault page**

![SKEER SliceForge Sample Vault browser](shots/vault-light.png)

**Dark mode**

![SKEER SliceForge dark editor](shots/editor-dark.png)

![SKEER SliceForge dark sequencer](shots/sequencer-dark.png)

![SKEER SliceForge dark step inspector](shots/sequencer-inspector-dark.png)

![SKEER SliceForge dark Sample Vault](shots/vault-dark.png)

---

## Status

| | |
| --- | --- |
| Formats | VST3 instrument and standalone, x64 Windows |
| Framework | JUCE 8.0.4, C++20 |
| Vendor / bundle | SKEER / `com.skeer.sliceforge` |
| Host requirements | VST3 host with MIDI input and multi-output support. The sequencer also plays out over a MIDI event bus, which a host that has already scanned the plugin will only see after a reload. |
| Sample Vault link | Optional read-only SKEER Sample Vault index with click-to-audition playback (auto-detected) |
| Branding | SKEER logo, IBM Plex Sans / IBM Plex Mono, Barlow Condensed, SKEER palette |
| Theme | Light and dark SKEER skins, switched from the header and remembered per user |
| Build toolchain | Visual Studio 18 2026 / MSVC 14.51, CMake 3.22+ |
| Installed plugin | `%ProgramFiles%\Common Files\VST3\SliceForge.vst3` |
| Installed standalone | `%LOCALAPPDATA%\Programs\SliceForge\SliceForge.exe` |
| Launch folder | `..\launch\` — `SliceForge.exe`, `SliceForge.vst3`, `SliceForge-1.0.0-Windows-x64.zip`, `start.bat` |
| Test suite | 190 engine/state checks, editor layout snapshots, VST3 host load |

The plugin bundle is self-contained (factory audio and SKEER brand assets are embedded, no
external resource folder). The build is about **24 MB**. The VST3 session identity is preserved
from earlier SliceForge builds, so existing host sessions resolve to this SKEER-branded build; the
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
cmake --build native/build --config Release --target SliceForge_VST3 SliceForge_Standalone --parallel 2
cmake --build native/build --config Release --target launch
```

If you are already inside `native/`:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --target SliceForge_VST3 SliceForge_Standalone --parallel 2
cmake --build build --config Release --target launch
```

The `launch` target runs `package-launch.ps1`, which refreshes the repository-root `launch/`
folder: it re-copies `SliceForge.exe`, replaces the whole `SliceForge.vst3` bundle folder, restamps the
portable zip from `Package/INSTALL.txt` and this README, and SHA-256-compares each copy against
its build-tree original. It is a separate target rather than a post-build step because Ableton
holds a loaded VST3's DLL open, and a build-time copy turns that lock into a failed compile that
hides the result you were trying to test. It also refuses to export while `SliceForge.exe` is running,
instead of killing a session that has no autosave.

`launch/` is the only place to look for something runnable:

```text
launch/SliceForge.exe                       standalone instrument
launch/SliceForge.vst3/                     VST3 bundle (Contents\x86_64-win\SliceForge.vst3)
launch/SliceForge-1.0.0-Windows-x64.zip     portable package
launch/start.bat                            install + standalone + browser in one click
```

`COPY_PLUGIN_AFTER_BUILD` is deliberately disabled, so the build never writes to a system
VST3 folder by itself; installing is the explicit step below, or `start.bat`.

### Install

`launch\start.bat` does the install, the standalone and the browser edition together. The install
has one destination, `%ProgramFiles%\Common Files\VST3` — the system VST3 folder every host scans
by default — so it asks for elevation with a single UAC prompt covering the copy step only, never
the standalone or the dev server. It stages the bundle beside its target, SHA-256-compares the
staged module against `launch\`, swaps, then removes any `SliceForge.vst3` left in
`%USERPROFILE%\Documents\VST3` or `%LOCALAPPDATA%\Programs\Common\VST3`, because a second copy
only produces a duplicate browser entry and two builds that drift apart. A folder locked by an
open DAW is skipped rather than the host terminated, and a dev server already listening on 5173 is
reused. `launch\start.ps1 -DryRun` prints that plan without touching anything; `-InstallOnly` runs
step 1 alone and exits non-zero if the bundle did not land.

For the manual path, system-wide (the terminal must be elevated):

```powershell
Copy-Item "launch\SliceForge.vst3" "$env:ProgramFiles\Common Files\VST3\" -Recurse -Force
```

Only if a host genuinely will not scan the system folder, point it there instead of installing a
second copy beside it. `-Vst3Dir` exists for a machine whose scan root really is somewhere else.

Then rescan VST3 plugins in your DAW. The pre-rename `VRAC+.vst3` carried the same VST3 session
identity as SliceForge and was deleted from `%LOCALAPPDATA%\Programs\Common\VST3` on 2026-11-14; on
a machine that still has it, delete that copy once the new bundle scans, or the host lists two
entries for one plugin. If a previous build is still open in a DAW, close the DAW before replacing
a `.vst3` bundle and rescan after restarting it.

Standalone: run `launch\SliceForge.exe` directly, or install a copy:

```powershell
Copy-Item "launch\SliceForge.exe" "$env:LOCALAPPDATA\Programs\SliceForge\SliceForge.exe" -Force
```

---

## Test

```powershell
# from the repository root
ctest --test-dir native/build -C Release --output-on-failure
```

Individual tests:

```powershell
native\build\SliceForgeSmokeTest_artefacts\Release\SliceForgeSmokeTest.exe
native\build\SliceForgeLayoutTest_artefacts\Release\SliceForgeLayoutTest.exe
native\build\SliceForgeHostTest_artefacts\Release\SliceForgeHostTest.exe
```

The host test asks JUCE's VST3 hosting path to scan and instantiate the built bundle, asserts the
**350** parameters a host sees once the VST3 wrapper appends its own `Bypass` and `Program` on top of
the 348 that `createParameterLayout` builds (9 globals before the pad loop, 21 for each of 16 pads,
the 2 output-stage controls, then `seq.pattern` appended last), pins `seq.pattern` to the last layout
slot by index and step count, accepts an eight-stereo-output bus layout,
processes MIDI note 36, checks the output is finite and non-silent, and round-trips the plugin
state. The total is asserted exactly, not printed and compared against a round number. The bare
processor asserts the 348 half, so both numbers are gated, in two different programs.

The same hosted instance carries the MIDI output bus. `producesMidi()` is asserted there because on a
hosted plugin JUCE derives it from the event output buses in the binary, so the check reads what a
host would actually route to rather than what this source claims about itself; the audio layout is
still `1 in / 8 out`. It then runs 48 host-synced blocks and requires what comes back out to be a
clean stream - equal note-ons and note-offs, nothing but notes, every one on channel 10 within
C1–D#2, every one inside the block that produced it, with the audio still non-silent. What it
deliberately does not assert is *how many* notes: a hosted instance exposes no trigger oracle, so the
exact event list, down to the sample offset and the velocity byte, is gated in `SliceForgeSmokeTest`.

The layout test writes light and dark snapshots for the kit, sequencer, step inspector and
Sample Vault pages, opens the step inspector through the real UI path and verifies that no child
control falls outside its parent.

`SliceForgeSmokeTest` covers:

- factory source loading and slicing
- all sixteen models producing audio
- all eight stereo output buses
- pan, velocity response, mute and solo gating
- sample-accurate MIDI note triggering
- sequencer MIDI output: the exact event list on the host's output bus - one note-on/note-off pair per
  trigger at the sample offset the scheduler reported, `36 + pad` on channel 10, the exporter's
  `round (velocity x 127)` byte, a 32-sample gate, nothing else in the buffer, nothing when the host
  transport is stopped, and no pass-through of the notes played at the plugin
- exported-file parity: the printed MIDI clip is the pattern the sequencer played - the same swing on
  the odd steps, the same lane mutes and solos, the same probability draws for one named pass, the same
  note map. Proven two ways: one fixture is rendered through the scheduler and both paths are asked the
  same questions, and the resulting tick lists are pinned as literals, because a differential alone
  stays green when the code the two paths share goes wrong together
- factory presets and full state round-trip
- undo/redo and randomisation
- MIDI CC learn and learned-parameter movement
- sidechain-input recording and recording adoption
- custom-sample state round-trip
- internal step-sequencer playback, host-synced transport, sequencer state and sequence undo
- step nudge, flams, per-lane polymeter, song-mode chaining and parameter locks
- extended sequencer state round-trip, factory/user pattern presets and MIDI export
- pattern duplicate: copies steps, ratchets, flams, nudges and locks plus lane mute/solo/loop to
  another slot, replaces the destination, leaves the source and the selection alone, refuses
  self-copies and out-of-range indices, reaches the engine, and is undone in one step
- SKEER Sample Vault index query, plus the sample-loading path proven on files the test writes
  itself: a generated wav loads with the right channel and sample counts, and a 4 KB macOS
  AppleDouble sidecar named `.wav` is refused without replacing the loaded kit. Which of a real
  music library's listed files still resolve is reported, not asserted — it is a property of the
  machine, not of this plugin
- vault audition playback: one-shot end, loop wrap, source-rate resampling, the output
  limiter and the bus-1 mix that carries it to the host
- output stage: bypassed is bit-exact, nothing is attenuated below the knee, the tanh leaves
  the knee at slope 1, full scale lands on 0.844, the curve is monotonic and bounded, OUT GAIN
  runs after the clipper and is range-clamped, the ±0.999 ceiling gets the last word, and the
  whole stage is measured through the processor on a known signal
- output stage state loaded from a kit saved before it existed (params stripped from the
  host-state XML) restores bypassed and still plays
- randomizer, Euclidean, parseable MIDI file and CPU-headroom checks

Current result: **190/190 passed** - that is what `SliceForgeSmokeTest.exe` prints on the tree
this file describes, so re-run it rather than trusting the number here - with the engine using
between **4.4% and 6.9% of one core** across separate runs of the same binary while continuously
triggering pads at 48 kHz / 512 samples (the one run behind that 190/190 printed 6.91%; the
three runs behind the 186/186 it replaced printed 4.59%, 4.57% and 4.77%, and the 175-check runs
before those printed 4.53%, 4.57% and 5.11% on three invocations of those bytes). The four
checks that took 186 to 190 add fixtures, not engine work, and the machine load of each run was
not measured, so the spread is not attributed to them. The spread is real, so the figure is
reported and never asserted: real-time safety rests on the finite-and-in-ceiling checks, not on
a wall-clock bound that measured the machine rather than the engine.

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
`%APPDATA%\SliceForge\theme.txt` and is applied the next time the plugin editor opens.

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

- **Patterns A–D** — four independent patterns with per-pattern lane mute/solo. Click a letter to
  edit and play it; right-click it to copy that pattern to another slot. Which one is live is a plugin
  parameter (**Active Pattern**, four choices, appended after every other control), so the host can
  automate it, learn it to a controller and recall it per position, and it travels with the saved
  session. The chips write that same parameter and mirror it back, so the UI and the host cannot
  disagree about what is playing.
- **Transport** — follows the host playhead and tempo; **PLAY** can preview on the internal
  clock when the host is stopped. **HOST** toggles host sync.
- **Length / division / swing** — 1–32 steps, 1/8, 1/8T, 1/16, 1/16T or 1/32, plus swing. One boundary is
  worth naming at the fine end: the plugin tells ordinary per-block jitter from a real transport jump with a
  tolerance of half a block, while a 1/32 step is a fixed size, so past about `block samples × BPM = 720 000`
  (4096 samples from roughly 176 BPM, 8192 from roughly 88, at 48 kHz) a jump of one whole step is read as
  continued playback.
  Nothing goes silent and edits still land — what does not happen is the hit-history reset, so a jump that
  lands on a step boundary can drop or repeat a single hit. Recorded as a known defect, with the gate that
  pins both sides of the seam, in `docs/next-steps-plan.md` section 8C.
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
- **Copy / Paste / Clear** — clipboard pattern-level operations. **COPY** takes the pattern you are
  editing and **PASTE** writes it over whichever pattern you switch to next; **CLEAR** empties the
  current one.
- **Copy to** — **COPY TO**, or a right-click on any pattern letter, duplicates one pattern into
  another slot in a single step, without going through the clipboard. The destination is replaced
  whole: steps, velocities, probabilities, ratchets, nudges, flams, parameter locks, lane mutes and
  solos, and lane loop lengths. The menu names the source and marks any destination that already
  holds a groove, so an overwrite is never a surprise. Your selection does not move, and one undo
  step brings back the pattern that was replaced. The song chain stores pattern *numbers*, so a
  chain slot pointing at the destination plays the copy from then on.
- **MIDI** — export a Standard MIDI File of the current pattern or song chain, or start a MIDI
  file drag into a host that accepts external file drops. The file is the groove you heard: swing,
  lane mutes and solos, probability dropouts, flams, ratchets and nudges are baked into it, and it uses
  the same `36 + pad` note map the plugin's own live MIDI out uses. One printed file is one *pass* — the
  plugin re-rolls its probability lanes every time the transport restarts, so the file names the pass it
  is a snapshot of inside itself, and looping that clip in your host replays that same set of dropouts.
- **Live MIDI out** — while the sequencer plays, every hit it schedules is also written to the
  plugin's own MIDI output bus as `36 + pad` on channel 10, at the sample offset the engine hears, so a
  drum rack or a second instrument can play along with the kit. There is no switch for it: the notes
  are the sequencer's own, and the grid's lane M/S is what takes them off the wire.
- **Keyboard** — `SPACE` starts/stops internal preview; `G` generates a random groove.

The sequence, chain, nudge/flams and parameter locks are saved with the host state and
`.vracpreset` files.

### SKEER Sample Vault page

SliceForge can read the **SKEER Sample Vault** index directly. It opens the vault's local SQLite
index read-only, so browsing and filtering do not decode or analyse every file again, and no
files are modified.

- **Automatic connection** — finds the installed vault database, the installer registry, a
  saved custom path, or a development checkout beside SliceForge. If the index is elsewhere, use
  **LOCATE DB** once; the path is remembered.
- **Browse** — search filenames, folders and indexed tags; filter by family, type, collection
  or favourites; sort by name, recency, duration or BPM.
- **Indexed detail** — family/type/BPM/key, duration, sample rate, channels, tags, notes,
  source path and the waveform generated by the vault's analysis.
- **Audition** — clicking a row decodes that file on a worker thread and plays it once on
  output bus 1, with a playhead over the real waveform. **PLAY** hears the same decode again
  without touching disk, **LOOP** repeats it until you stop it, and **STOP** cuts it. The
  audition is mixed after the master chain and limited, so it never pushes a playing kit
  above full scale. In a VST3 the host has to be processing audio for it to be heard — with
  the transport stopped and monitoring off the panel says so instead of pretending to play.
- **Load** — select a sample and press **LOAD SAMPLE** or double-click it. SliceForge loads the
  original source file into the current kit and returns to the KIT page. Samples that are
  still queued for vault analysis are shown and can still be loaded.
- **Refresh** — rereads the index after the vault has analysed more samples or imported new
  folders.

The vault remains the owner of its database; SliceForge only reads it.

### Global, presets and MIDI

- **GLOBAL** — fine tune, global Drumify macro, master volume, transpose, length scale, tone,
  DJ-style filter, stereo width, plus the output stage: **SOFT CLIP** and **OUT** (see
  *Outputs*).
- **Preset menu** — six factory kits: BIG CAT, NIGHT KITCHEN, BODY TALK, IRON WORKS, SHORT
  CIRCUIT, DUST LOOP.
- **SAVE / LOAD** — `.vracpreset` files stored under `%APPDATA%\SliceForge\Presets` (since the
  rename; earlier builds used `%APPDATA%\VRACPlus`). The host
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

Every enabled bus then passes through the same output stage, in this order:

1. **SOFT CLIP** (`global.clip`, 0–100 %) — a tanh saturator with a knee that starts at 0.999 and
   drops to 0.349 as the control opens. At 0 % the stage is bit-exact: nothing is added, nothing is
   attenuated. Above the knee the curve leaves at slope 1 and settles just under full scale, so a
   hot kit gets a rounded ceiling instead of a hard fold.
2. **OUT** (`global.out`, −24 to +12 dB) — plain output trim, applied *after* the clipper. It is a
   visible knob on purpose: the clipper never quietly makes up its own gain, so turning it up is
   always a decision you can see and undo.
3. **Ceiling** — a hard clamp at ±0.999 that always runs last, so no combination of the two knobs
   or a stacked host gain can hand the host a sample outside [−1, 1]. The folded main bus could
   previously reach about 1.25; it can no longer.

---

## Architecture

```text
Source/
  Params.*              parameter IDs, defaults, factory model metadata
  Presets.*             six factory kits
  Sequencer.*           4-pattern/32-step model, song chain, micro-timing, flams,
                        parameter locks, per-lane polymeter, pattern duplicate,
                        presets and MIDI export
  VaultLibrary.*        read-only SKEER Sample Vault SQLite index discovery and queries
  PluginProcessor.*     APVTS, sample/slice state, 32-voice engine, 8-bus host output,
                        sequencer scheduling, recording, MIDI learn, undo/redo, state,
                        vault audition mix on bus 1, per-bus output stage,
                        sequencer MIDI out on the host's event bus
  PluginEditor.*        resizable editor at base 1260×840, scaled via transform,
                        light/dark theme switching
  Engine/
    SampleData.*        embedded/user sample loading, 24-bit WAV helpers
    PreviewPlayer.*     vault audition: atomic hand-off, linear resample, loop wrap, limiter
    Slicer.*            RMS-flux transient detector
    Dsp.h               RBJ biquads, master filter/tone/width chain, OutputStage soft clipper
    VoiceEngine.*       32-voice fixed pool, 16 model synthesizers, stereo bus rendering
  UI/
    Theme / LookAndFeel / Controls / WaveformDisplay / PadGrid / PadsPanel /
    SourcePanel / PadEditorPanel / HeaderBar / FooterBar / SequencerGrid / SequencerPanel /
    VaultPanel
Tests/
  SmokeTest.cpp         190 audio/state checks
  LayoutTest.cpp        editor bounds + PNG snapshots
  HostTest.cpp          scans and loads the built VST3 through JUCE's host path
```

- 32 fixed voices; no audio-thread allocation or locking.
- A shared sample pointer is published atomically, so a source swap never interrupts a sounding
  voice.
- MIDI note events split the block at their sample position, so timing is sample-accurate.
- The sequencer uses per-pad step history, so micro-timed events can cross audio block boundaries
  without being lost or doubled.
- The sequencer's triggers reach the host twice over: as audio from the engine, and as one
  note-on/note-off pair on the MIDI event bus at the same sample offset, `36 + pad` on channel 10. The
  pair opens and closes inside the block that scheduled it, so no note can be stranded by a stopped
  transport or a pattern change, and the only memory involved is the host's own buffer growing to fit
  an event-heavy block.
- UI triggers use a lock-free FIFO; MIDI CC learn uses a lock-free FIFO and an AsyncUpdater.
- Audio is folded to the main output only when the host does not expose the aux buses.
- The project builds with the static MSVC runtime.

---

## Limitations / notes

- VST3 and standalone only; no AU/AAX.
- Recording uses the optional sidechain input bus. If a host does not expose it, load a file
  instead.
- Multi-output routing requires the host to enable the additional output buses.
- The MIDI output bus is part of what a host reads when it scans the plugin, so a host that already
  loaded SliceForge has to reload it (or rescan the folder) before the bus appears. Sessions saved
  against an earlier build still load; they just will not see the new bus until the plugin is
  re-instantiated.
- The MIDI output follows the sequencer, not the mixer: a pad muted with its **Mute** button still
  plays its notes out over MIDI, because the mute is an engine state and the step was genuinely
  scheduled. Only the pattern's own lane M/S and the step switch take a note off the wire.
- Native preset files include custom-sample PCM; they can be several MB for long recordings.
- The native build does not include the browser edition's WAV/ZIP export; render or bounce from
  the DAW instead.
- MIDI pattern export writes notes, velocity, nudge, flams and ratchets. Probability rolls and
  parameter locks are performance features and are not encoded in the MIDI file.
- Song mode uses eight chain slots, one pattern-length each. Parameter locks currently cover
  pitch, volume, pan, tone and length.
- The Sample Vault link is read-only and uses the Windows SQLite engine. A queued or
  unanalysed vault entry can still be browsed and loaded, but only analysed entries have
  metadata, tags and indexed waveform previews. Loading a sample into SliceForge still performs the
  instrument's own slice detection on that one sample.
- The factory library is original procedural material generated for this project. The real
  VRAC+ library was produced with foley artist Olivier Thys; this project is a tribute with its
  own recipes.

## Credits / disclaimer

SKEER wordmark and brand direction are used for the SKEER instrument line. IBM Plex Sans, IBM
Plex Mono and Barlow Condensed are bundled under the SIL Open Font License. JUCE is used under
its applicable license.

SliceForge is an independent interpretation inspired by VRAC+ by Beatsurfing. Not affiliated with or
endorsed by Beatsurfing / Druw Audio. The factory sound library is original procedural material
generated for this project. The browser edition lives at the repository root and is documented
in the root `README.md`.
