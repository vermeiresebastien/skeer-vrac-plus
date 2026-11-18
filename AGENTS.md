# Working rules for this repository

SKEER SliceForge is two editions of one instrument: a React/Vite browser app at the repo root
(`src/`, documented in `README.md`) and a JUCE 8 VST3 + standalone under `native/`
(documented in `native/README.md`). They are C++ and TypeScript implementations of the same engine design — there is no shared
source, so a parameter or range change usually belongs in both. "Usually" is not "always": the
sequencer, the Sample Vault link and the sidechain-input recording are native-only, and `src/`
contains no sequencer at all (`grep -ri "sequencer|pattern" src/` is empty). Mirror a change in
both editions when both have the surface; when only one does, say so instead of building a second
copy of a feature unasked.

## One launch folder

Every runnable deliverable lives in `launch/` at the repository root. Never point anyone at
`native/build/SliceForge_artefacts/Release/…`.

```
launch/SliceForge.exe                       standalone instrument
launch/SliceForge.vst3/                     whole VST3 bundle folder, not the inner DLL
launch/SliceForge-1.0.0-Windows-x64.zip     portable package
launch/start.bat / start.ps1                install + standalone + browser edition
```

- Refresh it with the explicit CMake target, never a `POST_BUILD` copy:

  ```powershell
  cmake --build native/build --config Release --target launch
  ```

  It runs `native/package-launch.ps1`. A post-build copy would turn Ableton's lock on a loaded
  VST3 module into a failed compile, hiding the result of the build being tested.
- The script stages its copies, SHA-256-compares the stage against the build tree, and only then
  replaces what is already in `launch/`; the zip is written last. Nothing in `launch/` is deleted
  before its replacement has been proven, so a failed export leaves the previous deliverables
  standing instead of emptying a tracked folder. The zip is restamped from the `launch/` binaries, the two start
  scripts, the four doc/licence files and the pictures `native/README.md` itself lists, and every archive member is hashed back
  against the file it claims to be. Timestamps are not evidence: a skipped copy leaves an old mtime
  on a file that exported fine, and a byte-identical file keeps the mtime of the copy that was
  skipped. The CMake target passes `-Config "$<CONFIG>"`, so a Debug build exports Debug binaries
  rather than quietly shipping whatever Release artefact was already on disk.
- The export refuses to run while `SliceForge.exe` is running. Close it rather than killing it — the
  standalone has no autosave.
- `launch/start.ps1` installs into one folder: `%ProgramFiles%\Common Files\VST3`, the system VST3
  path every host scans by default. Never install into `%USERPROFILE%\Documents\VST3` or
  `%LOCALAPPDATA%\Programs\Common\VST3` — a second copy buys nothing (the system folder is already
  scanned) and produces duplicate browser entries plus two builds that drift apart. A stray
  `SliceForge.vst3` found in either of those is only deleted when it is byte-identical to the
  installed bundle; a stray whose module hash differs is a build that exists nowhere else, so it is
  moved to `%LOCALAPPDATA%\SliceForge\retired\<yyyyMMdd-HHmmss>-<module-hash8>` instead. Retiring
  beats deleting: "superseded" and "the last copy of it" look identical until you try to rebuild
  them. The script elevates itself through one UAC prompt for the copy step only, and reads the
  installed module back against `launch/` before reporting success; `-InstallOnly` exits non-zero
  when it did not land. Override the destination with `-Vst3Dir`, e.g. for a host with a custom
  scan root. A folder locked by an open DAW is skipped rather than the host terminated. `-DryRun`
  prints the plan.
- `launch/` is tracked in git here (owner decision, 2026-11-14). This deliberately differs from
  the other skeer-audio projects, where `launch/` is fully gitignored; it costs ~75 MB of blobs
  and there is no `.gitattributes`, so Git LFS is configured but inactive.
- There is no Inno Setup installer in this repo (verified 2026-11-15: no `*.iss` anywhere). The
  things that can place a bundle are `launch/start.ps1` and, for the repo folder only,
  `native/package-launch.ps1`; `COPY_PLUGIN_AFTER_BUILD` is `FALSE` (`native/CMakeLists.txt:119`)
  so the build never writes to a scan folder. If an installer is ever added, the grep-smell for a
  second destination is `{userdocs}\VST3`, `{localappdata}\Programs\Common\VST3`, a `Check=`/`Tasks=`
  on the `[Files]` line, and `PrivilegesRequired=lowest` or
  `PrivilegesRequiredOverridesAllowed` — all four mean the fallback copy is still being offered.
- The bundle is self-contained: factory audio and brand assets are compiled in as the
  `SliceForgeAssets` binary-data target (`native/CMakeLists.txt:49`, linked at `:130`), so the
  module needs **nothing** next to it. This is what makes a machine-wide install safe. Do not add
  a payload file that is discovered relative to the module: for the VST3 that path is the *host's*
  folder, so it would work in the standalone and silently fail in every DAW. Anything shipped with
  the product belongs in binary data; anything the user owns belongs in `%APPDATA%\SliceForge`,
  which the plugin writes from the host's own (unelevated) process.

## Commands

```powershell
npm install
npm run dev              # worklet + vite, http://127.0.0.1:5173 (strictPort)
npm run build            # worklet + tsc -b + vite build; tsc -b is the only type gate
npm test                 # vitest run
npm run test:browser     # needs npm run dev in another terminal; playwright-core + installed Chrome
npm run test:screenshots # regenerates docs/screenshots

cmake -S native -B native/build -G "Visual Studio 18 2026" -A x64
cmake --build native/build --config Release --target SliceForge_VST3 SliceForge_Standalone --parallel 2
cmake --build native/build --config Release --target launch
ctest --test-dir native/build -C Release --output-on-failure
```

There is no lint or format tool and no CI. Do not add a formatter's opinion as a rule.

## Non-negotiables

- `native/Resources/` stays tracked: `native/CMakeLists.txt` lists every file there as a binary
  resource, so a checkout without them cannot configure.
- `COPY_PLUGIN_AFTER_BUILD` stays `FALSE`; installing is an explicit step.
- Product name is SliceForge, but these identifiers are frozen and must never be renamed — they
  are data keys, not branding:
  - `PLUGIN_CODE Vrac` and `PLUGIN_MANUFACTURER_CODE Erwm` (`native/CMakeLists.txt:109-110`): the
    pair is the VST3 module identity in the host session; changing either orphans every saved set.
  - `"VRACPlus"` (the APVTS ValueTree type, `native/Source/PluginProcessor.cpp:75`) and
    `"VRAC_EXTRA"` (`:1043`, `:1047`, `:1116`, `:1121`): the host-state XML the plugin reads back.
    Renaming them silently drops slices, sequencer and custom-sample data.
  - `0x56524143` / `'VRAC'` in `native/Source/Sequencer.cpp:12`: the magic written into and
    validated on `.vracpattern` files.
  - the `.vracpreset` / `.vracpattern` extensions: users' on-disk presets and patterns. The
    folder holding them is not frozen — it moved to `%APPDATA%\SliceForge` on 2026-11-14 (all under
    `native/`: `Source/UI/Theme.cpp:75`, `Source/PluginEditor.cpp:461`, `:491`, and
    `Source/VaultLibrary.cpp:143`), with the old `VRACPlus` contents copied over and hash-verified.
    Nothing reads the old path any more, so a checkout on a machine that still has one needs the same
    one-time copy.
  - Web storage keys `vrac-plus.presets.v1`, `.theme.v1`, `.midi.v1` (`src/state/storage.ts:5-7`),
    IndexedDB `vrac-plus` (`src/state/sampleStore.ts:7`), `'vrac-plus-preset'` and
    `_vrac-kit.zip` download names — renaming any of these loses saved kits or MIDI maps.
  - `VRAC+` when it refers to the *commercial* Beatsurfing product (disclaimers, the
    `beatsurfing.com/audio-plugins/vrac-plus` link). That is someone else's product name.
- Modules shared with the tests stay DOM- and Node-free (`src/types.ts`, `src/audio/engineCore.ts`).
- Report a build or test result from that command's own exit status and log. A piped command
  reports the pipe's status, and a background GUI process exiting 0 means the window closed.
- Three idioms of the native smoke suite, each found the hard way. `check (cond, name, detail)`
  **evaluates** its detail argument even when the check passes, so a render, a parse or any other work
  belongs in a named local above the check and not inside the string. The suite's list builders append a
  space after *every* element, so an expectation literal is `"36 "` and `"931 960 1080 "`, never `"36"` —
  a missing trailing space makes a correct exporter look broken. And `MidiMessage::getVelocity()` returns
  the raw 0..127 **byte** (`juce_MidiMessage.h:331`), while `getFloatVelocity()` (`:340`) returns 0..1 —
  multiplying the first by 127 yields 16129.

## Known drift — do not trust these without re-checking

- The native build tree was rebuilt from scratch on 2026-11-14 after the rename: the old
  `native/build/CMakeCache.txt` still recorded the pre-relocation root
  `D:/CURSOR LOCAL PROJECTS/Midi Tether/native/build`, which is why configuring that tree failed
  with "does not match the source used to generate cache". The initial commit's claim that the
  native build "needed no repair for the relocation" was wrong for that cache. If configure ever
  fails that way again, delete `CMakeCache.txt` and `CMakeFiles/` (or the whole `build/`, since
  `_deps/juce-build` caches the same paths) and reconfigure.
- `native/shots/*.png` is tracked (ignored until 2026-11-17) and the zip carries the pictures the native
  README links, so both READMEs render in a clone and in the package. `native/artifact/` is gone; its zip is the `launch/` archive.
- Committed docs lag the tree in this project. Verify a path in the current tree before quoting it.
  The same goes for the numbers in it, but only for some of them: the smoke total and the CPU figure
  are claims to re-measure, not facts — `native/Tests/SmokeTest.cpp` prints both and asserts neither,
  because a wall-clock bound fails on a loaded machine rather than on an engine that got slower. The
  exposed-parameter count is the exception, and the one that used to drift: `native/Tests/HostTest.cpp:110`
  asserts exactly 350 host-visible parameters — 348 from `createParameterLayout` (9 globals before the pad
  loop, 21 for each of 16 pads, the 2 output-stage controls, then `seq.pattern` appended last) plus the
  VST3 wrapper's own
  `Bypass` and `Program` — so a README that says 349 is now a failing test, not a stale document. Quote the
  total as 350 and name the composition; a bare number is how the two got confused in the first place.
  `native/Tests/SmokeTest.cpp` asserts the bare-processor view, 348, and the two differ by exactly the
  wrapper's pair: 348 and 350 are both current, and neither is a typo for the other.
- **Which repo owns which material.** SKEER SliceForge owns *one continuous recording becomes a playable
  16-pad kit* — the single-file auto-slice-into-pads path. SKEER redrum owns *already-made kit pieces* —
  ingest of pre-existing one-shots from a folder. The discriminator is the material type at the door: one
  continuous recording versus a folder of pre-cut hits. The two are complementary, not competing, so a
  feature that starts from a folder of hits belongs to redrum whatever this UI looks like, and a feature
  that starts from one take belongs here.
- `ctest --test-dir native/build -C Release` runs whatever binaries are already in the tree; it does
  not build. After editing UI layout, a bare ctest can pass against a stale `SliceForgeLayoutTest.exe`
  and rewrite `native/shots/*.png` without your change in them — which reads as "the control did not
  render". Build the test targets first (`cmake --build native/build --config Release`) when a
  snapshot or a check total looks wrong for no reason.
- MSVC intermittently fails a parallel compile with `D8040: error creating or communicating with
  child process`, sometimes alongside a bogus `C1001` internal compiler error in an unrelated file.
  Re-running the same target sequentially succeeds; it is not a code problem.
- `%LOCALAPPDATA%\Programs\Common\VST3\VRAC+.vst3` was removed on 2026-11-14. It carries the same
  `Erwm`/`Vrac` CID as SliceForge, so any machine still holding it lists two plugins for one
  identity until it is deleted.
- The bundle lived in `%USERPROFILE%\Documents\VST3` from the rename until 2026-11-15, when the
  install location was moved to `%ProgramFiles%\Common Files\VST3` and the `Documents` copy removed.
  `start.ps1` prunes that duplicate on every run. Other SKEER plugins moved to the system folder
  the same day, but `SKULPT.vst3` still exists only in `Documents\VST3` — so retire the duplicate
  *bundle*, never the scan path: telling a host to stop scanning `Documents` would unregister
  plugins that live nowhere else.
- `launch/start.ps1` overwrites whatever previous build is in the destination, so keep at most one
  `SliceForge*` folder in a scan directory — a `.old-build` sibling is not scanned, but it is still
  clutter; `%LOCALAPPDATA%\SliceForge\previous-build\` is where one was parked on 2026-11-14.
- The local folder is still `D:\Projects\skeer-audio\vrac-plus` and cannot be renamed from inside a
  live session: the app holds the workspace root, so the root returns "Device or resource busy"
  while subdirectories move fine. `D:\Projects\skeer-audio\move-to-sliceforge.ps1` does the move
  with Qoder closed, then repairs what recorded the old path (wipes `native/build` and
  `node_modules\.vite`, reconfigures, rebuilds, runs ctest, re-exports and hash-checks `launch/`).
- `origin` is `https://github.com/vermeiresebastien/skeer-vrac-plus.git`. Renaming that is a
  GitHub-side change that moves the public URL, so it is separate from the local rename.
- Six text files are now archive members, so their bytes are what a customer receives and
  `package-launch.ps1` hashes each one back: `launch/start.ps1`, `launch/start.bat`, `LICENSE`,
  `native/Package/INSTALL.txt`, `native/Package/THIRD-PARTY-NOTICES.txt` and `native/README.md`. That makes
  line endings a shipping property. Measured here: `core.autocrlf=true` with no `.gitattributes`, `start.bat`
  is `i/lf w/crlf` and the other five are `i/lf w/lf` — so Git will hand a fresh checkout CRLF versions
  of files this worktree holds as LF, and a zip member hash quoted from one layout is not
  reproducible from the other. Run `git ls-files --eol` before quoting a packaged file's hash, and
  settle it properly with the Step 17 `.gitattributes`/LFS decision rather than by normalising
  anything quietly.
- A JUCE plugin's Windows version resource is generated from a configure-time manifest: `juce_add_plugin`
  writes `native/build/SliceForge_artefacts/JuceLibraryCode/Info.txt` inside its own call, and `juceaide`
  turns that into `SliceForge_resources.rc` at build time with no declared dependency on the manifest. So a
  metadata field added to `juce_add_plugin` needs BOTH a re-configure *and* a deleted `..._resources.rc`
  (plus its `SliceForge_resources.res`) before it reaches the module: the build right after such an edit
  exits 0 and ships the old resource. `LegalCopyright` is the `COMPANY_COPYRIGHT` argument
  (`native/CMakeLists.txt:105`); an empty value omits the field from the .rc entirely.
- The archive is written with a pinned entry clock: one `DateTimeOffset(1980, 1, 1, 0, 0, 0, local base offset)`
  stamped on all 17 members before `entry.Open()` (`native/package-launch.ps1:201-218`), because an unpinned
  clock made every export a different ~28 MB tracked blob for identical payload. The cost of pinning is that
  every extracted file is dated 1980-01-01 and `Copy-Item` carries that date onto the install, so
  `Install-Vst3Bundle` restamps the landed bundle *after* both hash checks (`launch/start.ps1:156-163`): a host
  that decides whether to re-read a cached plug-in from the file's mtime must see a new build as new. Never move
  that restamp above the checks — bytes are the proof, the date is only a scan cue — and never extend it outside
  the scan folder: files in the user's own download folder are not ours to redate.

## Import ceilings are one number per edition

`native/Source/Engine/ImportLimits.h` and `src/lib/importLimits.ts` are the only places the take length (300 s), the
file length (600 s) and the browser byte cap are stated. Change them there and nowhere else: every door calls the
capacity function or imports the constant (`PluginProcessor::toggleRecording`, `SampleData.cpp`'s `decodeReader`,
`src/lib/audioFiles.ts`, `RecordModal.tsx`), and `src/lib/__tests__/importLimits.test.ts` parses the C++ header — so a
native-only edit reddens `npm test`. The byte clamp is not a second promise, it is what stops a duration becoming a
giant allocation, so raising the seconds while leaving the clamp *shortens* the take (48 MiB with a 300 s promise is
131 s at 48 kHz and 33 s at 192 kHz). At 192 kHz the clamp does bind, and the REC tooltip states what that rate
actually allows; keep the tooltip honest instead of printing "5 minutes".

`SliceForgeSmokeTest` links `/STACK:16777216` (`native/CMakeLists.txt:144-151`). Its `main()` holds roughly 1 MB of
engine objects by value in one frame — 21 processors at 16,240 B, 11 `SeqBlockOutput` at 10,264 B — so an exit of
`0xC00000FD` is the harness frame, not the engine; check the frame before suspecting a change. Production threads are
the ones that must not carry a 96 KB `SequencerData` on the stack: `serialisePattern` and
`SequencerPanel::writeMidiToFile` are heap copies for that reason. The suite flushes stdout inside `check()` because a
4 KB full buffer truncates a crashing run at a buffer boundary and names a check that never ran as the last one.
