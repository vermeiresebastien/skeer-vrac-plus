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
   declares `NEEDS_MIDI_OUTPUT FALSE` (`native/CMakeLists.txt:116`) *and* `processBlock` ends with
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
  `native/Source/` is empty in a repo that links `juce_dsp` (`native/CMakeLists.txt:132`) — but what
  `Slicer.cpp:16-44` actually runs is a **broadband RMS-envelope first difference**, which is not
  the same thing as spectral flux and will not become it by renaming. Calling it spectral cost the
  plan a step that assumed the gap was only the missing FFT. It is not: see Step 25 for the three
  non-spectral reasons it misses hits today.

---

## 1. Two things the research corrected

- **The parameter count is two numbers, and `native/README.md:159-166` states both.** The layout
  declared **347** when this was first worked out — 8 global + `source.sensitivity` + 21 parameters ×
  16 pads + `global.clip` + `global.out` (`native/Source/Params.cpp`, the pad loop adds exactly 21 ids)
  — while the host saw **349**, because JUCE's VST3 wrapper appends its own `Bypass` and `Program` to
  whatever the processor exposes. Round 2 of this plan read the 347 it had derived from the layout, saw the 349 the
  test printed, and concluded the README was wrong by two; it was the reading that was wrong. Nothing
  used to catch a disagreement between them: `HostTest.cpp` only *printed* the count and asserted
  `< 300`, which 347, 349 and 345 all satisfy. `native/Tests/HostTest.cpp:110` now asserts exactly
  350 and `native/Tests/SmokeTest.cpp:181` asserts the 348 the layout builds - one more than the
  derivation above, because Step 26 appended `seq.pattern` - so the composition in the README is a
  test result rather than a claim.
- **The naming risk the market lane flagged is mostly already retired.** The installed bundle is
  `SliceForge.vst3`; the `VRAC+.vst3` copy in `%LOCALAPPDATA%\Programs\Common\VST3` was deleted on
  2026-11-14. The `Vrac` / `Erwm` codes remain in the binary, but they are invisible session
  identity and renaming them would orphan every saved set — the frozen-identifier rule is correct
  and should stay. The open item is only whether "SliceForge" itself collides commercially
  (research surfaced a `sliceforge.io` beat-saber tool and a game-audio tool of the same name;
  **unverified** — check it before any public release, not before a personal build).

`AGENTS.md`'s frozen-identifier anchors were corrected with Step 30 (section 8B) on 2026-11-16: the codes are
`native/CMakeLists.txt:109-110` (Erwm at 109, Vrac at 110), the four `VRAC_EXTRA` sites are
`PluginProcessor.cpp:974`, `:978`, `:1036`, `:1041`, and the preset folder sits at
`PluginEditor.cpp:461` and `:491` rather than `:446` and `:476`. Read those numbers as a snapshot, not
as an address: this paragraph recommended `:956`, `:960`, `:1018` and `:1023` for `VRAC_EXTRA`, and they
were stale inside a day, because every step that lands edits the very files it cites. The grep strings
(`VRAC_EXTRA`, `PLUGIN_CODE`, `getChildFile ("SliceForge")`) are the durable half. Round 2's claim that
the README should read 347 is retracted above. What the README *was* wrong about is its check counts
(109 against a 117-check suite) and "about 5% of one core"; both were fixed with Step 29 on 2026-11-15.
  (Same exemption: that 117 and the 347 above are dated reads. The live pair is 190 checks and 350
  host-visible, re-measured 2026-11-16: Step 26's host-path gate was reworked into four per-arm checks,
  Step 28 added six, Step 27's exported-file parity eleven, and item 14's wiring audit four (8F).)
Everything else in `AGENTS.md`'s frozen list checked out, including `Sequencer.cpp:12`,
`storage.ts:5-7`, `sampleStore.ts:7`, `UI/Theme.cpp:75` and `VaultLibrary.cpp:143` — the last is
`native/Source/VaultLibrary.cpp`, not under `Source/UI/`, and round 1's Step 11 cited it without the
directory. `Theme.cpp` is the reverse: it lives in `Source/UI/`, and `AGENTS.md` now says so.

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

**Step 5 — Fix the claims (0.5 d).** Three sub-deliverables were written here. One was already built,
two are refuted, and the refutations are why this entry now carries evidence in place of a task.

- **Correct the four `AGENTS.md` line references — measured, there are none.** Every `file:line`
  citation in `AGENTS.md` (lines 61, 67, 99, 101, 102, 104, 108, 109, 112, 113 and 136 — seventeen:
  `native/CMakeLists.txt:109-110`, `:119`, `:49`, `:130`, `PluginProcessor.cpp:23`, `:974`, `:978`,
  `:1036`, `:1041`, `Sequencer.cpp:12`, `Source/UI/Theme.cpp:75`, `Source/PluginEditor.cpp:461`, `:491`,
  `Source/VaultLibrary.cpp:143`, `src/state/storage.ts:5-7`, `src/state/sampleStore.ts:7`,
  `Tests/HostTest.cpp:68`) was opened and read at HEAD `4abe4b0` on 2026-11-16, and each line says what
  the prose around it claims. The instruction is the stale item, not the references. Two traps, recorded
  so nobody re-derives this: the web paths are **root-relative** — `web/src/state/storage.ts` does not
  exist, so resolving them against `web/` invents staleness — and the one real defect in the same
  paragraph is *not* a line reference: `AGENTS.md:99` names `PLUGIN_CODE` before
  `PLUGIN_MANUFACTURER_CODE`, while `native/CMakeLists.txt` puts the manufacturer at `:106` and the
  plugin code at `:107`. That is a wording fix and belongs to the stage that already edits `AGENTS.md`.
- **Machine-hold the exact parameter count - built, and not by this step.** `native/Tests/HostTest.cpp:110`
  reads `if (instance->getParameters().size() != 350)` - a literal, so it reddens when the layout
  changes - and `:148` prints the composition (`parameters: 350 = 348 layout + Bypass + Program`), with
  `:143` printing the pattern control's host index. The bare-processor view is 348; the two differ by the
  wrapper's pair, and both numbers are current.
- **Retire or scope the "no audio-thread allocation" sentence — refuted, so the sentence stays.**
  `native/README.md:461` ("32 fixed voices; no audio-thread allocation or locking") is accurate, and
  adding the caveat asked for here would have injected a false claim into a correct document. The cited
  site was wrong twice over: `PluginProcessor.cpp:223-227` is the tail of `buildRuntime`
  (`runtime.padStarts[…]` at `:223`, `engine.setRuntime (runtime)` at `:226`, `}` at `:227`, and
  `processBlock` opens at `:229`), while the code meant is `:244`
  (`juce::AudioBuffer<float> inputBuffer;`) with the assignment at `:248`
  (`inputBuffer = getBusBuffer (buffer, true, 0);`) — stale by ~21 lines *and* naming a mechanism that
  does not exist, which is the combination that makes a plan entry unsafe to execute. In the vendored
  library: `getBusBuffer` returns a pointer-referencing buffer
  (`processBlockBuffer.getArrayOfWritePointers() + channelOffset, busNumChannels, …`,
  `juce_AudioProcessor.h:669-675`), which binds the non-owning constructor at
  `juce_AudioSampleBuffer.h:91-100` whose only storage call is `allocateChannels (dataToReferTo, 0)` at
  `:99`; `allocateChannels` (`:1231-1244`) carries the comment "try to avoid doing a malloc here, as
  that'll blow up things like Pro-Tools" at `:1235` and points `channels` at the member array
  `preallocatedChannelSpace[32]` (`:1290`) whenever `numChannels < 32`, reaching `allocatedData.malloc`
  only in the `else` branch at `:1240-1242` (`:1240` is the `else`, `:1242` is the `allocatedData.malloc`). That bound is ours, not the host's:
  (`PluginProcessor.cpp:48-70`) requires exactly `kNumBuses` output buses (`Params.h:9` — 8) and
  rejects any enabled bus that is not stereo (`:52-59`), with at most one input bus of mono or stereo
  (`:60-68`), so `busNumChannels` is ≤ 2 and the malloc branch is off this path by construction rather
  than by luck. And the chain named here — `operator=` → `setSize` + memcpy — does not bind: `:248`
  assigns a prvalue, so overload resolution picks the move assignment
  `operator= (AudioBuffer&&) noexcept` at `juce_AudioSampleBuffer.h:219`, while the deep copy that
  would call `setSize (…, false, false, false)` is `operator= (const AudioBuffer&)` at `:165` and never
  runs. `prepareToPlay` is also where JUCE tells you to size buffers (`juce_AudioProcessor.h:128-137`),
  so a write there is ordinary practice, not the realtime callback.
  **Limit of the evidence, stated rather than smoothed:** the scan for allocation tokens
  (`juce::String`, `std::vector`, `new`, `malloc`, `reserve(`, `HeapBlock`, `push_back`, `Array<`,
  `operator+`) covered `PluginProcessor.cpp:230-400` and `:470-520` and returned nothing. That is a
  range scan, not a call-graph proof — `buildRuntime` (`:210`) and whatever `serviceRecording` reaches
  outside those lines was not inspected. What is met here is "no contradicting evidence", which is
  enough to leave a correct document alone and not enough to claim the plugin allocates nowhere on the
  audio thread. A counted gate — an allocator shim, or a counted `operator new` over a rendered block,
  red before and green after — is what would turn this into a proof, and nobody has built one. No
  "hold a reusable member" change is to be written: there is nothing to hold.
  One runner fact worth recording here: a standalone `SliceForgeHostTest.exe` prints its `parameters:` line and
  exits 0 only when started from the repo root; from `native/` it prints `FAIL bundle not found` and exits 1.
  It links no shared code and loads the built VST3 at runtime, and it is the ctest `WORKING_DIRECTORY`
  (`native/CMakeLists.txt:183-184`) that supplies the root for the suite. As of this commit that line prints
  `parameters: 350 = 348 layout + Bypass + Program`; the count is stated as of the commit, not as a literal.

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

*That last sentence is superseded as of 2026-11-16: of the three, one was already built and two fell to
measurement. Read the entry above rather than this 2026-11-15 account of what remained.*

*Forward note, not a correction of the record above: as of Step 26 the layout carries 348 ids and a host
sees 350, because `seq.pattern` was appended. The 347/349 pair in the paragraph above is what those runs
printed, and it stays.*

Gate: `ctest --test-dir native/build -C Release --output-on-failure` green with the exact-count
assertion in place. Build the test targets first — bare ctest runs stale binaries. What is left of this
step is documentation, so the second half of the gate is that every citation in this entry was read out
of the tree it describes rather than copied out of this text: the seventeen `AGENTS.md` references, the
four library ranges above, and the write and read sites of `stateNotice`.

Built 2026-11-16 as a doc-only retraction. Gated by `ctest --test-dir native/build -C Release` →
"100% tests passed out of 3", exit 0, with the three test targets built first (exit 0, nothing
recompiled — the tree carries no source change), the smoke program's own `PASS - 148/148 checks passed`
at exit 0, and `SliceForgeHostTest.exe` printing `parameters: 349 = 347 layout + Bypass + Program`. The
148 is Stage A's total; this entry moved no check.

**`stateNotice` — the open half of the same audit, closed as a finding rather than as code
(2026-11-16).** Real defect, LOW severity, and **no falsifiable gate for the race itself**: there is no
Windows TSan in this toolchain, and a single-threaded console harness cannot induce the interleaving.
Under the rule that a gate which cannot go red forbids the work, nothing here is a lock, an atomic or a
POD rewrite. What makes it a defect rather than a non-issue is that there are five write sites, not one —
`PluginProcessor.cpp:97-99` inside `prepareToPlay` (which opens at `:72`) and `:1055`, `:1061`, `:1089`,
`:1137-1139` inside `setStateInformation` — against exactly one reader, `Source/UI/FooterBar.cpp:20`,
which takes a **copy** of the `juce::String` that `PluginProcessor.h:65` hands back; a copy-construct
racing a reassignment is the whole exposure. The `setStateInformation` writes are same-thread as the
paint and benign. The `prepareToPlay` one is not *proven* same-thread, because in the vendored wrapper
`setState` asserts the host message thread (`juce_audio_plugin_client_VST3.cpp:2908` calling `:2912`)
while `setActive` (`:2668`) neither asserts nor marshals and reaches
`preparePlugin (…, CallPrepareToPlay::yes)` at `:2692` — the path that calls `prepareToPlay`. A host that
activates on a device-change thread while the editor is painting puts both halves on different threads.
Recorded as a reading of the wrapper, not an observation: no run here reproduced a wrong-thread
activation, and the guard this argument turns on is `JUCE_ASSERT_MESSAGE_THREAD`, a `jassert`
(`juce_MessageManager.h:496-497`), which compiles out of Release — so the difference between `setState`
and `setActive` is diagnostic, not protective, in the shipping binary.

Two neighbours of the same finding *are* gateable and are left as owner decisions, not built here:
`droppedHits` is `fetch_add`-only (`PluginProcessor.cpp:269`, `:319`), never reset and asserted by no
test, so the footer count only ever grows for the life of the instance; and the notice is never cleared
outside `setStateInformation` (`:1055`), so a warning outlives the state that caused it.

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
(slice ends - built, 8O) is the part of "kit from one recording" that a listener actually hears, it is a
length computation rather than an algorithm, and Step 25 builds the scorer that makes any detector work
decidable. Step 7 now runs after both.

**Step 7 — Onset detection with an adaptive threshold (4-6 d).** *Round 2: this step now runs after
Step 25, which builds the measurement first. Do not start it until the fixture exists.*

Verified state: the detector is time-domain only — `grep` for `juce::dsp::FFT`, `performFFT` or any
`juce_dsp` include under `native/Source/` returns **nothing**, in a repo that already links `juce_dsp`
(`native/CMakeLists.txt:132`). `Slicer.cpp:8-96` is mono-summed RMS, 128-sample hop (±2.7 ms),
rectified first difference, 5-tap local maxima, fixed floor `0.012 × max`, fixed −6 ms pre-shift,
115 ms minimum gap. It is *line-for-line the same algorithm* as `src/audio/slicer.ts` (same `HOP = 128`
/ `WINDOW = 512`, same −6 ms pull-back, same `0.07` s dedup, same `8 + 46 × sens` target), which is
good news for the port and bad news for soft, pitched or noisy onsets.

The upgrade that earns its cost: log-magnitude spectral flux via `juce::dsp::FFT`, a median-filtered
adaptive threshold, min-to-max rather than fixed-floor selection, and parabolic sub-hop refinement —
an optional "snap to host grid" toggle for the last 10%. Zero new binary bytes, zero licence, no
absent toolchain, and it lands in the existing message-thread seam at `resliceAndAssignStarts`.
Reject a learned onset detector: it is the same 20% of benefit for an order of magnitude more cost.

Also in this step: native has no fallback when zero onsets are found — `Slicer.cpp:113-117` yields an
all-zero map and `padStartsFromSlices` (`:146-153`) therefore hands sixteen pads start 0, while the web
edition has `evenStarts` (`slicer.ts:169`). Port it. `SliceMapping` was dead code when this was written;
Step 24 wired it (`PluginProcessor.cpp:805`, asserted at `SmokeTest.cpp:2913-2942`), so that half closed.

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
is clean on the shared surface (the ids, ranges and units both editions carry match `src/types.ts:73-140`);
it is the math and the defaults that have split.
  NOTE, as of Step 26: the native layout now carries 348 ids and the web layout 347, because `seq.pattern`
  exists only in native. So the sentence above is about the surface the two editions still share, not about
  the two files holding the same count - the web side is behind, not the native side wrong, and sweeping
  `src/types.ts` is its own task.
The three that matter: native runs the master chain once **per bus** — 8 chains,
`VoiceEngine.cpp:677-688` — while web sums all eight outputs into **one** chain
(`src/audio/audioEngine.ts:114-119` connects every bus's gain to the same `chain.input`), and web has a
`DynamicsCompressor` at −7 dB 5:1 (`src/audio/masterChain.ts:57-62`) that native does not have at all;
13 of 16 default pad lengths differ between `native/Source/Params.cpp:49-66` and `types.ts:247-252`; and
`fastTanh` (`VoiceEngine.cpp:75-81`) is a rational approximation against `Math.tanh`.

Two of those three were closed by the Step 30 (section 8B) parity pass on 2026-11-16, and the sentence above is
left as the measurement it was: web no longer has a `DynamicsCompressor` at all (`src/audio/masterChain.ts`
is filter → tone → master fold → volume → output stage, matching `sliceforge::MasterChain`), and
`KIT_LAYOUT` (`src/types.ts:252-276`) is now a row-for-row transcription of `kPadDefaults`
(`native/Source/Params.cpp:49-66`) — all 16 rows were compared column by column, and `length` and `pan`
are per-pad rather than per-family because pads 7 and 8 are the same hat model at 0.09/0.08 with
opposite pan. What is *not* closed is the per-bus-versus-one-chain topology, which is Step 10's design
question and is why this step stopped promising a golden-vector harness.

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

**Step 16 — Licence floor (1 d).** DONE on 2026-11-17, including the string 8I landed. `LICENSE` (AGPL-3.0) was
already tracked; what was missing was conveyance **in the package**, and it is there now:
`native/Package/THIRD-PARTY-NOTICES.txt` names JUCE 8.0.4 at revision 51d11a2b, the Steinberg VST3
headers, the codecs with a symbol census that proves which are compiled in and which are not, the six
OFL fonts with each `.ttf`'s own copyright line and the licence text copied byte-identically, and the
factory WAVs as in-repo synthesis. The zip carries `LICENSE` and the notices; the About box cites both.
Closed by 8I, which refutes this line's own reason: `COMPANY_COPYRIGHT` is a `juce_add_plugin` argument (`CMakeLists.txt:105`).

**Step 17 — Make the zip reproducible and take the binaries out of git (0.5 d, owner decision).** DONE on
its determinism half, 2026-11-17 (§8L); the git half is still yours. Measured when this was first written:
`.git` was 205 MB, 318 loose objects, **zero packs**; `launch/` was 76.5 MB in HEAD; the zip and the exe were
plain blobs, and the restamp in `92336b5` added a fresh 27.75 MB blob for a payload-identical archive - two
objects of 27,709,732 B with different hashes - because `native/package-launch.ps1` let `ZipArchive` stamp
each entry with the clock it was running on (§8J measured 68 differing bytes of an otherwise identical re-export).
So every export cost about +28 MB forever, and it no longer does: the entry clock is pinned, and two exports
seven minutes apart are now one object. What is left is the decision - keep `launch/` in git (a recorded owner
call from 2026-11-14; Git LFS stays genuinely inactive, no `.gitattributes` and no `pre-push` hook), or move it.

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

Built 2026-11-16 in `4abe4b0`, gate red first: `FAIL  a looped bar re-rolls its probability` with pass 1
and pass 2 printing the identical 40-character signature `-PPPPP--P--PPPPPPP----P-PP-PPPPPP`, then green.
The check cannot pass for the wrong reason, and the reason is stronger than "same seed, same result":
`probabilityRollFor` is pure splitmix64 over `patternIndex`, `pad`, `absoluteStep` and `passCount` with no
RNG in it, and every `random.nextFloat()` in `Sequencer.cpp` belongs to the pattern generator and the
randomiser, not to the playback path (`:159` carries `juce::ignoreUnused (random); // see
probabilityRollFor`). So the `juce::Random` carried across the two render passes cannot be what makes them
differ, and the only input that changes between them is the pass counter. Two gaps in this gate were
closed later, both by Step 26: it asserted a bare inequality where a counted one is honest (21 of 33 steps
move between the passes, now pinned as a literal), and it covered one lane where the spec asked for
sixteen, which left the `(pad + 1)` term of the roll unexercised at any pad but 0.

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
predicts and assert **one** clear; then jump one whole bar and assert **two**. "One", not the zero this
entry originally asked for: engaging the transport clears the step history once, so a correct run counts
the engage plus the seek, and zero would demand that the engage not clear. Counted as history clears
through a test-only seam - the field is `SeqPlaybackState::historyClears`, one counter for both causes
(transport engage and tolerance trip) after the Step 26 rename, and "a reset happened" is otherwise
invisible from outside. Must fail today on the first half.

Built 2026-11-16 in `4abe4b0`, with the gate red first: `FAIL  sub-block host position rounding is not a
seek  [clears=9 expected=1]`, and the audible symptom red alongside it, `FAIL  overlapping host blocks do
not play one hit twice  [fires at 64000 = 2 expected=1 clears=60]`, then both green. Common tail for the
pair: `PASS - 148/148 checks passed`, `ctest -C Release` 100% out of 3, `npm test` 43 passed / 5 files,
`npx tsc -b` exit 0.

**Step 24 — Give every slice an end (1.5-2 d).** DONE on 2026-11-18 in both editions - recorded in 8O.
The single most audible step in this document.

Verified state when this was written: `Slice` is `{ double time; float strength; }` (`Slicer.h:10-14`). There is no end, no
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
precondition, promoted to its own step because round 1 buried it in a gate. **Status 2026-11-21: the
ruler is built and falsified - phases 1, 2 and 4 landed, see 8Q; what remains is phase 3's nine
rendered takes, which only the owner can press Export for, and the bless-plus-commit that follows.**

Verified state, none of it spectral: `kFloorRatio = 0.012f` (`Engine/SlicerTuning.h`, was inlined at
`Slicer.cpp:49`) is relative to the
*single loudest* rise in the file, so one crash buries every hi-hat in the same take; `kMinGapSeconds =
0.115` (same header, was `:48`) is hard-fused in seconds, so a 1/16 hat run at 200 BPM — 75 ms apart — cannot be sliced
at any sensitivity; and `target = round(kTargetBase + kTargetSpan × sens)` (was `:47`) means the knob labelled "Auto Slice
Sensitivity" changes only *how many* slices you get, never *where* they land or how weak a hit has to
be to qualify. The web edition has the same three constants (`src/audio/slicerTuning.ts`), so this is one
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

Built 2026-11-16, red first and then green, and re-measured on the final tree after the host-path gate was
reworked. Deleting the layout append printed
`FAIL  the parameter surface is the documented 348 controls  [347]` and
`FAIL  seq.pattern is a four-choice parameter appended after every existing id  [param=null range=0.000..1.000 step=0.000 last=global.out]`
at 160/169, alongside `selecting a pattern moves the parameter and tells the host`, both restore checks,
the parameter-over-blob disagreement check and three arms reading `fixture invalid param=0 want 1 mirror=2`
- and note what is *not* in that list: not one arm's render assertion fired, because the null guard answers
0 and the pre-check answers first (section 8C attributes each mutation to the component that catches it);
the dated first read of the same mutation was 149/151. Underneath it the host test printed
`FAIL expected 348 layout parameters plus the wrapper's Bypass and Program, got 349`.
That second line is the whole story about the count-only gate, so the falsification was repeated with the
total held at 350 by moving the append in front of the output-stage pair: smoke reddened on
`last=global.out` at 168/169 (first read 164/165) and the host test reddened on index position alone -
`FAIL seq.pattern is not the appended four-step control a host must see: index 345 of 350, steps 4,
default 0` - steps, default, name and discreteness all still correct, so the identity conjunct is what
fired. Both of those probe runs sit in logs named `…-m1final-…` and `…-m2final-…`; they are red
falsifiers wearing a `final` token, and since `.gitignore:10` excludes `native/build/pm/` and a log must
never be renamed after the fact, the strings quoted above are the record and the filenames are not.
Restored, the tree is `PASS - 169/169 checks passed` at CPU 4.45% of one core and
`PASS VST3 host test` printing
`seq.pattern: host id 825878785, index 347, 4 steps, default 0, named Active Pattern` and
`parameters: 350 = 348 layout + Bypass + Program`, with layout at 28 checks and ctest 100% out of 3, all at
exit 0, with `Sequencer.cpp` verified at `6c69b8d8…` in the same command as the green run.

Three things the entry above got wrong or did not know, and they are the record, not the plan:
- **The control is an `AudioParameterChoice` of four names, not the `AudioParameterFloat` 0…3 stepped by
  1 specified here.** `AudioParameterFloat` does not override `isDiscrete()`, so a stepped float keeps the
  `AudioProcessorParameter` default and the host draws a fader with continuous interpolation
  (`juce_AudioProcessorParameter.h:191-196` says so in its own doc comment); `AudioParameterChoice`
  overrides it (`juce_AudioParameterChoice.h:149`) and gives `getNumSteps() == 4` plus value texts. The
  Gate's "right range and step" survives the substitution; the widget does not.
- **"HostTest asserts the id exists" is not achievable**, because a hosted parameter reports the hashed
  numeric `Vst::ParamID`, never the APVTS string: `juce_VST3PluginFormat.cpp` builds `getParameterID()`
  as `String (cachedInfo.id)` and the export hashes the string in `generateVSTParamIDForParam`. `seq.pattern`
  surfaces as `825878785`, and `HostTest.cpp` pins that decimal with both library sites named in the comment
  above it. The string id is gated in `SmokeTest`, where it is the real thing - which is also why the
  frozen-identifier rule in `AGENTS.md` now has a second reason to hold: the hash is derived from the string.
- **`kSeqPatterns` and `kSeqPatternFromDocument` moved to `Params.h:20` and `:26`.** The layout declares the
  bank and the cache clamps the host into it, so the constants belong beside the layout; `Sequencer.h`
  includes `Params.h` and keeps its own copy of `kSeqSteps` and `kSeqChainLength`. The `Sequencer.h:12`
  cite above is where the constant sat when this entry was written.

Authority, and how far it reaches: the parameter is the render path's answer, the document's `activePattern`
is the mirror the offline callers read (export, preset blobs, standalone playback) and the field the next
save records. Four arms drive `patternIndexForStep` with the mirror set to the *next* pattern each time, so
no arm agrees by construction; the clamp case (`firedPads (99)`) and song mode (the chain wins and asserts
the exact set of heard pads, not the last one) sit beside it. `selecting a pattern moves the parameter and
tells the host` now carries an `AudioProcessorValueTreeState::Listener` spy, because the value alone was
green while nothing notified. Two serialisation falsifiers cover a pre-`seq.pattern` blob and a blob that
disagrees with the parameter. The last hole this entry can name is the wiring between the cache and the
scheduler, gated by the four per-arm host-path checks - see section 8C, which also corrects what this step
first wrote about whether that was possible at all, and records the two transport checks whose expectation
turns out to be pattern-coupled. The tolerance regime above one step per block stays open, with its own
defect entry in 8C.

Side effects, all swept in this commit: the layout is 348 parameters and a host sees 350, so `AGENTS.md:136-142`,
`native/README.md:48`, `:154-161`, `:199-204` and `:405` moved off 347/349 - and 348 (bare) and 350 (host) are
both current, not a typo for each other, because the wrapper appends `Bypass` and `Program` after the
layout. `SmokeTest.cpp:181` asserts 348 and `HostTest.cpp:110` asserts 350, each anchored to the previously
measured value in its own view rather than re-derived.

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

Built 2026-11-16, red first. With the eleven new assertions in place and the exporter still at its
previous shape, the smoke suite printed `FAIL - 175/184 checks passed` (exit 1) on `FAIL  the exported
clip swings the odd steps [clip 36=[0 240 480 720 960 1200 1440 1680 1920 2160 2400 2640 2880 3120 3360
3600 ]]`, `FAIL  a muted lane reaches the clip and is silent in the render [clip notes=4 live events=0]`,
`FAIL  the clip's probability lane is pass 2's draw [clip 41=[0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ]
live=[2 3 4 5 7 10 12 14 15 ]]`, `FAIL  the render path and the clip fire the same steps of the
probability lane [live2=[2 3 4 5 7 10 12 14 15 ] clip2=[0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ] live3=[0
1 2 5 6 7 8 10 11 13 15 ] clip3=[0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ]]`, `FAIL  the clip's onsets are
the onsets the scheduler picked` (same two lists, ticks not steps), `FAIL  the printed clip is a named
pass that re-rolls on request [pass2=… pass3=… default=… all sixteen steps]`, `FAIL  a soloed lane is the
only lane the clip prints [notes=[36 37 39 41 ]]`, `FAIL  the clip says which pass it is a snapshot of
[text=[SKEER SliceForge pattern]]` and `FAIL  flam and ratchet survive into the exported clip [clip
37=[931 960 1080 ]]`. Three of those nine — the swung-tick list, the pass-2 draw and the flam/ratchet
list — were red for two reasons at once, because my first draft had dropped the trailing space that this
suite's list builders put after every element, so the expectations could never match even a correct
exporter. Fixing that is what turned `flam and ratchet survive into the exported clip` green with no
change to the exporter at all: it is a characterisation check of what the old exporter already got right,
not a Step 27 gate. Every literal below was therefore re-proved red in its final shape by the mutation
series, and that series — not this paragraph — is the evidence to trust.

The change is five things, none of them a second copy of a decision. The shuffle is pulled out of
`scheduleAbsoluteStep` into `swingPpqForStep` (`Sequencer.cpp:168`), which the live path calls at `:195`
and the exporter at `:917`; behaviour at the live site is byte-for-byte the same expression, so the only
new thing on the render path is a function call. The exporter now asks the render path's three questions
in its order — `laneIsAudible` (`:902`), the step's own `on`/`probability <= 0` flags (`:907`), then
`probabilityRollFor (…, exportPass)` (`:910`) — where the live equivalent is the single fused test at
`:226`. The pass number is single-sourced: `kFirstRenderPass = 2` (`Sequencer.h:25`) with
`passCount = kFirstRenderPass - 1` (`:166`, value-identical to the old literal) and
`makePatternMidiFile (data, exportPass = kFirstRenderPass)` (`:117-118`), because the reset that engages
a clock increments the counter *before* it schedules, so pass 2 is the first thing anybody hears and a
printed clip is stamped with that same pass by default. The snapshot claim is written as an ASCII text
meta event inside the file (`Sequencer.cpp:874-881`), not in the UI's suggested name
(`SequencerPanel.cpp:950-951` still offers `SKEER SliceForge Pattern.mid`), because a name is the first thing
a user renames. The note map stops being a literal: `kPadMidiChannel`/`kPadMidiBase` (`Params.h:14`,
`:19`) are what the exporter emits (`Sequencer.cpp:930-936`) and what the live out already emitted, so
the clip and the wire cannot drift apart by construction.

Restored and re-measured: `PASS - 186/186 checks passed` at 4.59%, 4.57% and 4.77% of one core on three
invocations (exit 0 each time), layout at 28 checks, `PASS VST3 host test` printing
`parameters: 350 = 348 layout + Bypass + Program` and `midi out: 9 note-ons / 9 note-offs over 48 blocks`,
ctest 100% out of 3, every one at exit 0. All three test binaries and the VST3 were rebuilt before the
runs — the first host-test attempt judged the previous plugin bytes because only the console targets had
been rebuilt, which is P1's trap exactly.

Five mutations were then run against the final bytes, each restored byte-exactly by hash: no swing term in
the exporter → 184/186 (`the exported clip swings the odd steps` + the onsets differential); audible gate
deleted → 184/186 (mute check `[clip notes=4 live events=0]` and solo `[notes=[36 37 39 41 ]]`);
probability draw deleted → 183/186 (pass-2 literal, differential, named-pass); `exportPass` frozen to
`kFirstRenderPass` → 184/186 (differential + named-pass: pass 3 then equals pass 2, and the default is no
longer distinguishable from a request); the shared `swingPpqForStep` returning `0.0` → 184/186, reddening
`the exported clip swings the odd steps` *and* the render path's own `micro-timed steps survive
out-of-order block boundaries`, while both differentials stayed green. That last one is the honest
limitation of a differential: when the code both sides share goes wrong, they agree. It is why the tick
lists are pinned as literals (`SmokeTest.cpp:2171`, `:2191`, `:2211`) and why those three literals are
measurements of the render path, not formulas re-derived in the test. The host test is swing-blind — it
printed the same `9 note-ons / 9 note-offs` on the mutated bytes as on the clean ones — so it gates bus
availability and event counts (Step 28's job) and nothing about timing.

The falsification harness itself needed two fixes before its evidence could be trusted, and both are
worth recording because each would otherwise read as an engine result. The first pass died writing its own
log — it concatenated the host test's *exit code* where it meant the output — which left the tree inside
the last mutation and that leg's host log empty. The retry then ran a command string whose path carried a
single backslash, so python read `"\n"` as a newline, `cmd` answered `'.' is not recognized as an internal
or external command`, and the suite "exited 1" **without ever starting**. The re-run mutated
`swingPpqForStep` to `return 0.0`, built at exit 0, printed `FAIL - 184/186 checks passed` at smoke exit 1
on `the exported clip swings the odd steps` plus the render path's own `micro-timed steps survive
out-of-order block boundaries`, and restored in a `finally` block whose header records the mutated and
restored hashes: restored bytes = the pristine copy = the shipped blob (`Sequencer.cpp` is `i/lf w/lf`, so
its worktree hash equals its blob hash and the restore is self-proving). The final gate is the committed
tree: source restored 10:06:40 → `Sequencer.obj` 10:07:00 → smoke exe 10:07:01 → VST3 module 10:07:23 →
smoke `PASS - 186/186` (exit 0), layout 28 checks (exit 0), host `350 = 348 layout + Bypass + Program` and
`midi out: 9 note-ons / 9 note-offs` (exit 0), ctest 100% out of 3 (exit 0). Read this as the rule for the
next mutation series: record the pre-mutation hash *into* the red log, capture the exit codes rather than
the strings alone, and re-gate after the restore — a tree caught mid-mutation is not the defect, the
missing post-restore green is.

Not verified: no real host imported the printed clip and compared it against what the plugin played, so
"the pattern you heard" is proven between the two code paths in one process, not against Ableton's
timeline; the drag-to-host path (`SequencerPanel.cpp:946-961`) is unexercised by any test; and the file's
swing/tempo metadata is asserted as parsed ticks, not as what a host does with the set-tempo event.
Deliberately left: the sub-hit velocity asymmetry — the clip writes grace and ratchet hits at the lane's
main velocity while the scheduler dims them (0.62 / 0.72), so a printed flam is louder than a played one.
That is a sound decision, not an engine bug, and is now pinned by a characterisation check
(`SmokeTest.cpp:2183`, `"931 960 1080 "`) so that changing it is a decision someone makes on purpose.
Also deliberately not built: the plan's "print the backbone" switch (probability gates off) and a
pass-number *file name*, both UI work; and stamping the clip with the pass you just heard, which needs
`SeqPlaybackState::passCount` — audio-thread state — synchronised before any UI may read it. Both are
owner decisions, and the exported clip always says "pass 2" until then.

**Step 28 — Send the sequencer to the host as MIDI (2 d), or delete the implication that it does.**

Verified state: two independent blockers, both named in item 3 above — `NEEDS_MIDI_OUTPUT FALSE`
(`native/CMakeLists.txt:116`) and `midiMessages.clear()` at the bottom of `processBlock`
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

Built 2026-11-16, red first. With production at `82d8866` and the new assertions in place, the smoke suite
printed `FAIL - 171/175 checks passed` on `FAIL  MIDI out carries nothing but four note-on/note-off pairs
[0/0 notes, 0 other events]`, `FAIL  MIDI out notes carry the scheduler offset, 36 + pad and the 32-sample
gate [0 events: ]`, `FAIL  MIDI out notes are all on channel 10 [0 off-channel of 0]` and
`FAIL  MIDI out velocity uses the exporter's round(velocity x 127) mapping [0 note-ons: ]` (exit 1), and the
host test exited 1 on `FAIL hosted plugin advertises no MIDI output bus (producesMidi() is false)` with
`buses: 1 in / 8 out` still on the line above it — the audio layout is unchanged; what was missing is the
event bus. The change is `NEEDS_MIDI_OUTPUT TRUE` (`native/CMakeLists.txt:116`, plus a re-configure: the
macro is generated into `Defs.txt`, so without one the flag is a no-op), one shared drum-channel constant
(`Params.h:19`), a `producesMidi()` that follows the build's own answer and survives targets where the
macro does not exist (`PluginProcessor.h:43-50`), and `emitSequencerMidi` (`PluginProcessor.cpp:12-61`)
called in place of the bare clear at the bottom of `processBlock` (`:469-470`). Restored and re-measured:
`PASS - 175/175 checks passed` at CPU 4.53% of one core (4.57% and 5.11% on the two earlier runs of the same
bytes), `PASS VST3 host test` printing
`midi out: 9 note-ons / 9 note-offs over 48 blocks, peak 0.892715`, layout at 28 checks, ctest 100% out of
3, every one at exit 0. Seven mutations were then run against the final bytes and every one reddened the
smoke suite with a named check — no emission, no clear-first, gate 32→48, channel 10→1, truncated velocity,
onset +1 sample, and a loop bound that emits a phantom hit on blocks the scheduler left empty — but five of
the seven leave the host test green, so read 8D before repeating any of this as "the host test gates the
wire". Not verified: the Ableton acceptance above, which is the only evidence that a real sequencer records
what this emits; and a host holding an already-instantiated plugin will not see the bus until reload, which
is why it is in the commit message.

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

## 8B. Phase 7 — round 3, the hardening and parity pass (recorded 2026-11-16)

**Step 30 — Record the hardening/parity pass that landed without a spec (0 d, documentation).**

Verified state: Steps 1-29 were the entire plan, yet the tree took its largest single change since
the sequencer landed, and none of it had a step, a gate record or a `Built` line. Two sentences
compensated by inventing one: the frozen-identifier paragraph in section 2 credited the `AGENTS.md`
anchor corrections to a step that does not exist, and section 6 credited two of the 23 parity
divergences to a parity "pass" that was never numbered. Both now point here, with their measurements
left intact.

Decision: one entry, because the two halves are one pass and one rollback point, and because the
distinction worth recording is not native-versus-web but *which claims are now backed by a check*.
What the pass changed, re-verified in the current tree:

- **Native hardening.** `readFinite` (`Sequencer.cpp:310-320`) rejects a non-finite float and clamps
  the rest instead of letting a NaN latch into voice and filter state; it is applied to every step
  field and to the two header floats the pattern deserialiser reads (`:375-387`, `:755-756`). A blob
  shorter than `patternPayloadBytes` is refused rather than decoding as an empty pattern
  (`:328-333`, applied `:362-364`). A hit a block cannot fit is counted (`Sequencer.h:138`,
  `Sequencer.cpp:94-110`) and accumulated in the processor (`PluginProcessor.cpp:318-319`,
  `PluginProcessor.h:76`) so the footer can name it (`FooterBar.cpp:21-25`). The master stage clamps
  the block it is handed to the prepared buffer size (`VoiceEngine.cpp:679-692`). NaN can no longer
  leave the plugin (`Dsp.h:287`, reasoning `:280-286`). A reload that abandons a take, loses a
  factory source, cannot decode an embedded recording or cannot read the pattern blob says so in the
  same footer slot as the dropped-hit count (`PluginProcessor.cpp:97-98`, `:1061`, `:1089`,
  `:1137-1140`; accessor `PluginProcessor.h:65`; reader `FooterBar.cpp:20-29`), and an embedded
  recording's sample rate is sanity-checked before decode (`:1071-1073`). Playback events are a
  reused, capacity-bounded array rather than a per-block allocation (`PluginProcessor.h:121-134`).
  Layout assertions gained `isPlacedOnScreen` (`Theme.h:63`) and explicit `*ForTesting` seams
  (`PadEditorPanel.h:24-26`, `VaultPanel.h:24-32`, `SequencerPanel.h:33-36`). The suite now fails
  when an exposed parameter id has no formatting branch (`SmokeTest.cpp:1283-1289`), which pins
  `Controls.cpp:36-43`.
- **Web/native parity.** The browser master chain's `DynamicsCompressor` is gone: filter → tone →
  one tanh master fold → volume → output stage, sharing native's curve (`masterChain.ts:16-28`,
  `:73-81`), and the offline bounce folds before the master gain for the same reason
  (`exportKit.ts:88-89`). A non-finite output becomes silence in the browser too
  (`outputStage.ts:49`, mirroring `Dsp.h:287`). State read back from local storage or a dropped JSON
  file is rebuilt field by field through the same ranges `createParameterLayout` declares
  (`types.ts:426-537`, wired at `useKit.ts:335-368`). A persistence write that did not stick is
  reported instead of assumed (`storage.ts:33-52`, `:54-74`, `sampleStore.ts:39`, `:67`,
  `App.tsx:127`, `:276-278`, `:300-302`). The bundled worklet was regenerated, and the modal's
  autofocus effect no longer re-fires on every keystroke (`Modal.tsx:18-25`).
- **Packaging and docs.** `native/package-launch.ps1` stages every replacement, SHA-256-compares it
  against the build tree, re-hashes it after landing, writes the zip last and hashes the whole bundle
  rather than only the module DLL (115 insertions / 28 deletions); `native/CMakeLists.txt` passes
  `-Config "$<CONFIG>"` so a Debug launch target exports Debug binaries rather than whatever Release
  artefact was left on disk; `launch/start.ps1` grew the retire-don't-delete path, the single system
  VST3 destination and the read-back verification (106 insertions / 29 deletions); and the docs were
  synced (`AGENTS.md` 34/19, `README.md` 4/4 including `:245` "43 tests, 5 files",
  `native/README.md` 11/9 including `:155` the 349 composition and `:197`/`:389` the 139 total).
  *Both of those figures are superseded, and this bullet records what that commit changed, not what is
  true now: as of Step 26 the composition is 350 host-visible = 348 layout + Bypass + Program, and the
  total is whatever the newest green smoke run prints (`native/README.md` carries it, stamped there).*

What proves it — the checks that exist today, not a red-then-green record (see the last bullet):
the smoke suite refuses a pattern blob missing 1, 8 or half its bytes (`SmokeTest.cpp:736-750`);
plants a quiet NaN in a saved velocity, proves the poison landed, and asserts it reads back as the
0.8 default (`:762-770`); asserts the output ceiling maps NaN to silence and ±inf to full scale, and
that a poisoned block leaves the stage finite (`:1120-1135`); and asserts every exposed parameter id
has a formatting branch (`:1283-1289`). The web side runs 43 tests in 5 files, 13 of them in the two
files this pass added (`masterFold` 3, `sanitise` 10). `ctest -C Release` reports 3/3 and
`npx tsc -b` is clean.

Still missing, stated rather than smoothed over:

- Nothing re-validates APVTS parameter values inside `setStateInformation`; a corrupt host-state XML
  can still place an out-of-range value in every parameter, and `Dsp.h:287` is the only backstop for
  the ones that reach audio.
- `npm run build`'s `vite build` leg and its `dist/` output are unexercised, and no browser-edition
  runtime or screenshot lane was run against this pass.
- `LayoutTest.cpp` was rebuilt around the new panels (148 insertions / 20 deletions) but its
  snapshots were not reviewed by eye.
- Every gate above shipped with the fix it checks, so none of the hardening items has a
  red-then-green record. That is the honest reason this entry exists: a step number and a `Built`
  line are a claim about process, and this pass cannot make that claim.

Built 2026-11-16 (this pass), gated by: `ctest --test-dir native/build -C Release` 3/3, the smoke
program's own `PASS - 139/139 checks passed`, `npm test` 43/5 and `npx tsc -b` clean — all run
against the tree this entry describes.

---

## 8C. The defect Step 26 left open, the gate its first pass called impossible, and two checks that do not
test their own names (2026-11-16)

- **No host resync tolerance separates a seek from noise once half a block is as wide as a step.** The
  committed predicate is `jmax (0.02, blockPpq * 0.5)` (`Sequencer.cpp:304`), and legitimate sub-block
  disagreement is bounded by half a block while the smallest deliberate move is one step (`1 / division`).
  Those are separable only while `0.5 * blockPpq < 1 / division`, i.e. `numSamples * bpm < 720000` at 1/32
  on 48 kHz. Past the seam a one-step edit reads as playback and is swallowed (`SmokeTest.cpp` keeps the
  case green as the named `KNOWN DEFECT:` characterisation), which is reachable from the UI because
  `UI/SequencerPanel.cpp:270` offers `1/32`. Capping the tolerance at half a step was measured, not argued.
  (Probe logs live under `native/build/pm/`, which `.gitignore` excludes at `:10` - so the strings quoted
  below are the record, not the files.) The run printed three reds, and only one of them is the cap doing
  its job:
  `KNOWN DEFECT: a one-step 1/32 edit inside a 0.28 ppq block is not a seek` moves from `clears=1` to
  `clears=2`, which is exactly the falsifier that check's own comment names (`SmokeTest.cpp:953-955`).
  The other two are collateral, and both sit at settings the committed formula already separates.
  `sub-block rounding on a 1/32 grid is not a seek` (`SmokeTest.cpp:968-984`) is 4096 samples at 120 BPM
  with 0.08 ppq of per-block rounding, and it reddens at `clears=9`; `blockPpq` there is 0.1707, so the
  honest window is `(0.0853, 0.125)` - above half a block or every block trips, below one 1/32 step or a
  seek is swallowed - a cap at half a step lands it at 0.0625, and 4096 x 120 is well *below* the 720000
  seam. `a small block keeps the original tolerance window` (`SmokeTest.cpp:933-943`) is 512 at 120 with
  0.015 ppq of rounding and also reddens at `clears=9`, and that third red is the one nobody predicted -
  it is what identifies the expression that ran. The probe used the dispatch brief's own clamp,
  `jmin (blockPpq * 0.5, 0.5 / division)`, which has **no floor in it**: at 512/120 that is
  `min (0.0107, 0.125) = 0.0107`, under the 0.015 ppq the check calls ordinary playback, so every block
  trips. The floored variant `jmin (jmax (0.02, blockPpq * 0.5), 0.5 / division)` predicts that check
  green, so it is not what ran, and 0.0107 / 0.0625 / 0.0625 reproduce all three printed numbers. The
  brief's aside that "the 0.02 floor can never beat the cap" holds for the floored form only. So the cap
  is not the neutral "policy choice above the seam" this
  entry first called it - it spends the loud symptom (per-block double hits and torn flam/ratchet, since
  the same `reset` clears the step history and bumps the pass counter) to buy seek detection, and it
  breaks two cases where nothing was being bought. That trade is the owner's call, not a bug fix.
  The discriminating idea, if it is ever funded, is grid alignment: `std::abs (ppqStart * division
  - std::round (ppqStart * division))` near zero points at a deliberate edit, and `division` is already in
  scope. It needs its own control, because a host that always quantises to the grid would read as
  always-seeking.
- **The wire from the cache into the render path was ungated - and this entry's first pass called that
  unfixable, which was wrong.** Step 26 moved the pattern choice into `seq.pattern`, and `processBlock`
  passes `parameterCache.readActivePattern()` into `processSequencerBlock` as one int per block
  (`PluginProcessor.cpp:367`, with the argument itself at `:371`). The gates covered both ends - the
  callee, by a four-pattern differential driven through the new argument, and the cache, by
  `getPatternParameter()` - but nothing covered the wiring between them. Measured, at the time: stubbing
  `readActivePattern()` to `return 0` reddened four checks - `selecting a pattern moves the parameter and
  tells the host`, `a session saved with the parameter on pattern C comes back on pattern C`, `a session
  written before seq.pattern existed keeps its stored pattern` and `when the parameter and the blob
  disagree, the parameter plays and the mirror follows` (161/165, that run's own total) - and replacing the
  call-site argument with a constant reddened **nothing**. What this entry then asserted - that a headless
  test has no `AudioPlayHead`, so the host path cannot be driven, and that the only pad observable is a
  millisecond stamp too coarse to use - was a skipped search. `SmokeTest.cpp:15` already defines
  `MockHostPlayHead` and two fixtures already run a real `processBlock` through it (`SmokeTest.cpp:469`
  and `:528`, where each fixture declares its head; `render` is what calls `processBlock`, at `:95`);
  `getPadHitStamp` is
  public (`PluginProcessor.h:86`) and `updatePadHit` fires for every event the scheduler emits
  (`PluginProcessor.cpp:450`); and the differential needs no sub-millisecond resolution because it asks
  *which pad*, not *when*. The shipped gate is four checks, not one: `the host path plays the pattern the
  parameter names (pattern A)` through `(pattern D)`. Each arm builds its own processor and playhead,
  drives `seq.pattern` to `p`, sets that arm's document mirror to `(p + 1) mod kSeqPatterns` so no arm can
  agree by construction, and asserts the exact pair of pads that arm stamped - `wirePads { 3, 10, 9, 4,
  1, 12, 6, 7 }`, two per pattern, over 16 blocks of 512 at 120 BPM advancing by the engine's own
  `blockPpq`. Per-arm processors are load-bearing and were learned the hard way: the first cut was a single
  check with one shared processor and a *cumulative* expectation, and under the rotation probe it printed
  `arm 2 saw [9 4 1 12 6 7 ] expected [3 10 9 4 1 12 ]` at 165/166 while the fourth arm, whose cumulative
  set is the complete set, went green on "everything fired eventually". Four per-arm checks cannot saturate.
  Bracketed in both directions, and the asymmetry matters. Rotating the call-site argument reddens all four
  arms - `saw [9 4 ] expected [3 10 ] mirror holds pattern 1` through `saw [3 10 ] expected [6 7 ] mirror
  holds pattern 0` at 165/169 - and *that* is the falsifier that qualifies the gate. It was re-run on the
  bytes this commit ships, after the two playhead checks below were tightened, because the first n/n run
  predated them: same four strings, same four peaks to six decimals, `165/169`, exit 1. Pinning the argument to
  `0` reddens only arms B, C and D, because pattern A is what arm A is supposed to hear, and prints
  `saw [3 10 ] expected [9 4 ] mirror holds pattern 2` at 166/169 (dated read: that probe ran before the
  byte-tightening above, and is quoted as corroboration only, so it was not re-run): a constant mutation can never redden all
  n arms, since each constant is some arm's expected value, so that run is corroboration and is labelled as
  such, not the gate seen to fail. Attribution per mutation, because they are not interchangeable: the
  delete-the-append mutation reddens **no arm's render assertion at all**. `readActivePattern()` null-guards
  to `0` (`Params.cpp:305-306`), which is pattern A and therefore arm A's own expected value, so arm A
  printed `ok`, and arms B, C and D were caught one level earlier by their fixture pre-check -
  `fixture invalid param=0 want 1 mirror=2` - before a single block was rendered. That short-circuit was
  the gate's shape then, and item 14 read this very sentence as the defect it describes and removed it:
  the pre-check no longer excuses the render, and the same stub on the 190-check tree prints
  `fixture invalid param=0 want 1 mirror=2 saw [3 10 ] expected [9 4 ] mirror holds pattern 2 peak
  0.526246` at `FAIL - 183/190`, so a dead wire is now also a wrong thing heard. Section 8F carries that
  run. What that proved *then* is that the pre-check is the component detecting a missing parameter and the four-arm comparison is not; the
  comparison is qualified by the rotation, which is a mutation the cache cannot absorb. The pre-check earns
  its keep a different way: a parameter read that ever becomes asynchronous reports a broken arm rather than
  a broken wire. Two further findings came out of that work. First,
  `host-synced sequencer produces audio` and `parameter-locked sequencer step is audible` are not
  pattern-independent: they render the factory document's pattern 0 content, so pinning the pattern to 2
  silences them (`[0.0000]`) - they are gates on transport, accidentally coupled to what pattern 0 holds.
  Second, `render`'s `startBlock` defaults to 0, and at 0 the helper fires a manual `triggerPad`; a
  sequencer-only check has to pass the out-of-range 999 like the existing host checks do, or the audio it
  measures is not the sequencer's. A deterministic per-pad hit counter on the block output would still be
  the cleaner observable, and is the piece a future harness wants.
  What the gate does *not* buy: the five arms discharge the cache-to-render wire, and the shipped comment
  at `SmokeTest.cpp:1553-1555` promises section 8C says so in both places, so here is the other half -
  stamps give pad identity, not iteration, so nothing here can see a step that repeated or a step that
  dropped, and the mid-block half of the once-per-block parameter read (`PluginProcessor.cpp:369-370`) is
  still untested. A host that automates `seq.pattern` inside one render quantum can still hand that block
  two different answers, and no check in the suite would notice.
- **Two checks in the suite had names containing a verb their expressions never observed, and both are
  fixed on this tree.** `host-synced playhead advances` (`SmokeTest.cpp:502`) asserted
  `getSequencerPlayheadStep() >= 0` under a playhead that was frozen for the whole fixture, and
  `step sequencer advances the playhead` (`SmokeTest.cpp:367`) asserted the same `>= 0` on the internal
  clock. The observable is
  not worthless - `-1` is what `Sequencer.cpp:970` reports when there is no usable transport, so `>= 0`
  does separate "the host path answered" from "it did not" - but a wire that latched on step 0 prints 0
  forever and reads as a pass, which is precisely what a check called *advances* must not be able to do.
  Both now pin the arithmetic: the host fixture advances the head by the engine's own block step for 16
  blocks and expects step 1 (`floor (0.3413 x 4)`, `Sequencer.cpp:334`), the internal one renders 40 blocks
  and expects step 3 (`Sequencer.cpp:370` reports the step at the *end* of the block), and each fixture
  pins `division = 4` and `swing = 0` so the expectation is arithmetic on the clock rather than on a
  default. Falsified by replacing both `playheadStep` expressions with the constant `0`:
  `FAIL  step sequencer advances the playhead  [0]` and `FAIL  host-synced playhead advances  [0]` at
  167/169, exactly two reds, both these checks, nothing else touched - and both expectations were then
  re-measured green against the restored `Sequencer.cpp` in the same command that printed its hash.
- **Six `SmokeTest.cpp` line cites in this document point somewhere else, and none of them broke today.**
  Two edits landed in `SmokeTest.cpp` on the last pass of Step 26, and their effect on line numbers was
  measured rather than computed - the internal-clock fixture grew by 8 lines and everything below it
  shifted by 8, then the host fixture grew so that everything below *it* shifted by 27 in total
  (`parameter-locked sequencer step is audible` read at `:498` before and `:525` after): the four
  tolerance cites and the wiring-comment cite in 8C were re-pointed by
  reading the target line, not by arithmetic. The other six were already wrong at
  `e0b246d` - each one was checked against `git show HEAD:native/Tests/SmokeTest.cpp`, so this is
  transcription, not drift - and
  `AGENTS.md` means exactly this when it says a cite is a snapshot and not an address, and every number
  below was re-read at the target line rather than offset - the plan has grown by 215 lines and
  `SmokeTest.cpp` by 491 since this bullet was first written (the re-point itself is part of the 215), so
  both namespaces drifted:
  plan `:38` still resolves, and the `720`-sample calls it means are now `SmokeTest.cpp:1755` and `:1757`;
  plan `:138` and `:560` (written `:135` and `:557`) both ask to "tighten `SmokeTest.cpp:948` from
  `< 8.0 s`", and there is no such bound anywhere in the file - CPU is printed and deliberately not
  asserted, with the reason in the comment at `SmokeTest.cpp:2814-2816`; plan `:434` cites `:835` for the
  `0.349`/`0.844` pair, which is at `:2504` and `:2508`; plan `:985` names a check,
  `a Sample Vault track loads into SliceForge`, that is not in the suite (the vault block runs `:2244` to
  `:2345`, and the gate that loads a file this test owns is `a real wav loads through the user-sample path`
  at `:2308`); plan `:1069` cites `:1283-1289` for the formatting-branch gate, which is at `:2707-2728`;
  plan `:1094` cites `:736-750` for the truncated-blob refusal, which is at `:1846-1858`. Correcting the
  six entries is a prose edit in somebody else's historical record and is deliberately not done here; what
  is done is naming where each one actually lives.
  **Item 14 moved all of them again, and this list is re-pointed a second time for it.** `SmokeTest.cpp`
  grew by 96 lines across **two** net-inserting hunks (+47 at `:1561`, +49 at `:1633`, from `b8e4523`'s
  own hunk headers) beside two line-neutral edits, so the offset is uniform only *below the last of
  them* - every address this bullet re-points sits past `:1741`, and anything in the 17-line gap between
  the hunks would have moved by +47. Which is why the number was checked and not derived:
  each of the twelve numbers above was re-read at its target line in `b8e4523` and every one names the
  construct this bullet claims (`:2308` is `a real wav loads through the user-sample path`, `:2504` and
  `:2508` are the `0.349`/`0.844` pair, `:1788` is the `out.triggers[0].pad == 1` read below, `:2183` is the
  `"931 960 1080 "` characterisation). The file is now 2831 lines, and `SmokeTest.cpp` has grown by 587 since
  this bullet was first written.
- **One review finding on this tree was checked and withdrawn.** `song mode follows the pattern chain`
  (`SmokeTest.cpp:1788`) reads `out.triggerCount > 0 && out.triggers[0].pad == 1`, and that was flagged as
  asserting one element where the claim is about the set. It is not: `length = 4` and `ppq = 1.0` put
  absolute step 4 in chain slot 1 (`Sequencer.cpp:46`), `makeFeatureSequence` gives that slot pattern 1,
  and pattern 1 has exactly one lit step - so the first trigger *is* the complete set for this fixture.
  What is left is positional fragility, not a false green: a later edit that lights a second step in that
  pattern keeps the check green while no longer proving which slot ran. LOW, and the real song-mode
  discriminator is the exact-list differential at `SmokeTest.cpp:1388` that Step 26 shipped.
- **Every code-file cite in the five bullets above was re-read at its target line against `69e01ba`, and
  the drift was caused by the two code commits that followed `c57f277`.** The offsets are not one number,
  which is why arithmetic re-points nothing: `5f8815d` added 162 lines to `SmokeTest.cpp` but only 6 of
  them above `:361`, so `:361` moved to `:367` while `:771` moved to `:933`; `16258b3` added 329 more, so
  the vault-era cites ended up 491, 493 and 489 lines below where they were written; `Sequencer.cpp` moved
  +13 above the exporter helper and +47 below it (956 -> 1003 lines); and `PluginProcessor.cpp` moved +52
  in `5f8815d` alone (1196 -> 1255). Two dead cites were inside shipped source and were deliberately left
  where they are: `SmokeTest.cpp:523` cites `Sequencer.cpp:261` for the host-tempo preference (now `:274`),
  and `SmokeTest.cpp:1553`/`:1555` cite `Sequencer.cpp:291` (now `:304`) and `PluginProcessor.cpp:315-316`
  (the call is `:367`, the comment `:369-370`). Those are comment-only edits to files the gates compile, so
  each one costs a rebuild and a re-gate and should ride the next stage that touches a gated source, not a
  doc commit. **That stage came: `b8e4523` repaired both, on a pass that was already rebuilding all four
  targets for item 14, so they cost no extra gate.** `:523` now reads `Sequencer.cpp:274`, and the wiring
  comment reads `Sequencer.cpp:304` and `PluginProcessor.cpp:369-371` (comment plus argument, which is the
  honest span for "read once per block"). Both were line-neutral substitutions, which is why the twelve
  re-pointed addresses above could move by one uniform offset. What this section keeps re-learning is that a cite to a line is worth exactly one commit:
  cite the check name or the function name, and keep a number only where the sentence is about what a
  particular run printed.
- **The cap was then measured on this tree, not only on the one it was proposed on.** The probe recorded
  above ran while the suite was 165 checks, and it *inferred* that the floored form of the cap would keep
  `a small block keeps the original tolerance window` green. That inference is now a measurement, taken on
  `5afa40f` with `native/Source/Sequencer.cpp` at `607ef68f203f88528ddaf28efe804f7f0f8d96e7d597d0043c67c7d163d1e11b`
  (blob hash and worktree hash agree - this file is `i/lf w/lf`) and the suite at 186 checks. Applying the
  unfloored `jmin (blockPpq * 0.5, 0.5 / division)` prints `FAIL - 183/186 checks passed` with three reds:
  that small-block check at `[clears=9 expected=1]`, the `KNOWN DEFECT:` one-step-edit characterisation at
  `[clears=2 expected=1]`, and `sub-block rounding on a 1/32 grid is not a seek` at `[clears=9 expected=1]`.
  Applying the floored `jmin (jmax (0.02, blockPpq * 0.5), 0.5 / division)` prints
  `FAIL - 184/186 checks passed`: the small-block check comes back `ok` and the other two stay red at the
  same printed numbers. So the floor does buy that one case back, and the cap still spends the two settings
  where the committed formula already separates noise from a seek - which is the whole reason it is refused
  rather than tuned. Neither run is a gate and no check was changed: the source was restored inside the
  same run, `restored sha256` equals the pre-mutation hash above, `git diff` on the file is empty, the
  post-restore build exits 0 and the post-restore smoke exits 0 on `PASS - 186/186 checks passed`, and the
  three `launch/` hashes are unchanged because no exporter ran. The earlier
  `native/build/pm/cb3-clamp-probe.log` is superseded by this run - it describes a 165-check suite and
  carries no header, no hashes and no exit codes - and it is left under that name rather than renamed,
  because its name is not what is wrong with it.
- **The `hostResyncs` -> `historyClears` rename was executed in exactly one stage, and the disputed counts
  are three different true things.** In `e0b246d` the old name sits at 7 sites in 3 files
  (`Sequencer.cpp` 1, `Sequencer.h` 1, `SmokeTest.cpp` 5). All 7 are renamed 1:1 inside `c57f277` and
  nowhere else: `git log -S hostResyncs` returns `c57f277` plus `4abe4b0`, the commit that introduced the
  field, while `git log -S historyClears` returns `c57f277` alone. That commit removes 7 lines carrying the
  old name and adds 9 carrying the new one, because the 7 renamed pairs are joined by two net-new mentions
  - the `probabilityRollFor` comment at `native/Source/Sequencer.h:163` and this doc's own prose at `:669`.
  So: 8 live code sites on `5afa40f` (`Sequencer.h:153` and `:163`, `Sequencer.cpp:312`,
  `SmokeTest.cpp:875` `:1013` `:1101` `:1120` `:1123`), and zero old sites in any source or test file, in
  the commit or in the worktree - `git grep -q hostResyncs HEAD -- native` exits 1, and the same against the
  working tree. The rename moved no smoke total: exactly one of the 9 added lines is a `check (` call, and
  it is that check's own expression being renamed, not a check being added.
- **A grep claim this section itself made false, caught on the next read.** The bullet above first said
  "zero old sites in any tracked file", measured against the tree before it was written - and writing it
  down names the old string, so a whole-tree grep for it now exits 0 and hits only prose in these two
  bullets. Nothing was mis-measured; the scope was overstated. The rule that comes out of it is the one the
  census above is now written with: a rename census counts code and test sites, so the grep is scoped to
  `native/`, while a doc that discusses a rename is expected to contain the name it discusses. Prose
  occurrences in this file are the record, not the residue, and their count is deliberately not stated -
  any number written here would be one more thing this file has to re-measure just to stay true.
- **348 and 350 are two views of one surface, and the position under them is not a split.** The bare
  processor asserts 348 (`SmokeTest.cpp:181`) and the loaded VST3 asserts 350 (`HostTest.cpp:110`); the
  difference is exactly the wrapper's `Bypass` and `Program`, and both numbers are current - 347 and 349
  were the same two views before `seq.pattern`. That sentence already exists in `AGENTS.md`, in
  `native/README.md` and in `c57f277`'s own message, so no historical commit needed rewriting. What was
  nearly repaired into a wrong value by reading the two counts as a split is the index: `seq.pattern` sits
  at 347 in *both* views, because the wrapper appends its pair after the layout, so the host's own check
  (`HostTest.cpp:132`, `patternIndex != parameters.size() - 3`) and the bare check's `getLast()` agree on
  the same slot, and the host run prints `index 347` of 350. Note the asymmetry and why it is safe: the
  host derives its expected index from a count, which the bare view deliberately refuses to do for its
  total. Deriving the index is falsifiable, because an unregistered `seq.pattern` leaves `patternIndex`
  at -1 and reddens that same arm; deriving the total is not, because `16 * 21 + 9 + 2 + 1` is the
  layout's own arithmetic and goes green whether or not the parameter was ever added. So `:177` keeps its
  literal, and the comment at `:172-176` now says that in those words. That comment was then rewritten
  once more for a reason worth keeping: the first version of it was six lines where it replaced five, and
  one inserted line above the check shifted every live `SmokeTest.cpp` cite in this section by one -
  about twenty pointers, all correct the morning before. The fix is not to re-point twenty cites; it is
  that an edit inside a file this section cites by line number has to be line-neutral, so the comment is
  now five lines again - the sixth was the fault - and the two commits together are net-zero, so the pointers survived.
- **A re-link, and what it proves about the delivered module.** The rebuild this comment needed stamped a
  new `native/build/…/SliceForge.vst3`, so the build tree and `launch/` print different SHA-256s while
  nothing that compiles into the module changed. They differ in 4 bytes of 24,530,432: the low half of the
  PE `TimeDateStamp` at 0-based 304-305 and a second copy of the same field at 23,107,220-221, which
  decode to 1794822887 against 1794820043 - 10:54:47 versus the 10:07:23 this section's export chain
  records for the shipped module. So the module a host test loads out of the build tree is the module in
  the archive, byte for byte except its link stamp, and any packaging rule about "build != launch" should
  compare the decoded stamp delta before calling a re-link a divergence. `launch/` itself was not touched
  by this stage: all three deliverables re-verified against their recorded hashes.

## 8D. What Step 28 had to correct about its own gate, and what its host test does not prove (2026-11-16)

- **Neither half of the gate as written above is a thing a host can be asked to do.** "`HostTest` requests
  a layout with MIDI output enabled" is not an operation: bus layout is not requested by the caller, it is
  what the binary advertises, and JUCE derives a hosted plugin's `producesMidi()` from the count of event
  *output* buses it found (`juce_VST3PluginFormat.cpp:3508-3509`, reported at `:3186`). So the assertion
  became `HostTest.cpp:94` on the real instance, which reads what Live would route to rather than what this
  source claims about itself. "The count of MIDI notes equals the count of audio triggers" is not reachable
  either: the hosted instance exposes no trigger oracle, so there is nothing on that side of the equation to
  read. The count comparison therefore moved to `SmokeTest.cpp:512-666`, where the fixture lights exactly
  four steps of one pattern and the eight expected events are derived arithmetically from the step grid -
  the count is pinned by what the pattern says, not by an oracle, and the four checks at `:618-641` each
  name the property they compare. The split is the honest one, and it is the reason both programs are needed.
- **`NEEDS_MIDI_OUTPUT` is a configure-time macro, and the console targets define no `JucePlugin_*` at all.**
  The flag reaches the code through `…_artefacts/JuceLibraryCode/Release/Defs.txt`, so flipping it in
  `native/CMakeLists.txt:116` does nothing until CMake is re-configured; `SliceForge_artefacts` reads
  `JucePlugin_ProducesMidiOutput=1` and `SliceForgeHostTest`/`SmokeTest`/`LayoutTest` carry *zero*
  `JucePlugin_` defines, because `juce_add_console_app` has no plugin to describe. That rules out a bare
  `return JucePlugin_ProducesMidiOutput;` in `PluginProcessor.h:43-50` (it would not compile in three of the
  four targets) and it is why the emission at `PluginProcessor.cpp:470` is not gated on the answer: the
  buffer belongs to the caller, the wrapper only drains what it created, and gating on the flag would make
  the behaviour unobservable in exactly the programs that gate it.
- **Five of the seven falsifiers leave the host test green, so do not describe it as gating the wire.** It
  asserts a *clean stream* - equal on/offs, nothing but notes, all of them on channel 10 inside 36..51, all
  inside the block that emitted them, audio still non-silent - and a widened gate (`kPadMidiGateSamples`
  32→48), a one-sample onset shift, a truncated velocity byte and a dropped `midiMessages.clear()` (it feeds
  that buffer empty, so pass-through has nothing to pass) are all invisible to it. The
  phantom-event falsifier (loop bound `i < triggerCount` → `i <= triggerCount`, which emits a default
  `SeqStep` on every block - 36 of the smoke fixture's 40 had nothing scheduled at all, and its first check
  read `44/44 notes`) grew the host count from 9 pairs to 57 and still printed `PASS`: extra notes are clean
  notes. It went red only for the mutations that change what is on the wire rather than how it reads - no
  emission at all (`0 on / 0 off … audio peak 0.892715`) and the channel move (`18 off channel 10 or outside
  36..51`) - plus the pre-change tree, where the bus simply is not there. The exact-list check in the smoke
  suite is the load-bearing gate for pitch, timing, gate length and velocity; `HostTest` gates the
  advertisement and the shape.
- **A `MockHostPlayHead` that reports its default 120 BPM under a ramp built for another tempo makes a
  correct emitter look 224 samples late.** `scheduleHostBlock` prefers the host's tempo over the document's
  (`Sequencer.cpp:261`), so `samplesPerPpq` came out at 24000 rather than 12800 and step 1 landed at 6624
  instead of 6400 on the first green run. The emitter was right - the audio played the same sample - which is
  why the check caught it rather than drowning in it. Any new host-synced fixture has to put its tempo on
  the head; the pre-existing ones do (`SmokeTest.cpp:711`, `:887`, `:961`, `:979`), and the Step 28 block now
  says so at `:512-527`.
- **Two asymmetries are shipped on purpose and are now in `native/README.md`.** The live gate is 32 samples
  while the exporter's is `jmax (12, round (ticksPerStep * 0.45))` ticks (`Sequencer.cpp:885`): one is a
  performance, the other a clip that has to survive a tempo change, and no single number does both. And the
  emission follows the sequencer's own gate - `laneIsAudible`, i.e. the grid's lane M/S (`Sequencer.cpp:60-67`) -
  not the mixer's per-pad `mute` parameter, which is an engine state applied to the voice
  (`Params.cpp:184`, `VoiceEngine.cpp:307`). A muted pad therefore still plays its notes out over MIDI. That
  is defensible (the step really was scheduled) and it is a product decision, not a fact: if the owner wants a
  switch, or wants pad mute to silence the wire, both are one condition in `emitSequencerMidi`.
- **Not verified by any of this, and unverifyable headless:** the Ableton half of Step 28's gate - arm a MIDI
  track on the plugin's output, record, replay through a different rack. And the consequence of the flag
  change is a scan-time one: a host holding an already-instantiated SliceForge will not grow the bus until the
  plugin is reloaded, which is in the commit message per the decision above.

## 8E. Item 13: the host identity assert made observable, and what its own sweep found (recorded 2026-11-16)

Item 13a's premise is right about the mechanism and wrong about the history. The mechanism:
`native/Tests/HostTest.cpp:110` returns 1 on the parameter count at `:114` before the by-id block runs, so on a
build where `seq.pattern` is *deleted* the identity conjunct at `:132` is unreachable, and delete-only bisection
can therefore never show that half of the gate works. The history: the total-preserving reorder had already been
run twice, at 06:37:22 and 07:47:49 (`stageC-s26-mut2-host.log`, `stageC-s26-m2final-host.log`), and `c57f277`
quotes its FAIL line in its own message. What was missing is a capture on the then-current 186-check tree with a green
pair on restored bytes. This section is that capture.

- **The mutation is two lines.** `native/Source/Params.cpp` registers `seq.pattern` at `:199` and the output-stage
  pair at `:192-193`; moving the pair below the selector keeps the total at 348 layout / 350 host and changes only
  which control occupies the tail. `git stash show -p` records it as 2 added / 2 removed and the file stays 364
  lines, so the red window is line-neutral by construction.
- **Red, on a build of `904b289` carrying that reorder** (`native/build/pm/item13a-mut-*`, mutated source
  `39544ca5001a…`). All four of the brief's expectations, each quoted from its own command's output and exit:
  `  FAIL  seq.pattern is a four-choice parameter appended after every existing id  [param=Active Pattern range=0.000..3.000 step=1.000 last=global.out]`
  then `FAIL - 185/186 checks passed`, `smoke exit=1` - the smoke total did not move, one check did;
  `FAIL seq.pattern is not the appended four-step control a host must see: index 345 of 350, steps 4, default 0`,
  `host exit=1`, and **no count FAIL line**, which is the whole point: the assert that fired is
  `patternIndex != parameters.size() - 3` at `HostTest.cpp:132`, printed at `:137`. Index 345 is arithmetic, not
  a coincidence - 9 globals + 21 x 16 pads put the selector at 345 when it sits before the pair, and
  `size() - 3` is 347. ctest attributed it the same way: `33% tests passed, 2 tests failed out of 3` (Smoke,
  Host), `ctest exit=8`.
- **Green, after the restore and a rebuild of all four targets** (`item13a-final-*`, restored source
  `fb4f7b4480…`, equal to `git show HEAD:native/Source/Params.cpp | sha256sum`):
  `PASS - 186/186 checks passed`, `smoke exit=0`; the host prints `seq.pattern: host id 825878785, index 347, 4
  steps, default 0, named Active Pattern` (`:143`), `parameters: 350 = 348 layout + Bypass + Program` (`:148`)
  and `PASS VST3 host test` (`:282`), `host exit=0`; `100% tests passed out of 3`, `ctest exit=0`. So 13e's
  escape branch did not trigger: N = 347 as shipped, JUCE does append `Bypass` and `Program` after the layout,
  and that ordering is now both asserted (`:132`) and *seen to discriminate* - which is exactly what the
  deletion mutation could never give it.
- **What still does not see this defect: `SliceForgeLayoutTest`.** It is the one of the three that passed under
  the mutation. It re-derives its expectations from the same layout, so ordering is invisible to it;
  append-position is gated only by `SmokeTest.cpp:193-197` and `HostTest.cpp:132`.
- **13f, executed as asked, with a side effect nobody had priced.** The delta was parked in
  `git stash push -- native/Source/Params.cpp` rather than a scratch copy. Stash rewrites the file through the
  smudge filter, so an `i/lf w/lf` source came back `i/lf w/crlf`: its *worktree* sha256 moved to `12f9287b…`
  while the blob stayed `f9869c64…`, and `git status --porcelain` reported the tree clean the entire time.
  Getting the shipped bytes back took `sed -i 's/\r$//'` plus `git update-index --really-refresh`. Two rules
  follow: a restore is proved by `git show HEAD:<path> | sha256sum` and an empty `git diff --raw`, never by a
  worktree hash; and for a deliberately-red window, commit-then-revert beats `git stash`, because a stash is
  one mutable stack shared by every lane working in this tree.
- **13b was already discharged; its own file was not.** The 9 + 21 x 16 + 2 + 1 composition and the prescriptive
  "Quote the total as 350 and name the composition" shipped in `c57f277` at `AGENTS.md:143-150` and
  `native/README.md:154-161`; the brief's `:136`/`:137`/`:139` are `e0b246d` addresses, which is why they read as
  unpatched. Sweeping AGENTS.md's numeric cites instead found six dead ones - `PluginProcessor.cpp:23` (live
  `:75`), the four `VRAC_EXTRA` cites `:974`/`:978`/`:1036`/`:1041` (live `:1043`/`:1047`/`:1116`/`:1121`) and
  `HostTest.cpp:69` (live `:110`) - and each was proved live when written: `git show c57f277:native/Tests/HostTest.cpp`
  puts the `!= 350` assert on line 69, and `5f8815d` moved it to 110 by adding 41 lines above it. Drift, not
  transcription. All six were corrected line-neutrally - 183 lines before and after, 3 changed - and landed in
  `e6e2c44`, a concurrent docs commit whose own message re-verifies every one of those targets independently. The
  durable form is a name anchor ("the `!= 350` count assert"); `AGENTS.md:139` is what tells the reader to check.
- **13c and 13d, from these logs only.** Unique passing check names: 148 in `stageB-precommit-smoke.log`, 169 in
  `stageC-s26-commit-smoke.log`, 186 in `item13a-final-smoke.log`, and no name repeats inside any of the three.
  148 -> 169 is 27 added and 6 removed names; 169 -> 186 is 17 added and 0 removed. `c57f277` published "21
  checks added, 0 removed, 1 renamed", which reconciles as arithmetic (27 - 6 = 21) but undercounts the renames
  by five: the six names that disappeared are `the parameter surface is the documented 347 controls` plus four
  `… resyncs …` checks and `a small block keeps the original resync window`, all renamed in place into the
  `clears`/`tolerance` vocabulary. The true sentence is 21 new checks, 6 renamed in place, 0 deleted. Forward
  correction only - `c57f277` is shipped and amending it now rewrites three commits, which stays the owner's call.
- **One tuple re-measured rather than inherited - and re-measured again by 8H, which supersedes it.**
  `launch/` and the installed module were then byte-identical (`1a0747b8e6ca…`, 24,530,432 B, link stamp
  1794820043 = 10:07:23), and a build of that HEAD differed from them in four bytes: `cmp -l` names 305-306
  and 23107221-23107222, the two PE `TimeDateStamp` copies. The 8C three-way divergence was closed; the zip
  was then 5 members with 0 backslash separators. The green host test still
  loads the build copy, so it describes these bytes only through that comparison, not through the path it opens.
- **Not verified here, and untouched by it:** no install, no re-export, no push, and no source change
  survived this section; 13g's `0.08` literal is untouched, and `904b289..HEAD` is `AGENTS.md` alone.

## 8F. Item 14: the stub was ordered as a coverage probe and the tree reddened (recorded 2026-11-16)

The dispatch said the pattern parameter path has no test at all, predicted that stubbing
`readActivePattern` to `return 0` would leave the smoke suite green, and called that expected green the
finding. It was run on `1bfe8e8` exactly as ordered and it does not stay green, so the finding is not the
one the brief predicted - and the reason it is not is more useful than the prediction was.

- **183/190, seven reds, smoke exit 1.** `native/Source/Params.cpp:307` became `return 0;`, all four
  targets were rebuilt (`Params.cpp` is compiled three times over - SliceForge, SmokeTest and LayoutTest
  each own a copy of the TU, and the mutated object postdates the source in all three), and the suite
  printed `FAIL - 183/190 checks passed`. Four reds are value reads of the accessor itself -
  `selecting a pattern moves the parameter and tells the host [value=0 landed=yes notifications=1
  notified=2 expected=2 at 1 notification of 2]`, `a session saved with the parameter on pattern C comes
  back on pattern C [param=0 doc=0]`, `a session written before seq.pattern existed keeps its stored
  pattern [param=0 doc=3]`, `when the parameter and the blob disagree, the parameter plays and the mirror
  follows [param=0 doc=0]` - and three are the wiring gate's arms B, C and D. Arm A printed `ok`, because
  0 is its own expected value, which is the by-construction agreement 8C already records. So 14a's "zero
  coverage" was a stale snapshot of a 165-check tree, and the sixteen call sites that exist now refute
  it - twelve reads through `getPatternParameter`, four writes through `setPatternParameter`, counted on
  `b8e4523`.
- **Three of the four checks 14a asked for were already shipped, and are reported rather than copied.**
  (i) mirror 0 with parameter 3 *is* ladder arm D (`:1607`, mirror table at `:1609-1612`). (iii) mirror 3
  with the sentinel is the class of `with no parameter in sight the document still decides` at `:1398-1404`;
  the shipped case uses mirror 2, and mirror 2 versus 3 is not a new equivalence class for a branch whose
  whole answer is `data.activePattern`. Driving a sentinel through the processor at all is impossible: the
  caller clamps before it passes (`Params.cpp:307`), so the sentinel only ever reaches the callee from
  non-processor callers - which is why (iii) lives in the engine block. 14b's round trip is at
  `:1453-1467` and asserts *both* conjuncts the ruling asked for (`restored.getPatternParameter() == 2`
  and `restoredDoc->activePattern == 2`), so "if that reddens it is a real bug" never had to be tested: it
  is green and it is a gate. 14d is `Params.h:25`/`:31` with no magic 3 anywhere, and `Params.cpp:294-308`
  carries the `return 0` with the reason - `SequencerPanel.cpp:481` reads it unclamped and `:509` indexes
  `patterns[]` with it, so the sentinel would be an out-of-bounds read of a four-element array. 14c's two
  comments were already honest at `PluginProcessor.cpp:1094` and `SequencerPanel.cpp:367-374`.
- **The hole was real, and it was under the pre-check, not under the suite.** Seven reds, and *zero* of
  them at the render level: the ladder short-circuited on its fixture pre-check, so arms B, C and D failed
  with `fixture invalid param=0 want 1 mirror=2` before a block was rendered and the pad comparison never
  ran. 8C had written that behaviour up as a finding about which component detects what; item 14 read the
  same sentence as a defect and removed the short-circuit. The stub now prints
  `[fixture invalid param=0 want 1 mirror=2 saw [3 10 ] expected [9 4 ] mirror holds pattern 2 peak
  0.526246]` - the same seven checks, but four of them carry audible evidence, `peak` proves the engine
  rendered rather than stalled, and relaxing the pre-check can no longer un-gate the wire.
  `saw [` / `expected [` kept their exact spelling so the four `saw [` strings that live in 8C still
  match this file. 8E holds none of them, and that is said here rather than left to be found.
- **The fifth arm, and what it is actually worth.** The four ladder arms share one invariant - the mirror
  is `(parameter + 1) mod 4` - so an implementation keyed on that relation instead of on the value is
  invisible to all four. The witness is `data.activePattern == (live + 1) % kSeqPatterns ? live :
  data.activePattern`, the shape of "the mirror looks stale, trust the document instead". Built into
  `Sequencer.cpp:39-42`: **189/190, one red, and it is the new arm** - `saw [6 7 ] expected [3 10 ] mirror
  holds pattern 3`. Stated plainly, because the honest number is smaller than the enthusiasm: no ordinary
  mutation of either branch needs this arm (`min`-style confusions die at arm D, `max`-style and
  `live == 0 ? doc : live` die at arm A), the witness is constructible rather than plausible production
  code, and what the arm buys is pair coverage against a whole family of relation-keyed implementations
  for one line of a table.
- **Song mode had never travelled through the processor.** `songMode` is written in six places in the
  suite, three of them `true`, and none of those three reaches `setSequencerData`: two hand the document
  straight to `processSequencerBlock` and the third to `makePatternMidiFile`. So the flag's trip from
  the processor's ingest into `processBlock` was ungated, and so was the invariance 14c's ruling asked
  for.
  Three arms now (`:1693-1738`), each filling all eight chain slots with one pattern so that no arm
  depends on which slot the transport reached: (chain 3, parameter 0), (chain 3, parameter 3) and (chain 1,
  parameter 3). The first two are the ruling's "changing the parameter does not change the output" - same
  fixture, different parameter, identical expected pad pair; the third is the control that makes it a
  claim rather than a tautology, because an invariance asserted against one chain value is also satisfied
  by a chain that is ignored. Arm 1 is green by construction under "the parameter wins" (its parameter is
  its chain), which is precisely why the invariance is stated against a chain on D where arm 0 refuses a
  parameter of 0.
- **Two falsifiers, and the second one is the proof that this was new coverage.** Parameter-overrides-chain
  in `Sequencer.cpp:49-50`: 186/190, four reds - the two new discriminating arms *and* both pre-existing
  engine checks, arm 1 green as documented. Then `songMode` dropped on the floor at `setSequencerData`
  ingest: **188/190, and the only two reds are the new arms** - `song mode lets the chain pick the pattern
  and the parameter stay silent` and `song mode follows the pattern chain` both stayed green, because they
  hand the document straight to the engine and never touch the route that lost the flag. That pair of
  results is the whole argument: the first says the new arms agree with the old ones, the second says only
  the new arms can see the processor.
- **Gate, on the shipped bytes.** `b8e4523` added four checks, removed none, renamed none; the four
  existing ladder names are byte-identical, and the green log holds 190 smoke `ok` lines (218 unique
  names across the log, layout's 28 included). Build exit 0, `PASS - 190/190 checks passed` smoke exit 0,
  host `350 = 348 layout + Bypass + Program` and `PASS VST3 host test` exit 0, layout 28 checks exit 0,
  ctest `100% tests passed out of 3` exit 0. The `.obj` census closes at seconds: SmokeTest.obj 12:02:35 >
  cpp 12:01:54, Params.obj 11:51:51 > 11:51:01, Sequencer.obj 11:55:18 > 11:54:25, PluginProcessor.obj
  11:56:41 > 11:55:46, exe linked 12:02:36, VST3 11:56:31 > the last PluginProcessor.cpp write. CPU on
  this run: 6.91% of one core, one measurement, machine load unmeasured - 8C's warning about treating a
  wall-clock number as a comparison applies, and the four added checks add fixtures, not engine work.
- **Two errors this pass published, and their corrections.** (1) `b8e4523`'s message cites the sentinel
  check at `SmokeTest.cpp:1490-1496` and 14b's round trip at `:1549-1553`. Both are wrong: the true
  addresses are `:1398-1404` and `:1453-1467`, and the cause is that an offset was applied to two
  addresses sitting above the first inserting hunk, which therefore never moved. The tell is that the
  two wrong numbers are not even the same offset - `:1490` is `:1398` +92 and `:1549` is `:1453` +96 -
  so they were reconstructed, not re-read, which is what 8C's re-point bullet exists to prevent. The
  blob hash the same message publishes, `37a8fbfe5454…` for `native/Tests/SmokeTest.cpp`, was verified
  against `git show b8e4523:native/Tests/SmokeTest.cpp` and is correct. (2) Two lines in
  `item14-F3-songparam.log` labelled "restore proof" were printed after that run's mutation had already
  been applied, so they show a mutated hash; the true F1/F2/F3 restore proofs are in the F4 log header -
  Params.cpp `fb4f7b44…` and blob `f9869c64…`, Sequencer.cpp `607ef68f…` and blob `370e3eab…`, both equal
  to HEAD. Neither log is tracked (`.gitignore:10` = `native/build/`), which is why both corrections are
  written here and in the next commit message rather than left in an evidence directory.
- **Restore method, corrected against the dispatch.** The brief said `git stash pop`. Not used, and it
  would have produced a false clean: under global `core.autocrlf=true` with no `.gitattributes`, a
  git-mediated worktree write smudges an `i/lf w/lf` file to CRLF, which moves the worktree hash while
  `git status --porcelain` still reads clean - measured on this lane during item 13, and the warning
  prints on the very `git diff` used to check it. Every mutation this pass was applied and reverted with
  a byte-exact editor and proved by blob equality instead. `PluginProcessor.cpp` is the one dirty-by-default
  file in the tree (`i/lf w/crlf`), so its worktree hash `e8a4c882…` is *not* reproducible from the commit;
  the publishable value is the blob's `a73bf191…`, and both are recorded side by side in the log rather
  than one of them quoted alone.
- **The README paragraph grew the file by two lines, and that is where the drift lands.**
  `native/README.md` is 495 lines against 493 at `1bfe8e8`, so a cite to a README line below `:218` is
  +2 on this tree. Two of the plan's README cites were already dead before this commit, and the growth
  did not break them: `:250` cites `native/README.md:394` for "32 fixed voices; no audio-thread
  allocation or locking", which is `:439` at `1bfe8e8` and `:441` then, and `:537` cites `:422` for the
  JUCE licence sentence, which is `:488` and `:490`. Named here, not rewritten there, on 8C's precedent
  that re-pointing a historical record is a prose edit while saying where a string lives is a fact. 8H
  grew the file again (`:446` and `:495` now), re-pointed `:250` as a location fact, and deleted `:537`'s.
- **Not verified, and left open.** Mid-block automation of `seq.pattern` is still untested - one read per
  block is the design and nothing observes the inside of a block. A chain that varies *within* one bar is
  still gated only engine-side; the new arms deliberately remove slot arithmetic as a variable. Nothing
  installed, nothing re-exported to `launch/`, nothing pushed: the module in `Common Files\VST3` is still
  the owner's copy and the zip is still the 04:56 build, both behind this HEAD. 14e's item-13 list was
  delivered last pass (`e6e2c44`, `780ec07`, `1bfe8e8`) and the `0.08` literal is untouched, as ruled.

## 8G. Item 15: the notify-drop mutation reddened seven checks, and 15c's open list had already shipped

- **What 15 asked, and what the landed commit already was.** 15a(2), 15a(3) and 15b describe a working-tree
  snapshot, not a shipped one. The two locals they are built on - `notified` and the singular `firedPad` -
  have never existed in any commit on this branch: `git log --all -S "bool notified"` and
  `git log --all -S "firedPad = "` over `native/Tests/SmokeTest.cpp` both return nothing. What `c57f277`,
  the very commit under review, actually landed already carried `valueLanded`, the `NotifySpy` and the
  set-based song-mode assert, and all three are still there: `valueLanded` at
  `native/Tests/SmokeTest.cpp:1442-1443`, the four-conjunct host check at `:1444-1451`, the listener at
  `:1424-1440` (`spy.calls == 1` and `roundToInt (spy.last) == 2`), the comment naming which conjunct
  carries the host claim at `:1416-1423`, and the song-mode set at `:1361-1392` - `heard` plus `fires` over
  all 60 blocks, `list == "1 "` at `:1388`, the chain-slot justification at `:1379-1383`: all eight slots
  hold pattern 1, and pattern p lights row p. The five `firedPads` hits are the shared lambda at `:1299` and
  its four uses. Nothing was owed here, and the brief's `:1041` and `:1069-1072` cannot be re-pointed,
  because they were never committed bytes: at `e0b246d` those addresses sit in the Vault block, at
  `c57f277` in a grid-drawing lambda and the torn-lane counter, and from `5f8815d` onward in the
  probability/flam/ratchet setup. Unrelated code in every blob, which is what reading a working tree at
  06:21 costs.
- **The mutation, applied and reverted byte-exactly (`item15-mut1-*`).** One line, line-neutral, in
  `setParameterValue` at `native/Source/PluginProcessor.cpp:252` - the brief's `:195-201` was exact for
  the committed file at 06:21 (`git show e0b246d:native/Source/PluginProcessor.cpp` puts the function
  there and the call at `:199`), and it has since moved. Before:
  `parameter->setValueNotifyingHost (range.convertTo0to1 (value));`. After:
  `parameter->setValue (range.convertTo0to1 (value));`. Pre-mutation worktree SHA-256 `e8a4c882…`,
  blob id (SHA-1, `git hash-object`) `584537fc…`, commit-blob SHA-256 `a73bf191…`; mutated worktree
  `43d3d200…`; after the restore the worktree hash is `e8a4c882…` again, `cmp` against the copy taken
  before the edit is clean, `git diff` prints nothing and `git status --porcelain` is empty. No stash
  and no checkout were used, per 13f: this is the tree's only `i/lf w/crlf` source, so a smudge-filter
  write would have moved the worktree hash while `git status` still read clean. Red window 12:37:40
  to 12:39:22.
- **Red, quoted verbatim from `item15-mut1-verify.log`.** Build `BUILD_EXIT=0`, census closes the TU:
  `PluginProcessor.cpp` 12:37:40 < `SliceForgeSmokeTest.dir/Release/PluginProcessor.obj` 12:38:09 < the smoke
  exe 12:38:10, VST3 12:38:00. Smoke: `FAIL - 183/190 checks passed`, `SMOKE_EXIT=1`, CPU 4.65%. The seven
  failing lines, with their printed values:

      FAIL  selecting a pattern moves the parameter and tells the host  [value=0 landed=yes notifications=0 notified=-1000 expected=2 at 1 notification of 2]
      FAIL  a session saved with the parameter on pattern C comes back on pattern C  [param=0 doc=0]
      FAIL  a session written before seq.pattern existed keeps its stored pattern  [param=0 doc=3]
      FAIL  when the parameter and the blob disagree, the parameter plays and the mirror follows  [param=0 doc=0]
      FAIL  the host path plays the pattern the parameter names (pattern B)  [fixture invalid param=0 want 1 mirror=2 saw [3 10 ] expected [9 4 ] mirror holds pattern 2 peak 0.526246]
      FAIL  the host path plays the pattern the parameter names (pattern C)  [fixture invalid param=0 want 2 mirror=3 saw [3 10 ] expected [1 12 ] mirror holds pattern 3 peak 0.526246]
      FAIL  the host path plays the pattern the parameter names (pattern D)  [fixture invalid param=0 want 3 mirror=0 saw [3 10 ] expected [6 7 ] mirror holds pattern 0 peak 0.526246]

  Host `PASS VST3 host test` `HOST_EXIT=0` with `index 347` and `parameters: 350 = 348 layout + Bypass +
  Program` unmoved, layout `LAYOUT_EXIT=0`, ctest `67% tests passed, 1 tests failed out of 3` `CTEST_EXIT=8`
  naming only `1 - SliceForgeSmoke (Failed)`.
- **The library derivation held, the count of reds did not.** `readActivePattern` really does load the atomic
  JUCE writes only inside `parameterValueChanged`, so `live.getPatternParameter()` came back 0 after a
  `setPatternParameter (2)` that stored 2 into the parameter object - which is also the literal proof for
  15a(2): the FAIL line prints `landed=yes` on a run that reddens, so the value conjunct survives a notify
  drop and never could have carried the word "notified". The prediction was three checks; seven reddened,
  and one of the three predicted ("the two restore checks") is not a restore at all - `a session written
  before seq.pattern existed keeps its stored pattern` is the migration case. The two beyond that are the
  disagreement check and the three-arm host-path differential, and the differential saw it through its
  `fixture invalid` pre-check, not through a render expectation: arms B/C/D each refuse to run because the
  live index never left 0, while arm A and `pattern A against a document on D` stay green by construction
  because 0 is exactly what arm A expects. That is the sentinel degeneracy 8C and 8F keep running into.
- **The optional listener of 15a(3) is not redundant, and the run shows it.** The same FAIL line prints
  `notifications=0 notified=-1000`, so the `NotifySpy` dies with the atomic rather than duplicating its
  evidence. It was already in `b8e4523`; nothing had to be added, and the branch in the dispatch ("if nothing
  reddens, add the listener and say so") did not fire.
- **Three checks stay green under this mutation and should.** `song mode lets the chain pick the pattern and
  the parameter stay silent`, `with no parameter in sight the document still decides` and `a pattern above the
  bank is clamped, not walked off` all call `processSequencerBlock` with an explicit live index and never
  touch APVTS, so they gate engine precedence rather than the notify path. No coverage was lost; the notify
  path is guarded by seven checks and the song-mode arms guard a different invariant.
- **13a and 13b, which 15c still listed as open, shipped last pass.** 13a is in `780ec07`: the
  total-preserving reorder that moves the output-stage pair below `seq.pattern`, reddening smoke on
  `last=global.out` at `185/186` and the host on `index 345 of 350` with the count unchanged, the assert that
  fired being `patternIndex != parameters.size() - 3` at `native/Tests/HostTest.cpp:132` and its print at
  `:137`, then green at 186/186 with `N = 347`. 13b is in `c57f277` plus `e6e2c44`: `AGENTS.md:143` cites
  `native/Tests/HostTest.cpp:110` asserting exactly 350 and `:144` gives the 9 + 21 x 16 + 2 + 1 composition,
  so the brief's "AGENTS.md 136-139 still instructs 349" is false of the file as shipped.
- **The other two 15c premises were stale too.** "There is no green smoke or host log yet" was written
  against the failed 06:11 build: `item13a-final-smoke.log` prints `PASS - 186/186 checks passed` at 11:17
  and `item14-final-gate2.log` prints `PASS - 190/190 checks passed` at 12:03, with this pass's
  `item15-final-gate.log` the third. "11 files, all code, zero docs" described a tree caught mid-mutation;
  this pass started from a clean `eca3a79`. All of those logs are gitignored (`.gitignore:10`), which is why
  the numbers are repeated here instead of left as pointers.
- **Green pair on the restored tree (`item15-final-gate.log`).** `BUILD_EXIT=0` with `PluginProcessor.cpp`
  compiled in all three of the module, smoke and layout targets (log lines 33, 45, 53). Census at seconds:
  `PluginProcessor.cpp` 12:39:22 < `SliceForgeSmokeTest.dir/Release/PluginProcessor.obj` 12:40:02 < exe
  12:40:03; VST3 12:39:48; `SmokeTest.obj` 12:02:35 > `SmokeTest.cpp` 12:01:54 and `HostTest.obj` 08:46:10 >
  `HostTest.cpp` 08:45:03 and `LayoutTest.obj` 09:32:58 > `LayoutTest.cpp` 00:29:24, those three TUs
  correctly not recompiled because their sources did not move this pass. Then, quoted from this log:

      PASS - 190/190 checks passed                          SMOKE_EXIT=0   CPU: 4.51% of one core
      seq.pattern: host id 825878785, index 347, 4 steps, default 0, named Active Pattern
      parameters: 350 = 348 layout + Bypass + Program       PASS VST3 host test   HOST_EXIT=0
      LAYOUT_EXIT=0     100% tests passed out of 3          CTEST_EXIT=0

  The total is 190, so `native/README.md:218` needed no restamp; what did move is the CPU sentence at
  `:221-223`, which claimed one run behind the 190 and now describes two, 6.91% and 4.51%, both inside the
  advertised 4.4-6.9 band. The red leg's 4.65% is not in that list because it printed 183/190.
- **Delta accounting for this pass.** 190 to 190: 0 checks added, 0 removed, 0 renamed. Item 15 changed no
  source and no test; the only files in this commit are `docs/next-steps-plan.md` and `native/README.md`.
  Inserting this section shifts every plan line below its own start, which breaks nothing: a grep for
  `next-steps-plan.md:` followed by a number, over `AGENTS.md`, `native/README.md`, `docs/` and both the
  source and test trees, returns zero cites to plan line numbers.
- **Not verified, and not claimed.** Two other sites carry the same call and were not mutated:
  `setParameterNormalised` at `PluginProcessor.cpp:259`, which is the MIDI-CC route, and the local `set`
  lambdas inside `ParameterCache::writePad`/`writeGlobal` at `Params.cpp:317` and `:350`. So "the notify path
  is guarded" is proven for the pattern route only; the CC and batch-write routes have no per-site
  falsifier, and adding one is a Step-26-sized job, not a doc fix. Nothing ran in a real DAW; the host test
  is JUCE's own wrapper. Nothing installed, nothing re-exported to `launch/`, nothing pushed, and the `0.08`
  literal is untouched per 13g.

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
  Step 29 finding is that competitor's bug reproduced *here*, in the same place, and Step 24 was the
  fix (built, 8O); and the one combination nothing in the field or the family does is **chop → sequence → hand the
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
  where the no-slice-end defect was obvious. Step 24 has landed and the content fix has not, so the
  demo still shows the wrong thing.

Built 2026-11-16 as a boundary rule, not as code: the owner resolved it (SliceForge owns one
continuous recording becoming a playable 16-pad kit; redrum owns a folder of already-made kit pieces;
the discriminator is the material type at the door), and the rule now lives in `AGENTS.md:151-156` under
"Which repo owns which material". Nothing was built for it, which is the point - the step was a decision.

---

## 11. Order of work

Round 1's sequence is superseded. It put the slicer rewrite (Step 7) and an offline bounce (Step 8)
ahead of any measurement, gated the bounce on a property it cannot have, and led into a parity harness
whose answer is a design decision rather than a fixture. Round 2's sequence, with each link justified:

**1. Close out Phase 0 first — done, 1.5 d.** Step 29 (0.25) landed first, because everything below is
judged by `ctest`; Steps 23 (0.25) and 22 (0.5) then landed together in `4abe4b0` on 2026-11-16, in the
order that forced the tolerance before the pass counter. Step 5 (0.5) closed as *evidence* rather than
code: its three sub-deliverables were one already built (`HostTest.cpp:110`'s exact 350-parameter
literal), one measured to not exist (the "stale" `AGENTS.md` references), and one refuted by JUCE itself.

**2. Make the host able to see the instrument — 4 d.** Step 26 (1.5), Step 28 (2), Step 27 (0.5) in that
order: the pattern parameter first, because MIDI out is useless if every clip plays pattern 0; then
MIDI out; then making the offline export honest, which reuses the same swing and roll code and so
should land after Step 22 rather than before it. This block is why the sequencer stops being a feature
*inside* the plugin and becomes one in the session, and it is cheaper than anything in Phase 5.
All three landed in that order on 2026-11-16, so the block is closed; what is left of it is acceptance
in a real host, which no check in this repo can do.

**3. Make the kit-from-one-file work — 2.5 d, after Step 21.** Step 24 is built (8O, 1.5 d banked of
its 2); it was the highest-value step in the document on pure listening grounds, and it is a length
computation, not an algorithm — which is why it moved ahead of Step 7. What is left in this item is
Step 25 (1 d): the fixture that makes any detector work decidable. It sits behind the boundary
decision because `redrum` is already in this lane, and starting it before that is answered builds it twice.

**4. Then, and only then, the heavier things.** Step 6 Rate (3 d) → Step 7 detector (4-6 d, now
measured) → Step 10 topology (2 d) → Step 8 bounce (3-4 d, gate corrected) → Step 9 contract (0.5 d) +
Step 11 Vault (1-2 d) → Step 16 licence (1 d) → Steps 12-15 (10 d) → Steps 17-19 (3.5-5.5 d).
Step 16 is one day and is the only item here that blocks *conveying* a build to anyone, so it moves
forward the moment a public release is actually on the table — it does not need to wait behind Phase 5.

Steps 1-19 and 22-29 add up to **41.75 dev-days** at midpoint (the ranges in Steps 7, 8, 11, 18, 19, 24
and 26 widen that to ~38-45 d), so "roughly 40" is the honest headline. **10.75 d are banked** — Steps 1-4
(2.75), Step 29 (0.25), Step 16 (1), Step 5 (0.5), Steps 22 (0.5) and 23 (0.25), Steps 26/27/28 at 3.75
(`c57f277`, `16258b3`, `5f8815d`), Step 17's determinism half (0.25) and Step 24 (1.5, recorded in 8O) — so
**~31 d remain**, plus Step 20 (macOS 3-5 d), Step 21 (a decision, not code) and the Ableton listening
pass, which banks nothing by itself. Round 1's "~31 d", then "8.0 d / ~33.75 d", then "9.25 d / ~32.5 d"
are the three stale pairs this paragraph has printed: true the day they were written, a landed step behind
by the time the next pass read them. What is left that changes what you can claim in Ableton is Step 25 (1) - and from
2026-11-21 that is only Step 25's phase 3: the scorer exists and is falsified (8Q), the renders it has to score do not yet.

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
- **Corrected forward, 2026-11-16 — two committed messages published a hash their own commit cannot
  reproduce, and this bullet existed before both of them.** `c57f277` (Step 26) and `5f8815d` (Step 28)
  each list `native/Source/PluginProcessor.cpp` under "restored bytes" using the **working-tree**
  SHA-256 — `704272544c…` and `e8a4c882…` — while the blob each of those commits actually stores is:

  | commit | published (worktree, unreproducible) | `git show <commit>:native/Source/PluginProcessor.cpp \| sha256sum` |
 |---|---|---|
  | `c57f277` | `704272544c8cbea9…` | `f9ecce1d300c2318a0a18fc68fa6dfaeae8242515ee7a1b4042f4cf017d4f0c1` |
  | `5f8815d` | `e8a4c88277bbfd8e…` | `a73bf191a6a9d15bafb7ef0d2a99f6ce26ecec3bf096b5c594c4139e5b36ba60` |

  The cause is the one measured above: `git ls-files --eol` reports `i/lf w/crlf` for this file, the only
  source file in either set that is CRLF in the working tree, so `git add` normalized it on the way in
  and the raw working-tree hash can never be recomputed from the commit. Every other hash in both
  messages *does* reproduce (`fb4f7b44…`/`6c69b8d8…`/`d8f171a6…`/`63f08cd4…` for `c57f277`;
  `93eb69d7…`/`fb875e6c…`/`f9375308…`/`cf7a74ac…`/`61cffc9b…` for `5f8815d`), which is why the mistake is
  easy to miss: four and five of six are right, so the list reads as if it were all right.
  **The rule for any future record line is `git show <sha>:<path> | sha256sum`, never `sha256sum <path>`,**
  and if a worktree hash is quoted it must be labelled as the worktree hash.
  This is a forward correction, not an amend: when the owner authorized the correction `c57f277` was
  `HEAD~1`, and `5f8815d` then made it `HEAD~2`, so rewriting it would also rewrite `82d8866` and
  `5f8815d` and orphan two citations that name `82d8866` as their red-test baseline (the `5f8815d`
  message's own "Production at `82d8866`" line, and §8D above). Nothing is pushed, so both routes were
  open; the cost of the rewrite is every reference this day wrote down, and the defect is a *number in a
  message*, which the corrected pair above answers. The CR-stripped `diff` remains the restore witness
  for this one file.

Market claims about VRAC+ (release July 2026, €39 early bird rising to €149, VST/VST3/AU/AAX on
macOS and Windows, the "same slice for every pad" complaint, per-pad multi-slicing added September
2026) come from public reporting and a KVR thread, not from the repo, and round 2 did not re-verify
them:
[Synthanatomy](https://synthanatomy.com/2026/07/beatsurfing-vrac-the-synth-that-turns-any-sample-into-a-drum-kit-now-in-fully-featured.html),
[KVR forum thread](https://www.kvraudio.com/forum/viewtopic.php?t=631449),
[Dubspot review](https://blog.dubspot.com/beatsurfing-vrac-plus-2026),
[Audio Plugin Guy on free VRAC](https://www.audiopluginguy.com/free-plugin-review-vrac-by-beatsurfing/).
§8K answers both halves on 2026-11-17 and refutes the ask's premise while doing it: the collision is real
but sits outside this category, and Portico, Influx and Ripchord are not this product's competitors at all.

---

## 8H. Step 16 landed, and the package can now install itself (recorded 2026-11-17)

Appended at the end rather than inserted after 8G, on the AGENTS.md precedent: a section placed at
`:1716` would shift every cite below it, and this file cites its own line numbers.

- **The defect was in the archive, not in the engine.** `INSTALL.txt` told the reader to run
  `launch\start.ps1`, and the zip contained five members with no script in any of them. The script that did
  exist could not have run from an extracted folder anyway: it derives `$RepoRoot` from its own parent and
  step 3 then calls `npm run dev`. So the sentence "copy the bundle in, or let the script do it" was true of
  a source checkout and false of the thing a customer receives - which is the whole of what "working compile
  install file" means. Both halves are fixed: `native/package-launch.ps1` now stages `LICENSE`,
  `THIRD-PARTY-NOTICES.txt`, `launch/start.ps1` and `launch/start.bat` as members and puts all four into the
  same per-member hash gate as the binaries, and `launch/start.ps1` detects a package with
  `$IsRepoCheckout = (Test-Path <root>/src) -and (Test-Path <root>/node_modules)` and reports the browser
  edition as *not part of this package* instead of starting Vite.
- **`native/Package/THIRD-PARTY-NOTICES.txt` is new: 15,448 B, 281 lines, eight sections plus the verbatim
  OFL 1.1.** The present/absent tables are an object-file census of *this* build tree, keyed on one
  distinctive long symbol per component (short substrings - `AU`, `b2`, `gl` - hit 16 to 40 objects and say
  nothing): FLAC and Ogg in `juce_audio_formats.obj`, jpeg/png/HarfBuzz/SheenBidi in `juce_graphics*.obj`,
  zlib in `juce_core.obj` and `juce_graphics.obj`, Steinberg VST3 in `juce_audio_plugin_client_VST3.obj`;
  AudioUnitSDK, AAX, LV2, VST2, Oboe, GLEW, Box2D and CHOC/QuickJS named in no object of
  `SliceForge.dir`, `SliceForge_VST3.dir` or `SliceForge_Standalone.dir`. The deliverable-side HarfBuzz
  claim was first written as "the exported name `hb_shape`" and the measurement refutes it: the module holds
  17 `hb_shape` hits, every one of them the prefix of a longer MSVC-rendered name
  (`hb_shape_plan_create2`, `_cached2`, `_execute_internal`, `hb_shape_plan_t`), and
  `hb_shape(?![0-9A-Za-z_])` occurs zero times - so what the bytes prove is statically linked HarfBuzz code,
  not a HarfBuzz export surface, and the notices file now says exactly that. The licence
  grant question resolved the other way from how it looked: AGPL-3.0 is the *correct* leg for JUCE 8.0.4 and
  for the Steinberg headers, not a conflict, so no escalation was owed. The OFL block - the 4,068 B from the
  `SIL OPEN FONT LICENSE Version 1.1` line onward - is byte-identical to the copy at
  `D:/Projects/skeer-audio/controller/assets/fonts/ibm-plex-sans/LICENSE.txt`, both sides
  `6863ff4dc820f24756b6acfb38a2c6bc19d5b8d71b17030330a6f16eb4cee348`; that file's own first 361 bytes are IBM's
  copyright preamble, which section 5 carries per face instead, so the whole files are *not* equal and the
  notices claim is scoped to the block. Each of the six faces carries its
  own copyright record read out of its `name` table, not out of a filename.
- **Tuple, measured on the exported bytes and re-measured after a second full build** (the relink moved no
  content hashes, which is why both readings agree): module
  `ce66de924ffe2366529e2cff1d3dc2d702a633ba4ea4790f87927082f97d2f94` (24,530,432 B) identical in
  `native/build/…/VST3`, in `launch/`, as the zip member, and in
  `C:\Program Files\Common Files\VST3`; standalone `a2b6c9013ba33c13c8ab3605d0458d5e699e7ba7731f8323a0f64c08ba746760`
  (24,220,160 B); `moduleinfo.json` `df39eafcbd1a21ff5e4e9fa1f32b3d97c514d93a34ec6c071c393049119572da`.
  The 10:07 `1a0747b8e6ca…` module that 8E recorded is superseded, and with it 8E's "differs in exactly four
  bytes": `launch/`, the zip and the installed copy are now one artifact, not two that a `cmp -l` reconciles; 8I supersedes this tuple.
- **Text members, as worktree hashes** (`INSTALL.txt` `db6a85d58fa4a738…`, notices `3acf5e7d39f8b01f…`,
  `README.md` `14c83acb121f4d23…`, `LICENSE` `0d96a4ff68ad6d4b…`, `start.ps1` `7938fe08558d170f…`,
  `start.bat` `0366b728d29b4465…`). These are *worktree* hashes and they are the right unit here, because
  they are what the zip gate compares: `unzip`-side bytes, not checkout-side. A fresh checkout on this
  machine reproduces them only for `start.bat` (`i/lf w/crlf`, 2 CRLF pairs, 88 B); the other five are LF
  here with `core.autocrlf=true` and no `.gitattributes`, so a checkout that smudges them will hash
  differently - the hazard AGENTS.md now names, and the one Step 17's `.gitattributes`/LFS decision owns.
- **Every gate reported from its own exit status:** `cmake --build native/build --config Release` 0; `ctest`
  3/3 0; `SliceForgeSmokeTest.exe` `PASS - 190/190 checks passed` 0 (CPU printed 4.94 %, asserted by nothing);
  `npm run build` 0 and `vitest run` 0 (5 files) earlier in the same pass, and still valid because `git status
  src` is empty and the newest `src/` file predates this work; the `launch` target 0; a re-opened zip with 9
  members, 0 backslash separators and 0 mismatches against the files it claims; `start.ps1 -DryRun` 0;
  `start.ps1 -InstallOnly` 0 (one UAC prompt, elevated child, self-verified `CE66DE92…`); the extracted
  package's own `start.ps1 -DryRun` 0 and `-InstallOnly -Vst3Dir <scratch>` 0 with no repo and no Node, hash
  matching `launch/` file for file. The scratch scan root was deleted afterwards; nothing else on the machine
  was written except the system VST3 folder.
- **What this does not prove.** No real DAW has loaded these bytes - `SliceForgeHostTest` is JUCE's headless
  host path and it opens the *build* copy, so the install is proven by hash equality, not by a host sighting,
  and Ableton acceptance is still the owner's step. `LegalCopyright` was absent here, and the reason this
  record gave for it was wrong - the CMake path does have the field, and 8I landed it. The census
  above is a property of this target selection: enabling AAX or LV2 later makes the "absent" rows false and
  the notices file has to be re-measured, not edited. And the archive's web-edition claim was fixed by
  scoping the docs to what ships - the browser edition is still repo-checkout-only, which is a product
  limitation of this packaging decision, not a bug in `start.ps1`.
- **Cite state after this pass.** `native/README.md` grew 495 -> 501 lines, +5 below its `:154`, so the two
  cites a reader acts on were re-pointed as location facts (`:96` -> `README.md:159-166`, `:250` ->
  `README.md:446`, the latter having been wrong by 47 lines even before this commit); the records that name
  an address *of their own time* (`:291`, `:814`, `:1462`, `:1597`, `:1699`) were left alone, per the
  8C/8D precedent restated at `:1598`. `AGENTS.md:151-156` replaces the `:143-148` cite at `:1786` - that one
  was transcription, not drift, proved by `git diff -U0 AGENTS.md` naming only a line-neutral hunk at `:35`
  and an append-only hunk at `:184`. This file's *edit set below the append* is 18 additions against 18
  deletions - line-neutral against the 1925-line file that every cite in it addressed, and that number is
  of-then (this file was 2476 lines that day, 2943 at the 8Q renumber); the whole commit carries 96 against 18, because 8H itself is 78.

- **Corrected forward, same day: `b481d74` published a hash it had not measured.** Its "zip
  `166175ed33fe8ba8a6a7c92bdcf4b05914bb8145ee908d7c14f7d1a4f3a4b1a4`" line is wrong from byte 13 onward -
  the true SHA-256 of `launch/`'s copy and of its blob is
  **`166175ed33fe3f77dd27440f7a6b96d2627101ff8148cedf13024417cb3cc156`**. The first twelve hex digits were
  measured; the remaining fifty-two were written from the shape of a hash rather than out of a command. That
  makes three hash defects this plan has recorded and only two of them the same defect: `c57f277` and
  `5f8815d` published *worktree* hashes for a `w/crlf` file, which are unreproducible, while this one is a
  number typed instead of copied, which is unreproducible *and* never measured. The tripwire is the one
  already named here: a hash is copied out of the tool output into the message, never retyped.
  Re-deriving the whole set with `git show HEAD:<path> | sha256sum` also produced two facts the message
  understated. `launch/SliceForge.vst3/Contents/Resources/moduleinfo.json` is `i/lf w/crlf` - 59 CRLF pairs,
  1,400 B in the worktree against 1,341 B in the blob - so `df39eafcbd1a21ff…` is the *deliverable* hash
  (worktree = zip member = installed, all three proven equal) and `14207bb8c801699b…` is the blob; the
  commit's binary block is about deliverable bytes, so its number is the right unit, but only because it was
  not labelled as such. And the `LICENSE` member hash the archive gate compares, `0d96a4ff68ad6d4b…`, was
  measured and left out of the list while the four text members around it were named.
  This correction is docs-only, so the deliverable tuple above still describes the built bytes: nothing was
  recompiled, re-exported, re-installed or re-gated by it, and the 190/190 plus ctest 3/3 in `b481d74` remain
  the green that owns `ce66de92…`.

---

## 8I. LegalCopyright landed, and the .rc that hides it is not dependency-tracked (recorded 2026-11-17)

Appended at the end for the 8H reason: inserted anywhere else it would shift every cite below it, and this
file cites its own line numbers.

- **The string Step 16 left open is in the deliverable.** `native/CMakeLists.txt` is +3/-0 against `3b70d7d`
  (181 -> 184 lines): one `COMPANY_COPYRIGHT` argument inside `juce_add_plugin` (`:105`) and the
  two comment lines above it. Read back out of the PE, its value is `SliceForge 1.0.0 - licensed under the
  GNU Affero General Public License v3.0 - Corresponding Source:
  https://github.com/vermeiresebastien/skeer-vrac-plus`. `${PROJECT_VERSION}` expands at configure time, so
  the field cannot drift from `FileVersion`. It names the licence and the source location, not a holder,
  because nothing in this tree asserts one.
- **Two published sentences were wrong, and both were mine.** 8H's "JUCE's CMake path has no `LegalCopyright`
  field — only its Projucer `.rc` examples do" is refuted by the vendored source: `COMPANY_COPYRIGHT` sits in
  `_juce_initialise_target`'s `one_value_args` (`JUCEUtils.cmake:1993`), is copied to the INHERITED
  `JUCE_COMPANY_COPYRIGHT` property at `:2096`, and that property is exactly what the manifest writer reads
  (`:363`). The second wrong sentence was the premise of my first attempt: a `set_property(TARGET SliceForge
  PROPERTY JUCE_COMPANY_COPYRIGHT …)` at the bottom of the file re-configures, builds and exits 0 while
  emitting nothing - `juce_add_plugin` calls `_juce_initialise_target` and then
  `_juce_configure_plugin_targets` (`:2189-2190`), and the manifest is written inside the second, so a write
  after the call has been read too late.
- **The second trap, now an `AGENTS.md` rule: the generated `.rc` is not dependency-tracked against the
  manifest.** `JUCEUtils.cmake:823-826` declares it as a custom-command OUTPUT whose only `DEPENDS` is the
  icon, so even the correct argument ships nothing on the build immediately after the edit. Ordered evidence:
  build exit 0 with the generated rc holding its five version strings and a `Translation`, none of them
  `LegalCopyright`; then the rc and `SliceForge_resources.res` were deleted, the whole project built again
  (exit 0), and the regenerated rc carries the new line at `:19`.
- **Read back out of the PE**, by `[System.Diagnostics.FileVersionInfo]` rather than out of the `.rc`: the
  string is present in the build module, the build standalone, the `launch/` module, the `launch/` exe and the
  installed module - five copies, one value. The three test executables report `LegalCopyright=[]`, which is
  the wanted outcome: the property is per-target, so the plugin's argument did not leak sideways into
  `SliceForgeSmokeTest.exe` and its siblings. Their `CompanyName` is JUCE's `yourcompany` placeholder -
  unsightly, and no test binary is a deliverable.
- **Tuple, superseding 8H's and superseded in turn by 8J's,** because 8J left every binary alone and restamped the archive around eight new members. module `b15476669eb2b5f3ff960d8cd09fce4df242c3144f2236541e6a27b3a239470d` (24,530,944 B - 512 B more than the `ce66de92…` 8H
  recorded, which is the entire cost of the added resource string) identical in `native/build/…/VST3`, in
  `launch/`, as the zip member, and in `C:\Program Files\Common Files\VST3`; standalone `c5478449cc40b07f7171416b2ca611cabcf5e8ba6b2a77701b1f8b95f1c1da97`
  (24,220,672 B, also +512 over its `a2b6c901…`); `moduleinfo.json` `df39eafcbd1a21ff5e4e9fa1f32b3d97c514d93a34ec6c071c393049119572da`, unchanged, as it must be -
  it is not the version resource. `THIRD-PARTY-NOTICES.txt` moved to `ff6e1227bbca1195721059f9a2df1cb2c6b511980d30f1f0214ad524d42f8efe` at the same 15,448 B, and the
  zip to `4b98b27299654e4d960413cbc35caa086998b7e2d0ade4a35acb7edc5735ebd0` (27,787,968 B) because that member sits inside it: 9 members, 0 backslash separators, 0
  mismatches against the files it claims. `start.ps1`, `start.bat`, `LICENSE`, `INSTALL.txt` and
  `native/README.md` are byte-for-byte what 8H measured - and 8H's extracted-package rehearsal was re-run on
  this archive rather than inherited, because "the mechanism did not change" is a reasoning step and this
  repository does not gate on those. From a folder with no `src/` and no `node_modules`: `-DryRun` 0 printing
  the browser-edition refusal, `-InstallOnly -Vst3Dir <scratch>` 0 with the scratch module hashing
  `b1547666…` file for file against `launch/`, and the module read straight out of the archive reporting the
  `LegalCopyright` string. The scratch folder and the extraction were then deleted. `start.bat` stays the
  member whose two hashes differ by layout: `0366b728d29b4465…` as worktree and zip member, `b831971f35d1ce53…`
  as blob (`i/lf w/crlf`).
- **Every gate reported from its own exit status:** `cmake --build native/build --config Release` 0 on the
  stale-rc pass and 0 on the regenerated pass; `--target launch` 0 twice; `ctest -C Release` 3/3 0 twice, the
  second taken after the final export; `SliceForgeSmokeTest.exe` `PASS - 190/190 checks passed` 0, CPU printed
  4.38 % and asserted by nothing; `start.ps1 -DryRun` 0; `start.ps1 -InstallOnly` 0 with one UAC prompt, an
  elevated child and its own file-for-file check against `launch/`. `git status -- src` is empty, so 8H's web
  gates (`npm run build` 0, `vitest run` 0, 5 files) still hold for this tree and were not re-run.
- **Cite state after this pass.** Three inserted lines move every `native/CMakeLists.txt` cite at or past
  `:101` by exactly +3, and no shifted number crosses a digit-width boundary, so each fix is byte-neutral:
  `:106-107`->`:109-110`, `:113`->`:116`, `:116`->`:119`, `:127`->`:130`, `:128-129`->`:131-132`,
  `:129`->`:132`, `:180-181`->`:183-184`. Thirteen sites: `AGENTS.md:61`, `:67`, `:99`;
  `native/Package/THIRD-PARTY-NOTICES.txt:91`; and this file's `:52`, `:86`, `:116` (whose prose "Erwm at 106,
  Vrac at 107" moved with it), `:234`, `:287`, `:380`, `:937`, `:965`, `:1378`. A validator printed the source
  line each cite now lands on, and all thirteen name the construct they always named. `AGENTS.md:184` also
  claimed "Five text files are now archive members" while `native/package-launch.ps1:146` stages
  `native/README.md` as a sixth - now six, and "the other four" is "the other five".
- **What this does not prove.** No real DAW has loaded these bytes: `SliceForgeHostTest` is JUCE's headless
  path and it opens the *build* copy, so the installed module is proven by hash equality and the About-box
  sighting in Ableton is still the owner's step. And a `LegalCopyright` that names no holder is a choice, not
  a finished job - AGPL section 5 is satisfied by a pointer to the Corresponding Source, but if SKEER or a
  person is to be named, that sentence is the owner's to write.

---

## 8J. The screenshots ship now, and the packer reads the link list out of the README (recorded 2026-11-17)

Appended rather than inserted, on the 8H/8I precedent: anything placed before `:1929` shifts every cite below it.

- **Found by a census, not by a report.** `native/README.md` is an archive member, and the member inside
  `launch/SliceForge-1.0.0-Windows-x64.zip` linked eight `shots/*.png` files that a nine-member archive did
  not contain. Every customer who extracted the package therefore saw eight broken images in the document
  whose job is to show the product - and unlike the repo reader, they cannot regenerate them. The same files
  were gitignored, so a fresh clone broke those eight *plus* the six `native/shots/` links in the root README.
  `AGENTS.md:137` recorded only the clone half ("broken in a fresh clone"): true, and silent about the copy
  that ships.
- **The owner chose "ship in zip + track in git",** and three measurements made that the cheapest option
  rather than the most expensive one. (a) The PNGs are byte-stable: `SliceForgeLayout` ran twice more after
  the first hash set - `ctest -R Layout` at 33.05 s (`100% tests passed out of 1`), then inside the full suite
  at 8.13 s - and
  `sha256sum -c` returned 10/10 `OK` (exit 0) on the third generation. That generation was written *after*
  `git add`, and `git status` still reports `A` and never `AM` for all ten paths, so on every generation
  measured a `ctest` run leaves the tracked tree clean - which is a fact about these four runs, not a promise
  the layout test makes. (b) The fix that looks cheapest is not a fix:
  `gh repo view` answers `visibility: PRIVATE` and an unauthenticated `raw.githubusercontent.com` fetch of a
  *tracked* file (`LICENSE`) is 404 on both `master` and `main`, so GitHub image URLs would have broken in
  exactly the hands that receive the zip. (c) PNGs are not a new kind of blob here - `docs/screenshots/` is
  already tracked at 1,526,195 B - and 1,180,600 B is 1.6% of the ~75 MB of `launch/` blobs that Step 17's
  `.gitattributes`/LFS decision is actually about, so that pending decision did not move.
- **The picture list is read out of the README, not written beside it.** `native/package-launch.ps1` now
  calls `Get-ShippedShotList`, which collects the `![...](shots/*.png)` links from the shipped README and adds
  each as a member under the same relative name. Both halves of the invariant are gated: a linked picture that
  is not on disk throws, and a *relative* image link this step does not carry also throws - so an archive can
  no longer contain a README pointing at an image it lacks, which is the same rule the file already applies to
  a start script and the licence text. An absolute URL is the only excused link, because that target is not the
  archive's to supply. The character class admits no separator, so `shots/../../x` is never a member name; the
  four refusal paths were probed by lifting the function out of the shipped file's AST (`PARSE_ERRORS=0`,
  `FUNCTION_LOADED_FROM_SHIPPED_SOURCE`) and calling it on modified copies in `%TEMP%` - missing file,
  `../docs/…` link, `shots/../../CMakeLists.txt` and an absent `shots/` folder each threw, an `https://` badge
  did not (`ABSOLUTE_URL_ALLOWED=count 8`), and the real README gave `POSITIVE_COUNT=8` and
  `POSITIVE_TOTAL_BYTES=838303` - the same 838,303 B the archive later measured from the other side.
- **8 of the 10 tracked PNGs ship,** because 8 are linked. `editor-large.png` (201,826 B) and
  `sequencer-large.png` (140,471 B) are layout-test output no document points at, and a member nothing
  references is dead weight: 1,180,600 − 838,303 = 342,297, which is those two files exactly.
- **An orphan refused to be committed by accident.** `git add native/shots` staged 11 paths: ten PNGs plus
  `native/shots/.png`, a valid 667-byte 1260×110 image dated 2026-11-14 01:51:31 - the rename day, three days
  before any name in `native/Tests/LayoutTest.cpp:129-271` (ten `getChildFile ("…png")` sites) wrote a file there, and invisible to plain `ls`
  because a leading dot hides it. It is not a deliverable and nothing writes it, so it was unstaged and moved
  to `D:\Projects\tools\sliceforge-rehearsal\orphan-shots-dot-png-667B-2026-11-14T01-51-31.png` rather than
  committed or deleted: a folder that was ignored until this pass has no history to explain an unexplained
  artifact, and retirement beats deletion when the alternative is a rebuild from nothing.
- **Gates, each from its own exit status, each recorded in its own log.** `cmake --build native/build
  --config Release` exit 0 - and it compiled nothing: an 18-line log (19 with the `EXIT=0` this pass appends),
  12 per-project `->` lines, **zero**
  `cl.exe` or `link.exe` lines, and the only `.cpp` mention is the `BinaryData*.cpp` generator line.
  `ctest --test-dir native/build -C Release --output-on-failure` exit 0, 3/3. `SliceForgeSmokeTest.exe` run
  directly: exit 0, `PASS - 190/190 checks passed`, with 190 `ok` lines counted in the same log. That suite ran
  twice against the same binary and printed `CPU: 4.21%` then `4.34%` of one core, and Layout took 8.13 s then
  27.24 s - the measurement behind `AGENTS.md`'s rule that the CPU figure is printed and not asserted. The 190
  is the suite's own tally and asserts nothing either, which is why it is quoted here beside the log that
  prints it rather than as a gate. The export log ends with its own banner, `launch/ refreshed (Release,
  v1.0.0)`, and `$ErrorActionPreference = 'Stop'` makes that line reachable only on the clean path. The
  package was then checked by a program that is not the packer: every one of the 17 members hashed out of the
  archive stream against the file it claims in the tree, `MEMBERS=17 FAILURES=0`, exit 0. A second export with
  *nothing* changed in the tree then measured the archive's own reproducibility: same 17 names, **0** member
  hash mismatches, same 28,584,705 B - and 68 bytes different, in 34 runs of exactly 2 bytes, which are the 17
  local headers and the 17 central-directory records at their DOS time field. So the zip is `d69b1f52…` where
  the export twenty-one minutes earlier was `5be46f61…` — and the "+28 MB per export" that `:547` drew from this
  was true of its own present tense only: §8L pins the clock, so the cost is history now, not a rule. The
  extracted-package rehearsal ran on the
  `5be46f61…` archive; every member of `d69b1f52…` hashes identical to it, so that rehearsal does describe the
  bytes being committed. Archive shape: 0 backslash separators, 0 directory rows, 8 `shots/` members totalling
  838,303 B.
  Shipped README: 8 links, 8 present as members, and 8 present as extracted paths on disk
  (`EXTRACTED_FILES=17`, `README_LINKS=8 BROKEN=0`). The extracted-package rehearsal was **re-run, not inherited** - 8I's own
  lesson - with `PATH` trimmed to `System32;Windows` for a child launched by absolute path
  (`HAS_NODE=False`, `HAS_NPM=False`): `-DryRun` exit 0 printing the browser-edition refusal,
  `-InstallOnly -Vst3Dir <scratch>` exit 0 with the scratch module `b1547666…` and "matches launch/ file for
  file". A `git ls-files -z` census over both READMEs: 18 image links, 0 unresolved-and-untracked.
- **Tuple, superseding 8I's.** The module is `b15476669eb2b5f3ff960d8cd09fce4df242c3144f2236541e6a27b3a239470d`
  (24,530,944 B) **unchanged** in all four places - build, `launch/`, zip member, `C:\Program Files\Common
  Files\VST3` - and that is the point: the export regenerated `juce_binarydata_SliceForgeAssets/BinaryData*.cpp`,
  so "the module cannot have moved" was a claim that needed measuring rather than reasoning. Standalone
  `c5478449cc40b07f7171416b2ca611cabcf5e8ba6b2a77701b1f8b95f1c1da97` (24,220,672 B), `moduleinfo.json`
  `df39eafcbd1a21ff5e4e9fa1f32b3d97c514d93a34ec6c071c393049119572da`, `LICENSE` 34,523 B at
  `0d96a4ff68ad6d4b6f1f30f713b18d5184912ba8dd389f86aa7710db079abcb0`, `INSTALL.txt` `db6a85d5…`,
  `THIRD-PARTY-NOTICES.txt` `ff6e1227…`, `start.ps1` `7938fe08…` and `start.bat` `0366b728d29b4465…` as 8I
  measured them. Every one of those is the *worktree* form the archive stores, which for `start.bat`
  (`i/lf w/crlf`) is by definition not the form the blob holds: `0366b728d29b4465…` in the zip,
  `b831971f35d1ce53…` from `git cat-file blob :launch/start.bat`. Of 8I's nine members, one changed bytes and
  eight are new. `native/README.md` went to 29,772 B (from 29,541) with the **worktree** hash `09242229b57f5f591bdba40f7d912e98fffb0460f04cbd5ea42a47b2fb71242a`
  - labelled, because `git ls-files --eol` says `i/lf w/lf` and `git diff --numstat` warned "LF will be replaced
  by CRLF the next time Git touches it". The distinction the warning draws is between the two ways of reading
  this commit, not between two numbers: `git cat-file blob :native/README.md | sha256sum` returns this same
  `09242229…` (measured after staging, and every other text path in this commit hashes equal between blob and
  worktree for the same reason), so the number is reproducible from the object forever - while a fresh checkout
  on this machine gets a CRLF copy of it, which is the thing Step 17 still owes. The zip went to `d69b1f523d8811d7c009a442d05ef2a9e60057b6144a7b08d296705ac44b20bf`
  (28,584,705 B), and 8I's `4b98b27299654e4d960413cbc35caa086998b7e2d0ade4a35acb7edc5735ebd0` at 27,787,968 B
  was re-derived from the blob `git show HEAD:launch/…zip` returns - it reproduced exactly, which is the base
  the +796,737 belongs to. That delta closes member by member, read out of both archives with `zipfile`:
  795,671 B is the eight PNGs after deflate (838,303 B raw, 5.08% - PNG is already compressed), 66 B is the
  README after deflate (+231 raw), and the remaining 1,000 B is eight local headers plus eight central-directory
  records - 8×(30+46) fixed bytes plus the 196 bytes of member names counted twice, exact to the byte, which is
  the only way a zip can grow without a single payload byte changing. No install step ran, because the module
  bytes did not change and the installed copy already hashes equal to `launch/`.
- **Doc surface, held line-neutral on purpose.** `.gitignore` line 4 was *replaced* with a comment, not
  deleted, so `native/build/` stays at `:10` for the four cites (`:774`, `:1131`, `:1582`, `:1685`) that depend
  on it. `AGENTS.md:137` was rewritten inside its own two lines, so the cites into `AGENTS.md`
  (`:61`, `:67`, `:99`, `:139`, `:143-150`, `:151-156`, `:184`) did not move; its `:184` "Six text files are
  now archive members" stays true, because the eight new members are images. `AGENTS.md:35`, the sentence that
  lists what the zip is restamped from, gained "the pictures `native/README.md` itself lists" inside its own two
  lines too, so the file is still 199 lines and every cite above was re-read at its number after the last edit.
  In this file, Step 17's `:547` still said `package-launch.ps1:65-69` *uses `Compress-Archive`*, which Step 16
  made false - and a cite that no longer names the construct is worse than no cite, because it reads as a
  location. The conclusion it carried is intact and re-measured: the writer embeds entry mtimes, so every
  export still costs a fresh ~28 MB blob. `native/README.md` grew
  501 → 502 lines, the inserted line sitting below the file's highest cited line `:446`, so no README cite
  moved. Three `Package/` references were corrected in the two different ways the two readers need: `:85`
  describes the *packer*, so it now names the repository paths in full (`native/Package/…`); `:495` tells the
  *receiver* where the licence text lives, so it names `THIRD-PARTY-NOTICES.txt` - the name that file has
  beside the README in the archive - and then says where the repository keeps it. `:303` cited
  `docs/next-steps-plan.md` to someone who will never own that file; it now says "the planning document" with
  the repo path in parentheses. And the packer's comment that explained the separator bug by counting "two of
  this archive's nine members" now names the *kind* of member instead ("the module is a folder and the
  screenshots live in a subdirectory"), because a member total inside a comment is a number that goes stale the
  first time the census changes - and this pass is the evidence: it went from nine to seventeen.
- **What this does not prove.** Every check above is a *path* test; nothing rendered the markdown and looked
  at the eight pictures. `docs/screenshots/sliceforge-mobile.png` is tracked but unlinked (4 links against 5
  files in the root README), which is a documentation gap, not a broken image. Tracking `native/shots/*.png`
  makes every `ctest` a rewrite of ten files; the determinism of those bytes is bullet (a) above, and the
  27.24 s Layout run in the gates bullet regenerated all ten again. What tracking *does* mean is that a stray
  `git add -A` is a binary commit waiting for the day a generator changes, so staged paths still get audited.
  And Step 18's instruction to "stop `LayoutTest` writing ≥10 PNGs into `native/shots/`" (`:556`) was
  written when the folder was ignored: the determinism measured here answers the *churn* half of that worry,
  not the "a test should not mutate the tree" half, which still stands and is still the owner's to schedule.

## 8K. The two market items the plan admitted it had never confirmed (recorded 2026-11-17)

- **Half the ask was not well-formed, and the repo is why.** What `:1924` now points at here used to read
  ```Still **not** confirmed after two rounds: the "SliceForge" name-collision risk, and the current prices
  of Portico, Influx and Ripchord.``` and `grep -n "Portico\|Influx\|Ripchord"` over that file returned that
  one line and no vendor for any of the three - which is still the census, this section being the only other
  place the names appear. A price cannot be confirmed for a product whose maker the record never
  states, so the item was resolved by identifying each name instead. **Ripchord** is Trackbout's *free* MIDI
  chord-progression tool (vendor page at 2.7.0, a KVR product entry and plugins4free all agreeing);
  **Portico** is Rupert Neve Designs' channel-strip and mastering line (5043, Portico II MBP); **Influx** is
  ambiguous in audio - FKFX Audio's Influx/Influx Pro is a tube-distortion "sound mangler", and an "Influx
  Kit" exists inside Spitfire Audio's Aperture/AFRORACK bundle. None of them is a *one continuous recording
  becomes a playable kit* product, so no price among them describes this market, and the sentence is corrected
  rather than filled in. The category's real neighbours are Initial Audio's Slice, Antares Auto-Tune Slice,
  FL Studio's Slicex/Fruity Slicer, Ableton's Slice to New MIDI Track and Maschine's Sample Slicer.
- **VRAC+ is the comparable, and its claims now rest on a fetch rather than on reporting.** A direct GET of
  the vendor page (1,002,427 B, `"dateModified":"2026-10-13T17:54:24+00:00"`) contains the literal strings
  `Original price was: € 149.00.` and `Current price is: € 39.00.` and `VST3 · AU · AAX · macOS & Windows`,
  and it sells per-pad work today: `PER PAD Warp, Impact, Instability, Tone Shift, Super and a 3-band EQ`,
  `16 Pads 8 Stereo outputs 4 Model families`. So the price, platform and format claims at `:1916-1918` are
  first-hand. What the page does *not* carry is a changelog, so "per-pad multi-slicing added September 2026"
  stays reporting and the "same slice for every pad" complaint stays a KVR thread - both are still on the
  sources named at `:1920-1922`. Resale trackers scanned today show `$39.00 $149.00`; the vendor's bundles
  page (dateModified 2026-11-05) is in a Black-Friday promotion at €39 across paid items, so today's €39 is a
  November price that coincides with July's early bird rather than the early bird still running. That last
  distinction is the research pass's finding, and the one item here I did not measure myself.
- **The name collision is real, current, and outside this category - and it predates us.** Verified by direct
  fetch: Steam application 5238480 titled **SliceForge**, developed and published by NikiBStudio, released
  **28 Sep 2026**, priced **10,25 €**, store copy "SliceForge turns an audio file into a custom map for Beat
  Saber" and "Drop any audio file (mp3, wav, flac, m4a, ogg) and SliceForge analyzes the track — BPM, beat
  grid, drum hits, song structure". Same bare name, same one-audio-file-in / playable-thing-out promise, and
  it launched about six weeks *before* this repository adopted the name on 2026-11-14 - the order matters,
  because it means the name was already taken rather than taken up after us. A Mac App Store entry as
  "SliceForge: VR Map Maker" at $9.99 (listing seen 2026-10-12) is the one part that rests on search snippets:
  the direct fetch 404'd, so it is recorded as unverified. Separately, a USPTO application for the identical
  mark - serial **50136261**, filed **2026-09-29**, International Class **042** (SaaS) - is attributed to
  Strategic Sloth LLC, the game-UI suite at sliceforge.io; that too is a filing-index summary, not the register.
- **No official register was searched in any territory, and that is the residual.** TSDR answered 401;
  `tmsearch.uspto.gov` served a 125,603-byte SPA shell containing neither the string "SliceForge" nor the
  serial number; Justia sat behind a JavaScript challenge; EUIPO, WIPO Global Brand Database and TMview were
  unreachable. So this section produces a risk rating and not a clearance. The rating is **MEDIUM**: nothing
  found named SliceForge competes in plugins or instruments, and the music/DSP goods classes look open, but
  the name is live and recent in paid adjacent software and an identical-mark application is pending in a
  software class. Closing it properly means a professional search in the classes that matter (IC 009 for
  recorded-signal-processing software, 015/041 as applicable), which is an owner decision with a price
  attached - as is any answer to "rename or keep", which is the same decision `AGENTS.md` already freezes the
  data keys against.
- **Awareness friction is not a legal finding.** "Slice" is descriptive and crowded in exactly this niche, so
  the first word of the compound costs discoverability regardless of who owns what; and "-forge" is already
  used in drum tools by Drumforge. For the house mark, "SKEER" shows no audio *product* collision, but there
  is a "SKEER SAMPLE PACK" on Bandcamp and a Spotify/Beatport artist named SKEER - a search-ranking cost for a
  sample-based instrument, which is the kind of thing a rename does not fix.
- **What changed in the repo.** `:1924-1925` is rewritten to point here instead of asserting a gap; nothing
  else quotes a price for those three products, and the census is the grep above re-run over the four doc
  surfaces - five matching lines in this file (four of them this section, one the `:1925` pointer) and **zero**
  in `AGENTS.md`, `native/README.md` or `INSTALL.txt`, so the only place the names live now is the record that
  says what they are. No frozen identifier moved: `PLUGIN_CODE Vrac`,
  `PLUGIN_MANUFACTURER_CODE Erwm`, `"VRACPlus"`, `"VRAC_EXTRA"`, `0x56524143` and the `.vracpreset`/
  `.vracpattern` extensions are session and file data keys, and a naming risk in the market does not make it
  safe to orphan a user's saved sets.

## 8L. Step 17's determinism half: the archive clock is pinned, and the date that reaches a scan folder is
not (recorded 2026-11-17, closing after midnight)

- **Two files, and who wrote which half.** `native/package-launch.ps1` gained 11 lines and lost none
  (`git diff --numstat` 11/0, 303 lines): `$ZipFixedStamp` at `:201-202` and
  `$entry.LastWriteTime = $ZipFixedStamp` at `:218`, assigned **before** `entry.Open()` - and that order is not
  a style choice. A probe that sets it after `Open()` throws `Cannot modify entry in Create mode after entry has
  been opened for writing.`, so the packer would have died at the first member; the same run measured the pinned
  write producing `time=0x0000 date=0x0021` and an unstamped header `0x016F/0x5D72`, i.e. the live clock on
  2026-11-18: a local wall clock with no zone stored anywhere, which is why the pinned pair is zone-independent
  rather than merely constant. That hunk arrived in this working copy at 23:38:14, from the
  other lane working the same step, before this lane's first export - recorded here as their edit independently
  verified rather than as mine. `launch/start.ps1` gained 8 and lost none (326 lines): the install restamp at
  `:156-163`, which is the consequence measured below. Both are `i/lf w/lf`, and the equality with what Git will
  store was proven without touching the shared index - a temporary `GIT_INDEX_FILE` over `read-tree HEAD` +
  `git add` was used to read the would-be blob, and the real index held 0 staged paths afterwards. (Not
  `git hash-object`: that prints a SHA-1 blob *id*, which is not the SHA-256 of the bytes and never was.)
  `native/package-launch.ps1` `8ddad5dfa78f2822a9695de1c2b5917952f8207b04471f9c3b6ebe8a23c60690`,
  `launch/start.ps1` `c9000294ffe22b007bca2bc4f574f9d800dbc1abdbc082cfb8c1f8286397c70f`.
- **The mechanism, re-measured on the two archives that still exist.** The committed archive
  `d69b1f523d8811d7c009a442d05ef2a9e60057b6144a7b08d296705ac44b20bf` (28,584,705 B, re-read from
  `git show HEAD:launch/SliceForge-1.0.0-Windows-x64.zip` so the comparison is against the object, not a copy)
  carries **two different stamps inside one export**: `time=0xB389 date=0x5D71` on the first member and
  `0xB38A/0x5D71` on the other sixteen - 22:28:18 then 22:28:20, because the clock was never even constant
  within an archive. The current archive carries one value on all 17: `time=0x0000 date=0x0021`, which decodes
  to 1980-01-01 00:00:00. Walking the central directory by hand instead of through a library: for the five
  members whose payloads did not move between the two archives, the only differing bytes are local header
  `+10..+13` and central record `+12..+15` - four bytes in each of two structures per member, so **34 runs of 4
  = 136 bytes** separates a pinned from an unpinned export of the same content. §8J's 68 bytes in 34 runs of 2
  is that same layout with the date already equal: only the two time bytes move. Extra fields are 0 bytes long
  in both structures and no local/central pair disagrees, so there is no extended timestamp left to pin.
- **What the pin buys, and the one thing it does not.** Measured live, not inherited: export H at 23:47:55 and
  export I at 23:56:35, with an edit to this file at 23:52 between them. `cmake --build native/build --config
  Release --target launch` run unpiped, `EXIT=0` in each log next to its own banner
  `launch/ refreshed (Release, v1.0.0)`, and the SHA-256 census of all six files under `launch/` identical
  before and after - the archive is `9589ff5541949d4441caf497d816d37c6235ddf9c8690a001e3e13a852a279cf`,
  28,585,011 B. Log I contains zero `cl.exe` and zero `link.exe` lines, so the export compiled nothing and the
  bytes it shipped are the 17:49 build; no file under `native/Source` moved in this change, so this section
  claims no engine re-gate and needs none. What determinism does *not* buy: a different .NET is free to choose
  different deflate output or version bytes, so the proof is "re-exports on this machine are one object", and
  cross-toolchain byte-identity belongs to Step 18.
- **The consequence nobody had measured: a pinned archive installs at 1980.** All 17 members unpack dated
  1980-01-01 (`EXTRACTED_DISTINCT_DATES=1980-01-01 DISTINCT=1` over `EXTRACTED_FILE_COUNT=17`) and `Copy-Item`
  carries that onto every installed file. The red leg is real, not hypothetical - the same rehearsal against the
  pre-fix installer printed `INSTALLED_MODULE=24,530,944 B mtime=1980-01-01 00:00:00`, hash `B1547666…`,
  `CHILD_EXIT=0`: the installer reported success and handed the host a plug-in that looks forty-six years older
  than the one it replaced. A host that decides whether to re-read a cached plug-in from the file's mtime - the
  exact cue this repo uses elsewhere, size:mtime pairs in Live's rescan evidence - would read a new build as no
  change at all. Green, same probe after `:156-163`: `mtime=2026-11-18 00:01:45`,
  `INSTALLED_MODULE_SHA=B1547666…`, `INSTALLED_DISTINCT_DATES=2026-11-18`, `CHILD_EXIT=0`, and the script's own
  `matches launch/ file for file`. Both rehearsals ran with `PATH` trimmed to `System32;Windows` on a child
  launched by absolute path from the extracted archive, so they describe what a customer's machine does.
- **Where the restamp stops, on purpose.** The extracted `SliceForge.exe` still reads 1980-01-01
  (`INSTALLED_STANDALONE_MTIME`): it is the user's file in the user's folder, no host scans it, and silently
  redating files outside the install path is a worse surprise than a date that looks old. Stated as a boundary
  rather than coded, and the stamp itself sits after *both* bundle hash checks - the bytes are the proof of what
  was installed, the date is only a scan cue.
- **Tuple, superseding 8K's.** HEAD's zip is `d69b1f52…` at 28,584,705 B; the working tree's is `9589ff55…` at
  28,585,011 B, and the +306 closes member by member with no residue: 16 of 17 payloads byte-identical, one
  changed (`start.ps1` 16,397 → 17,143 raw, deflated 5,656 → 5,962 = +306), and member count and name order are
  unchanged. So apart from the 34 stamp fields, the only bytes that differ are that member's own CRC and sizes
  plus the stored local-header offsets of the eleven members written after it - two bytes each of `+42..+45`,
  because a 306-byte shift fits in the low half. All 17 members were re-hashed out of the archive stream
  against the file each claims: `TREE_COMPARE_FAILURES=0`. Module
  `b15476669eb2b5f3ff960d8cd09fce4df242c3144f2236541e6a27b3a239470d` (24,530,944 B) unchanged in build,
  `launch/`, zip member and `C:\Program Files\Common Files\VST3` - and the installed copy there is dated
  2026-11-17 17:49:58, which predates the pin, so the machine-wide install is already correctly stamped and this
  change asks for no re-install. Standalone `c5478449cc40b07f7171416b2ca611cabcf5e8ba6b2a77701b1f8b95f1c1da97`,
  `moduleinfo.json` `df39eafcbd1a21ff5e4e9fa1f32b3d97c514d93a34ec6c071c393049119572da`, `start.bat` as a member
  `0366b728d29b4465…` against blob `b831971f35d1ce53…` - still the `i/lf w/crlf` exception. Two numbers 8K
  published are now only true of HEAD: `28,584,705 B` and `start.ps1 7938fe08…`. And one number this section
  refuses to publish: the pin-only intermediate export measured earlier this evening is not quoted, because the
  next export overwrote its bytes and no witness is left to re-derive it from; the layout claim it carried is
  derived above from archives that still exist on disk.
- **Three artefacts of my own probes, recorded so the next reader does not repeat them.** `FILES=10` in the
  first two rehearsal logs was Windows PowerShell 5.1 resolving `.Count` on a single string to its `Length` -
  `"1980-01-01"` is ten characters - so the probe now prints `DISTINCT=1` beside the real file count. The
  `EXTRACT_EXIT=` line was deleted: `Expand-Archive` has no exit code, and a line that reports one narrates a
  fact that does not exist. Logs F and G captured `EXIT_F` / `EXIT_G` after a pipe, which is `tail`'s status and
  not cmake's, so the reproducibility claim rests on H and I alone.
- **Doc surface, held line-neutral where it had to be.** Step 17's paragraph at `:543-551` was rewritten inside
  its own 9 lines, so `:556`, `:774`, `:1131`, `:1582`, `:1685`, `:1916-1925`, `:1929` and `:2061` did not move;
  §8J's sentence that asserted the +28 MB cost in the present tense now points here, inside its own line; §11's
  banking arithmetic was re-derived in place (8.0 d banked, so ~33.75 d remain, and the Ableton-visible remainder
  is 1.25 d); `AGENTS.md` gained its 8-line rule at EOF (199 → 207 lines), below `:184` - the highest line any
  cite into it names - so all twelve of the `AGENTS.md:` cites in this file still land. `native/README.md` was
  deliberately not touched: it makes no timestamp claim, and editing a member would buy nothing the archive does
  not already prove.
- **What Step 17 still owes, and it is not code.** The git half is unchanged by any of this - `launch/` stays
  tracked, Git LFS is still genuinely inactive (no `.gitattributes`, no non-sample hook in `.git/hooks`,
  re-checked while writing this), and the pin only removes the *accidental* blob. Every real payload change
  still costs ~28 MB, which is the decision `:543` hands back. Owner-side, in order: commit this, decide the git
  half, then the install (bytes already equal, mtime already correct) and the push. Ableton acceptance is still
  the gate that no headroom in this document can substitute for.

## 8M. The import length became one promise per edition, and the smoke suite's stack was already spent (recorded 2026-11-18)

- **What was asked, and what the tree admits.** "make sure the midi slice import function works like a charm and
  implement that system in a more clear way. max length of the import also up that to 5min of audio." Neither
  edition imports MIDI: the only `.mid` sites under `native/Source` are the two *export* doors
  (`SequencerPanel.cpp:933`, `:951`), the editor's drop gate accepts audio extensions only
  (`PluginEditor.cpp:407-413`, so a dropped `.mid` is declined rather than half-read), and the web edition's MIDI is
  a WebMIDI device with no file reader. "5 min of **audio**" therefore binds on the recorded take, which was 120 s;
  the file doors were already at 600 s. What was reviewed is the real path the words describe - import → AUTO SLICE →
  pads → sequencer/MIDI out - and this section is explicit that no MIDI ingest exists to review.

- **One statement per edition.** `native/Source/Engine/ImportLimits.h` (new) holds `kMaxTakeSeconds = 300.0` (`:17`),
  `kMaxFileSeconds = 600.0` (`:21`), `kMaxRecordBytes = 256 * 1024 * 1024` (`:28`) and the three capacity functions
  (`:31`, `:41`, `:47`); `src/lib/importLimits.ts` (new) holds the browser's four numbers and `takeIsFull`. Before
  this, the native take was `currentSampleRate * 120.0` samples under a 48 MiB ceiling while the native file cut at
  `48000.0 * 60.0 * 10.0` samples whatever its rate was, and the browser spread the same policy over three files as
  five literals - `elapsed >= 120` and "Up to 2 minutes" in `RecordModal.tsx`, "up to 2 minutes" in `HelpModal.tsx`,
  and `160 * 1024 * 1024`, `10 * 60` and `0.05` in `audioFiles.ts`. Every door now calls one of two functions: the
  take at `PluginProcessor.cpp:1003`, the file at `SampleData.cpp:61-62`, and both browser refusals build their
  message out of the constants - same words as before, now derived: the 160 MB and 10-minute texts the suite quotes.

- **The trap, and why the byte clamp had to move with the seconds.** The promise is a duration; a duration at a
  sample rate is a byte count. Raising the take to 300 s and leaving the 48 MiB clamp would have *shortened* the
  take it promises to lengthen: measured under that mutation (red leg 2 below) the take stopped at 131.07 s at
  48 kHz, 32.8 s at 192 kHz, and every rate in between was the clamp rather than the promise. At the chosen
  256 MiB the duration binds first at every rate a drum instrument runs at - 300 s at 96 kHz is 28,800,000 frames,
  inside the clamp - and only 192 kHz sees the clamp, where five minutes would be 460 MB behind a REC button. The
  button states what actually applies there: `getRecordingLimitSeconds()` (`PluginProcessor.cpp:1018`,
  `PluginProcessor.h:124`) feeds the tooltip at `SourcePanel.cpp:111-114`, so 192 kHz reads 175 s, not "5 minutes".

- **Two latent defects the subsystem was carrying.** The web record readout counted seconds past 59 - it printed
  `00:119.9` for 1 minute 59, because `formatTimer` was written against the two-minute cap (`RecordModal.tsx:334-341`
  now rolls the minutes field). And the slicer's ceiling existed twice in one file: `MAX_SLICES` was imported and
  unused while the detector's own break said `64` (`src/audio/slicer.ts:9`, `:79`).

- **The smoke suite was dying before any assertion could fail, and no log had recorded it.** `SliceForgeSmokeTest.exe`
  exited `0xC00000FD` - STATUS_STACK_OVERFLOW - inside the Step-27/28 migration block. The death was first mislocated
  three hundred lines earlier because stdout is a 4 KB full buffer on Windows, so the log ended at a buffer boundary;
  flushing per check moved the truth, and a stack census then measured **1,004,448 bytes consumed at the suite's first
  check** against the 1,048,576-byte reserve the PE header declares. The harness is one 2,950-line `main()` that holds
  its engine objects by value - 21 processors at 16,240 B each, 11 `SeqBlockOutput` at 10,264 B, a `SeqPattern` at
  24,672 B - and MSVC gives each block its own slot, so the frame is ~1 MB before any of it runs. That is a property of
  the harness, not of the engine, so the fix is split: the smoke exe links `/STACK:16777216` (`CMakeLists.txt:144-151`,
  MSVC-only) and the two production sites that put a 96 KB `SequencerData` on a *host* thread's stack moved to the heap
  (`Sequencer.cpp:848` in `serialisePattern`, `SequencerPanel.cpp:972` in `writeMidiToFile`). `make_shared` value-
  initialises exactly as the locals did - every `SequencerData` member has a default initialiser - so the bytes a
  `.vracpattern` carries are unchanged, and the suite proves it. The per-check flush stays with the reason attached.

- **A new gate that read the wrong bytes, caught by itself.** The disk round-trip block (`SmokeTest.cpp:1918-1974`)
  writes the clip the way the UI does and reads it back the way a host does. Its first draft parsed MThd bytes 10..11
  as the tick unit and printed `[1]`: in a 4-byte-tag header those bytes are the *track count* (0..3 "MThd", 4..7
  header length, 8..9 format, 10..11 tracks, 12..13 time format - `juce_MidiFile.cpp:498-503`). The division is now
  read at 12..13. `juce::MidiFile` has a `setTicksPerQuarterNote` and no getter, which is why the check reads the
  file's own bytes at all: that is stronger than asking the object what it thinks it wrote.

- **Gates, and the red legs that measure them.** Green: native smoke **206/206** (`final-smoke.log`, exit 0), ctest
  **3/3**, `npm run build` exit 0 (the only type gate - it is what caught this test's `node:fs` import; the header is
  now read with Vite's `?raw`), vitest **53/53**, and the browser suite **27/27** against a real Chrome with fake
  media/MIDI devices - which is the gate that actually walks both import doors (`setInputFiles` then decode+slice, and
  record → take → source) and ends with `no page errors` / `no console errors`. Two mutations of `ImportLimits.h`, each restored and the restore
  proven by `sha256sum 0539080bba11e2f1a6efecc1098861a58597e8e673d95600c685b25b06c270cc` equality:
  - `kMaxTakeSeconds = 120.0` → native 7 FAILs (four rate capacities, `the take promise is what the clamp allows`,
    `a rate the host has not given is the promise`, `REC offers five minutes at this rate [120]`), 0 file-door FAILs;
    web 1 FAIL - `match the native edition, ceiling for ceiling`, the cross-language gate doing its one job.
  - `kMaxRecordBytes = 48 MiB` → native 6 FAILs (three capacities, `the byte clamp binds at 192 kHz [32.8 s]`, the
    promise-vs-clamp equality, `REC offers five minutes at this rate [131.072]`), and the by-construction green legs
    stayed green: `a rate the host has not given is the promise` (no clamp to bite) and both ten-minute file checks.
    Web 1 FAIL - `leaves the native byte clamp wider than five minutes at 96 kHz`, which is the arithmetic that the
    owner's "up to 5 min" request silently breaks if the clamp is not moved.

- **Tuple after the re-export, and what the archive proves.** `launch` target exit 0 with `SliceForge.exe` not running.
  The module is one artifact at three addresses: build `e3346f4dbc2a0273eab184ef717601162ea31962dd37db90958aaf3d600afb31`
  == `launch/SliceForge.vst3/Contents/x86_64-win/SliceForge.vst3` == the same member extracted from the zip.
  `launch/SliceForge.exe` `b3537ecadaae81d3fdf59f209989fcf5957ce5d4ab0d9b4757bcb1202b7f4c69` == the zip's member;
  `start.ps1` `c9000294ffe22b007bca2bc4f574f9d800dbc1abdbc082cfb8c1f8286397c70f` == the zip's member; and because
  `native/README.md` is one of the six text files a customer receives, so its tree bytes
  `241f137990325e409a8775e4854b175a5988cff6613ec54b9e4d95d451c3d615` are the member's - the five-minute REC text
  ships. The archive is 27.3 MB, `f6e0e29b3c6c44bef1f7281845eabf75ddcd8295da298800f53cd47edd4e33dc`,
  17 members, `unzip -t` exit 0, and - re-checked here because §8L recorded a backslash defect - **0 of 17 member
  names contain `\`** (read with `System.IO.Compression`, not `grep`, which mangles the pattern under Git Bash).
  A second `launch` export after the doc edits produced the *same* zip hash and the same module hash, and
  `SliceForgeSmokeTest.exe` is byte-identical before and after (`057d32073601bc9765d9e699ba10051dc97e97d15e03760ceb5fee0a7179fd67`),
  so the binary that printed 206/206 is the binary still in the tree. The stamp census backs the green:
  `ImportLimits.h` restored 00:44:46, `SampleData.obj` 00:46:15, `SmokeTest.obj` 00:46:37, exe 00:46:41.

- **Still open, and not code.** The install into `%ProgramFiles%\Common Files\VST3` and the push are owner actions;
  the installed module is still the pre-§8M build, so no green test in this section describes what his DAW loads.
  Ableton acceptance remains the gate nothing here substitutes for. One drift is named, not fixed: an over-length
  file is **truncated** by the native door and **refused** with a message by the browser door. Both are defensible
  and they are not the same product behavior; choosing between them is an owner call, and §8M's numbers do not
  depend on which way it goes.

## 8N. Docs only: the plan's own cites and totals caught up with the tree (recorded 2026-11-18)

The owner asked for the installer recompiled, committed and pushed. The recompile is the interesting half:
`cmake --build native/build --config Release --target launch` exited 0 and changed nothing, because the
pinned 1980 entry clock makes an unchanged payload produce a byte-identical archive. Measured on this
machine, after the export, against `launch/`: zip
`f6e0e29b3c6c44bef1f7281845eabf75ddcd8295da298800f53cd47edd4e33dc`, `SliceForge.exe`
`b3537ecadaae81d3fdf59f209989fcf5957ce5d4ab0d9b4757bcb1202b7f4c69`, the VST3 module
`e3346f4dbc2a0273eab184ef717601162ea31962dd37db90958aaf3d600afb31`, `start.ps1`
`c9000294ffe22b007bca2bc4f574f9d800dbc1abdbc082cfb8c1f8286397c70f` — all four equal to what §8L and §8M
recorded. `ctest` re-run beside it: 3/3, exit 0, smoke `PASS - 206/206 checks passed`, host
`parameters: 350 = 348 layout + Bypass + Program`. There was therefore nothing to commit from the build,
and the only honest diff left in the tree was this file describing itself wrongly.

- **Four test cites re-read rather than re-derived.** `native/Tests/HostTest.cpp:69` → `:110` (the
  `!= 350` literal), its composition print `:107` → `:148`, the pattern-index print `:102` → `:143`, and
  `SmokeTest.cpp:171`/`:177` → `:181` (the `== 348` check). One was deliberately left alone: `:237` quotes
  the string `AGENTS.md` held when Step 5 audited it at `4abe4b0` — a record of its own time, per the
  precedent at `:1999`. `AGENTS.md:143` already cites `:110`, so nothing live disagrees any more.
- **§11's arithmetic was the actual defect, and it was a present-tense claim.** It said "Eight days are
  banked … ~33.75 d remain" and named Steps 5 + 23 + 22 as the Ableton-visible remainder, all three of
  which have since landed. Banked is **9.25 d** (2.75 + 0.25 + 1 + 0.5 + 0.5 + 0.25 + 3.75 + 0.25, the
  last of which is Step 17's determinism half), remaining **~32.5 d**, and what is left that changes what
  you can claim in a host is Step 24 (1.5-2 d) then Step 25 (1). The stale pair stays named inside the
  corrected sentence, the way Round 1's "~31 d" is named, because the recurrence is the lesson: a total in
  prose is a claim with an expiry date, and the steps that expire it are the ones that land.
- **Line-neutral where it had to be: 21 against 21 below the append.** Item 1 of §11 had grown to seven
  lines from five earlier in this session — two lines is enough to move every address below it — so it is
  compressed back inside its original five, and `git diff --numstat` printing equal additions and
  deletions is the check. It is the only one that catches a tidy whole-line rewrite that shifts the file;
  this section then appends 41 lines of its own, blank separator included, which is why the commit carries
  62 against 21. What the two-line shift would have cost: the *live* cites into this file all address
  `:38`, `:138`, `:434`, `:543-551`, `:985`, `:1069` or `:1094` - found with a grep for "plan `:" - and
  every one of them sits above the edit, so none of them moved. The two that sit below it are `:1924-1925`
  and the `:1925` pointer, and both are §8H's and §8L's records of their own commits: exempt, per the
  precedent named at `:1999`.
- **Push discharged, install still open.** `git push` was authorised in the same instruction, so §8M's
  "the push [is an] owner action" is closed by this commit for the repository. The install is not: `%ProgramFiles%\Common
  Files\VST3` still holds the pre-§8M bundle — a different artifact from the one that printed 206/206 —
  and the Ableton listening pass remains the gate nothing in this document can stand in for.

## 8O. Step 24: every slice got an end, in both editions, and the native README was sixteen checks
stale (recorded 2026-11-18)

The defect the plan had carried since round 1 was that an onset is a start and nothing else. A dropped
take therefore became sixteen overlapping copies of itself: each pad played from its own onset to the end
of the file, so eight hits on one recording sounded like eight whole recordings stacked on each other.
The fix gives every slice an end — the distance to the onset that follows it, and to the end of the
material for the last one — and writes it into the pad's existing LENGTH parameter. Both editions, one
design each: `sliceGapSeconds` at `native/Source/Engine/Slicer.cpp:98-109` and `src/audio/slicer.ts:112-117`.

- **The parameter choice was the decision, and it cost nothing.** LENGTH already existed, already gets
  multiplied back by the global in `VoiceEngine.cpp:149` and `engineCore.ts:280`, and is already saved
  with the set, automatable, host-visible, undone by the existing snapshot path and grabbable afterwards.
  So the derivation divides the seconds by `jmax (0.25, global.length)` and by nothing else: no per-voice
  correction, and velocity's own length influence stays the player's rather than a thing the engine
  quietly compensates. No parameter was added, which is why the host census is still
  `350 = 348 layout + Bypass + Program` — measured in the same run as the gate below, not carried.
- **One statement of the gap per edition, and a mutation forced it.** `sliceMapFromSlices` fills
  `mapping.padLengths[pad]` from the same helper (`Slicer.cpp:141`) and the processor's LENGTH write calls
  that helper too (`PluginProcessor.cpp:827-828`), so the map and the knob are one computation rather than
  two copies of it — and `SmokeTest.cpp:2925` compares the map against a gap stated independently, so a
  drift cannot pass silently. The
  `jmax (start, …)` inside the helper is not decoration: a slice can be placed past the end of the
  material from the pad editor, and without it that slice's length goes negative and clamps to the floor.
  The forward-walk distribution rule that mutation C re-broke lives at `Slicer.cpp:119-141` and its web
  twin at `slicer.ts:132-146`.
- **Native gate.** `217/217 checks passed`, exit 0, on a full rebuild (exit 0) whose object stamps close
  the loop at seconds: `Slicer.obj` 15:55:00 ≥ `Slicer.cpp` 15:54:28, `PluginProcessor.obj` 15:53:47 ≥
  15:53:24, `SmokeTest.obj` 15:28:24 ≥ 15:24:17, exe linked 15:55:01, printed 15:55:50. Five clean runs
  of these bytes printed CPU 4.52 / 4.56 / 4.86 / 5.04 / 5.75 % of one core. `ctest` 3/3 exit 0 (smoke
  1.61 s, layout 29.33 s, host 0.71 s), host `parameters: 350 = 348 layout + Bypass + Program`,
  `PASS VST3 host test`. The step added eleven check sites — 201 → 212 by the `^ *check (` census, which
  excludes the `void check (…)` definition that a looser grep counts and makes 213 — and each new site
  runs once, so the runtime total moved 206 → 217.
- **Native falsifiers, three, on the gated bytes.** A deleted the LENGTH write from
  `resliceAndAssignStarts` → 2 FAILs, `215/217`, pad 0 came back at its 0.4600 default where 0.8987 was
  expected. B made `sliceGapSeconds` ignore `durationSeconds` → 3 FAILs, `214/217`, pad 3 at the 0.0200
  clamp floor, and MSVC's `C4100: 'durationSeconds': unreferenced parameter` in the build log is a free
  second witness that the mutation was really compiled. C put the `pad % n` wrap back → 1 FAIL,
  `216/217`, `[110, last pad on slice 3]`. An earlier cycle, on a tree that was still being built, had
  run the same three plus a gap-forced-to-zero probe at 215 / 216 / 214 / 212 — and the log named
  `s24-smoke-green.log` printed **FAIL - 213/217**: the filename asserts a verdict its content denies, so
  the content is what this section records.
- **Web gate.** `npm test` exit 0, `Tests 65 passed (65)` across 7 files; `npm run build` exit 0, which
  is the only type gate. The census re-derived from the sources rather than remembered: 53 → 65 tests,
  6 → 7 files, `audio.test.ts` 13 → 21 `it`s and `sliceEnds.test.ts` new with 4.
- **Web falsifiers, five, each restored by SHA-256 against the bytes read at the top of the run.**
  W-A a slice's end forced to the end of the file (the bug itself) → 6 red; W-B the knob given raw
  seconds, global division gone → 3; W-C `t = i % n` → 2; W-D the drop-a-take door in
  `factoryPresets.ts:253` stopped assigning → 2; W-E the SENSITIVITY door at `useKit.ts:140` stopped
  assigning → 1. Every probe printed `restored against baseline: MATCH` and the restored tree re-ran
  green. The pre-mutation hashes are the ones in the bullet on bytes below.
- **Two test-shape lessons, both paid for.** The first W-B run left the session test green because the
  expectation called the same helper as the code under test — a shared oracle is not a check; restating
  the gap independently from the raw slice array made it red, and the native check was then re-read for
  the same tautology. And the forward-walk test reddened *correct* code at first: it compared the
  legitimate restart step (pad 3 → pad 4). The rule is `indices[n] === 0`, monotonic from `n + 1` on. W-C
  is the reason that test exists at all — the four distinctness checks are blind to a wrap, on both sides.
- **One test seam, and it is a seam only.** `reducer`, `HistoryState` and `Action` are now exported from
  `src/state/useKit.ts` because SENSITIVITY is the live reslice door in the browser and a function call
  is the only way to reach it without a renderer. Nothing about the reducer's behavior changed; `useKit`
  still owns the dispatch.
- **Deliberate non-derivation, and the residual that comes with it.** `buildPresetSession`
  (`factoryPresets.ts:213`) does not re-derive, so the factory programs keep the override lengths they
  ship with, and a test pins that rather than leaving it to reading. The host-state read-back path
  likewise. What is *not* re-derived is the hand-edit doors: `addSliceAt` and `deleteNearestSlice`
  (`PluginProcessor.cpp:833-862`) edit the slice list and call `postUiChange()` without touching LENGTH,
  and `assignSliceLengths` appears exactly once in `useKit.ts` (:140), so the web `addSlice`/`deleteSlice`
  cases do the same. Place a slice by hand and that pad keeps the length it had. Recorded as a named
  residual in both READMEs, not smoothed over — it is why the AUTO SLICE text says the knob stays
  grabbable. The other residual is upstream of this step: `minGapSeconds = 0.115` (`Slicer.cpp:48`,
  `slicer.ts:63`) still fuses a fast hat run into one slice, and that is Step 25's work, not this one's.
- **The native README was sixteen checks stale, and the restamp was wrong once before it was right.**
  `cc971bb` raised the suite 190 → 206 and restamped this document while missing `native/README.md`, so
  the file that ships inside the zip was describing a build three commits old. Fixing it then produced a
  second defect: the CPU sentence was written from memory of writing it and printed 4.34 / 4.72 / 5.00 —
  none of which appears in a clean 217/217 log — while the runs at 4.56 and 4.86 went unquoted. The draft
  was not confined to a scratch file, because `native/README.md` is an archive member: the first export
  packed it, and that archive's `README.md` hashed `ed0cf4d1488eb0f6f111b7c880f2ceae683a1c75a47827d5ce10605c27167627`
  until the second export replaced it. Neither archive was installed or pushed, so the un-measured figures
  reached shipped-shaped bytes but never an artifact anyone else can reach. Both are now re-derived from
  the log census, and the band tightened from 4.3-5.8 to 4.5-5.8 to match the five figures beside it. The
  root cause is the one §8N named: a number in prose is a claim with an expiry date, and re-measuring means
  running the command, not re-reading the sentence. The root README's
  "(43 tests, 5 files)" was stale the same way before this step even started — HEAD was already 53 in 6.
- **§8N's banking pair is superseded by this step**, per its own note: 9.25 d / ~32.5 d described `05b85ef`;
  §11 now says 10.75 d banked and ~31 d remain, with Step 24's 1.5 d of its 1.5-2 d estimate banked here.
- **Deliverables re-exported from the gated tree, and the tuple agrees.** Two exports (both exit 0): the
  first because `native/README.md` is one of the six text members, the second because the CPU sentence
  above changed after it. Final: `SliceForge.exe` `83745c09b1cb3c0ce986b69f6efdce6acad79c3fb474b138736ec319652f065e`
  identical in build, `launch/` and as a zip member; the VST3 module
  `9f74b739600a474a19612d7adc8fa120de0f1be376088b9a5ff5da85a779b2d7` identical in all three; zip member
  `README.md` `b1f36d416c2126c9843c79aae6a7999de3e98a6d53a2213919b508a55632669a` equal to the file on disk;
  archive `a2cba3f37b3b5f2b4cda7b727198f773e2181589b425691d12edbe9d30b3d072`, 28,586,060 bytes, 17 members,
  **0 members containing a backslash** — §8L's separator defect re-checked on these bytes rather than
  assumed. That closes the three-way divergence §8M left open for the repo copies; the *installed* module
  is still a different artifact, because installing is an owner action. *(Closed 2026-11-21 14:42 - see the ship record at the end of 8Q; the paragraph above is the record of that day's export, not of the current tree.)*
- **Those text-member hashes are layout-dependent, and it is now measured rather than warned about.**
  `git checkout-index -f` against this index — `core.autocrlf=true`, no `.gitattributes`, confirmed empty
  `attr/` column — returned every packaged text member that this worktree holds as LF with exactly one CR
  per LF: `launch/start.ps1` 17,143 → 17,469 bytes (CR 326), `LICENSE` 34,523 → 35,184 (661),
  `native/Package/INSTALL.txt` 6,642 → 6,757 (115) and `THIRD-PARTY-NOTICES.txt` 15,448 → 15,729 (281),
  each one `bytes + LF` exactly. Those four are files this step does not modify, so content is constant by
  construction and the only variable is the smudge; `launch/start.bat`, the one member already `w/crlf`,
  checked out byte-identical at `0366b728d29b4465…`, which is the control that settles the difference as
  layout and not a stale index. The archive is written from the worktree, so a customer who extracts it
  gets the LF bytes and the published member hash matches; only someone checking a member against a
  *cloned* repo sees a different hash, and that is Step 17's `.gitattributes` decision rather than a defect
  in this tuple.
- **The bytes this step leaves.** Worktree SHA-256, and for the one `i/lf w/crlf` file the `git
  hash-object` blob id beside it, because a checkout under `core.autocrlf=true` cannot reproduce a CRLF
  worktree hash: `Slicer.cpp 1c628a3ed9afb2a09ca7ab2338838ea87bad6dad4b111383fc7757bab945895a`,
  `Slicer.h 029c1f18fc0f3fdd860e356f5bf98caf0da1c11d018a768213f09dd620df0a2a`, `PluginProcessor.cpp
  73e3eb24f7b8d591adb37abcb3aead420fe3bfced1b16c9d2017abf69156f0f6` (1278 lines, 1278 CR bytes) whose
  `git hash-object` is `cdfd6fb275fe4d787346e68b289777021954ac9e` — a SHA-1 blob id, not a SHA-256, and
  labelled as such; `SmokeTest.cpp 1bc9acc46a55e7ae723b1480ae3158abec40beab277cc6ba563fcb97f3cb6657`,
  `slicer.ts 82b1bbf9903c4b466e2225a79e87da5c339a73dbbdd718a68f1c370cde202385`, `factoryPresets.ts
  d5c21fc4a2160f77a3042724a921e20d8219887591fd75cb373ae8fc7ae500e3`, `useKit.ts
  6a2c1249c68eb3694e34bd843b78600072e8df437b29b3778c94a28720a9e3ba`, `audio.test.ts
  2a79ace1278ec60227565eb36ecf90324903c7ac030d5ed54fac97442183dfa1`, and `sliceEnds.test.ts
  c0472e5c4a7f406169d0f9b2c0dc87b401ddf6372f16811843d3b817170b4c88`, new and untracked until this commit.
- **Five live cites moved, re-pointed from the tree rather than guessed.** A script printed, for every
  cite into the eight files this step touched, what the address lands on in the worktree and what it lands
  on at HEAD, so each one could be judged on content instead of arithmetic: `AGENTS.md:101-102` (the frozen
  `VRAC_EXTRA` sites, `:1043/:1047/:1116/:1121` → `:1066/:1070/:1139/:1144`, and `"VRACPlus"` `:75` → `:73`,
  already off by two before this step), `:2405`'s take door `:983` → `:1003`, `:2414`'s
  `getRecordingLimitSeconds()` `:998` → `:1018`, `:393`'s zero-onset path `Slicer.cpp:99-102` → `:113-117`
  with `evenStarts` `slicer.ts:129` → `:169`, and §8B's `useKit.ts:341-355` → `:335-368` (the whole
  `loadSavedKit` callback, which is what that sentence was reaching for). Same edit re-pointed `:394`: it
  called `SliceMapping` dead code and asked for it to be wired — Step 24 wired it, so the sentence now says
  so. What was deliberately left alone: Step 4's `PluginProcessor.cpp:920` and `PluginProcessor.h:70`,
  which describe the tree as that step found it on 2026-11-14 and are followed by their own "Built" note —
  exempt on the precedent §8N cites at `:1999`.
- **Still owed on Step 24, and none of it is code.** The commit and push of it, the install into
  `%ProgramFiles%\Common Files\VST3`, and the listening pass that is the actual point: eight separate hits
  instead of eight overlapping copies of the whole take. The checks prove the lengths are right and the
  rendering is finite and in the ceiling; they cannot prove the hit reads as one hit.

## 8P. The groove bank: sixty named patterns, and the grid write that makes a two-bar groove audible
(recorded 2026-11-19)

The ask was three things at once — every preset pattern must span all 32 steps, add a lot more genres,
and let the player click through named variations inside a genre. It became one native-only feature: the
six factory presets are gone and a 60-row bank stands in their place. There is no web mirror because
`src/` has no sequencer at all, which `AGENTS.md` calls out as the exception to the both-editions rule.
Four owner decisions fixed the shape before any code: a groove writes **pattern + length + feel** (not
just the grid), navigation is **genre submenus plus a cycle item** (no new toolbar widget), voicing is
**separate** — a groove never re-voices the pads unless the player asks — and the bank is **20 genres × 3
variations**.

- **The data is one new file.** `native/Source/GrooveBank.cpp` (654 lines, untracked until this change):
  sixty `constexpr GrooveLane[]` tables, twenty `constexpr GrooveVoice[]` tables, and `kGrooves[]` at
  `:586` behind `grooveBank()` at `:650`. Genre-major and contiguous, three rows per genre, so the menu
  groups by walking runs instead of re-reading strings: HOUSE, TECHNO, TRAP, BOOM BAP, DRUM & BASS,
  UK GARAGE, AFRO, DANCEHALL, BREAKS, ELECTRO, REGGAE & DUB, FUNK & SOUL, LO-FI, HARD & CLUB, DISCO,
  TRIP HOP, POP, ROCK, EBM & INDUSTRIAL, LATIN — named accordingly (`HOUSE CLASSIC / DEEP / DRIVE`,
  `TRAP DARK / ROLLER / DRILL`, `AFRO 12 / AFRO HOUSE / AMAPIANO`, …). A lane mask is two adjacent
  16-character literals, which the compiler folds into one 32-character string: the grid is 32 steps, and
  a literal long enough to hold it is what makes bar two exist at all. The alphabet is documented beside
  the struct (`Sequencer.h:76-83`) and every character in it is pinned by a check, so a typo fails a test
  instead of hiding as a rest. Polymeter is per-lane (`GrooveLane::loop`) and the bank keeps short-cycling
  lanes — AFRO 12's hat on 12 and clap on 6 (`GrooveBank.cpp:193-195`) and AFRO HOUSE's hat on 12 (`:201`) —
  and "the bank still contains a lane that cycles short of the grid" is a check in its own right. Division is
  **4 for all sixty rows**, stated in the file's own preamble (`GrooveBank.cpp:5-15`) with the reason:
  32 steps is exactly two bars only at sixteenths, so a genre whose two bars are not four beats each cannot
  keep the full-grid promise — and triplet and shuffled feels therefore come from `swing` and the `<`/`>`
  pushes, not from a division that would move the bar line.
- **"Spans 32 steps" is two claims, and only one of them is in the data.** `wrapStep` (`Sequencer.cpp:25-30`)
  folds every step into the lane's loop length, and a lane with no loop of its own takes the document's
  `length` (`:204` is the call) — so a 32-step mask behind a 16-step grid is inaudible from step 16 on. That
  makes the promise a *grid* write, not a *pattern* write, and `applyGroove` does it at `Sequencer.cpp:632`
  (`data.length = kSeqSteps`, in the function at `:623`) together with the genre's tempo, division and
  swing — BPM clamped into `kSeqMinBpm…kSeqMaxBpm` so a groove can never ask for a value the slider cannot
  show. `applyGroove`'s declaration comment (`Sequencer.h:126-130`) states the rule where a caller will read
  it: the four grid fields are what make a genre a genre, and a second bar written behind a 16-step window is
  data nobody hears.
- **The API, all in `Sequencer.h`:** `grooveBank()`, `isGrooveStepChar()`, `grooveLabel()`
  (`Sequencer.cpp:567`), `grooveGenreRun()` (`:540`), `nextGrooveVariation()` (`:572`),
  `grooveVoicing()` (`:582`), `makeGroovePattern()` (`:596`) and `applyGroove()` (`:623`). The genre run
  compares with `std::strcmp` on purpose — in JUCE 8.0.4 `juce::CharacterFunctions::compare` is a template
  over char-pointer types and will not bind two `const char*`, so the claim is not available as a one-liner.
- **The UI is one button that changed its name.** `PAT PRESET` is now `GROOVES`
  (`SequencerPanel.cpp:1193` geometry, `:331` click, tooltip `:332-335` states the size and the
  length/feel behaviour). `showGrooveMenu()` (`:817-871`) adds one submenu per genre run, ticks the row
  that was last applied, then `Next <GENRE> variation`, `Match <GENRE> sounds`, and the two
  `.vracpattern` items that used to live on the old menu — save and load are untouched. Cycle (`:892`)
  and Match SOUNDS (`:903`) are gated on a pick: inventing a genre the player never chose, or moving their
  sounds because a menu opened, are both the silent-compensation pattern the owner has rejected.
- **What this deleted, stated plainly.** `PatternPreset` (HEAD `Sequencer.h:70-81`), `patternPresetName`,
  `makePatternPreset`, `SequencerPanel::showPatternPresetMenu`/`applyPatternPreset` and the two smoke
  checks that drove them (`factory pattern preset contains polymeter and notes`,
  `pattern presets save and load correctly`) are gone — the bank replaces them, so `GrooveBankTest.cpp:429`
  now proves a short-cycling lane exists and `:419` proves every one of the sixty survives a save/reload.
  The default kit's own opening pattern is `makeDefaultPattern` (`Sequencer.cpp:655`), which never came
  from the preset system and still produces it byte-identical to HEAD. No parameter was added, so the host
  census is unchanged and the host test still proves it.
- **Native gate.** `PASS - 237/237 checks passed`, exit 0; layout `32 checks`, exit 0 (was 28); host
  `parameters: 350 = 348 layout + Bypass + Program`, `seq.pattern: host id 825878785, index 347, 4 steps`,
  `PASS VST3 host test`, exit 0; `ctest` `100% tests passed out of 3`, exit 0; build exit 0 run
  sequentially. The bank contributes **19 check sites** (`^ *check (` census, the same anchor §8O uses)
  that print **22 rows**: the slot-isolation site runs once per pattern slot. SmokeTest's own sites went
  212 → 210 (−2 for the preset checks, the hook itself adds none) and LayoutTest 31 → 35. Object stamps
  close the loop at seconds against the restored sources: `Sequencer.obj` 20:38:42 ≥ 20:38:25,
  `GrooveBank.obj` 20:43:29 ≥ 20:43:09, `SequencerPanel.obj` 20:41:15 ≥ 20:40:32,
  `GrooveBankTest.obj` 20:36:55 ≥ 20:36:41, `LayoutTest.obj` 20:28:25 ≥ 20:26:45,
  `SmokeTest.obj` 20:19:11 ≥ 20:14:33. Gated binaries: Standalone `SliceForge.exe` 24,254,464 B
  `0e2a0d1bc6f01c01ea7c0ae160409e99eec110b54a898e24afb7375bf4a2127c` and the VST3 module 24,564,736 B
  `a1828f3eca6e1ad80cf368599b13238dcb77f53da8aec129344fe52a6bb9f1fa`. Worktree SHA-256 of the sources,
  beside the `git hash-object` blob id, because four of these files are `w/crlf` and a fresh checkout
  cannot reproduce a worktree hash: `GrooveBank.cpp 32840417ef872d1df267…` (blob `2fa288e8af81d119109e…`),
  `GrooveBankTest.cpp f9b56d26f9424ae87a2d…`, `GrooveBankTest.h 5856a7e3c2a89207ea93…`,
  `Sequencer.cpp 1f4c5bc60e491808ee05…`, `Sequencer.h 2c12792bb5843d34ea6f…`,
  `SequencerPanel.cpp caa40864555ce6d8beae…`, `SequencerPanel.h 8b8d8e94d4e3c0f61d0c…`,
  `SmokeTest.cpp 0ae458f8a2acb9b1c727…`, `LayoutTest.cpp 1ed0cc5e8ee6b2ed80c7…`,
  `CMakeLists.txt ac9d8c32e86695c0ae58…`.
- **Five falsifiers, applied and reverted on the gated bytes**, each count read out of that probe's own log
  (script in `native/build/`, which is not a proof carrier — that is why this paragraph exists). **A**
  deleted `data.length = kSeqSteps` → smoke `235/237` exit 1 with the grid write (`:352`) and the audible
  check (`:535`) both red, plus one layout red (`LayoutTest.cpp:385`, `[slot 0 length 16 pattern=bank]`,
  exit 1); the length write is therefore covered at the audio seam and not only by the data check. **D**
  shortened a 32-character mask to 30 → smoke `236/237`, red only on `:163` (`pad 0 mask is 30 steps, want
  the full 32`). **E** emptied bar two of `houseClassic`: run against the *weak* audible check it reddened
  `:236` alone and left `:535` green — the false green below — and the same probe re-run after the window
  fix reddened both, `235/237`. **B** made NEXT VARIATION re-apply the same row → exactly one red, the
  layout cycle check (`LayoutTest.cpp:409`, `[index 0 same genre pattern=bank]`), with the picker, the
  menu-to-document and Match SOUNDS checks all still `ok`. **C** relaxed the Match SOUNDS guard so unvoiced
  pads got written too → exactly one red (`LayoutTest.cpp:456`, `[4 of 4 voiced pads landed, 12 wrong]`).
  B and C are panel-handler mutations, so only the layout seam reaches them and their probes ran layout
  alone — neither log is a smoke result, and the smoke voicing checks call `grooveVoicing()` directly and
  were never under either mutation. All five reverted: each log carries the restored file's SHA-256, and
  `probeE2-red.log` is the single log that stops before its own revert line, so that one restore is proven
  by the tree instead — `GrooveBank.cpp 32840417ef872d1d…`, `Sequencer.cpp 1f4c5bc60e491808…` and
  `SequencerPanel.cpp caa40864555ce6d8…` each still hash to the value every revert line names. The clean
  re-gate afterwards is the run recorded above.
- **Two lessons, both paid for in a false green.** The first version of the audible check only asked
  whether the playhead reached the upper half of the grid, and stayed **green on a bank whose second bar
  had been emptied by hand**: `scheduleHostBlock` scans two steps past both ends of the block
  (`Sequencer.cpp:326-327`, and the internal clock repeats the window at `:362-363`) and `wrapStep` folds
  what it finds back into the pattern, so the block that reports step 31 is the same block that carries the
  next loop's step 0 hit. A trigger carries no step, so attribution has to be by window, and `>= 16` was not a
  window. It is now `17…30` with swing and every nudge stripped first, because a swung odd step in bar one
  also crosses the middle. The census that proved it is in `native/build/`: one boundary block, playhead
  31, one trigger at offset 462. Second: two of the five checks above were red **before** they were
  strengthened — `:163` accepted any mask *at most* the grid, so a 30-step lane passed, and `:236` accepted
  bar two as a byte-for-byte copy of bar one. Both clauses were added, and both probes re-run. The rule is
  written into `AGENTS.md` as the fourth smoke-suite idiom, because it cost one green run here and will cost
  the next author one too.
- **UI leg, and what no test can see.** JUCE exposes no accessor for a `PopupMenu`'s items, so the menu
  cannot be read back headlessly. The layout test therefore drives the three handlers through the panel
  (`applyGrooveForTesting`, `cycleGrooveForTesting`, `matchGrooveSoundsForTesting`,
  `lastGrooveForTesting` in `SequencerPanel.h`) and asserts the result on the processor's own document —
  which also covers the undo and commit path. It does **not** cover the grouping code in `showGrooveMenu`
  (`:817-871`: the submenu build at `:827`, the cycle item's label at `:839`, the id dispatch at
  `:860-862`, item ids `kGrooveIdBase = 1000` with span 512 at `:159-162`): no headless test has ever
  rendered the menu tree, and the screenshots cannot show it because the menu does not exist until a real
  click opens it. The layout run that gated this step rewrote the five `native/shots/sequencer*.png`, which
  now carry the GROOVES label on the toolbar; no new shot was pinned, because a pinned render of a menu
  nobody drove would prove less than the handler checks.
- **Docs this step had to catch up.** `README.md:263-268` and `native/README.md`'s sequencer bullets said
  ten tools ending in `PAT PRESET` with "six built-in pattern presets … FOUR FLOOR, BOOM BAP, HALF TIME,
  TRAP, AFRO 12, BROKEN"; both now describe GROOVES and the bank, and every 217/28 total in both files is
  restamped to 237/32 from the run above, including `native/README.md`'s at-a-glance row and its
  architecture listing (which gained `GrooveBank.cpp` and `GrooveBankTest.*`). The arithmetic is worth
  stating because it reconciles two numbers: the 217 §8O stamped there is still true of the tree §8O gated,
  and this step is `217 − 2 + 22 = 237` — the preset checks that died and the rows the bank prints.
  `AGENTS.md`'s "three idioms of the native smoke suite" became four, with the scheduler-attribution trap as
  the new one, since it cost a green run here and will cost the next author one too. All four doc files are
  `i/lf w/lf` and stayed pure LF under the edits (CRLF pairs: 0 in each), which matters because
  `native/README.md` is one of the six text members of the archive: §8O publishes it as member
  `b1f36d416c2126c9843c79aae6a7999de3e98a6d53a2213919b508a55632669a`, and after this step it is
  `1d9c87509c3145d20cc79a8973cadb9cb69aa57eb3de4d9cedb6793c365ec56c` (533 lines) — so the re-export has to
  restamp that member rather than reuse the Step 24 value. Its siblings from the same edit:
  `README.md e917618097267c1d26a623d3f574bea7f24e3a46265536845711287e551c2975` (649 lines) and `AGENTS.md
  6689372ba70ad9561640632abb300d898b5a29b2658dae25b1b6c05dc2470284` (231). Being `w/lf`, all three are
  reproducible from a fresh clone, unlike the four `w/crlf` sources above; this section deliberately records
  no hash of its own file, because editing it would invalidate what it says.
- **Owed after this, none of it code.** The owner's commit and push — this step and Step 24 are both still
  uncommitted at `c966a71`, 30 dirty paths (26 modified, 4 untracked) — then the `launch/` and zip re-export
  from the committed tree. Measured now: `launch/` carries the Standalone `83745c09b1cb3c0c…` and the module
  `9f74b739600a474a…`, while the gated build is `0e2a0d1bc6f01c01…` / `a1828f3eca6e1ad8…`, so the archive in
  the repo predates this change and no green test has ever described it. *(Superseded 2026-11-21: the re-export and the install both landed that afternoon; the ship record at the end of 8Q carries the tuple that now agrees in all four places.)* Then the install into
  `%ProgramFiles%\Common Files\VST3`, and the listening pass: sixty rows pass every structural check, and
  whether HOUSE DEEP reads as house and DILLA reads as Dilla is a judgement no check here makes.
  One disclosure for whoever reads the diff: the README that `9659b78` committed two steps ago says
  `217/217` and still names `PAT PRESET` with "six built-in pattern presets", and its own commit subject
  claims every number on it was re-measured. Both are true of that commit — the bank was not in the tree
  when it was written — so the `237/32` restamp above is not a correction of the peer's measurement, it is
  what the tree now needs. It lives only in the working tree: a `git checkout README.md` while cleaning up
  would silently put `217/217` and `PAT PRESET` back and no test would notice.

## 8Q. Step 25 phases 1, 2 and 4: the auto-slicer gets a ruler before it gets an opinion (2026-11-21)

The detector had no ground truth. Its acceptance was a listening session, and the one number attached to it - "pull the
reported onset back 6 ms" - was folklore nobody could say yes or no to, because nothing measured the error's *sign*. Phase 7's
FFT idea was queued behind the same missing ruler: without a before number there is no after number, so the change could not
be proposed, only tried. Step 25 builds the measurement first and touches the detector not at all.

### What landed

Six tuning literals moved out of the two detectors into one header per edition, mirroring the `ImportLimits` discipline:

| File | sha-256 (16) | provenance |
| --- | --- | --- |
| `native/Source/Engine/SlicerTuning.h` | `583caff534cc0267` | worktree LF, 43 lines - the number is reproducible from the commit |
| `src/audio/slicerTuning.ts` | `7301135a96ab355b` | worktree LF, 24 lines - ditto |
| `src/lib/__tests__/slicerTuning.test.ts` | `5d591d545a39388c` | worktree LF, 49 lines - ditto |
| `native/Tests/SlicerScoreTest.h` | `9132642c0da298c4` | worktree LF - ditto |
| `native/Tests/SlicerScoreTest.cpp` | `2a807803dbfc8bad` | worktree LF, 734 lines - ditto |
| `src/audio/slicer.ts` | `8f6ff72531ebc651` | worktree LF - ditto |
| `native/Tests/SmokeTest.cpp` | `dbad628bf5fa5011` | **blob**, from the staged index (`git show HEAD:native/Tests/SmokeTest.cpp \| sha256sum`, 171,613 bytes). The worktree file is `w/crlf` and hashes `0e817bb59c9858a1` - publish the blob, never that one. |
| `native/CMakeLists.txt` | `acce43ccb3ae0a03` | **blob**, same rule; worktree `2339f75b08c7d807` is the CRLF view (8,104 bytes as a blob). |

`SLICEFORGE_SOURCES` lists `.cpp` files only, so a new header needs no CMake entry; the scorer needed one line at
`Tests/SlicerScoreTest.cpp` in the smoke target's `target_sources`, one `#include`, one callback next to the groove bank's,
and `main()` became `main (int argc, char** argv)` to read the bless flag - `argv`, not the environment, so the switch cannot
be set by an inherited shell variable and `ctest` (which passes no argument) can never reach it.

The extraction was proved behaviour-preserving before anything was built on it: a temporary full-precision probe printed every
onset and strength of the click fixture, and the 9-decimal witness hashes to the same value on both sides of the edit
(`d3480b28…`), with the whole smoke log byte-identical apart from the CPU line. Types were held on purpose - `float` for
base/span/floor, `double` for the gaps and the bias - so the expression trees are unchanged, not merely equal.

### Measured on this tree

`PASS - 253/253 checks passed` exit 0 (`native/build/s25-final-smoke.log`), layout 32 checks, host `350 = 348 layout +
Bypass + Program`, `ctest` `100% tests passed out of 3` exit 0, `npm test` 68 tests across 8 files exit 0, `npm run build`
exit 0. CPU printed 4.45% of one core. The suite grew **237 → 253**, and the sixteen are constant: eight fixture-state gates
plus eight scorer controls, none of which depends on how many takes are on disk, so blessing the real renders will not
restamp the total again.

### Falsifiers: every one of the sixteen was driven red

Synthetic material, kept entirely in `native/build/s25-synth/` (gitignored) so no generated click track can be committed into
the folder that ships the factory library: a 12-click 8-second wav plus a marks file whose reference is exact by construction.
A blessed table was produced from it, then each guard broken in turn. Every leg restored the file it touched and the run
returned to green at the same baseline hash (`650866e86d5dffbb` before, after).

| Leg | state forced | what reddened |
| --- | --- | --- |
| baseline deleted | table absent with rows on disk | `the slicer baseline is present for the takes on disk`, alone |
| baseline truncated to `{ "schema": ` | unparseable | `…is readable` **and** `…lists exactly the fixture take/sensitivity pairs` - an unreadable table blesses nothing, so both are true |
| `tunings.kFloorRatio` 0.012000 → 0.012001 | blessed under other tuning | `…blessed under the tuning this binary was built with`, alone |
| blessed `recall` 1.0000 → 0.9000 | drift down | band, `[synth-clean@0.20 recall 1.000 vs blessed 0.900 (tol 0.020)]` |
| blessed `biasMs` -10.240 → -30.000 | **improvement** | band - the symmetric rule working: getting better is also a change to read |
| blessed `fpPerMin` 0.00 → 40.00 | false positives dropped to silence | *green the first time* - see the defect below |
| a second marks file, not blessed | inventory grew | `…lists exactly the fixture take/sensitivity pairs`, naming all three rows |
| marks file whose `take` ≠ file name | bad marks | `every slicer marks file parses and every wav it names decodes` + `BLESS REFUSED` |
| marks file naming a missing wav | bad marks | same check, `wav "synth-ghost.wav" did not decode` + `BLESS REFUSED` |
| a 20-onset take at sens 0.20 (target 17) | capped | `no fixture take asks for more onsets than its sensitivity can return` **and** `BLESS REFUSED: refusing to bless a table that cannot pass` |

The tuning literals themselves then went through the same treatment end to end, one literal per rebuild, against the blessed
table - so a red had to come out of the whole chain, tuning → detector → scorer → band:

| Mutation | tuning guard | band | what the numbers said |
| --- | --- | --- | --- |
| `kFloorRatio 0.012 → 0.0` | RED | RED | `detN` 17/33/49 = exactly each sensitivity's target, `recall` still 1.000, `fpmin` 37.5/157.5/277.5. **Recall alone calls this perfect.** |
| `kMinGapSeconds 0.115 → 0.5` | RED | RED | `recall` 0.583 (7 of 12), `fpmin` 0.00. **The false-positive column alone calls this perfect.** The two halves are complementary, which is the argument for keeping both. |
| `kDedupGateSeconds 0.07 → 0.0` | RED | green | no metric moved - see finding 2 |
| `kOnsetBiasSeconds 0.006 → 0.0` | RED | RED | `medAbsMs` 10.24 → 4.24, `biasMs` -10.24 → -4.24 - see finding 3 |
| `kTargetSpan 46 → 20` | RED | green | the 12 clicks still fit under the smaller target; the tuning block in the baseline is what catches this class |

Four pre-existing checks in `SmokeTest.cpp` - `the detector finds the four clicks it was given` and its three companions - went
red under the floor and min-gap mutations too, so the extraction did not weaken the constraints the suite already carried.

**The defect the matrix caught, in the scorer rather than the engine.** The false-positive band was written as
`|got - blessed| > max(fpFloor, blessed * (fpRatio - 1))`. With `fpRatio = 2.0` the relative slack equals the blessed value
itself, and since the metric cannot go below zero, `blessed - got ≤ blessed` - so the lower side of that band is
arithmetically unreachable and a blessed 40/min against a scored 0/min passed as a 100 percent improvement. The band is now
asymmetric by construction (`fpViolates`): an absolute `fpFloor` per minute of slack downward, because a bigger drop than
that is a real change in what fires, and a relative allowance upward, because 40 → 42 on a dense kit is not a finding. The
rule is pinned in code by an eighth control that asserts all four corners of it, so it cannot silently become one-sided again.

### Three findings, none of them fixed here

1. **`kDedupGateSeconds` cannot bind.** The selection pass keeps accepted onsets `kMinGapSeconds` apart in hops; the bias is a
   constant and cancels out of a difference; `0.07 < 0.115`. So the dedup test is dominated and unreachable as written, which
   is why its mutation moved nothing - confirmed by derivation and by measurement, not by a single run. Left in place; the
   comment in `SlicerTuning.h` now states the relation instead of promising a behaviour that never happens. It becomes the
   binding constraint the moment it is raised above the min gap, which is a tuning decision for the owner, not a cleanup.
2. **On clicks the -6 ms correction overshoots.** The corrected detector reports 10.24 ms *early* against a click whose true
   onset it otherwise finds every time; withdrawing the correction cuts the median absolute error to 4.24 ms. This is not yet
   a verdict on drums: a click has an instantaneous edge where a kit hit spends milliseconds reaching its peak, and the
   correction was written for that rise. It is the first signed measurement of a claim that had only ever been asserted, and
   it says the number is at least 6 ms too strong in the direction it moves.
3. **`native/shots/*.png` is not a deterministic render.** Two consecutive `SliceForgeLayoutTest.exe` runs of the same binary
   produced different bytes for `sequencer-large.png` (`b64d206f…` then `95cc8fd8…`) while the other nine reproduced exactly.
   A shot diff therefore proves nothing about a code change, and any ctest run leaves that one file dirty. Five shot files are
   dirty against HEAD at the time of writing, from the sequencer work in the tree plus every layout run since.

### Doc carriers and the state of the gate

`AGENTS.md` gained the rule *Auto-slicer tuning is one number per edition*, including both traps above, and
`native/README.md` gained *Auto-slicer ground truth* plus the restamped totals. Present-tense numbers restamped from the run
above: `README.md`'s suite table (`253/253`, `68 tests across 8 files`) and `native/README.md`'s at-a-glance row, its
*Current result* paragraph, its arithmetic sentence (`217 − 2 + 22 = 237`, then `237 + 16 = 253`) and its `Tests/` tree
listing. Historical run records in earlier sections were left as written, dated, and annotated rather than rewritten -
including this file's own self-recorded line count at the 8H section, now marked *was 2476 that day*. **Those four 253s
were the correct stamp for commit `7a92a6d` and are superseded by the 256 recorded at the end of this section, and in turn by the 257 recorded below.**

Nothing here is neutral about line numbers: the Step 25 entry grew by three lines and the two status annotations by one more,
so every `:NNNN` cite below roughly `:712` in this file sits four lines lower than the section that printed it. That includes
the count in the sentence before this one. `docs/next-steps-plan.md` has been declaring its own cites dead for eleven sections
now; this is one more named shift, not a new class of problem, and it is why the step entries above cite `SlicerTuning.h`
by symbol rather than `Slicer.cpp:47-49` by number.

At the `7a92a6d` gate the green was on the bytes as they stood: the last native source to move was `SlicerTuning.h` at
11:46:36 (a comment), the full build finished after it, and `s25-final-smoke.log` printed `253/253` at 11:48:58. The doc
edits after that touched no translation unit, so no re-gate was owed by them - but any source edit does owe one, and the
number to compare against is in that log, not in this paragraph.

### The completion audit: three promised checks that did not exist

The plan asks for more than the first commit shipped. Diffed phase 2 (steps 5-13) against `Tests/SlicerScoreTest.cpp`
clause by clause, three promises had no witness, and each was written as an assertion rather than documented away:

1. **Precision was not a column.** Step 7 lists `p` among the per-row metrics, and the row printed none. A detector can raise
   its slice count by firing at nothing, and without `tp / detN` the table cannot tell that from an improvement. The corners
   are now pinned: a silent detector leaves it undefined, one that fires at nothing scores 0.000, a clean set is 1.0, a
   doubled set is 0.5, and twelve-of-thirteen is that exact fraction rather than a rounding.
2. **`markProvenance: midi-note-times` was a claim, not a check.** Step 10 puts the clip's own `.mid` beside the marks JSON
   precisely so a re-render can be verified against the same notes; the scorer read the provenance string and moved on. It
   now requires `fixtures/marks-src/<take>.mid`, parses it (tempo map → ticks → seconds), and refuses a marks file whose
   onsets differ from that file by more than a microsecond, or that has no `.mid` at all. Without this the reference is a
   transcription somebody typed - the exact failure the schema exists to prevent. The reader itself is tested over a standard
   MIDI file built in memory, so the rule is gated today and not only when the renders land.
3. **Zero references were only exercised on hand-written vectors.** Step 19 names a *sweep*. It is now a real one: six seconds
   of 100-4000 Hz sine ramped 0 → 1, energy that only ever rises and therefore contains no impulsive onset, pushed through
   `detectSlices` and scored against nothing. The scorer half is the assertion. The detector half produced a number worth
   keeping, so the row prints as `info SLICERSCORE ctl-sweep …` on every run: **33 slice points on a swell**, 330 a minute.
   A measurement that is only visible inside a failure message is a measurement nobody has - which is why that line exists,
   and why the harness's detail argument does not.

Three falsifiers, one per new check, each on the file restored by hash afterwards (`9d2fc9c66a441bfd…` before and after every
one of the three):

| Mutation | Outcome |
| `precision` additionally guarded by `refN > 0` | RED `254/256` - the precision corners *and* the swell row |
| a zero-reference row handed `recall = 1.0` | RED `254/256` - the no-reference check *and* the swell row |
| marks-src tolerance 1 µs → 50 ms | RED `255/256` - "an onset edited 20 ms late is refused by it" |

Green on the restored bytes, which are the bytes this commit carries: `PASS - 256/256 checks passed`, exit 0, CPU 5.26% of one
core (`native/build/s25b-smoke3.log`), with the census closed at seconds - `SlicerScoreTest.cpp` written back at 12:19:47, its
`.obj` compiled 12:20:08, `SliceForgeSmokeTest.exe` linked 12:20:09. Layout was deliberately not re-run again: the one thing it
does that this tree cares about is rewrite `native/shots/*.png`, which the owner gated out of the commit, and finding 3 above
records that those bytes are not deterministic anyway. Totals restamped to 256: `README.md`'s suite table, `native/README.md`'s
at-a-glance row, *Current result*, the arithmetic sentence (now `237 + 19 = 256`), its run count and CPU list, the *Auto-slicer
ground truth* section's new `p` column and `marks-src` rule, and the `Tests/` tree listing.

### The midi writer bug, and what digital silence gave for free

Phase 3 needs nine `.mid` files, so the generator was written before any audio existed and validated against the only reader that
matters: `midNoteOnsets`, the production path. The first nine files were refused with *"midi carries no tempo map, so its ticks cannot be read as seconds"*,
and the fault was in the generator rather than the scorer - the tag was written `Mtrk`, so the reader named the
consequence (no tempo map) instead of the cause (every track skipped). `MidiFile::readFrom` compares
the tag against the four bytes `"MTrk"` exactly, **silently skips** any chunk that is not it, and still returns `true`. So a
format-1 file with a conductor track and eight data tracks written in the wrong case parses "successfully" holding zero tracks.

Two things followed. The generator now writes `MTrk`. And the scorer got a guard, because the failure mode is a silent accept
rather than an error: when `readFrom` returns true but `getNumTracks() == 0`, `midNoteOnsets` reports *"midi parses but holds no
MTrk chunks, so every track in it was skipped"* instead of handing the comparison an empty list. Without it, a marks file whose
midi was unreadable would score every take against an empty truth and agree with itself - the `refN > 0` rule one level up, since
the provenance check has to be able to fail loudly when there is nothing to compare. The proof is the round trip, not the file
count: the one trio put through the real reader (`kick-quad`'s midi, marks and a stand-in wav) came back with no load problem and
its onset list matched the marks JSON, which is the evidence the other eight share before any of them is committed.

Then the stand-in wav told us something the rendered material was supposed to. It is digital silence, and it scored
`detN` 17, 33 and 49 at sensitivities 0.20, 0.55 and 0.90. Those are not detections; they are `targetAt (sens)` -
`kTargetBase + kTargetSpan*sens`, rounded. **The detector hands back the sensitivity knob's count for a file that contains no
signal at all**, because `kFloorRatio` is a fraction of the file's own peak and a peak of 0 makes the floor `0 >= 0`, which every
candidate passes.

That is recorded as a **characterisation test**, not as approved behaviour: `a dead take still yields the knob's target count:
the relative floor is degenerate at peak 0` prints `detN` and `target` for all three sensitivities and asserts the identity, so
the count stays pinned to the knob rather than to the material, and whoever gives silence a real answer has to move that name on
purpose. It was falsified with the actual fix rather than a fake mutation: a zero-peak early return in `detectSlices` took the
three rows to `detN=0` and produced `FAIL - 256/257` with that one name and no other red. Restored by
`git checkout -- native/Source/Engine/Slicer.cpp`; `git hash-object` returns `13328bb9dce3f6498f395627a87d94140dde8305`, equal to
the HEAD blob, with `git diff --stat` empty. `sha256sum` of that worktree path is **not** a publishable number: the file is
`i/lf w/crlf`, and the probe's Python write had flattened it to LF, so the *bytes* differ from the pre-probe worktree while the
*content* is the committed one. The fix itself is ten lines and is **not in this commit** - Step 25's rule is that the detector
does not move until the ruler has real material on it.

The same rows carry a reading rule for the metric. On a take whose references are 300 ms apart, a detector firing on *nothing*
matched 8 of 16 references inside the +/-50 ms window: `recall=0.500`, and `p=0.242` bought by firing 33 slices. Half that score
is window geometry - a 100 ms window over 300 ms of spacing covers a third of the timeline by chance, and the 16th-note fixtures
(75 ms apart) are covered outright. So `recall` on dense material is nearly meaningless and the columns that survive are
`recall25`, `p`, `fpmin` and `biasMs`. Nothing in the scorer changes; this is why `recall25` was never optional, and it is the
first thing to state when a blessed row is quoted as a quality claim.
The guard is exercised by its own control rather than trusted: the suite writes a one-track file through JUCE, flips every
`MTrk` tag in the bytes to `Mtrk`, and requires the reader to refuse *and* to name the reason
(`a midi whose track tag is not MTrk is refused by name instead of parsing as empty`, printing
`tampered=yes cleanReads=yes brokenReads=no guardNamed=yes`). Deleting the guard leaves `brokenReads=no` - the tempo check still
refuses - but `guardNamed=no` and the message falls back to the tempo one, and the suite prints `FAIL - 257/258` with that single
name. So the check is about the *named* refusal, not about a bool, and it cannot be satisfied by an unrelated failure. That file
carries uncommitted work, so it was restored from the bytes read at the top of the probe and verified by SHA-256 equality
(`0474a9bd…`), never by `git checkout`.


Gate on the bytes this commit carries: `PASS - 258/258 checks passed`, exit 0, CPU 4.91% of one core, no compiler
warnings (`native/build/s26-final.log`, which opens with the blob census: `git hash-object` equals `git rev-parse HEAD:` for
`native/Source/Engine/Slicer.cpp` (`13328bb9dce3`) and `SlicerTuning.h`, and differs only for the two files this commit changes). The census
closed at seconds - cpp 13:26:16 < obj 13:28:00 < exe 13:28:02 - so the gated binary is built from the restored `Slicer.cpp` (`3d6829f220ca`) and the current scorer TU, which is
what proves both mutants left the executable and not merely the tree. The slicer file now owns 21 of the 258: eight on fixture state
plus thirteen controls. Totals restamped in `README.md`'s suite table and `native/README.md`'s at-a-glance row, *Current result*,
the arithmetic sentence (`237 + 21 = 258`), its run count and CPU list, and the `Tests/` listing.

### The ship record: the tuple agrees in all four places (2026-11-21 14:42)

The re-export, the screenshot restamp and the machine-wide install were gated on the owner and are done. Every number below
is from the runs named with it, and the source tree was provably clean first: `git status --porcelain -- native/Source
native/Resources native/CMakeLists.txt src` printed nothing, and HEAD was `f61087b`, so what was compiled is what is committed.

- `cmake --build native/build --config Release --target SliceForge_VST3 SliceForge_Standalone --parallel 2` exit 0
  (`native/build/ship-build.log`); it linked `SliceForge_SharedCode.lib`, the Standalone and the VST3 bundle.
- `ctest --test-dir native/build -C Release` **3/3, exit 0** (`ship-ctest.log`): smoke 9.71 s, layout 33.15 s, host 1.05 s.
  The host leg loads the module built above, so the shipped VST3 was exercised by the 350-parameter check, not just compiled.
  *(Load it as weak as the mechanism: `HostTest.cpp:33-38` opens the **build-tree** bundle, and that copy merely equalled
  `launch/`, the archive member and the install by hash. The legs that passed the installed and the extracted bundles as
  `argv[1]` are 21:45's section, five hours later.)*
- `SliceForgeLayoutTest.exe` exit 0, `PASS layout screenshot: ... (32 checks)` (`ship-layout.log`), which regenerated
  `native/shots/`. Five files moved: `sequencer-dark`, `sequencer-inspector`, `sequencer-inspector-dark`, `sequencer-large`,
  `sequencer-light`. Finding 3 still holds, so `sequencer-large.png` is expected to differ again on the next run.
  *(Held until 21:53, then fixed: the harness wait is 400 ms and seventeen runs of the rebuilt exe wrote the committed
  bytes without a single drift - see the 21:53 section. Nothing here was re-rendered as a result.)*
- `cmake --build native/build --config Release --target launch` exit 0: `launch/ refreshed (Release, v1.0.0)`, zip 27.3 MB.
- **The tuple.** VST3 module `641cde970ba49a5e...` at 24,564,736 B, identical in the build tree, in `launch/`, and read back
  out of the archive into a scratch directory; Standalone `25ca095cce9f792a...` identical in all three as well. The archive is
  `d2e920f5002cca83acc6cdbbbc0d215ab4c9a8be851a67a329fbf9d025bc64ae`, 28,604,732 bytes, 17 members, **0 members containing a
  backslash**, `unzip -t` exit 0, every entry stamped 1980-01-01 by the pinned clock.
- **The install.** `launch/start.ps1 -InstallOnly` exit 0 through one UAC prompt: it copied to `C:\Program Files\Common
  Files\VST3\SliceForge.vst3`, read the module back against `launch/` twice, and the independent re-hash agrees
  (`641cde97...`). Its mtime is 14:42:05 because the script restamps *after* both hash checks, as designed, so a host that
  keys its plug-in cache on the date sees a new build as new. No `SliceForge.vst3` remains in `%USERPROFILE%\Documents\VST3`
  or `%LOCALAPPDATA%\Programs\Common\VST3`, and nothing was retired - the only copy of the previous build was the one in the
  destination, which the script overwrites by design.

What that closes: the module Ableton loads had been `0f7927ab560599f6...` at 24,527,360 B - a build no green test in this repo
has ever described, and the reason this plan kept three hashes apart. All four positions (build, `launch/`, archive, install) are
now one artifact, and the ctest trio above ran against it. The listening pass is still the owner's.

### Tier 1: the ruler stopped being idle (2026-11-21 16:40)

Every green run since the scorer landed printed `takes=0 baseline=absent`, and it took the ship commit
to make that worth reading: the blessed table the falsifiers were run against lived in
`native/build/s25-synth/fixtures/`, which is a gitignored build folder. So the committed tree gated
nothing - `fixtures/marks/`, `fixtures/marks-src/` and `fixtures/slicer-baseline.json` did not exist in
it - and the paragraphs in AGENTS.md and the native README that describe those files were describing
the scratch directory. The 258/258 at 14:42 was real for everything except the fixture half, where it
was eight greens that had nothing to compare.

- **What is in the repo now.** `fixtures/tools/synth-takes.cjs` (blob `7641e8dd0e05a610`), two wavs
  (`synth-clean` `15dbd719330faa64` twelve hits at 500 ms, `synth-16ths` `ae66267a65216307` thirty hits
  at 200 ms), their two marks files (`ea50752182f719b7`, `56135f34e30a9963`) and
  `fixtures/slicer-baseline.json` (`16b0772815674998`, six rows). `node fixtures/tools/synth-takes.cjs
  --check` re-renders all four payload files in RAM and compares against the bytes on disk; it exits 0,
  and the two wav digests it prints are the blob digests above, so the audio is reproducible from the
  repo rather than asserted by whoever generated it.
- **One rule changed, on purpose.** `capped` used to be a per-row failure in both the check and the
  bless. Measured against the plan's own material that rule bans most of the corpus: of the nine authored
  patterns the *reference* counts are 9, 16, 20, 32, 32, 32, 34, 34 and 48 against a 0.20 ceiling of 17 slices,
  so seven of nine would have been refused for being dense. The gate is now per take - `uncoverableTakes`
  in `SlicerScoreTest.cpp` (blob `4a0bd20fafe1ba05`), called by the check *and* by `writeSlicerBaseline`
  so blessing cannot step around it - and a capped row is logged by name as a ceiling rather than a miss.
  No take is uncoverable: the densest, `hat-16th`, is 48 references against the grid's 49-slice ceiling at
  0.90, so it is admissible by one slice and needs no thinning.
  *(Corrected 2026-11-21, same day. The first version of this bullet counted **notes** - 9, 18, 20, 32, 32,
  36, 44, 44, 51 - and concluded "eight of nine refused" and "`hat-16th`, at 51 events, is beyond every knob
  and must be thinned". `onsets_seconds()` dedupes note *starts*, and two instruments struck on one beat are
  one onset to a detector, so 51 notes are 48 references and 44 are 34. Nothing was thinned; the mistake was
  reading the generator's `notes=` column as `refN`, and commit `1669a6d` published it in its message.)*
- **Falsifiers, both run against these bytes.** *wk-f1*: `kOnsetBiasSeconds` 0.006 → 0.020 (header
  `583caff534cc026705fbb32358dfde116a6c9ac8e8f91c1a81e64d92cae97e44` before, `193f31b3803ef8a77063aeb1a5710e31df15a4f91dd92fbe95ea6e6379104bf4`
  during) gave `FAIL - 255/258`: the tuning-stamp check named the constant, the band check caught all six
  rows, and `medAbsMs` moved 10.24 → 24.24 - exactly the 14 ms, so `biasMs` is honest arithmetic - while
  `recall25` fell 1.000 → 0.750. The restored header hash-equals the pre-mutation value, rebuilt, and
  `PASS - 258/258` again. *wk-f2*: a temporary 60-onset take turned both the coverability check and the
  inventory-parity check red and made the bless refuse with the same sentence; the temp files were
  deleted, the baseline hash did not move, and green returned.
- **Gate.** `PASS - 258/258` exit 0 from the repo root *and* from `native/` (both resolve the committed
  `fixtures/`, and ctest runs from the root by `native/CMakeLists.txt:193-194`), `ctest` 3/3 exit 0,
  `npm test` 68/68 in eight files. The check total did not move - the change replaced one report with
  one - and the twenty-one slicer checks are eight fixture-state gates and thirteen controls, which is
  what the native README had been claiming all along in one place and contradicting in another.
  No plugin source was rebuilt, so the 14:42 tuple still describes the shipped module.
- **What this is not.** It is not tier 2 and it does not close Step 25. Clicks are the easiest material
  the detector will ever see: instantaneous edge, no masking, no room. The `-10.24 ms` bias is a fact
  about clicks, and the question the whole phase exists to answer - does the 6 ms pull-back help a kit
  hit whose attack takes milliseconds - is still unanswered. Do not read the table as a verdict.

### Tier 2's authorship moves into the tree, and its reproducibility check turns out to be worktree-local (2026-11-21 17:10)

- **What landed.** `fixtures/tools/make-takes.py` - the generator that had been scratch in
  `native/build/s25-take-src/`, i.e. one `git clean -xfd` away from deleting the tier-2 corpus - plus its
  two tracked output sets: the nine `fixtures/marks-src/<take>.mid` sidecars and the nine
  `fixtures/tools/notes/<take>.notes.json` clip grids. It also gains `--publish`, which writes the marks
  JSONs into `fixtures/marks/` and **refuses, atomically, for any take whose wav is not on disk**: with
  eight of nine stand-in wavs present it exited 1 naming `swing-16ths` and wrote nothing at all, and with
  nine it wrote 27 files. Nothing about the audio is automated - `fixtures/*.wav` is the owner's call.
- **The relocation is proved by outputs, not by reading the diff.** All nine `.mid` files re-derived by the
  new script are byte-identical to the scratch generator's, all nine marks JSONs are byte-identical when
  published against stand-in wavs, and the nine notes grids match the scratch bytes plus one trailing LF
  (added deliberately: these are text files now). `python fixtures/tools/make-takes.py --check` exits 0.
- **A stray sidecar gates nothing, measured.** The suite ran with all nine `.mid` files in the tree and
  zero corresponding marks files: `PASS - 258/258`, exit 0. The scorer walks `fixtures/marks/` and only
  then looks for the `.mid` beside a take, so committing authorship ahead of audio is safe in exactly the
  direction that matters - the reverse (a reference without audio) is the named red that `--publish` now
  refuses to create.
- **The defect this found is in my own previous commit.** `1669a6d` published `synth-takes.cjs --check` as
  proof that tier 1 is "byte-reproducible", and both that check and the new one compared the LF text
  payloads byte for byte. That is a property of this worktree, not of the repo: `git checkout-index
  --prefix=<temp>` reproduces what a clone gets, and `fixtures/marks/synth-clean.marks.json` comes out
  `{\r\n` where the blob is `{\n` - `core.autocrlf=true`, no `.gitattributes`, and the Step 17 decision that
  would settle it is still deferred. Falsifier: the nine notes grids and two marks files were rewritten with
  CRLF pairs, and the pre-fix raw comparison returned *not equal* on all eleven (4216 bytes on disk against
  4215 re-derived). With the tolerance in, both checks exit 0 and say what they did - "18 files: 9 byte-exact,
  9 identical once the checkout's LF->CRLF is undone" and `marks crlf-checkout` - so a clone reads as a clone
  and drift still reddens. Restored by re-running the generators; every restored file hash-equals its
  `HEAD`/index blob (`ea50752182f719b7`, `56135f34e30a9963`).
- **The gate was never layout-sensitive, which is why this stayed a tooling bug.** With the tree in its CRLF
  state the suite still printed `PASS - 258/258` exit 0: JUCE's JSON reader treats `\r` as whitespace, so a
  checkout-translated marks file parses to the same onsets. Only the reproducibility scripts disagreed with
  themselves. The tolerance is not a substitute for `.gitattributes` - it is the honest version of a claim
  that was previously overstated, and both checks now report how many files needed it.
- **Doc sweep, one number found wrong.** `native/README.md:241` split the twenty-one slicer checks as
  "eight fixture-state gates plus eleven controls" - 8 + 11 = 19, contradicting the same file's `:305`
  ("eight fixture-state gates and thirteen controls"). Thirteen is right: `grep -c "report (" Tests/SlicerScoreTest.cpp`
  is 21, and the run's `ok` lines divide into the eight fixture-state gates and thirteen controls that score the scorer. The same
  line still named the retired rule ("no capped row"), so it now names the one in force.
  *(Commit `a116532` cites that second line as `:304`, one short: the CRLF sentence added above it in the same
  commit shifted it, and the message was written from the pre-shift reading. The blobs its message publishes were
  re-derived from the commit and match, so only that one line number was stale. Fixed here rather than by amending.)*
- **Gate for this change.** `PASS - 258/258` exit 0 on the LF tree and again on the CRLF tree, `ctest` 3/3
  exit 0 (`Total Test time 10.86 s`), `npm test` 68/68 in eight files, both generators re-run after the
  edits, and `native/shots/` unmodified by the ctest run. No plugin source touched, so the shipped tuple
  still describes the module - and nothing here is rebuilt, re-exported, installed or pushed.
  Commit `a116532`, 23 files, +292/-21 (`AGENTS.md` +15/-6, this plan +62/-8, `native/README.md` +14/-4,
  `fixtures/tools/make-takes.py` +184, `fixtures/tools/synth-takes.cjs` +8/-3, 18 new fixture files).
  Blob digests as published in that message and re-derived from the commit afterwards with
  `git show HEAD:<path> | sha256sum`: make-takes.py `0554974a53d5980b`, synth-takes.cjs `8dc2c008712c663a`,
  `marks-src/hat-16th.mid` `f99753563aa759c9`, `notes/hat-16th.notes.json` `f2c02ec25d5ddc09`,
  AGENTS.md `fdff18beaa30f41b`, native/README.md `7c13326803f6ef89`. These are blob digests, deliberately -
  the finding above is why a worktree digest is not the reproducible one.

### The re-gate before the push found the screenshots could not be trusted (2026-11-21 18:20-18:40)

Four commits had landed on top of the last shipped tuple without a native build behind them, and the branch
carries the owner's standing "do these" for push, so the first job was to prove HEAD rather than assume it.

- **The build was not the tree.** `SliceForgeHostTest.exe` on disk was dated 11-16 08:46 while the plugin's
  `SliceForge_SharedCode.lib` was dated 11-21 16:38, because every build since then named targets
  (`--target SliceForge_VST3 SliceForge_Standalone`, `--target launch`) and `ctest` does not build. It is *not*
  a wrong result though, and the distinction matters: `native/Tests/HostTest.cpp` was last edited 11-16 08:45,
  one minute before that exe, and it includes only `JuceHeader.h` - no plugin header. So the stale binary was a
  faithful build of the current test source, loading the current module at runtime. The full
  `cmake --build native/build --config Release --parallel 2` relinked it anyway, which is the fix for the habit,
  not for a defect.
- **Gate on `4f57213`, all from that build.** Smoke `PASS - 258/258 checks passed` exit 0 (`CPU: 4.59% of one
  core for 2.1s audio`), layout `PASS layout screenshot: ... (32 checks)` exit 0, host
  `parameters: 350 = 348 layout + Bypass + Program` and `PASS VST3 host test` exit 0
  (`first note peak: 0.650336`, `midi out: 9 note-ons / 9 note-offs over 48 blocks, peak 0.892715`,
  `state: 121494 bytes`, `editor: 1260 x 840`), `ctest -C Release` `100% tests passed out of 3` exit 0 in
  `19.03 s`, `npm test` `Tests 68 passed (68)` in 8 files exit 0. The 32-check layout total is what the plan
  already records (`:2720` "was 28"); the older 28s are dated run records, not claims about now.
- **`launch/` does describe the tested code, and it is now proved byte-wise.** The build tree's module had been
  relinked at 16:38 after the launch export at 14:38, so the two hashes differed (`81c540543a3249f6` vs the
  launch and installed `641cde970ba49a5e`, standalone `25ca095cce9f792a` in both - the export described below
  did relink that standalone, whose hash is now `75eaf3e3829fc230`). `cmp -l` on the module pair
  returns **four bytes**, at 305-306 and 23137941-23137942, which is the same pair of sites 8E recorded at
  `:1485` as "the two PE `TimeDateStamp` copies" (there 305-306 and 23107221-23107222; the second moved because
  the file grew). Same size, same code. So the install is not a fifth artifact and the green host
  run does speak for what the DAW loads. **Superseded six minutes later, and only partly:** a re-export was in
  fact done (`7ada9e5`), but not for these binaries - the archive's `README.md` was found stale. What this
  bullet established survives it: the exported module is still the 16:38 link, now proved `cmp`-equal to the
  build copy at zero differing bytes rather than four.
- **The real finding: `native/shots/sequencer-large.png` has two states for one unchanged binary.** The first
  layout run after the build dirtied it (`git status` said `M`), and the run inside `ctest` wrote a second
  variant - `f847391743dd894a`, 140 428 B against the committed `0ca0385a86706075`, 140 451 B. Ten further
  runs of the same exe, nothing edited, all wrote the committed bytes, so the variant is intermittent rather
  than a new truth; no other file in `native/shots/` moved in any of the twelve. Inflating both IDAT streams
  and undoing the row filters (no imaging library,
  `native/build/regate/pngdiff.py`) localises it exactly: payloads the same 5 644 800 B, **13 differing scanlines
  confined to x 1086..1128, y 17..62** of a 1680x1120 grab. That is one header control, not the layout.
  Mechanism: the grab waits `runDispatchLoopUntil (40)` after the resize (`native/Tests/LayoutTest.cpp:200-202`)
  while the 30 Hz editor timer reaches `header->refresh()` only on every 10th tick on the sequencer page
  (`native/Source/PluginEditor.cpp:317-323`), and `HeaderBar::refresh()` is what sets the undo/redo enabled state
  (`native/Source/UI/HeaderBar.cpp:95-114`). Whether the header refreshed inside a 40 ms window is scheduling.
- **What that means for the restamp the owner asked for: it is already satisfied, and it must not be "fixed" by
  re-pinning.** The committed pictures equal what a current build writes in 10 of 12 runs. The one-line change
  that would make it deterministic is to wait past the 333 ms refresh cycle (or call `refresh()` before the
  snapshot), and its cost is that it re-renders the tracked set by changing *when* every grab happens - a
  decision about the shipped documentation images, so it is left open rather than taken. Recorded in
  `AGENTS.md`'s drift list so nobody re-pins on a single run again. It also reaches the archive: the zip carries
  these pictures, which is a second route by which a re-export changes hash for identical code, alongside the
  one the pinned entry clock already closed.
  *(Both halves of that cost estimate were wrong, and the 21:53 section is the correction. The wait change did not
  re-render anything: the committed bytes are the refreshed state, so seventeen runs after the fix reproduced them
  exactly and the tracked set never moved. And the zip does not carry these pictures - `package-launch.ps1:106`
  collects the eight names linked from `native/README.md`, and `sequencer-large.png` / `editor-large.png` are not
  among them, so the flake was repo-only and no re-export was ever implied.)*
- **Logs and tools.** Behind this section, in `native/build/regate/`: `build.log`, `smoke.log`, `layout.log`,
  `layout2.log`, `host.log`, `ctest.log`, `npm.log` from the re-gate, then `export.log`, `smoke2.log`,
  `host2.log`, `zipcheck.log` from the export, plus the two throwaway tools that produced the findings here -
  `pngdiff.py` (inflate both IDAT streams, undo the row filters, print the differing rows) and `zipcheck.py`
  (each zip member against the file it claims, non-zero exit if any differ). That directory is gitignored and
  dies on `git clean -xfd`; the numbers above, not the files, are the record. The 21:45-22:00 work added
  `install.log` (the elevated install's own lines plus `INSTALL_EXIT=0`), `host-installed.log` and
  `host-zip.log` (the host test against the scan folder and against the extracted archive), `det-build.log`,
  `det-run1.log`…`det-run3.log`, `det-sweep.log` (`SWEEP_DONE runs=14 drifts=0`) and `shots-baseline.txt`.
- **One transcription slip, published and left published.** Commit `7ada9e5`'s message opens a bullet with
  "`SliceForge.exe` in `launch/` is `75e8af...`-no: `75eaf3e3829fc230`" - a half-deleted keystroke that
  survived into the text. `75eaf3e3829fc230` is the hash; nothing else in that message was affected, and every
  figure in it was re-taken from its own command. Corrected here rather than by amending, the same way
  `a116532`'s stale `:304` was handled, because the convention this branch has settled on is that the record
  keeps its mistakes where they were made and says so.

### The install caught up with the re-export, and the host test stopped testing a proxy (2026-11-21 21:45-21:50)

The *ship record* above installed the 14:42 link, and the 18:38 re-export then moved `launch/` onto the
16:38 one. Nobody re-ran the install, so by 21:45 the machine-wide bundle was four bytes from the
archive's, and the bullet below that says the install is no longer owed had stopped being true on its own
terms. Both halves of that are fixed here, and the second one turned out to be worth more than the first.

- **The gap, measured before touching it.** `cmp -l` between the installed module and `launch/`'s returned
  four bytes - 305-306 and 23137941-23137942 - the two PE `TimeDateStamp` copies again, the same pair that
  separated the build tree from `launch/` two hours earlier. Same code, older link. `641cde970ba49a5e...`
  installed against `81c540543a3249f6...` in `launch/`, at identical 24,564,736-byte size.
- **The install, re-run.** `launch/start.ps1 -InstallOnly`, exit 0, one UAC prompt: `elevated: installed
  C:\Program Files\Common Files\VST3\SliceForge.vst3 (81C54054...)`, then its own read-back against `launch/`
  twice, then an independent `cmp` that now reports **zero** differing bytes. The module's mtime is 21:47:54
  because the script restamps after both hash checks, as designed. `SliceForge.exe` was not running and no
  host held the bundle, so nothing was skipped or retired; the scan folder still holds one `SliceForge*`,
  and `%USERPROFILE%\Documents\VST3` and `%LOCALAPPDATA%\Programs\Common\VST3` still hold none.
- **The host test does not have to load the build tree, and now does not.** `HostTest.cpp:33-38` builds its
  default path from the cwd and then replaces it wholesale with `argv[1]`. So the plugin-format check ran
  twice more against the two artifacts a customer actually touches - the bundle in the scan folder, and a
  copy extracted from `launch/SliceForge-1.0.0-Windows-x64.zip` - and both exited 0 with the same census the
  build-tree leg prints: `parameters: 350 = 348 layout + Bypass + Program`, `seq.pattern: host id 825878785,
  index 347, 4 steps, default 0, named Active Pattern`, `first note peak: 0.650336`, `midi out: 9 note-ons /
  9 note-offs over 48 blocks, peak 0.892715`, `state: 121494 bytes`, `editor: 1260 x 840`. Every host green
  recorded anywhere earlier in this plan - including the one the ship section calls "the shipped VST3 was
  exercised" - loaded `native/build/...`. These two are the first that loaded the shipped bytes themselves.
  `AGENTS.md` now carries that path rule as a drift warning, because a green host leg is not evidence about
  an installation unless someone passes the path.
- **The tuple, re-measured at 21:49.** Module `81c540543a3249f691899f35b125b6204881bd14a219ee9ae7d48f97570ec509`
  in the build tree, in `launch/`, as the archive member, and in `C:\Program Files\Common Files\VST3` - four
  places, one hash. Standalone `75eaf3e3829fc2301430a710bd4b6837c7f3988813c47802157bc46e640e2a16`, identical
  between `launch/` and the build tree. Archive `127e872a862ef5e98b4181abf197125097d8abbb6189114ef60e94f993bc6b1e`,
  28,605,757 B, 17 members, all 17 byte-identical to the tree per the 18:39 check.
- **What this still leaves unproven, so the next reader does not over-read it.** No harness drives
  `SliceForge.exe`: the standalone has no test target, so the standalone half of the tuple is only ever
  proven to be a byte-for-byte copy of something that compiled. The ten layout screenshots are rendered by
  the test exe's own compiled copy of the UI sources, not by the shipped module. And the extractor is the
  archive's own `extractall`, not Explorer, so a Windows-shell unzip is still outside the evidence.

### The screenshot coin flip was a 40 ms harness wait, and fixing it cost no re-pin (2026-11-21 21:53-22:00)

Finding 3 above stopped being a landmine instead of staying a warning. The owner had to weigh it because
the assumed price was re-rendering ten shipped pictures; the price turned out to be zero, and the reason
it is zero is worth more than the fix.

- **The change:** `native/Tests/LayoutTest.cpp`, both scaled grabs - the `runDispatchLoopUntil (40)` that
  follows `setSize (1680, 1120)` became `(400)` at `:152` and at `:204`, past the ~333 ms it takes the
  editor's 30 Hz timer to reach its every-tenth-tick
  `header->refresh()` on the sequencer and vault pages, with the reason in three comment lines. +5/-2 in
  the test harness only; no product source moved.
- **The rebuild touched no deliverable.** `cmake --build native/build --config Release --target
  SliceForgeLayoutTest` exit 0: it compiled `LayoutTest.cpp` and relinked that one exe. `SliceForge.exe`
  is still `75eaf3e3829fc230...` and the VST3 module still `81c540543a3249f6...` in the build tree, byte-for-byte
  what `launch/`, the archive and the scan folder hold - re-hashed after the build, not assumed.
- **The proof is a count, not a vibe.** Three runs printed `RUN1_EXIT=0`…`RUN3_EXIT=0` and wrote
  `sequencer-large.png 0ca0385a86706075` (the committed bytes) and `editor-large.png 43365e6ae6aa0026`.
  A scripted sweep then ran the same exe fourteen more times, hashing **all ten** files after each exit
  against a baseline captured before the sweep: `SWEEP_DONE runs=14 drifts=0`, and `git status` afterwards
  shows one modified file - the harness. Nothing was re-pinned, because the bytes never moved.
- **Why 17 runs is the right number, and what it does not buy.** The pre-fix rate was 2 odd states in 12
  runs (~17%), so seventeen identical runs by luck alone is 0.83^17 ≈ 4%. That is a demonstration, not a
  guarantee: the wait is now a constant with a derivation instead of a race, and the failure mode if a
  future editor timer moves is still a coin flip. `AGENTS.md` says so where a reader would shorten it.
- **Which state was which, corrected from the direction of the evidence.** The 18:20 section could not say
  whether the committed majority was the refreshed header or the stale one. It was the refreshed one: the
  longer wait reproduces the committed bytes every time, and the two oddities were runs that grabbed the
  header *before* `refresh()` re-asserted the undo/redo enabled state.
- **One measurement of mine that was wrong, recorded so nobody re-derives it.** Mid-sweep I hashed
  `sequencer-large.png` and got a third digest, `3a8065a17c076a1e`. That was a torn read - the harness had
  just `deleteFile()`d and was streaming the PNG. The sweep's own compares run after the process exits, so
  the 14/14 stands; but a tracked asset being generated is not a state, it is a race.
- **The cite shift this edit caused, in one place instead of four.** Every line number this plan quotes
  inside `LayoutTest.cpp` below :148 moves by three. `:200-206` → `:203-209`; `:200-202` → `:203-205`; the
  ten png sites are now `:130-275` (the old `:129-271` was already one short at each end); and 8N's three
  falsifier cites land on the checks they name at `:388`, `:412` and `:459`. Verified by printing those
  lines, not by arithmetic - the check names in the old records match the strings now on them.
- **And one claim of mine from an hour ago was over-stated:** that the archive carries these pictures, so
  a restamp could move the zip hash for identical code. It does not carry these pictures. `native/README.md`
  links eight, `package-launch.ps1:106` collects exactly those, and the archive holds eight `shots/` members;
  `sequencer-large.png` and `editor-large.png` are tracked and never shipped. So the flake was repo-internal
  all along, and the archive's reproducibility rests on 8L's pinned entry clock alone.

### The browser suite had not run since the web half of the ruler landed (2026-11-21 22:03)

- **What was actually unclaimed.** `README.md:548` publishes `| Browser end-to-end | npm run test:browser | 27/27 checks
  passed, exit 0 |`, and the newest runs of that suite recorded anywhere in this plan are `:297` (2026-11-15) and 8M's gate
  bullet at `:2446` (2026-11-18). In between, `7a92a6d` (11-21 12:07) became the last commit to touch `src/` and it rewrote the browser
  edition's slicer - `src/audio/slicer.ts`, `src/state/useKit.ts`, `src/state/factoryPresets.ts` - moving the six tuning
  literals into `src/audio/slicerTuning.ts`. So the row was describing a tree three days and one refactor older than the
  sentences around it. `npm test`'s 68/68 pinned the *constants'* parity across the editions; it never starts an engine.
- **The run.** `npm run dev` (Vite 7.3.7, ready in 3266 ms, worklet rebuilt in 12 ms, 127.0.0.1:5173) then
  `node scripts/e2e.mjs`: 27 `ok` lines, `27/27 checks passed`, exit 0 read off the command rather than the pipe. The
  server was stopped afterwards and the listener on 5173 confirmed gone.
- **Which checks ride the changed code**, and why the run is not a duplicate of the unit suite: `slice navigation moves the
  pad start`, `slice add/delete updates the slice list`, `custom sample decodes and slices`, `custom-sample kit restores
  after reload`, `recording produces a take`, `recording becomes the source`, `full kit export downloads a zip`. Those call
  the browser `detectSlices` inside an AudioWorklet in a real Chrome with fake media and MIDI devices - the only place that
  path is executed end to end, in either edition.
- **It cost the tree nothing.** `test-results/`, where the run's downloaded kit zip lands, is gitignored (`.gitignore:3`),
  so the suite leaves no tracked change. `native/build/regate/e2e.log` is the witness and, like every log here, is not a
  carrier - this section is.

### Still owed by Step 25

- **Phase 3, and only the owner can do it:** render nine 4-bar takes from real kits in Live (44.1 kHz / 24-bit / stereo,
  -6 dBFS, first downbeat at t = 1.0 s, the hat run at 200 BPM, dry), then the marks files whose reference is the clip's own
  note times - each one needing its `fixtures/marks-src/<take>.mid` beside it, since the scorer now refuses a `midi-note-times`
  marks file it cannot re-derive. Everything downstream is ready: the scorer, the schemas, the bless, the gate. When the wavs
  land it is one bless and one commit, and findings 2 and 3 above get their real measurements.
  **No edit needed, contrary to what this bullet said first:** it claimed `hat-16th` carries 51 notes and so
  outruns the grid's 49-slice ceiling, and told the next reader to thin it to 48 events before the render session.
  The ceiling is compared against `refN`, and `refN` is the count of distinct note **starts** - `hat-16th` has 51
  notes but 48 references, because the kick and the hat share beats 0, 4 and 8. Nine of nine takes are coverable
  at 0.90 and seven are capped at 0.20; all nine are admissible under the per-take `capped` rule described above,
  which is what let them in at all. The nine `.mid` files and note grids now live in the tree
  (`fixtures/marks-src/`, `fixtures/tools/notes/`, generated by `fixtures/tools/make-takes.py`), so the render
  session starts from committed authorship instead of gitignored scratch.
- **Phase 3 render decisions, taken 2026-11-21.** The nine tracks and clips are built in the owner's current Live set through
  abletonmcp from his own commercial sample packs, and abletonmcp's dataset-training consent was opted in for that session. One
  deviation from the recipe above is deliberate: a single global tempo of 200 BPM for all nine takes, because the render is one
  manual export of the whole arrangement and per-take tempi would need nine exports. The generator's `OFFSET_BEATS 4.0` matches
  the arrangement placement, and the marks files carry wall-clock times derived from that one tempo map - which is exactly why
  `fixtures/marks-src/<take>.mid` is mandatory rather than advisory.
  *(Superseded on two points by the owner's ruling of 2026-11-22, recorded in the section at the end of this file: the nine tracks
  are built in a **disposable** Live set - "Do not modify an existing working session" - and a render of pack material does not
  enter this repository at all, so the consent decision above no longer has a publishing consequence here. The single 200 BPM tempo
  map, the `OFFSET_BEATS 4.0` placement and the mandatory `.mid` all stand.)*
- **Four of the eight spec categories are not producible from this authorship, and are named here rather than dropped.**
  `:727` asked for a dry drum loop, a crash-heavy loop, a foley bag, a spoken phrase, a sustained pad with soft attacks, a fast
  hat run, a piano phrase and one level-matched pair. What the committed grids do cover, re-counted from
  `fixtures/tools/notes/` on 2026-11-21 rather than copied: `backbeat-8ths` 36 notes / 32 onsets, `crash-tail` 9 / 9,
  `hat-16th` 51 / 48, `kick-quad` 18 / 16, `ride-32nd` 32 / 32, `swing-16ths` 32 / 32, `tom-fill` 20 / 20, and the pair
  `toploop-a` + `toploop-b` - 44 notes and 34 distinct starts **in both**, byte-identical note lists, two `.mid` files of 417 B,
  so "level-matched" is the same performance at two gains and not two similar loops. Still missing: the foley bag, the spoken
  phrase, the sustained pad with soft attacks, and the piano phrase.
  These are not omissions of effort but of reference: for a hand-pluck or a consonant the true onset is not a grid position, so a
  `markProvenance: midi-note-times` file would be a lie, and for a slow attack the perceptual onset *is* the thing a detector
  chooses, which makes it the measurement and not the ground truth. The scorer's handling of those shapes is covered by code
  that needs no audio - the ramped sine sweep, which contains no impulsive onset at all (`SlicerScoreTest.cpp:778-803`), the
  80-click take that outruns the slice budget (`:747`), and the rule that a take with `refN = 0` prints undefined instead of a
  perfect 1.000 (`:699`). What stays **unevidenced** is the detector's accuracy on soft-attack real audio: no number in this
  corpus or the pending one speaks to it, so Step 7 inherits it as a product-side limit rather than a solved case. The one
  deferred category that is cheaply buildable is the piano phrase - a note-time reference for a struck, decaying instrument is
  honest - and it needs only a preset the owner picks, plus the same single export.
- **The authorship needs no setup when the audio lands.** Both reproducibility checks re-run 2026-11-21 22:09 against `dd38bc4`,
  each with its own exit status: `python fixtures/tools/make-takes.py --check` → 0, *"OK - every tracked .mid and notes grid is
  re-derived by this script (18 files: 18 byte-exact, 0 identical once the checkout's LF->CRLF is undone)"*, and
  `node fixtures/tools/synth-takes.cjs --check` → 0 with `synth-clean.wav 15dbd719330faa64 / 1411244 B` and
  `synth-16ths.wav ae66267a65216307 / 1234844 B` (16-hex prefixes, as the tool prints them) and both marks files *"exact"*.
  The zero in that first line is this worktree's own LF layout, not a refutation of the CRLF tolerance - a smudged checkout is
  what exercises that branch, and it is the case `git checkout-index` was measured for above.

- **A licensing gate the owner clears before any wav is committed.** `origin` is a public repository
  (`github.com/vermeiresebastien/skeer-vrac-plus`). Rendering material from commercial packs and committing the wav fixtures
  publishes derivative third-party audio under this repo's licence, which is not the same decision as committing a marks JSON of
  note times. The suite is green with `fixtures/` absent, so the audio is optional to the gate, and the options at bless time are
  a private remote for the audio, pack material the owner is free to redistribute, or keeping the nine takes on the bench and out
  of git. None of that blocks the *measurement*; it blocks its *commit*, and it is asked of the owner rather than decided here.
  *(Reading this on 2026-11-21 16:40: the sentence "the suite is green with `fixtures/` absent" was the vacuity, not a feature -
  see the tier 1 section above. It is still the right description of what the gate does with **no** takes, and it is still wrong
  to treat it as a reason to skip the decision: with tier 1 committed, adding a take now means adding its bytes, because a marks
  file whose wav is missing is a named red.)*
  *(ANSWERED 2026-11-22, ruling 2: the owner declines to commit pack-derived audio to this public repo until redistribution rights
  are verified, and the private references stay private. The consequence for the gate, which is the part this bullet could not see
  in advance, is structural: `kBaselineName` is a fixed path inside `fixtures/` (`native/Tests/SlicerScoreTest.cpp:36`) and
  `loadTakes` enumerates `fixtures/marks/*.json`, so a bless taken while only private wavs are on disk writes a baseline that
  passes here and reddens a fresh clone. The public tier therefore blesses the tracked tree; private-tier scoring happens in a
  copy of the repo outside it. Mechanics and the per-take tier assignment are in `fixtures/tools/render-sheet.md`.)*
- **Not built, deliberately:** the browser-side scorer. `slicerTuning.test.ts` pins the constants across the editions, and the
  native scorer owns the audio; a second scorer would be a second ruler to keep true.
- **Deliverables: DONE 2026-11-21 14:42.** The `launch/` re-export, the `native/shots/` restamp, the machine-wide install and
  the push are no longer owed - see *The ship record* above for the tuple, which agrees in all four places for the first time.
  Still owed by the owner: the listening pass.
  *(The install half fell open again the moment the 18:38 re-export moved `launch/`: the scan folder kept the 14:42 bytes and was
  four stamp bytes behind the archive for three hours. It is shut by the section above, which also lands the first host green
  taken against the installed bundle and the extracted archive instead of against the build copy.)*

### The five rulings became a render sheet, and the sheet's own gate found two defects in it (2026-11-22 20:35-21:05)

The owner answered the open items with five rulings: render from a **disposable** Live set and touch no existing session, keeping
the render and every session-recovery decision his; commit no pack-derived audio to this public repo until redistribution rights
are verified, and keep private references separate from public fixtures; prepare a listening checklist that keeps measurable
timing error apart from musical quality; verify identity, rate, channels, duration, level and content **before** any bless, and
bless only after he approves what he heard; and ship the sheet with the commercial-versus-redistributable split named per clip.
Nothing here runs a render. What landed is the preparation, plus the gate that refuses bad preparation.

- **`fixtures/tools/render-sheet.md` (new).** Project setup (200 BPM one map, 4/4, swing off, empty master, no sends, metronome
  off), the region geometry (bar 1.1.000→7.1.000 = 24 beats = **7.200 s**, clip 4 bars at bar 2.1.000 so the first onset is
  1.200 s and the narrowest ring-out is 7.200 − 5.925 = 1.275 s), the import rule for `fixtures/marks-src/*.mid` (the +4-beat
  offset is already in the note positions, so it drops at bar 1 and not bar 2), the GM pitch→role map, the nine-clip table with
  notes/refs/IOI/velocity spread, the exact export settings, the bless order and the listening checklist. **Tail = Never** is the
  setting the whole file turns on: `3 Beats` adds 0.900 s, which is 132 300 frames and a duration red, and the ring-out is inside
  the region precisely so no rendered tail is ever needed.
- **The answer to "which clips require commercial material" is: none.** What three of them need is *real decaying acoustic
  sources*, and a pack is only the fastest way to get one - a recording of his own kit serves identically and would be
  committable. `ride-32nd` at 37.5 ms washes on a real bell but not on a synthetic ping, so a good synthetic score would
  overstate the detector; `swing-16ths` carries its ghost as a −6 velocity step, and a synthesised hit has no velocity layers, so
  the peak-*relative* floor (`kFloorRatio`, `native/Source/Engine/SlicerTuning.h:28`) cancels the exact variable under test;
  `crash-tail` needs inharmonic partials to ring under re-triggered toms. The other six - `kick-quad`, `backbeat-8ths`,
  `toploop-a`, `toploop-b`, `hat-16th`, `tom-fill` - are a fair timing test from material this repo already owns.
- **`fixtures/tools/verify-takes.py` (new) is ruling 4 in code:** identity/rate/channels/bit depth/frames/peak/clipping/DC, the
  0.000-0.200 s pre-roll, the ring-out rules, and the pair's grid equality, refusing before any publish. Run against the tree as
  it stands it exits **1** naming all nine absent; `--report-only` prints the same census at exit 0. Its decoder was validated
  first: a 24-bit stereo control written with a known 0.5 peak read back `peak -6.02 dBFS` and `dc -7.49e-02`, matching an
  independent decode of the same bytes to the digit.
- **The first draft of its tail rule was wrong, and the control suite is what proved it.** It asserted that nothing may exceed
  −48 dBFS after last onset + 1.000 s - but that window is where a long-ring take legitimately lives. Control A, a *correct*
  render of `crash-tail` whose last hit decays 40 dB/s, measured **−43.92 dBFS** there: a conforming file reddened, on the one
  fixture whose stated purpose is a long ring. The rule is now change-based (a rise past the authored end, plus an end-of-file
  click rule), because energy in a tail is decay and not an onset, while the hazard worth catching is material that *starts*
  after the reference stops. Controls B-E: a stray hit at 6.600 s reddens on a **20.2 dB** rise; a clip displaced 1.1 s reddens
  at **−8.0 dBFS** in the pre-roll; a live tail cut at the last sample reddens at **−13.1 dBFS** - and that last one needed its own
  control, because the first cut-off test tripped the rise rule too and proved nothing about the click rule's independence, so
  control E holds the tail non-decaying to make the two windows equal and leave the click rule the only one that can fire.
- **A false statement was about to be published into a tracked file.** `make-takes.py`'s `marks_json` hardcoded
  `"peakDbfs": -6.0` for all nine, and `SlicerScoreTest.cpp:295-345` reads exactly five fields out of a marks file - `schema`,
  `take`, `markProvenance`, `wav`, `onsets` - so the level claim is **ungated**: nothing would ever have caught it contradicting
  `toploop-b`, which is the −12 dB twin. It now derives from `PEAK_DBFS` (−18.0 for that take), and `bars: 4` became
  `clipBars: 4` + `regionBeats: 24` because the field named the clip while the file is six bars. `--check` is unaffected and
  re-run green: 18 files, 18 byte-exact, exit 0 - marks are only ever written by `--publish`.
- **The pair's separation was an unrecorded disagreement between two tools,** `make-takes.py` saying "-6 dB gain" while the
  verifier spec said −18 dBFS, and the plan fixing no number at all ("the same performance at two gains", above). Settled at
  12 dB / −18 dBFS and stated in both files, on the reason rather than the arithmetic: because the floor is peak-relative, the
  pair's job is to test level invariance, and a doubled separation doubles the signal without approaching a 24-bit noise floor.
- **Corpus totals, measured twice this session:** the nine grids carry **257 references** (32+9+48+16+32+32+20+34+34), counting
  distinct note *starts* - `hat-16th` is 51 notes/48 refs and `toploop-a` 44/34 because two instruments on one beat are one onset
  to a detector. The two totals agree between `verify-takes.py`'s `grid_onsets_s`, a direct count over `notes/*.json`, and
  `make-takes.py --check`'s printed `onsets=`. `fixtures/marks-src/toploop-a.mid` and `toploop-b.mid` are byte-identical at 417 B
  (`01f2137c…`), which is the pair's whole claim in one comparison.
- **What the bless will do when it is authorised, so it arrives as a known change:** `fixtures/slicer-baseline.json` goes from 6
  rows to **33** (nine takes × 0.20/0.55/0.90, plus the two synthetic takes already there). Seven of the nine will report
  `capped = True` at 0.20 because `targetAt(0.20)` = 8 + 46 × 0.2 = **17** slices and only `kick-quad` (16) and `crash-tail` (9)
  have fewer references than that; no take approaches the 49 ceiling, `hat-16th` at 48 being the tightest by one slice. The band
  is symmetric, so an improvement reddens until blessed - which is the gate, not a defect.
- **Two traps of mine, both recorded because both touched the tree.** Inspecting `marks_json` through `runpy.run_path` executed
  `make-takes.py`'s *default* branch - the script writes unless it is given `--check` or `--publish`, so an import-for-reading is
  a write - and rewrote all 18 tracked generator outputs. No bytes moved: `git status --porcelain fixtures/` listed only the
  intended `make-takes.py` edit, `git diff --stat` on the data files was empty, and `--check` re-verified 18/18 byte-exact - but
  their mtimes are now 20:47:16, so a later `find -newer` attestation against `fixtures/marks-src/` or `notes/` will read them as
  freshly authored and must be discounted for this timestamp. Second: `python -m py_compile` left `fixtures/tools/__pycache__/`
  in the tree (removed), and `.gitignore` does not cover `__pycache__` - staging by explicit path is the only thing keeping a
  Python bytecode dir out of this repo's commits.
- **Pre-existing doc defect closed in passing:** `:3377-3380` of this plan carried the piano-phrase sentence twice, two identical
  lines repeated; the duplicate is deleted, so cites into this file past that point moved by two lines before this section added
  any. The 2026-11-21 render-decision bullet above is annotated as superseded on its two changed points rather than rewritten,
  because it records what was decided that day.
- **Not run, by ruling:** `--publish` (no wav exists), `--bless-slicer-baseline` (waits on his listening approval), any render,
  any audio addition, and any binary rebuild. **Now on the owner:** the render itself, then the listening pass and the bless
  approval; the source tier for `ride-32nd`, `swing-16ths` and `crash-tail` (private pack vs an original recording that could be
  committed); `fixtures/user-groove.wav` (see the audit below - as first written here, this bullet called it referenced by no code,
  which was wrong and is corrected there). And the push: `3e421cc` was pushed on his word, `667007b..3e421cc`.

### The user-groove provenance audit, and the false claim it began with (2026-11-22 21:05-21:22)

- **The claim I had to retract first.** `3e421cc` states that `fixtures/user-groove.wav` "is referenced
  by no code, no doc, no test and no marks file". The grep behind it searched `src/`, `native/`,
  `scripts/` and `fixtures/` for the string but its pattern set omitted `.mjs`, so it missed
  `scripts/e2e.mjs`, which uploads the file as the browser suite's custom-sample leg. `git grep` at HEAD
  finds it. It is load-bearing, not orphaned. The same commit's other clause holds: no marks file names
  it, so it cannot affect any slicer score.
- **A second regression, found while correcting the first.** `README.md` used to say the browser test
  depends on this fixture; commit `9659b78` deleted that sentence while the code kept consuming it. The
  dependency is now documented again in `README.md`'s suite paragraph, and it is the kind of loss that
  makes a later "nobody uses this" claim sound plausible. That is what this whole bullet is: my own
  grep, made to look like a fact about the tree.
- **Origin is not establishable.** Git: one blob (`493a3d05379a…`, 768 044 B, sha256 `3040b1fd70cff830…`,
  binary so blob == worktree bytes) entering in the initial import `192df10` and never modified - and that
  commit's message documents the provenance of the six `Resources/library` WAVs and the fonts while saying
  nothing about this seventh audio file. The file: `fmt ` + `data` and zero bytes outside them, which is a
  hand-rolled writer, not ffmpeg/Audacity/a DAW. The audio: exactly 4.0000 s, sound at exactly 0.2000 →
  3.9007 s, 14 non-silent runs separated by 13 gaps of exact digital zero, 350.0 ms envelopes, true stereo,
  peak 27851 = floor(0.85 x 32767). Not `encodeWav` (rounds: 27852), not the factory recipe path
  (normalises to 0.88: 28835), not drumgen's writers (mono 44.1 kHz), byte-identical to nothing in the
  family's tracked audio (42 WAVs, all hash-compared), and no tracked script in the thirteen git repos of
  the twenty-one project folders under `D:/Projects/skeer-audio` names it.
- **Two of my own probes were vacuous and were rerun.** The first "shared block" scan compared only
  offset 0 of every candidate against a file whose first 38 KB is silence, so it "matched" seven of
  eight; the polarity test ran over exactly the documented 200 ms lead-in, where `L == -R` is
  `0 == -0`. Re-run: full-file `L == -R` is **False**, and a gain-invariant FFT correlation of every hit
  against both channels of all eight candidate sources peaks at 0.64 against ~1.0 expected for a paste
  and ~0.04 for chance at this window length. Discarding those is why the audit takes three sittings.
- **The dependency is proven at runtime, not only by grep.** `npm run test:browser` re-run at 21:24-21:26
  on this commit: `27/27 checks passed`, exit 0, with all three legs that consume the fixture green -
  `custom sample decodes and slices` and `custom sample is audible` and `custom-sample kit restores after
  reload` (`scripts/e2e.mjs:157`, `:164`, `:180`). So the suite passes *because* it decodes this file, and
  README's `27/27` row is current as of that run. `npm test` exit 0, 68 tests across 8 files, at 21:24:05.
  The dev server was started for the run and stopped after it; port 5173 left with no listener.
- **What this changes for Step 25.** Nothing in the render path. It changes the recommendation on this
  file: it cannot be cleared as licensed and it cannot be shown to infringe, so the way to close it is to
  stop depending on it - regenerate the fixture in-repo the way `fixtures/tools/synth-takes.cjs` already
  does, byte-reproducibly. The e2e leg asserts only `sourceId` starts with `user-` and `slices >= 4`, so
  replacement moves no acceptance threshold. The file itself is untouched, per his ruling that deleting
  or rewriting it is a separate decision. *(Superseded twenty minutes later: he ruled the separate
  decision, and it was replacement. See the next section.)*

### The fixture is generated now, and two of my own graders failed before it did (2026-11-22 21:30-21:51)

He approved pushing both correction commits - `70c1df3` and `45806a6` are on `origin/master`, verified as
a fast-forward with `git ls-remote` agreeing with local HEAD - and ruled on the uncertain fixture: replace
it with a project-owned synthetic take, keep the thresholds, add a byte-reproducibility check, do not
rewrite history, and do not push the replacement until he has reviewed it.

- **`fixtures/tools/make-user-groove.cjs`** synthesises five voices from constants and one xorshift32
  seed, peak-normalises to 0.80, and writes the same bare `fmt `/`data` container. The new bytes are
  1 728 044 B, sha256 `1998499da87776b9aafc76fd3e0b86e2…`, 48 kHz / 2 ch / 16-bit, exactly 9.0000 s, no
  sample at full scale, DC -73.4 dBFS, `L != R`. Reproducibility rests on `--check` comparing a fresh
  render against the tracked file, and on the digest pinned in the source; it does still rest on this
  engine's `Math.sin`/`Math.exp`, so the claim on the file is "this engine re-renders these bytes", not
  "any C library will".
- **My first grader was wrong about the audio, and the audio was fine.** The contract check demanded
  "energy that holds within 6 dB for a second" and read it off the same 8 ms RMS envelope it used for
  onsets. 384 samples is shorter than one period of the pad's 110 Hz fundamental (9.1 ms), so short-time
  RMS flickers on perfectly steady material: measured on the shipped bytes, the 3.00-4.00 s pad window is
  **10.45 dB of spread at an 8 ms hop and 0.73 dB at 50 ms**, and the steadiest 1.0 s window anywhere in
  the file at 8 ms is still 7.27 dB - the bound was not satisfiable by any window, so the check could only
  ever have been red. Fixing the window, not the audio, is why the pinned digest never moved across this
  whole section.
- **And my second grader was a tautology.** I replaced the refractory onset detector with a Schmitt
  trigger specifically so `minSpacing` would stop reporting the suppression constant, then probed it: a
  mutant that moves the clank to 100 ms - and one at 50 ms - after the kick still reported the tightest
  gap as **0.500 s**, because the hysteresis merges a close pair into one onset. A spacing claim cannot be
  measured by a detector that disarms on spacing. The assertion now reads the authored positions
  (`authoredGap`, 0.250 s) and says so, and the byte-level census is left to make only the claim it can
  support: events exist, they are findable, one voice is transient and one is sustained.
- **Four falsifiers, each red on exactly its own assertion.** clank at 100 ms and at 50 ms →
  `tightest authored spacing` only; pad voices deleted → `sustained material` only; hits deleted, pad only
  → `events found` and `transient material`. Scratch copies were written beside the generator, run, and
  deleted; `git status` on `fixtures/` afterwards shows the wav and the generator and nothing else.
- **`slices >= 4` is saturated and I did not read it as evidence.** The app returns 33 slices for this
  fixture, which is not a measurement of anything: `src/audio/slicer.ts:72` computes
  `target = round(TARGET_BASE + TARGET_SPAN * sens)` and `:88` fills to it, so at the 0.55 default any file
  with 33 candidates over the floor gets 33. Aligning the 33 reported times against the 15 scored events
  puts 11 inside ±50 ms and 22 elsewhere - on the pad's beating and in the tail - which is the same
  target-fill behaviour AGENTS.md already records for a dead take. Accuracy gets judged by Step 25's
  scorer against marks files, not by a count on a fixture that is deliberately outside the corpus.
- **Suites, on the replacement bytes.** `npm run test:browser` 28/28 exit 0 at ~21:41 with the new
  `custom-sample fixture re-renders byte-identical` check first among the fixture legs, `npm test` 68 tests
  across 8 files exit 0 at 21:48:40, `npm run build` (worklet + `tsc -b` + vite) exit 0 at 21:49. The dev
  server was started for the run and stopped after; port 5173 has no listener.
- **What did not happen.** No `marks` file for the new fixture, so `loadTakes` never sees it and
  `fixtures/slicer-baseline.json` is untouched - `--publish` and `--bless-slicer-baseline` remain gated on
  the nine renders and his listening approval. No threshold moved: the e2e legs still assert `slices >= 4`
  and `peak > 0.001`. No history rewritten: the old blob is still in `192df10` and in every commit after
  it, and replacing the working file does not remove it from the repository's past.

