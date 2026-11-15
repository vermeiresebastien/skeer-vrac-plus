# SKEER SliceForge — next steps

Round 1 produced 2026-11-14 from eight parallel research lanes (DSP engine, sequencer, DAW
workflow, web/native parity, Sample Vault, platform/build/legal, market, frontier capability).
Steps 1-4 were built and falsification-tested the same day; Steps 5-21 were not.

Round 2, 2026-11-15, re-ran the fan-out from that post-fix state (nine lanes: post-fix regression
sweep, slicer, Live workflow and export, parity harness, sequencer next tier, sibling boundary,
licensing and release engineering, market positioning, web edition) and then verified every
load-bearing claim by hand before ranking. Eleven of thirteen held. Two round-1 claims did not,
and both are corrected in place below, because they changed a ranking: the headless
`autostem.exe` (item 5) and the "spectral flux" description of the slicer (Step 7). Line numbers
are current; effort is in focused dev-days. New work is Steps 22-29, in Phase 7.

As of 2026-11-15 the suite is green: **117/117 checks, 3/3 ctest targets, exit 0.** When this round was
written it was 113/114 with target 1 red, and Step 29 carries both the cause and the built fix. Do not
read the plan's "built" notes as "green today" without re-running `ctest` — `AGENTS.md` says committed
docs lag the tree, and this document is the latest example.

---

## 0. How this was decided

Each candidate move was scored on three questions: **does it make a claim in the READMEs true**,
**can you hear or see the difference in Ableton**, and **what does it cost to be wrong**. That
ordering is deliberate — this project's failure mode is not missing features, it is features that
are advertised, tested green, and silent in a real host.

Five findings drove the ranking. All four code ones are verified defects, not opinions.

1. **Flams and ratchets do not fire at a normal block size.** `native/Source/Sequencer.cpp:140`
   accepts a step only if its *base* event lands inside the current block, marks it consumed at
   `:145`, then computes sub-hit offsets at `:160` and `:170`. A flam sits `0.12 × stepSamples`
   (`kFlamStep`, `:14`) *before* the step. At 120 BPM, 1/16, 48 kHz, `stepSamples` is 6000, so the
   grace hit is at −720 samples; with a 512-sample block that offset is always negative and is
   dropped, and because the step was already marked as handled the next block never revisits it.
   Ratchets are the mirror case: 6000/4 = 1500 samples between sub-hits, so only `r = 0` survives.
   The tests hide this: `native/Tests/SmokeTest.cpp:494` and `:510` call
   `processSequencerBlock(..., 7000, ...)` and `:529`/`:531` use `720` — block sizes no host uses.
   Ratchet *scheduling* is never asserted at all. Two documented, sold features are dead in a
   stock Ableton session.
2. **Warp is sample-rate-frozen.** `native/Source/Engine/VoiceEngine.cpp:525-526` computes the
   warp one-pole coefficients from the literal `48000.0f`. `setSampleRate` exists (`:89`, called at
   `:555`) but never recomputes them, so every non-48 kHz host gets the wrong crossover — and the
   web edition, which *does* recompute (`src/audio/engineCore.ts:590`), disagrees with native at
   44.1 kHz and 96 kHz.
3. **The host never sees the sequencer.** `native/Source/PluginProcessor.h:34` returns
   `producesMidi() == false`. The pattern only leaves the plugin as an offline SMF file drag, which
   `native/Source/UI/SequencerPanel.cpp:922-931` already warns may fail in the host you are in.
   There is no MIDI clip, no Ableton session-browser content, no effect on the DAW's own meters.
   Round 2 named both mechanisms, because fixing one without the other does nothing: the bundle
   declares `NEEDS_MIDI_OUTPUT FALSE` (`native/CMakeLists.txt:113`) *and* `processBlock` ends with
   `midiMessages.clear()` (`PluginProcessor.cpp:390`), after every other stage has run.
4. **Native writes no audio at all.** `SampleData::writeStereoWav` (`native/Source/Engine/SampleData.cpp:163`)
   has exactly zero callers in the source tree — it is declared, compiled and unused. The web
   edition has a complete kit-render path (`src/audio/exportKit.ts`, `src/components/ExportModal.tsx`).
   So "print me sixteen one-shots from this recording" is the one workflow hole that blocks a
   session rather than slowing one, and the dead writer proves the work was already started.
5. **The strategic frame is not the one the READMEs assume — and it is wider than round 1 said.**
   SKULPT, a sibling in the same family, records itself as "feature complete and covered by tests":
   sample-modelling drum machine, Kit and Tonal modes, eight tracks with two *morphable* sources
   each, a polymetric sequencer with MIDI export, a modulation matrix, eight separate outputs, eight
   factory presets. That is most of SliceForge's second half. What SliceForge uniquely owns is the
   *first* half — one recording, auto-sliced across sixteen pads — plus the browser edition and the
   Sample Vault link. **Round 2 found a second sibling sitting on that first half too**: `redrum`
   ships as "Expressive Drum" 0.20.0 — 128 pads, drag/drop auto-slicing with editable
   recurring-sound groups, a v2 transient/body hybrid *resynthesis* engine, template-guided
   two-layer loop separation, and an internal sample-accurate pattern transport
   (`D:/Projects/skeer-audio/redrum/README.md`). So the family now has *three* engines that chop a
   recording and *two* that model one, and "the slicer is uniquely ours" is not true. Step 21 is the
   decision that sorts this out, and it is now a three-way boundary, not a two-way one.
   Round 1 also killed stem separation on a false fact — see the retraction below.

**Two round-1 claims retracted after verification.**

- *"Stem separation already ships next door as a headless `autostem.exe` with `run`/`verify`/`bench`."*
  False. `SeparatorFactory.IsImplemented (kind) => kind == EngineKind.Live`
  (`D:/Projects/skeer-audio/autostem/src/StemForge.Core/Engines/SeparatorFactory.cs:22`); the Local
  and Cloud catalogue rows are listed as not implemented and `Create` throws
  `EngineUnavailableException` for both. The only working engine drives Ableton's own separator
  through UI Automation, which needs Live 12.1+ and the desktop to itself. *Separation still does not
  belong in this plugin* — but for the honest reason (no local runtime: nothing of torch,
  onnxruntime, librosa, madmom or essentia is installed here), not because a headless tool exists.
- *"The slicer's failure mode is that it is time-domain rather than spectral."* Half true, stated
  wrongly. There is no FFT — verified, `grep` for `juce::dsp::FFT` / `performFFT` under
  `native/Source/` is empty in a repo that links `juce_dsp` (`native/CMakeLists.txt:129`) — but what
  `Slicer.cpp:16-44` actually runs is a **broadband RMS-envelope first difference**, which is not
  the same thing as spectral flux and will not become it by renaming. Calling it spectral cost the
  plan a step that assumed the gap was only the missing FFT. It is not: see Step 25 for the three
  non-spectral reasons it misses hits today.

---

## 1. Two things the research corrected

- **The parameter count in `native/README.md:155` is wrong: it is 347, not 349.** The layout is
  8 global + `source.sensitivity` + 21 parameters × 16 pads + `global.clip` + `global.out` =
  **347** (`native/Source/Params.cpp`, the pad loop adds exactly 21 ids). Nothing catches the
  drift: `native/Tests/HostTest.cpp:61` only *prints* the count and `:65` asserts `< 300`.
- **The naming risk the market lane flagged is mostly already retired.** The installed bundle is
  `SliceForge.vst3`; the `VRAC+.vst3` copy in `%LOCALAPPDATA%\Programs\Common\VST3` was deleted on
  2026-11-14. The `Vrac` / `Erwm` codes remain in the binary, but they are invisible session
  identity and renaming them would orphan every saved set — the frozen-identifier rule is correct
  and should stay. The open item is only whether "SliceForge" itself collides commercially
  (research surfaced a `sliceforge.io` beat-saber tool and a game-audio tool of the same name;
  **unverified** — check it before any public release, not before a personal build).

Four stale line references in `AGENTS.md` were confirmed wrong and re-confirmed today:
`native/CMakeLists.txt:105-106` should be `:106-107` (Erwm at 106, Vrac at 107), and the four
`VRAC_EXTRA` sites are `PluginProcessor.cpp:956`, `:960`, `:1018`, `:1023` — each about 24 lines off
what `AGENTS.md` says. Round 2 also asserted that `native/README.md:155` should read 347, not **349** —
that correction is retracted: `SliceForgeHostTest.exe` prints `parameters: 349` on this build, so the
README was right and the plan was wrong. What the README *was* wrong about is its check counts (109
against a 117-check suite) and "about 5% of one core"; both were fixed with Step 29 on 2026-11-15, and
`AGENTS.md`'s line references are still live work in Step 5. Everything else in `AGENTS.md`'s frozen list
checked out, including `Sequencer.cpp:12`, `storage.ts:5-7`, `sampleStore.ts:7`, `Theme.cpp:75` and
`VaultLibrary.cpp:143` — that last one is `native/Source/VaultLibrary.cpp`, not under `Source/UI/`, and
round 1's Step 11 cited it without the directory.

The smoke total has moved three times since round 1 and none of the moves was a DSP change: 109 → 114
when the Phase 0 gates landed, 114 → **113 passing / 1 failing** from the library-coupled vault check,
and that same check replaced → **117** when Step 29 was built. `AGENTS.md` warns that these numbers are
claims to re-measure, and this document is now the third example of it. The CPU figure moved too:
**4.43% and 6.12% of one core in two runs of the same binary** on the same machine minutes apart, which
is a ~38% spread; the two runs after Step 29 printed 4.39% and 4.75%, so the observed range on one
machine is now 4.39-6.12%. That makes Step 19's "tighten `SmokeTest.cpp:948` from `< 8.0 s`" more subtle
than it looked — a gate tightened to measured × 3 on a 4.5% measurement would fail on a 6.1% run, so it
needs a band derived from a spread, not a multiplier on a single sample.

---

## 2. Phase 0 — make the advertised things true

Do this before adding anything. It is roughly four days, touches no data format, and every step is
audible or measurable.

**Step 1 — Pre-roll the sub-step events (1.5 d).** Give the scheduler a pending window: a step whose
flam or ratchet offsets fall outside the current block keeps those unemitted sub-hits queued for the
next block, and `rememberTriggered` (`Sequencer.cpp:145`) becomes conditional on *all* of a step's
events having been emitted. Same fix serves both host and internal clock paths.

Gate: a new smoke check that drives `processSequencerBlock` at **512** and asserts a 1-flam step
produces 2 triggers and a 4-ratchet step produces 4, then re-run the existing sequencer checks at 512
rather than 7000/720. The check must fail on the current code — if it passes before the change, the
check is wrong, not the fix.

Built 2026-11-14, and *not* as a pre-roll queue: a queue that only defers events forward cannot
recover a flam grace, because the grace precedes the main hit and belongs to the block *before* it.
Each sub-hit is now positioned from its own absolute place on the timeline, inside the existing ±2
step window, and only the main hit keeps the per-step history guard. The side effect is that a
probability draw could be flipped once per block that looks at the step — three times for one hit —
so the roll is positional (splitmix64 over pattern, pad and absolute step) instead of `random.nextFloat()`.
Gate met: 4 checks fail on the pre-change scheduler (`flam… [1: 6000]`, `ratchet… [1: 6000]`,
`each event exactly once [1: 6000]`, `probability… mixed=15 played=0 skipped=18`) and pass after it. The nudge and flam checks that used to
be handed a 7000-sample block now render 24 blocks of 512 and assert absolute event positions; the
720-sample micro-timing check stays as it was, because its subject is the overlapping window itself.

Round 2 found a defect **in** this fix, and it is my own: the positional roll keyed on
`absoluteStep` is frozen across a host loop, because in host mode the absolute steps come straight
from `host.ppq` and a loop rewinds it. Step 22 is the fix; the gate above did not catch it because
`renderOffsets` advances `host.ppq` monotonically and never exercises a rewind.

**Step 2 — Recompute the warp coefficients in `setSampleRate` (0.5 d).** One-line family of fixes in
`VoiceEngine.cpp`, plus store the rate instead of baking `48000.0f` into two member initialisers.

Gate: render a known pad at 44.1 kHz and at 48 kHz and assert the warp-out energy ratio moves by less
than the tolerance you set at 0% and 100% warp. This will change the sound at 44.1 for existing users;
say so in the commit rather than presenting it as neutral.

Built 2026-11-14, and the gate as written was too weak: the warp error scales with the rate ratio, so
at 44.1 kHz the corners move only ~8% (200→183.75 Hz) and a 44.1-vs-48 A/B passes on both builds — a
test with no teeth. Measured instead as "what Warp does must not depend on the host's rate", on a
300 Hz tone through the warp split at 0% and 100% warp, at 44.1 / 48 / 96 kHz:

| rate | before | after |
|---|---|---|
| 44.1 kHz | 2.19 dB | 2.39 dB |
| 48 kHz | 2.41 dB | 2.41 dB (bit-identical) |
| 96 kHz | 4.13 dB | 2.50 dB |

Gate met on the spread between the three rates: 1.94 dB before, 0.11 dB after, limit 0.5 dB. 48 kHz is
unchanged, so the reference rate does not move; 44.1 kHz moves by 0.2 dB and 96 kHz by 1.6 dB — that is
the correction, and it belongs in the commit message, not in a changelog that calls it neutral.

**Step 3 — Stop the QWERTY pads from eating host shortcuts (0.25 d).** `PluginEditor.cpp` matches the
16 letter keys and `return true`s with only Shift and Alt inspected; `isCtrlKeyDown()` /
`isCommandKeyDown()` / `isMainMenuKey()` are never checked, so Ctrl+A and Cmd+D trigger pads in a
host. The web edition already guards correctly (`src/App.tsx:341`) — mirror it.

Gate: synthetic key press with the Ctrl modifier must fall through to `return false`.

Built 2026-11-14 in `LayoutTest`, which calls the editor's public `keyPressed` directly. Note that
neither `isCtrlKeyDown()` nor `isMainMenuKey()` — both named above — exists in JUCE 8; the guard is
`isCtrlDown() || isCommandDown()`, and Shift/Alt stay legal because they are the accent and ghost
modifiers. Gate met with preconditions alongside it (a bare letter *is* consumed, Space *does* choke,
an unmapped letter falls through), and the three fall-through checks fail when the guard is disabled.

**Step 4 — Tell the truth about REC (0.5 d).** The sidechain bus ships disabled
(`PluginProcessor.cpp:14`) and `isSidechainInputActive()` (`PluginProcessor.h:70`) has **zero
callers**, so the button arms, records silence, and never offers USE. Read it in `SourcePanel`, show
an explicit "input bus disabled in the host" state, and give the take a DISCARD —
`discardRecordingTake` already exists and is only called from inside `PluginProcessor.cpp:920`.

Gate: with the bus disabled, REC must be visibly unavailable rather than silently armed.

Built 2026-11-14. `applyRecordingState()` in `SourcePanel` now reads `isSidechainInputActive()` and
drives REC/STOP/USE text, enable state, tooltips and a DISCARD chip that exists only while a take is
waiting; the status line says when the host has not opened the sidechain bus. One change beyond the
brief, and it was the actual silence problem: the editor's slow UI tick (30 Hz, every tenth tick, so
~330 ms) *auto-adopted* a ready take into the source, with no confirmation and no way to say no.
That is now a user
press on USE. Gate met: 12 UI-level checks, including both directions (REC disabled while the bus is
closed, REC enabled once it is open) so the check cannot pass on a dead button, and "REC is
unavailable while the host input bus is closed" fails when `setEnabled (inputActive)` is reverted to
`setEnabled (true)`.

**Step 5 — Fix the claims (0.5 d).** Correct the four `AGENTS.md` line references. Add a
`HostTest` assertion for the *exact* parameter count so the number is machine-held from now on. Retire
the "no audio-thread allocation" sentence or scope it honestly — `PluginProcessor.cpp:223-227` copy-assigns
a `juce::AudioBuffer` every block whenever the sidechain bus is enabled, which is an allocation
(`getBusBuffer` → `operator=` → `setSize` + memcpy). Either hold a reusable member or document the
exception.

Round 2's headline for this step — "347 not 349" — is **retracted on measurement**: `SliceForgeHostTest.exe`
on the current build prints `parameters: 349`, so `native/README.md:155` was right and my own plan was
wrong. The numbers in that README that *were* stale are fixed as of 2026-11-15: **117** checks replace 109
in three places (the summary table, `SmokeTest.cpp` in the tree map, and the "current result" line), the
CPU figure is now the measured 4.4-6.1% band instead of "about 5%", and the Sample Vault coverage bullet
says what Step 29 actually proves rather than "indexed-sample loading", which is no longer what the check
does. Verified against the run, not against the text: `ctest` 3/3 and `PASS - 117/117 checks passed`, and
`npm test` → 30 tests / 3 files with `npm run test:browser` → `27/27 checks passed`, which is why the root
`README.md` needed no change. What is left in Step 5 is the `AGENTS.md` line references, the exact-count
assertion, and the allocation sentence.

Gate: `ctest --test-dir native/build -C Release --output-on-failure` green with the exact-count
assertion in place. Build the test targets first — bare ctest runs stale binaries.

---

## 3. Phase 1 — do the thing the product is named for

**Step 6 — Split pitch from rate, and band-limit the read head (3 d).**

Verified state: `VoiceEngine.cpp:171` sets `srcInc = pitchRatio × resampleRatio` and `renderTo` does
two-point linear interpolation. There is **no** pitch-without-length, **no** stretch-without-pitch, and
**no** per-slice rate, in either edition. `length` only scales envelope constants and `maxLife`
(`:145`, `:195`, `:220-221`) — it truncates, it does not stretch. "Variable Rate Audio Chopping" is the
concept the project is a tribute to, and the control that names it does not exist. This is also the
cheapest fix in the whole document, because the read head is *already* a variable increment.

Scope it as: a bipolar per-pad `rate` parameter that drives `srcInc` independently of `pitch`,
compensated in the envelope time constants so `length` still means what it says, plus a 4-tap
cubic/Farrow read that kills the linear-interp fizz on pitched-up foley. Append the new parameter to
the layout so existing `VRAC_EXTRA` blobs stay readable; the frozen keys are untouched.

Gate: a new smoke check asserting that `pitch = +12, rate = 0` halves the duration versus
`pitch = 0, rate = 0` while `pitch = 0, rate = +12` leaves duration unchanged. Then the real gate —
A/B a pitched kit against the old build and judge it by ear.

---

## 4. Phase 2 — the slicer, which is the first impression

Round 2 split this phase. The step written here is the *detector*, and it is no longer first: Step 24
(slices have no end) is the part of "kit from one recording" that a listener actually hears, it is a
length computation rather than an algorithm, and Step 25 builds the scorer that makes any detector work
decidable. Step 7 now runs after both.

**Step 7 — Onset detection with an adaptive threshold (4-6 d).** *Round 2: this step now runs after
Step 25, which builds the measurement first. Do not start it until the fixture exists.*

Verified state: the detector is time-domain only — `grep` for `juce::dsp::FFT`, `performFFT` or any
`juce_dsp` include under `native/Source/` returns **nothing**, in a repo that already links `juce_dsp`
(`native/CMakeLists.txt:129`). `Slicer.cpp:8-96` is mono-summed RMS, 128-sample hop (±2.7 ms),
rectified first difference, 5-tap local maxima, fixed floor `0.012 × max`, fixed −6 ms pre-shift,
115 ms minimum gap. It is *line-for-line the same algorithm* as `src/audio/slicer.ts` (same `HOP = 128`
/ `WINDOW = 512`, same −6 ms pull-back, same `0.07` s dedup, same `8 + 46 × sens` target), which is
good news for the port and bad news for soft, pitched or noisy onsets.

The upgrade that earns its cost: log-magnitude spectral flux via `juce::dsp::FFT`, a median-filtered
adaptive threshold, min-to-max rather than fixed-floor selection, and parabolic sub-hop refinement —
an optional "snap to host grid" toggle for the last 10%. Zero new binary bytes, zero licence, no
absent toolchain, and it lands in the existing message-thread seam at `resliceAndAssignStarts`.
Reject a learned onset detector: it is the same 20% of benefit for an order of magnitude more cost.

Also in this step: native has no fallback when zero onsets are found — `Slicer.cpp:99-102` leaves all
sixteen pads at start 0, while the web edition has `evenStarts` (`slicer.ts:129`). Port it. And
`SliceMapping` (`Slicer.h:19-24`) is dead code; either wire it to the pad reassignment the UI needs or
delete it.

Gate: a fixture of 8-10 known takes with hand-marked hit times, asserting median onset error drops
against the current detector on the *same* files. Keep it level-matched, and keep slice→pad assignment
stable so previously saved kits keep their times.

---

## 5. Phase 3 — get audio out

**Step 8 — Native kit bounce and clip drag-out (3-4 d).**

Render the sixteen pads offline to a folder of 24-bit WAVs plus `kit.json` and a README, mirroring
`src/audio/exportKit.ts` — including the master chain and the output stage, so a bounce leaves the way
the live output does. Wire the existing `writeStereoWav`, run the render on a detached thread with a
progress state, and add audio-file drag-out via `performExternalDragDropOfFiles` on a temp file. The
offline render must use a *copy* of the runtime, never the live engine.

Gate: the bounced kit A/B-ed against the same kit recorded through Ableton's own master must be
identical to within float rounding at `SOFT CLIP = 0`, and the level must not be raised to make the
new path look better.

Round 2 amends this gate, because as written it is unachievable. The plugin does not sum the buses
and then master them: it runs a **separate master chain per bus** (`VoiceEngine.cpp:677-688`, one
`masterChains[busIndex]` each with its own filter/tone/volume) and only then folds the disabled buses
into bus 0 with `std::tanh` (`PluginProcessor.cpp:432-441`). A host downmix is sum-then-once; a
multi-bus bounce is once-each-then-sum, so the two diverge as soon as more than one bus is audible —
correctly, and by design. State the gate as *identical when only bus 0 carries sound*, and either
pull Step 10 (topology) in front of this one or accept that a stem bounce is not the same object as
a master recording and say so in the bounce's README.

---

## 6. Phase 4 — stop paying for two engines twice

**Step 9 — Write the parity contract instead of the harness (0.5 d, downgraded from 2 d).**

Measured drift: **23 verified divergences** between the two implementations, and exactly one
cross-edition test — `src/audio/__tests__/outputStage.test.ts:13-14,30,61`, which merely restates two
constants (`0.349`, `0.844`) that are also hardcoded in `SmokeTest.cpp:835`. The parameter *surface*
is clean (347 ids, ranges and units match `src/types.ts:73-140`); it is the math and the defaults that
have split. The three that matter: native runs the master chain once **per bus** — 8 chains,
`VoiceEngine.cpp:677-688` — while web sums all eight outputs into **one** chain
(`src/audio/audioEngine.ts:114-119` connects every bus's gain to the same `chain.input`), and web has a
`DynamicsCompressor` at −7 dB 5:1 (`src/audio/masterChain.ts:57-62`) that native does not have at all;
13 of 16 default pad lengths differ between `native/Source/Params.cpp:49-66` and `types.ts:247-252`; and
`fastTanh` (`VoiceEngine.cpp:75-81`) is a rational approximation against `Math.tanh`.

Round 2 changes what to *do* with this. A golden-vector harness asserts "equal within tolerance", and
the topology difference above is not within any tolerance: per-bus compression and global compression
are different processing, so a fixture would fail on day one, get widened until it passed, and then
report parity that does not exist. Three facts, all verified today, decide it:

- the master-chain split is a **design question** (Step 10), not a numerical one — no fixture answers it;
- `src/` has **no sequencer at all** (`grep -ri "sequencer|pattern" src/` returns zero lines), so the
  browser edition cannot be the same instrument however many vectors match;
- the WASM route that would make one core real is unfunded and blocked (`VoiceEngine.cpp` has ~36
  `juce::` references to break first, and emscripten is not installed).

So: replace the harness with a written contract. One page, `docs/edition-contract.md`, listing what the
two editions share (engine design, parameter surface, slicer algorithm), what deliberately differs
(master-chain topology, sequencer/Vault/recording = native-only), and which differences are *unresolved*
(default pad lengths, `fastTanh`). Keep the one existing cross-edition test. Then either fund Step 10
and revisit, or accept the browser edition as a demo of *most* of the plugin and stop describing the
two as one engine in the READMEs. Cost of being wrong: a page of prose versus two days of a harness
that quietly becomes a rubber stamp.

**Step 10 — Decide the master-chain topology (2 d).** Per-bus × 8 versus global × 1 is a *design
choice* that has drifted into an accident. Pick one, implement it in both editions, document it as a
behaviour change.

**Step 11 — Harden the Vault contract (1-2 d).** Round 2 had to fix this step's own citations before
trusting its conclusion, and the conclusion survived: `VaultLibrary.cpp` is `native/Source/`, not under
`Source/UI/`, and the line range round 1 gave (`:396-452`) is `findDatabaseFile()`, not the query. What
is actually there: `queryTracks` builds one hardcoded column list at `:540-548` — 18 named `tracks`
columns, 30 `columnText` reads across the file — and `grep PRAGMA` over the whole file returns
**nothing**, so there is no `table_info` handshake and no version check of any kind. `schema_version`
and `app_version` are read at `:487-488` into `VaultLibrary::Status` (`VaultLibrary.h:63`) and *never
read again* — `grep -rn schemaVersion native/Source/` returns those two lines and the declaration, which
is the whole story: the plugin fetches the version and ignores it. The first column rename in the
sibling app is therefore a raw `no such column` failure, exactly as round 1 said, for the reason round 1
said. Three further verified fragilities: the DB is opened read-only with `SQLITE_OPEN_NO_MUTEX`
(`:173`) and `busy_timeout` 400 ms (`:185`); `step()` errors are swallowed (`:259`, `:271` compare
against `kSqliteRow` and return nothing useful), which silently truncates result lists; and the
development fallback at `:347-350` tries four folder names ("SKEER AUDIO - AUDIOVAULT", "SKEER SAMPLE
VAULT", …) while the actual sibling directory is `sample-vault` — that route is already dead, and the
working discovery is the `%LOCALAPPDATA%\SKEER Sample Vault\data\vault.sqlite3` candidate plus the
`SKEER_VAULT_DB` / `_DATA` / `_ROOT` environment variables at `:399-410`. Round 2 adds one thing to the
list, learned from Step 29: the query filters `WHERE t.missing=0` (`:548`), which trusts the sibling's
staleness flag completely — on this machine that filter admits 156,237 tracks of which 12,135 files
exist, and it admits a 4 KB macOS sidecar as audio. Add a `PRAGMA table_info` column map, a degraded
mode that shows what exists, a *used* schema version, and a size/extension sanity check before offering
a track to the loader. Do **not** change the sibling; it is another worker's lane.

---

## 7. Phase 5 — depth, where it is uniquely yours

Only the parts SKULPT does not already own.

**Step 12 — Per-pattern `length`, `swing` and `division` (3 d).** These three live on the *project*,
not the pattern (`Sequencer.h:49-51`), so you cannot make a half-speed fill or a 3/4 lane inside a 4/4
song. Move them into `SeqPattern`, serialiser version 3, keep the v1/v2 readers intact. This is the
biggest workflow wall in the sequencer after the dead flams.

**Step 13 — Record performance into a pattern (4 d).** Live pad hits arrive at
`PluginProcessor.cpp:338-344` and are dropped, because `toggleRecording` (`:887`) is the *audio*
take recorder. Add MIDI-in recording with a step grid derived from `division`, overdub, and quantise.

**Step 14 — Groove-aware mutate (1 d).** `Sequencer.cpp:550-569` flips `s.on` for *every* step whose
roll beats `amount`, backbone included, so "mutate" is a destruct-the-groove button. Restrict flips to
off-grid steps and protect the kick/snare backbone.

**Step 15 — Expose the kit seed (2 d).** Seeds are already per-voice and deterministic
(`VoiceEngine.cpp:121`, `:136`), so making the kit seed one saved number makes every randomised and
auto-sliced result reproducible and shareable. Cheap, and it is what makes a "record your kitchen"
clip re-playable by someone else.

Deferred on purpose: pattern bank 4 → 8 (~1 d, mechanical, but decide alongside the format bump);
lock *automation lanes* (6 d, large UI, and it is SKULPT's modulation matrix); bus sends/reverb/delay
(5 d, collides with multi-out semantics); granular texture layer (blocked by the 32 fixed voice pool).

---

## 8. Phase 6 — the boring parts that gate any public build

**Step 16 — Licence floor (1 d).** `external/JUCE/LICENSE.md` is AGPLv3-or-commercial and JUCE 8
ignores the splash flag, so compliance is source conveyance. There is **no tracked `LICENSE` file**
and `native/README.md:422` says only "under its applicable license". Add `LICENSE` plus a
`THIRD-PARTY-NOTICES.txt` (the OFL texts for IBM Plex / Barlow — verified present in both `.ttf`
name tables — JUCE, the Steinberg VST3 headers), ship it inside the zip, link it from an About box,
and add the missing `LegalCopyright` to the version resource. This is the single largest blocker to
conveying the build to anyone.

**Step 17 — Make the zip reproducible and take the binaries out of git (0.5 d, owner decision).**
Measured: `.git` is 205 MB, 318 loose objects, **zero packs**; `launch/` is 76.5 MB in HEAD; the zip is
27,750,643 B and the exe 24,210,944 B as plain blobs. The restamp in `92336b5` added a fresh 27.75 MB
blob for a payload-identical zip — two objects of 27,709,732 B with different hashes — because
`native/package-launch.ps1:65-69` uses `Compress-Archive`, which embeds mtimes. So every export costs
about +28 MB forever. Git LFS is configured but genuinely inactive (no `.gitattributes`, no
`pre-push` hook), so it currently manages nothing. Fixing the determinism is half a day; moving
`launch/` to release assets reverses a recorded owner decision from 2026-11-14, so it is yours to make,
with the measured cost in front of you rather than a preference.

**Step 18 — Minimum useful CI (2-3 d).** One Windows job: configure, build **sequentially** (dodges
the known `D8040` / bogus `C1001` parallel flake), `ctest`, upload the zip. Caveat that must be
checked before promising it: the GitHub Windows images may not carry Visual Studio 18 2026, which
would make a self-hosted runner a prerequisite. Also stop `LayoutTest` writing ≥10 PNGs into
`native/shots/` during ctest — a test that mutates the tree is not a test.

**Step 19 — Golden audio + an honest CPU gate (1-2 d).** Commit per-factory-source SHA-256s and
tolerance-checked reference renders; tighten `SmokeTest.cpp:948` from `< 8.0 s`. Round 2 amends the
second half: two runs of the same binary minutes apart printed 4.43% and 6.12%, so the gate must be a
band measured across runs on the machine, not a multiplier on one sample — otherwise the commit that
"tightens" the gate is followed by the week it fails for nobody. This is the only real DSP regression
net, and it is what makes the macOS parity claim meaningful later.

**Step 20 — macOS, if it ever matters (3-5 d, no AAX).** Real blocker: `VaultLibrary.cpp:7-99`
hand-rolls the SQLite ABI against `LoadLibraryW(L"winsqlite3.dll")`, so the VAULT page cannot exist
off-Windows without bundling SQLite. Everything else is portable — `getSpecialLocation` already
returns sane per-platform roots (`PluginEditor.cpp:445`, `:475`, `Theme.cpp:74`), and the
`LOCALAPPDATA`/`APPDATA` reads are guarded. Then parameterise `FORMATS` and the
`Contents\x86_64-win` paths (`package-launch.ps1:22,30,77`), and stop painting the literal `"VST3"`
in `HeaderBar.cpp:189`. AU is a `FORMATS` entry plus entitlements and notarisation. **AAX is blocked
by policy, not code** — the SDK needs an Avid account and a click-through agreement, and commercial
release needs their sign-off plus iLok. No macOS code from an earlier attempt exists in the tree.

---

## 8A. Phase 7 — round 2, ranked

Eight new steps, ~7 dev-days all in. Ordered inside the phase by the same three questions as
everything above: does it make a claim true, can you hear it in Ableton, what does it cost to be
wrong. Each was verified against the tree before it got a number.

**Step 22 — Give the probability roll a pass counter (0.5 d).** This is a defect in Step 1, and it is
audible in the exact situation Step 1 was written for.

Verified state: `probabilityRollFor` (`Sequencer.cpp:120-132`) is a pure function of `patternIndex`,
`pad` and `absoluteStep`. In host mode those absolute steps come straight from `host.ppq`
(`:233`, and the window `floor(ppqStart·division)−2 … ceil(ppqEnd·division)+2` at `:252-256`), so when
Ableton loops a region the *same* absolute steps are re-evaluated and the roll returns the *same*
answer. The dropout set is frozen for the life of the loop. The doc comment at `:117-118` — "the
absolute step keeps counting while the pattern loops, so every pass through the bar still gets an
independent draw" — is true for the internal clock, where `state.internalPpq` only ever moves forward
(`:297`), and false for host sync. Nobody caught it because the Step 1 gate advances `host.ppq`
monotonically.

Decision: add a pass counter to the key, not a clock and not the shared RNG. `SeqPlaybackState` gains
`int64_t passCount`, incremented in the existing `if (reset)` branch at `:240-241`, and
`probabilityRollFor` takes it as a fourth input. One correction to make before building, because it
would have been the wrong seam: the `ppqEnd < ppqStart` early-return at `:243-250` is **unreachable**.
`ppqEnd` is `ppqStart + numSamples × bpm / 60 / sampleRate` (`:234-235`) and both terms are always
positive — `bpm` is floored through `host.bpm > 0 ? host.bpm : max (20, data.bpm)` at `:232` — so a
host loop-back is never seen there. It is caught, correctly, by the discontinuity test one branch
above at `:238`, because a rewind of a whole bar is far larger than any tolerance. So: increment on
`reset`, and note the dependency — while the 0.02 constant is narrower than a block (Step 23),
`reset` also fires spuriously and the pass counter drifts upward for no reason. That is harmless for
sound, because any extra integer still yields an independent-looking flat draw, but it is the reason to
land Step 23 first or together, so that "pass" means a pass.

Why keying on discontinuity rather than on bar number: a loop that ends mid-bar has no integral bar
count to add. And why not the two obvious smaller diffs — a wall-clock seed, or `random.nextFloat()`
back: either breaks the property Step 1 exists to give, that all blocks looking at one position come to
the same answer, which is what stops a rolled-out hit from leaving its flam behind or its ratchets
playing without it.

Gate: a smoke check that renders the same one bar twice at identical host ppq positions with
probability 0.5 on all sixteen lanes and asserts (a) the two passes are not the same set, and (b) the
`mixed == 0` invariant from Step 1 still holds inside each pass — every sub-hit of a step still
arrives or leaves together. Must fail on today's code, and it must fail on the *first* assertion only,
not because (b) broke.

**Step 23 — Scale the host resync tolerance to the block (0.25 d).**

Verified state: `if (! reset && std::abs (ppqStart - state.expectedHostPpq) > 0.02) reset = true;`
(`Sequencer.cpp:238`) compares a *fixed* 0.02 ppq against what is really "did the last block end where
this one starts". One block is `numSamples / samplesPerPpq` ppq, and `samplesPerPpq = 60 × rate / bpm` —
24000 at 120 BPM / 48 kHz. So a 512-block is **0.0213 ppq, already wider than the 0.02 tolerance**, a
1024-block at 120 is 0.0427, a 2048-block at 90 BPM is 0.064, and a 1024-block at 180 is 0.064. The
constant only has headroom over a block at small blocks *and* slow tempo — 512 at 60 BPM is 0.0107.
That is the finding, and it is sharper than "wrong at settings a real session uses": at the plugin's own
default tempo and rate, any host whose reported ppq disagrees with our prediction by a whole block trips
the reset on *every* block, clearing the step history each time (`:241`). The visible symptom of that is
not silence but double hits, because the history guard is exactly what stops a neighbouring block's
overlapping ±2-step window from re-emitting a main hit. Note what is *not* claimed here: no specific
host was measured, and in the smoke harness the predicted and reported ppq agree bit-for-bit, which is
why this has never fired in the tests.

Decision: `std::max (0.02, blockPpq * 0.5)` — keep the existing value as a **floor** rather than
replacing it. Half a block at 512/120/48k is 0.0107, so a bare `blockPpq * 0.5` would be *stricter* than
today at the most common settings and could newly trip resets that currently don't happen; taking the
max makes the change widen-only, which is the only direction that can be defended without a host to
measure against. `blockPpq` is the value already computed a few lines below at `:234-235`, so this is
one expression. Do not go past half a block: beyond that a genuine seek goes undetected and the
sequencer plays the wrong part of the bar.

While in this function: the unreachable `ppqEnd < ppqStart` guard at `:243-250` is dead in exactly the
way described in Step 22 — no host can make it true. Deleting it is legitimate (it is three lines of
impossible state, and `AGENTS.md` already says not to keep what is certainly unused), but it is not a
requirement of this step and it must not be deleted *instead* of fixing the tolerance, because the two
look like the same concern and are not.

Gate: a smoke check in two directions, because a widened tolerance can always be widened enough to
pass vacuously. Feed 8 consecutive 2048-sample blocks at 90 BPM with exactly the ppq the scheduler
predicts and assert zero resets; then jump one whole bar and assert **one** reset. Instrument it by
counting history clears through a test-only seam, since "a reset happened" is otherwise invisible from
outside. Must fail today on the first half.

**Step 24 — Give every slice an end (1.5-2 d).** The single most audible step in this document.

Verified state: `Slice` is `{ double time; float strength; }` (`Slicer.h:10-14`). There is no end, no
duration, nothing downstream that knows the distance to the next onset. `resliceAndAssignStarts`
(`PluginProcessor.cpp:686-726`) publishes only start seconds; the voice length comes from
`lengthSeconds = pad.length × global.length × velocityLength` (`VoiceEngine.cpp:149`) and is otherwise
oblivious. So dropping one recording on the plugin gives you sixteen pads that each play from their
onset *to the end of the file*: the kick pad contains the snare that follows it, the snare contains
the room after that, and the last pad is the tail. The kit sounds like one long drone triggered from
sixteen places, which is the first ten seconds of any demo — and `padStartsFromSlices`
(`Slicer.cpp:98-114`) makes it worse when the file yields fewer than sixteen onsets, because
`position = pad % slices.size()` (`:109`) hands several pads the *same* start. That is precisely the
complaint the market lane found documented against the competitor ("all pads come from the same
slice"), reproduced here for free.

Decision: at slice-assignment time, derive a per-pad length from the inter-onset gap — pad *n* gets
`t[n+1] − t[n]`, the last pad gets the file tail — and write it into the existing `pad.length`
parameter rather than into a new hidden field. Explicit and overridable: LENGTH stays a knob the user
can grab afterwards, nothing is silently compensated, and because `pad.length` is already an APVTS
parameter it saves, automates and mirrors with zero format change. Fix the aliasing in the same pass:
when there are fewer slices than pads, distribute over distinct indices before repeating.

Gate: assert on a synthetic file with four known onsets in a known duration that, for each *distinct*
slice, the pad(s) assigned to it get `t[i+1] − t[i]` — and that the four distinct gaps sum to the file
duration, last one included, because the last slice's end is the file. Assert the aliasing case at the
same time: with 4 slices and 16 pads, exactly four distinct starts and four distinct lengths, not
sixteen arbitrary ones. (Say it as *distinct slices*, not "the sixteen lengths sum to the duration":
they would sum to roughly four times it, and a gate written that way would fail on correct code.) Then
the real gate — one recording into Ableton, play pads 1 through 8 in order, and hear
eight separate hits instead of eight overlapping copies of the whole file. This check must fail on
today's build; on today's build every pad's length is whatever the kit default was.

**Step 25 — Build the slicer's ground truth before touching the slicer (1 d).** This is Step 7's
precondition, promoted to its own step because round 1 buried it in a gate.

Verified state, none of it spectral: `floor = maxFlux × 0.012f` (`Slicer.cpp:49`) is relative to the
*single loudest* rise in the file, so one crash buries every hi-hat in the same take; `minGapSeconds =
0.115` (`:48`) is hard-fused in seconds, so a 1/16 hat run at 200 BPM — 75 ms apart — cannot be sliced
at any sensitivity; and `target = round(8 + 46 × sens)` (`:47`) means the knob labelled "Auto Slice
Sensitivity" changes only *how many* slices you get, never *where* they land or how weak a hit has to
be to qualify. The web edition has the same three constants (`src/audio/slicer.ts`), so this is one
decision to fix in two places, not two.

Decision: make a fixture and a scorer first. Eight to ten real takes committed under `fixtures/` — a
dry drum loop, a crash-heavy loop, a foley bag, a spoken phrase, a sustained pad with soft attacks, a
fast hat run, a piano phrase, and one level-matched pair — each with hand-marked onset times in JSON
next to it. A new smoke check scores any detector against those marks: median absolute onset error,
recall at a fixed slice count, false positives per minute, printed as `info` lines so the numbers land
in the test log and stay comparable. Score today's detector and commit the table. Only then does Step 7
get its FFT, and it gets it with a before/after instead of an opinion. Reject "adaptive threshold" as a
standalone change: with no scorer, nobody can tell whether it helped or merely moved the misses around.

Gate: the scorer exists, runs in `ctest`, and reports today's numbers *before* any detector edit. If
the first run of the scorer already shows the fast-hat run and the crash-heavy loop failing at 100%
recall, that is the proof the fixture is load-bearing and Step 7's scope should include `minGapSeconds`
and the floor, not only the FFT.

**Step 26 — Put the pattern on the host's map (1-1.5 d).**

Verified state: `kSeqPatterns` is **4** (`Sequencer.h:12`); `activePattern` is not a parameter —
`Params.cpp` adds only `global.*`, `source.sensitivity` and the 21 × 16 pad ids, and the active
pattern travels inside the `VRAC_EXTRA` blob (`Sequencer.cpp:686`) — so it cannot be automated, MIDI-
learned or CC-mapped. `setActivePattern` exists only on the UI panel (`SequencerPanel.cpp:502-507`),
and the CC table maps a controller number to a *parameter id string*
(`PluginProcessor.cpp:572-579`), which is exactly the seam a `seq.pattern` parameter would slot into
for free.

Decision: append one `AudioParameterFloat` — `"seq.pattern"`, 0…3, stepped by 1 — to the layout, and
make it the single source of truth the audio thread reads, with the UI chips both reflecting and
writing it. Additive append only: existing `VRAC_EXTRA` blobs stay readable, the frozen keys are
untouched, and old sets come up on pattern 0 plus their stored value. This buys host automation,
MIDI-learn, controller mapping and clip-level pattern recall in one step. Deliberately **not** done
here: growing the bank to 8. That is a serialiser version bump and a UI change; bundling it with a
parameter append is how two format changes land in one commit and one of them breaks a saved kit.

Gate: `HostTest` asserts the id exists with the right range and step, and that moving the parameter
between two values makes `processSequencerBlock` schedule a different pattern at the same host ppq —
compared, not absolute, so the check does not depend on what pattern 0 happens to contain.

**Step 27 — Make the exported MIDI file the pattern you heard (0.5 d).**

Verified state: `makePatternMidiFile` (`Sequencer.cpp:753-816`) is better than round 1 assumed — it
does emit nudge (`:783`), flams (`:799-800`) and ratchets (`:801-802`). It also, verifiably, ignores
three things the live scheduler applies: **swing**, because `baseTick` is
`absolute × ticksPerStep + s.nudge × ticksPerStep` (`:782-783`) with no `data.swing` term anywhere in
the function, while `scheduleAbsoluteStep` shifts odd steps by `swing / division` (`:153-155`);
**probability**, because the only gates are `s.on` (`:779`) and nothing else — the roll is not consulted;
and **mute/solo**, because the loop tests `s.on` but never calls `laneIsAudible`, which exists and is
used by the live path (`:52-60`, called at `:186`). So a muted lane prints into the clip, and a
rolled-out hit prints as a hit.

Decision: reuse the live code rather than re-deriving it — call `laneIsAudible`, add the same swing
expression, and apply the same `probabilityRollFor` so the file matches one *particular* pass. Be honest
about what that means: with Step 22 landed, a printed file is a snapshot of one pass and looping the
clip in the host will *not* re-drop. Say so in the file name (`… pass 3.mid`) or in the text meta event
at `:765`, and give the user a "print the backbone" option (probability gates off) if they want a clip
they intend to loop. Do not touch the channel-10 / `36 + pad` mapping without checking it against the
plugin's own note map first.

Gate: build a pattern with swing 0.3, lane 3 muted, lane 5 at probability 0.5 and a flam plus a
ratchet on lane 1; export; parse the SMF back with `juce::MidiFile` and assert the off-grid onsets, the
absent lane 3, the present lane 1 with 3 events, and that lane 5 is not simply all-there. Three
separate assertions, so the check says which lie the exporter is still telling. Fails today on all
three counts.

**Step 28 — Send the sequencer to the host as MIDI (2 d), or delete the implication that it does.**

Verified state: two independent blockers, both named in item 3 above — `NEEDS_MIDI_OUTPUT FALSE`
(`native/CMakeLists.txt:113`) and `midiMessages.clear()` at the bottom of `processBlock`
(`PluginProcessor.cpp:390`). The triggers already exist as `(sampleOffset, pad, velocity)` pairs in
`sequenceOutput` before that clear (`:310-314`), so the data is present and thrown away.

Decision: flip `NEEDS_MIDI_OUTPUT` to TRUE (`IS_MIDI_EFFECT` stays FALSE, `producesMidi()` follows the
flag), and instead of clearing, write one note-on/note-off pair per trigger at its sample offset,
channel 10 and note `36 + pad` to match the exporter from Step 27, with velocity mapped the same way.
Keep the internal audio path exactly as it is — the plugin still plays itself; host MIDI is an extra
out, not a replacement, so a Live session can record the pattern into a clip and drive *another*
instrument with it. Two things to say out loud before building it: changing the advertised bus info
means a host must reload the plugin before an existing session sees the MIDI bus, so it belongs in the
commit message and not in a changelog; and do not pass incoming MIDI through to the out bus, because
that double-triggers the moment a keyboard is routed in.

Gate: `HostTest` requests a layout with MIDI output enabled, runs one bar of host-synced
`processBlock`, and asserts a note event lands at the sample offset the scheduler reported — and that
the count of MIDI notes equals the count of audio triggers, so an unemitted sub-hit cannot appear on
one path and not the other. Then the real gate, and the reason this outranks a lot of Phase 5: in
Ableton, arm a MIDI track to SliceForge's output, record, and play the clip back through a different
drum rack.

**Step 29 — Stop the smoke suite from asserting the state of the music library (0.25 d).**

Verified state when this was written: `ctest --test-dir native/build -C Release` reported 113/114 and
failed target 1.
The failing check is `a Sample Vault track loads into SliceForge` (`SmokeTest.cpp:762-767`), which
takes the first `.wav` the vault query returns and asserts `loadUserSampleFromFile` decodes it. That
file is `D:\Old (to backup)\First recital from sebastiaan\sunhee2\audio\
wetransfer_recital-audio-files_2021-12-08_1506\._R_20211011-194229.wav` — 4096 bytes, magic
`00 05 16 07 … Mac `, a macOS AppleDouble resource-fork sidecar that is not audio in any sense, indexed
by the sibling app as `format='WAV'`, `missing=0`, with `channels`, `sample_rate` and `duration_ms` all
NULL. The query sorts `ORDER BY t.filename COLLATE NOCASE` by default (`VaultLibrary.cpp:565-568`), and
`._R_…` sorts near the front of any collation, so the suite picks it nearly every time. **SliceForge
refusing it is the correct behaviour.** The test is what is wrong: a check whose pass/fail depends on
which junk files a user's library happens to contain is not a gate, and it will go red on a machine
that has done nothing wrong.

Decision: keep the intent — the Vault→plugin load path works — by scanning a bounded prefix (say the
first 20 candidates) and requiring at least one to load, reporting the refused count as `info` rather
than as a failure; and add a *separate* deterministic check that a known-bad sidecar is refused, using a
committed fixture rather than whatever the library contains. Related and worth recording, but **not for
this repo to fix**: the vault DB carries 156,237 rows marked `missing=0` while only 12,135 of its `.wav`
paths exist on disk this minute — the `missing` flag is stale by an order of magnitude. The DB and its
indexer belong to `sample-vault`; it is another worker's lane, so report the number and move on.

Gate: the suite goes green with **no source change** under `native/Source`, and goes red if
`loadUserSampleFromFile` is broken for real audio. That second half is the whole point: a check that
cannot fail when the code is broken is not a test, and neither is one that cannot pass when the code is
fine.

Built 2026-11-15, and the Decision above is what the build *rejected*. Requiring at least one of the
scanned candidates to load is the same dependency in milder form: measured on this library, only 2 of
the first 8 listed `.wav` paths still exist and 1 of those 2 decodes — so a machine with slightly worse
luck, or the same machine after a drive letter changes, goes red for no SliceForge reason. The vault scan
is now bounded (8 candidates) and reports at `info` only; what it *asserts* is three deterministic checks
on files the test writes itself into `File::tempDirectory`: a 12,000-sample stereo tone round-trips
through `writeStereoWav` and loads with the right sample and channel counts, and a 4096-byte zero-filled
block carrying `00 05 16 07 … Mac ` at offset 8 — the exact AppleDouble shape that turned the suite red —
is refused. Total goes 114 → **117/117 PASS**, `ctest -C Release` 3/3, exit 0. Not a committed fixture:
`native/Resources/` is enumerated file-by-file in `CMakeLists.txt`, and a 4 KB binary there buys nothing a
`MemoryBlock` does not.

Both directions falsified with a temporary one-line edit in `PluginProcessor.cpp`, then reverted. (a)
`return false` at the top of `loadUserSampleFromFile` → 115/117, failing `a real wav loads through the
user-sample path [load returned false]` and `…is refused and does not replace the kit [633600]`, and the
info line falls to "0 decoded". (b) `if (sample == nullptr) return true` → 116/117, failing only the
refusal check, `[… is refused and does not replace the kit [load returned true]]`, and the info line rises
to "2 decoded". So the gate's second half holds: a loader that accepts garbage cannot pass, and a loader
that rejects audio cannot pass. Under (a) the refusal check fails as a *consequence* — the 633,600 in its
detail is the factory kit the tone never replaced — which is the chaining working, not a false alarm.

Own mistake worth keeping: the first version of the refusal check asserted `getSamplePtr() == nullptr`
after a refused load, and it failed on a *correct* plugin, because the constructor loads a factory source
(`PluginProcessor.cpp:130-133`) — `getSamplePtr()` is never null on a fresh processor. Refusal is measured
as "the kit is still the one you had", on the same processor, not "no kit".

---


## 9. Not doing

Learned/ML onset detection and in-plugin stem separation — the runtime is absent (nothing of torch,
onnxruntime, librosa, madmom or essentia is installed) and the honest DSP upgrade captures most of the
benefit. Round 1's other reason — that a headless `autostem.exe` already exists — is retracted above;
the sibling's only working engine is Ableton's own separator through UI Automation, so if separation
ever happens in this family it happens *in* autostem's Local engine, which `SeparatorFactory.cs:22`
still reports as not implemented. Markov pattern generation over four patterns — far too little
training data, and `kSeqPatterns` is 4, not the 8 round-1 notes assumed. An arpeggiator — different instrument.
Cloud kit sharing — nothing about this project needs a server. AAX — the ceremony is disproportionate
to the project. A formatter or lint rule — repo policy, not a defect. Rebuilding SKULPT's second half
inside SliceForge — that is the family's duplication problem, and it is Step 21.

---

## 10. Step 21 — the decision that outranks all of the above

**Step 21 — Write down what SliceForge is *for*, given SKULPT *and* Expressive Drum.** One paragraph,
in `AGENTS.md`, that survives a stranger reading the repo. Round 1 could write it as
"SKULPT is the modelling engine and is feature complete; SliceForge is the *chop one recording into a
playable kit* instrument plus the browser edition plus the Vault link". Round 2 cannot, because
`redrum` already sells the chop: 128 pads, drag/drop auto-slicing with editable recurring-sound
groups, a v2 transient/body hybrid resynthesis engine, template-guided two-layer loop separation and
an internal sample-accurate pattern transport (`redrum/README.md`). The paragraph now has to name
three engines and answer one question the family has not answered: **which repo owns "one recording
becomes a playable kit"?** Until it does, Step 24 and Step 25 are spending effort on a lane a sibling
may already intend to win — and that is the only reason these two steps are ranked below the sequencer
fixes, despite being the ones a customer would hear first.

Every phase above is compatible with any answer. But the answer decides whether Phase 5 is worth four
weeks, whether the browser edition is a marketing wedge or a second instrument, and whether the 23
engine divergences are a bug to fix or a sign that one of the two engines should become WebAssembly.
Three forks worth naming explicitly:

- **The browser edition.** Options are: keep as a zero-friction demo (≈0 code, but every shared tweak
  must be hand-mirrored); compile the de-JUICEd `Engine/` folder to WebAssembly and have one core
  (≈6-9 d, and **emscripten is not installed** — no `emcc`, no `~/emsdk`, no `wasm32` cargo target,
  plus `VoiceEngine.cpp` carries 38 `juce::` references to break first, re-counted today); let it rot;
  or grow it into the full instrument (10-15 d and it *doubles* the drift surface). Round 2 re-ranks
  these: the demo option is the only one the evidence supports, because the edition has **no sequencer
  at all** (`grep -ri "sequencer|pattern" src/` → zero lines), so "one engine, two surfaces" was never
  actually the situation. Step 9 is therefore now a one-page contract, not a harness, and the harness
  returns only if the WASM route gets funded.
- **Distribution.** The market research is blunt that a Windows-only VST3 cannot compete feature-for-
  feature with a €39-to-€149 product that ships AU, AAX and macOS at once — and that VRAC+'s actual
  documented complaint pattern (the "all pads come from the same slice" problem, the multi-out bug,
  no ADSR and no effects in the free VRAC) is *exactly* the ground SliceForge already holds. Round 2
  changed one half of that sentence and strengthened the other. Changed: "no adjacent tool pairs a
  kit-builder with a sequencer this deep" is no longer safe inside the family either — `redrum` has an
  internal sample-accurate pattern transport and auto-slicing in the same product. Strengthened: the
  Step 29 finding is that competitor's bug reproduced *here*, in the same place, and Step 24 is the
  fix; and the one combination nothing in the field or the family does is **chop → sequence → hand the
  sequence to the host**, which is exactly Steps 24, 26, 27 and 28 lined up. That is the defensible
  claim, and it is worth more than any new voice-adding feature. The realistic models stay
  free/open-source with a hosted browser demo, or personal portfolio; the prerequisites you do not have
  are a macOS build, signing, and a 60-90 second one-take video — and round 2 can now name what the
  video should be, because Steps 24 and 28 are its whole arc: one field recording dropped in, sixteen
  separate hits out, then the pattern recorded into an Ableton clip and played by something else.
- **Content.** The factory library is 73.9 seconds of synthesised material across six WAVs — body,
  circuit, foley, kitchen, scrap, vinyl — re-measured today with `wave` rather than taken from the
  plan (round 1 said ≈78 s over "six worlds of ~12.5 s"; built from 24 event recipes on a fixed
  0.47-0.64 s grid, `src/audio/sampleLibrary.ts:597-635`), baked into 16-bit files in
  `native/Resources/library/`. That does not deliver the "found sound" promise the copy sells ("Six
  found-sound worlds, recorded as recipes", `src/components/LibraryModal.tsx:74`), and it is
  metronomic by construction. The cheapest honest fix is real recordings — yours, or Vault-sourced —
  not better synthesis. Note the tension with Step 24: real recordings are also the first material
  where the no-slice-end defect becomes obvious, so the content fix and Step 24 should land in the
  same week or the demo will show the wrong thing.

---

## 11. Order of work

Round 1's sequence is superseded. It put the slicer rewrite (Step 7) and an offline bounce (Step 8)
ahead of any measurement, gated the bounce on a property it cannot have, and led into a parity harness
whose answer is a design decision rather than a fixture. Round 2's sequence, with each link justified:

**1. Close out Phase 0 first — 1.5 d, now 1.25 d.** Step 29 (0.25) is built, and it had to come first
because everything below is judged by `ctest`. Then Step 23 (0.25) before Step 22 (0.5), which is the
order round 2's own verification forced: Step 22 keys its pass counter on the reset that Step 23's
tolerance decides, so landing the tolerance first is what makes "pass" mean a pass. Step 5 (0.5) is the
only item left in that block that has no dependency on anything else here.

**2. Make the host able to see the instrument — 4 d.** Step 26 (1.5), Step 28 (2), Step 27 (0.5) in that
order: the pattern parameter first, because MIDI out is useless if every clip plays pattern 0; then
MIDI out; then making the offline export honest, which reuses the same swing and roll code and so
should land after Step 22 rather than before it. This block is why the sequencer stops being a feature
*inside* the plugin and becomes one in the session, and it is cheaper than anything in Phase 5.

**3. Make the kit-from-one-file work — 2.5 d, after Step 21.** Step 24 (2) then Step 25 (1). Step 24 is
the highest-value step in the document on pure listening grounds, and it is a length computation, not
an algorithm — which is why it moves ahead of Step 7. Step 25 is the fixture that makes any detector
work decidable. Both sit behind the boundary decision because `redrum` is already in this lane;
starting them before answering "who owns the chop" is how the family builds the same feature twice.

**4. Then, and only then, the heavier things.** Step 6 Rate (3 d) → Step 7 detector (4-6 d, now
measured) → Step 10 topology (2 d) → Step 8 bounce (3-4 d, gate corrected) → Step 9 contract (0.5 d) +
Step 11 Vault (1-2 d) → Step 16 licence (1 d) → Steps 12-15 (10 d) → Steps 17-19 (3.5-5.5 d).
Step 16 is one day and is the only item here that blocks *conveying* a build to anyone, so it moves
forward the moment a public release is actually on the table — it does not need to wait behind Phase 5.

Steps 1-19 and 22-29 add up to **41.75 dev-days** at midpoint (the ranges in Steps 7, 8, 11, 18, 19, 24
and 26 widen that to ~38-45 d), so "roughly 40" is the honest headline. Three days are banked — Steps 1-4
(2.75 d) and Step 29 (0.25 d) — so **~38.75 d remain**, plus Step 20 (macOS, 3-5 d) and Step 21 (a
decision, not code) outside the total. Round 1 of this document said "~31 d remain" against the same
step list; that number was arithmetic I did not check, and it is the reason the estimates are now summed
in the open here. What is left of the work that changes what you can claim in Ableton is exactly: Step 5
(0.5) + Step 23 (0.25) + Step 22 (0.5) + Steps 26/27/28 (3.75) = **5 d**. The rest is depth and drift
control.

Per phase: build, run `ctest` *after* building the test targets, then judge the audio in Ableton
level-matched against the previous build. A phase is done when the check that describes the old
behaviour fails on the new code and the new check passes on it. Step 29 is the one exception to that
rule, and it is stated as an exception rather than smuggled in: it changes no source under
`native/Source`, so its proof is "green with the code untouched, red if the loader breaks".

---

## Appendix — verification method

Round 1 (2026-11-14): eight read-only lanes; claims established by direct source reading,
line-number checks, `git count-objects -v`, `du -sb launch/*` and a sibling-README survey under
`D:/Projects/skeer-audio/`. Steps 1-4 were then built the same day: each gate was falsified by
reverting exactly one file — `Sequencer.cpp` for Step 1 (4 named checks fail, 108/112),
`VoiceEngine.cpp` for Step 2 (rate spread 1.94 dB against the fixed 0.11 dB), the modifier guard and
`setEnabled (inputActive)` for Steps 3 and 4 — and every revert was restored and SHA-checked.

Round 2 (2026-11-15): nine lanes again, but their reports were treated as intent rather than fact,
because round 1 produced two wrong claims that survived delegation. Each load-bearing claim was
re-checked by hand at the named line before it was allowed to influence a ranking. What that caught:
the two retractions in section 0; a third-party line reference in round-1 notes that pointed at
`src/audio/audioEngine.ts:105-118` when the bus fan-in is `114-119`; and the `VaultLibrary.cpp` path
ambiguity (the file is `native/Source/VaultLibrary.cpp`, not the `Source/UI/` the plan implied).
Verified with commands, this session, from the tree as it stands:

- `ctest --test-dir native/build -C Release --output-on-failure` → **67% passed, 1 of 3 targets failed**,
  Smoke `FAIL - 113/114 checks passed`, Layout and Host passed; the single failing check is the vault
  load, reproduced three consecutive runs, so it is deterministic and not a race. CPU printed 4.43%
  and 6.12% in two runs of the *same* binary — a ~40% spread between runs is why Step 19's gate has to
  be a band, not a number.
- `grep -ri "sequencer|pattern" src/` → 0 lines (the AGENTS.md claim is true today).
- `git count-objects -vH` → 318 loose objects, 203.51 MiB, **0 packs**; `du -sh .git` → 205M;
  `du -sh launch` → 73M. Step 17's numbers hold.
- Repo-wide `find` for `LICENSE*`/`COPYING*`/`NOTICE*` outside `node_modules` and `native/build` → hits
  only inside `native/external/JUCE/**`. Nothing tracked at any SKEER level. Step 16 confirmed.
- The vault DB, read-only via `sqlite3 … mode=ro` against
  `%LOCALAPPDATA%\SKEER Sample Vault\data\vault.sqlite3` (discovered by reading the candidate logic at
  `VaultLibrary.cpp:330-360`, since no `vault-path.txt` exists under `%APPDATA%\SliceForge`):
  156,237 rows `missing=0`, 146,289 with a `.wav` suffix, 12,135 of those actually on disk, and the
  first by rowid a 4096-byte AppleDouble sidecar with `Mac ` at offset 8 and NULL channels/rate/duration.
- Siblings read only, nothing in them modified: `redrum/README.md` (Expressive Drum 0.20.0) and
  `autostem/src/StemForge.Core/Engines/SeparatorFactory.cs:22`.

Step 29 was built the same day, and `native/Tests/SmokeTest.cpp` is the only file it changed:

- `cmake --build native/build --config Release --target SliceForgeSmokeTest SliceForgeLayoutTest
  SliceForgeHostTest` → exit 0; `ctest --test-dir native/build -C Release` → **exit 0, 100% tests
  passed out of 3** (Smoke 3.07 s, Layout 12.59 s, Host 0.68 s); `SliceForgeSmokeTest.exe` → exit 0,
  `PASS - 117/117 checks passed`, `info vault: 2 listed wavs readable, 1 decoded`.
- Falsified both directions with a one-line edit to `loadUserSampleFromFile`, then reverted:
  refuse-everything → 115/117, accept-a-null-decode → 116/117. The strings each run printed are quoted
  in Step 29.
- Restoration proved, not assumed: `git show HEAD:native/Source/PluginProcessor.cpp | sha256sum` →
  `e202c904cf1e…`. `sha256sum` of the worktree file does **not** match that, and must not —
  `core.autocrlf` is `true` here, so the blob is LF (42,436 B) and the working file is CRLF (43,549 B).
  The check that means "restored" is `diff <(git show HEAD:$f | tr -d '\r') <(tr -d '\r' < $f)`, which is
  empty, together with an empty `git diff -- $f`. `git status` still lists the file as modified because
  the mtime moved.

Market claims about VRAC+ (release July 2026, €39 early bird rising to €149, VST/VST3/AU/AAX on
macOS and Windows, the "same slice for every pad" complaint, per-pad multi-slicing added September
2026) come from public reporting and a KVR thread, not from the repo, and round 2 did not re-verify
them:
[Synthanatomy](https://synthanatomy.com/2026/07/beatsurfing-vrac-the-synth-that-turns-any-sample-into-a-drum-kit-now-in-fully-featured.html),
[KVR forum thread](https://www.kvraudio.com/forum/viewtopic.php?t=631449),
[Dubspot review](https://blog.dubspot.com/beatsurfing-vrac-plus-2026),
[Audio Plugin Guy on free VRAC](https://www.audiopluginguy.com/free-plugin-review-vrac-by-beatsurfing/).
Still **not** confirmed after two rounds: the "SliceForge" name-collision risk, and the current prices
of Portico, Influx and Ripchord.
