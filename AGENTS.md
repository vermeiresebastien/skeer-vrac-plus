# Working rules for this repository

SKEER SliceForge is two editions of one instrument: a React/Vite browser app at the repo root
(`src/`, documented in `README.md`) and a JUCE 8 VST3 + standalone under `native/`
(documented in `native/README.md`). They are parallel C++ and TypeScript implementations of the
same engine design — there is no shared source, so a parameter or range change usually belongs in
both.

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
- The script deletes its previous export before copying so a stale file cannot survive a build,
  restamps the zip from `launch/` binaries plus `native/Package/INSTALL.txt` and
  `native/README.md`, and SHA-256-compares every copy against its build-tree original.
  Timestamps are not evidence: a skipped copy leaves an old mtime on a file that exported fine,
  and a byte-identical file keeps the mtime of the copy that was skipped.
- The export refuses to run while `SliceForge.exe` is running. Close it rather than killing it — the
  standalone has no autosave.
- `launch/start.ps1` installs into whichever VST3 folder already contains `SliceForge.vst3` (checked:
  `%USERPROFILE%\Documents\VST3`, `%LOCALAPPDATA%\Programs\Common\VST3`,
  `%ProgramFiles%\Common Files\VST3`) and falls back to `Documents\VST3`, where the other SKEER
  plugins are scanned, when none does; override with `-Vst3Dir`. It skips a folder locked by an
  open DAW instead of terminating the host. `-DryRun` prints the plan.
- `launch/` is tracked in git here (owner decision, 2026-11-14). This deliberately differs from
  the other skeer-audio projects, where `launch/` is fully gitignored; it costs ~75 MB of blobs
  and there is no `.gitattributes`, so Git LFS is configured but inactive.

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
  - `PLUGIN_CODE Vrac` and `PLUGIN_MANUFACTURER_CODE Erwm` (`native/CMakeLists.txt:105-106`): the
    pair is the VST3 module identity in the host session; changing either orphans every saved set.
  - `"VRACPlus"` (the APVTS ValueTree type, `native/Source/PluginProcessor.cpp:23`) and
    `"VRAC_EXTRA"` (`:932`, `:936`, `:994`, `:999`): the host-state XML the plugin reads back.
    Renaming them silently drops slices, sequencer and custom-sample data.
  - `0x56524143` / `'VRAC'` in `native/Source/Sequencer.cpp:12`: the magic written into and
    validated on `.vracpattern` files.
  - the `.vracpreset` / `.vracpattern` extensions: users' on-disk presets and patterns. The
    folder holding them is not frozen — it moved to `%APPDATA%\SliceForge` on 2026-11-14
    (`Theme.cpp:75`, `PluginEditor.cpp:446`, `:476`, `VaultLibrary.cpp:143`), with the old
    `VRACPlus` contents copied over and hash-verified. Nothing reads the old path any more, so a
    checkout on a machine that still has one needs the same one-time copy.
  - Web storage keys `vrac-plus.presets.v1`, `.theme.v1`, `.midi.v1` (`src/state/storage.ts:5-7`),
    IndexedDB `vrac-plus` (`src/state/sampleStore.ts:7`), `'vrac-plus-preset'` and
    `_vrac-kit.zip` download names — renaming any of these loses saved kits or MIDI maps.
  - `VRAC+` when it refers to the *commercial* Beatsurfing product (disclaimers, the
    `beatsurfing.com/audio-plugins/vrac-plus` link). That is someone else's product name.
- Modules shared with the tests stay DOM- and Node-free (`src/types.ts`, `src/audio/engineCore.ts`).
- Report a build or test result from that command's own exit status and log. A piped command
  reports the pipe's status, and a background GUI process exiting 0 means the window closed.

## Known drift — do not trust these without re-checking

- The native build tree was rebuilt from scratch on 2026-11-14 after the rename: the old
  `native/build/CMakeCache.txt` still recorded the pre-relocation root
  `D:/CURSOR LOCAL PROJECTS/Midi Tether/native/build`, which is why configuring that tree failed
  with "does not match the source used to generate cache". The initial commit's claim that the
  native build "needed no repair for the relocation" was wrong for that cache. If configure ever
  fails that way again, delete `CMakeCache.txt` and `CMakeFiles/` (or the whole `build/`, since
  `_deps/juce-build` caches the same paths) and reconfigure.
- `native/shots/*.png` is gitignored, so the image links in both READMEs are broken in a fresh
  clone. `native/artifact/` no longer exists — its zip became `launch/SliceForge-1.0.0-Windows-x64.zip`.
- Committed docs lag the tree in this project. Verify a path in the current tree before quoting it.
- `%LOCALAPPDATA%\Programs\Common\VST3\VRAC+.vst3` was removed on 2026-11-14; the bundle is
  installed at `%USERPROFILE%\Documents\VST3\SliceForge.vst3`, hash-matched against `launch/`. It
  carries the same `Erwm`/`Vrac` CID, so any other machine with the old copy still lists two plugins
  for one identity until it is deleted.
- The local folder is still `D:\Projects\skeer-audio\vrac-plus` and cannot be renamed from inside a
  live session: the app holds the workspace root, so the root returns "Device or resource busy"
  while subdirectories move fine. `D:\Projects\skeer-audio\move-to-sliceforge.ps1` does the move
  with Qoder closed, then repairs what recorded the old path (wipes `native/build` and
  `node_modules\.vite`, reconfigures, rebuilds, runs ctest, re-exports and hash-checks `launch/`).
- `origin` is `https://github.com/vermeiresebastien/skeer-vrac-plus.git`. Renaming that is a
  GitHub-side change that moves the public URL, so it is separate from the local rename.
