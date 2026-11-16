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

- **The parameter count is two numbers, and `native/README.md:154-161` states both.** The layout
  declared **347** when this was first worked out — 8 global + `source.sensitivity` + 21 parameters ×
  16 pads + `global.clip` + `global.out` (`native/Source/Params.cpp`, the pad loop adds exactly 21 ids)
  — while the host saw **349**, because JUCE's VST3 wrapper appends its own `Bypass` and `Program` to
  whatever the processor exposes. Round 2 of this plan read the 347 it had derived from the layout, saw the 349 the
  test printed, and concluded the README was wrong by two; it was the reading that was wrong. Nothing
  used to catch a disagreement between them: `HostTest.cpp` only *printed* the count and asserted
  `< 300`, which 347, 349 and 345 all satisfy. `native/Tests/HostTest.cpp:69` now asserts exactly
  350 and `native/Tests/SmokeTest.cpp:171` asserts the 348 the layout builds - one more than the
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
`native/CMakeLists.txt:106-107` (Erwm at 106, Vrac at 107), the four `VRAC_EXTRA` sites are
`PluginProcessor.cpp:974`, `:978`, `:1036`, `:1041`, and the preset folder sits at
`PluginEditor.cpp:461` and `:491` rather than `:446` and `:476`. Read those numbers as a snapshot, not
as an address: this paragraph recommended `:956`, `:960`, `:1018` and `:1023` for `VRAC_EXTRA`, and they
were stale inside a day, because every step that lands edits the very files it cites. The grep strings
(`VRAC_EXTRA`, `PLUGIN_CODE`, `getChildFile ("SliceForge")`) are the durable half. Round 2's claim that
the README should read 347 is retracted above. What the README *was* wrong about is its check counts
(109 against a 117-check suite) and "about 5% of one core"; both were fixed with Step 29 on 2026-11-15.
  (Same exemption: that 117 and the 347 above are dated reads. The live pair is 186 checks and 350
  host-visible, re-measured 2026-11-16: Step 26's host-path gate was reworked into four per-arm checks,
  Step 28's MIDI output bus added six, and Step 27's exported-file parity added eleven.)
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
  `native/CMakeLists.txt:106-107`, `:116`, `:49`, `:127`, `PluginProcessor.cpp:23`, `:974`, `:978`,
  `:1036`, `:1041`, `Sequencer.cpp:12`, `Source/UI/Theme.cpp:75`, `Source/PluginEditor.cpp:461`, `:491`,
  `Source/VaultLibrary.cpp:143`, `src/state/storage.ts:5-7`, `src/state/sampleStore.ts:7`,
  `Tests/HostTest.cpp:68`) was opened and read at HEAD `4abe4b0` on 2026-11-16, and each line says what
  the prose around it claims. The instruction is the stale item, not the references. Two traps, recorded
  so nobody re-derives this: the web paths are **root-relative** — `web/src/state/storage.ts` does not
  exist, so resolving them against `web/` invents staleness — and the one real defect in the same
  paragraph is *not* a line reference: `AGENTS.md:99` names `PLUGIN_CODE` before
  `PLUGIN_MANUFACTURER_CODE`, while `native/CMakeLists.txt` puts the manufacturer at `:106` and the
  plugin code at `:107`. That is a wording fix and belongs to the stage that already edits `AGENTS.md`.
- **Machine-hold the exact parameter count - built, and not by this step.** `native/Tests/HostTest.cpp:69`
  reads `if (instance->getParameters().size() != 350)` - a literal, so it reddens when the layout
  changes - and `:107` prints the composition (`parameters: 350 = 348 layout + Bypass + Program`), with
  `:102` printing the pattern control's host index. The bare-processor view is 348; the two differ by the
  wrapper's pair, and both numbers are current.
- **Retire or scope the "no audio-thread allocation" sentence — refuted, so the sentence stays.**
  `native/README.md:394` ("32 fixed voices; no audio-thread allocation or locking") is accurate, and
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
  (`native/CMakeLists.txt:180-181`) that supplies the root for the suite. As of this commit that line prints
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
layout. `SmokeTest.cpp:171` asserts 348 and `HostTest.cpp:69` asserts 350, each anchored to the previously
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
lists are pinned as literals (`SmokeTest.cpp:2075`, `:2095`, `:2115`) and why those three literals are
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
(`SmokeTest.cpp:2087`, `"931 960 1080 "`) so that changing it is a decision someone makes on purpose.
Also deliberately not built: the plan's "print the backbone" switch (probability gates off) and a
pass-number *file name*, both UI work; and stamping the clip with the pass you just heard, which needs
`SeqPlaybackState::passCount` — audio-thread state — synchronised before any UI may read it. Both are
owner decisions, and the exported clip always says "pass 2" until then.

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

Built 2026-11-16, red first. With production at `82d8866` and the new assertions in place, the smoke suite
printed `FAIL - 171/175 checks passed` on `FAIL  MIDI out carries nothing but four note-on/note-off pairs
[0/0 notes, 0 other events]`, `FAIL  MIDI out notes carry the scheduler offset, 36 + pad and the 32-sample
gate [0 events: ]`, `FAIL  MIDI out notes are all on channel 10 [0 off-channel of 0]` and
`FAIL  MIDI out velocity uses the exporter's round(velocity x 127) mapping [0 note-ons: ]` (exit 1), and the
host test exited 1 on `FAIL hosted plugin advertises no MIDI output bus (producesMidi() is false)` with
`buses: 1 in / 8 out` still on the line above it — the audio layout is unchanged; what was missing is the
event bus. The change is `NEEDS_MIDI_OUTPUT TRUE` (`native/CMakeLists.txt:113`, plus a re-configure: the
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
  (`types.ts:426-537`, wired at `useKit.ts:341-355`). A persistence write that did not stick is
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
  committed predicate is `jmax (0.02, blockPpq * 0.5)` (`Sequencer.cpp:291`), and legitimate sub-block
  disagreement is bounded by half a block while the smallest deliberate move is one step (`1 / division`).
  Those are separable only while `0.5 * blockPpq < 1 / division`, i.e. `numSamples * bpm < 720000` at 1/32
  on 48 kHz. Past the seam a one-step edit reads as playback and is swallowed (`SmokeTest.cpp` keeps the
  case green as the named `KNOWN DEFECT:` characterisation), which is reachable from the UI because
  `UI/SequencerPanel.cpp:270` offers `1/32`. Capping the tolerance at half a step was measured, not argued.
  (Probe logs live under `native/build/pm/`, which `.gitignore` excludes at `:10` - so the strings quoted
  below are the record, not the files.) The run printed three reds, and only one of them is the cap doing
  its job:
  `KNOWN DEFECT: a one-step 1/32 edit inside a 0.28 ppq block is not a seek` moves from `clears=1` to
  `clears=2`, which is exactly the falsifier that check's own comment names (`SmokeTest.cpp:790-792`).
  The other two are collateral, and both sit at settings the committed formula already separates.
  `sub-block rounding on a 1/32 grid is not a seek` (`SmokeTest.cpp:805-820`) is 4096 samples at 120 BPM
  with 0.08 ppq of per-block rounding, and it reddens at `clears=9`; `blockPpq` there is 0.1707, so the
  honest window is `(0.0853, 0.125)` - above half a block or every block trips, below one 1/32 step or a
  seek is swallowed - a cap at half a step lands it at 0.0625, and 4096 x 120 is well *below* the 720000
  seam. `a small block keeps the original tolerance window` (`SmokeTest.cpp:771-781`) is 512 at 120 with
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
  (`PluginProcessor.cpp:315`, with the argument itself at `:319`). The gates covered both ends - the
  callee, by a four-pattern differential driven through the new argument, and the cache, by
  `getPatternParameter()` - but nothing covered the wiring between them. Measured, at the time: stubbing
  `readActivePattern()` to `return 0` reddened four checks - `selecting a pattern moves the parameter and
  tells the host`, `a session saved with the parameter on pattern C comes back on pattern C`, `a session
  written before seq.pattern existed keeps its stored pattern` and `when the parameter and the blob
  disagree, the parameter plays and the mirror follows` (161/165, that run's own total) - and replacing the
  call-site argument with a constant reddened **nothing**. What this entry then asserted - that a headless
  test has no `AudioPlayHead`, so the host path cannot be driven, and that the only pad observable is a
  millisecond stamp too coarse to use - was a skipped search. `SmokeTest.cpp:15` already defines
  `MockHostPlayHead` and two fixtures already run a real `processBlock` through it (`:454`, `:481`);
  `getPadHitStamp` is
  public (`PluginProcessor.h:70`) and `updatePadHit` fires for every event the scheduler emits
  (`PluginProcessor.cpp:398`); and the differential needs no sub-millisecond resolution because it asks
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
  `fixture invalid param=0 want 1 mirror=2` - before a single block was rendered. What that proves is that
  the pre-check is the component detecting a missing parameter and the four-arm comparison is not; the
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
  What the gate does *not* buy: the four arms discharge the cache-to-render wire, and the shipped comment
  at `SmokeTest.cpp:1391-1394` promises section 8C says so in both places, so here is the other half -
  stamps give pad identity, not iteration, so nothing here can see a step that repeated or a step that
  dropped, and the mid-block half of the once-per-block parameter read (`PluginProcessor.cpp:315-316`) is
  still untested. A host that automates `seq.pattern` inside one render quantum can still hand that block
  two different answers, and no check in the suite would notice.
- **Two checks in the suite had names containing a verb their expressions never observed, and both are
  fixed on this tree.** `host-synced playhead advances` (`SmokeTest.cpp:496`) asserted
  `getSequencerPlayheadStep() >= 0` under a playhead that was frozen for the whole fixture, and
  `step sequencer advances the playhead` (`SmokeTest.cpp:361`) asserted the same `>= 0` on the internal
  clock. The observable is
  not worthless - `-1` is what `Sequencer.cpp:923` reports when there is no usable transport, so `>= 0`
  does separate "the host path answered" from "it did not" - but a wire that latched on step 0 prints 0
  forever and reads as a pass, which is precisely what a check called *advances* must not be able to do.
  Both now pin the arithmetic: the host fixture advances the head by the engine's own block step for 16
  blocks and expects step 1 (`floor (0.3413 x 4)`, `Sequencer.cpp:321`), the internal one renders 40 blocks
  and expects step 3 (`Sequencer.cpp:357` reports the step at the *end* of the block), and each fixture
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
  `AGENTS.md` means exactly this when it says a cite is a snapshot and not an address: `:38` points at a
  7000-sample block call that no longer exists (the oversized block is gone, and the `720`-sample calls it
  means are now `:1497` and `:1499`); `:135` and `:557` both ask to "tighten `SmokeTest.cpp:948` from
  `< 8.0 s`", and there is no such bound anywhere in the file - CPU is printed and deliberately not
  asserted, with the reason in the comment at `:2227-2229`; `:431` cites `:835` for the `0.349`/`0.844`
  pair, which is at `:1917` and `:1921`; `:870` names a check, `a Sample Vault track loads into SliceForge`,
  that is not in the suite (the vault block runs `:1657` to `:1760`, and the gate that loads a file this
  test owns is `a real wav loads through the user-sample path` at `:1719`); `:954` cites `:1283-1289` for
  the formatting-branch gate, which is at `:2120-2141`; `:979` cites `:736-750` for the truncated-blob
  refusal, which is at `:1588-1602`. Correcting the six is a prose edit in somebody else's historical
  entry and is deliberately not done here; what is done is naming where each one actually lives.
- **One review finding on this tree was checked and withdrawn.** `song mode follows the pattern chain`
  (`SmokeTest.cpp:1530`) reads `out.triggerCount > 0 && out.triggers[0].pad == 1`, and that was flagged as
  asserting one element where the claim is about the set. It is not: `length = 4` and `ppq = 1.0` put
  absolute step 4 in chain slot 1 (`Sequencer.cpp:46`), `makeFeatureSequence` gives that slot pattern 1,
  and pattern 1 has exactly one lit step - so the first trigger *is* the complete set for this fixture.
  What is left is positional fragility, not a false green: a later edit that lights a second step in that
  pattern keeps the check green while no longer proving which slot ran. LOW, and the real song-mode
  discriminator is the exact-list differential at `SmokeTest.cpp:1226` that Step 26 shipped.

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
  `native/CMakeLists.txt:113` does nothing until CMake is re-configured; `SliceForge_artefacts` reads
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

Built 2026-11-16 as a boundary rule, not as code: the owner resolved it (SliceForge owns one
continuous recording becoming a playable 16-pad kit; redrum owns a folder of already-made kit pieces;
the discriminator is the material type at the door), and the rule now lives in `AGENTS.md:143-148` under
"Which repo owns which material". Nothing was built for it, which is the point - the step was a decision.

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
All three landed in that order on 2026-11-16, so the block is closed; what is left of it is acceptance
in a real host, which no check in this repo can do.

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
Still **not** confirmed after two rounds: the "SliceForge" name-collision risk, and the current prices
of Portico, Influx and Ripchord.
