# Step 25 phase 3 — Ableton render sheet for the nine takes

Owner ruling of 2026-11-22, item 1 and item 5: prepare a concise render sheet for a **disposable**
Ableton project, name which clips need commercial material, and stop there. The render, and any
session-recovery decision, is the owner's. Nothing in this file automates Live.

Reference material: the nine note grids in `fixtures/tools/notes/` and their SMF twins in
`fixtures/marks-src/`, approved as the truth for this phase. They are original authorship, so the
grid commits cleanly; what a render adds is sound, and sound is where the licensing question lives.

Three stop conditions bind this whole page:

1. No `--publish` and no `--bless-slicer-baseline` until `verify-takes.py` is green on all nine.
2. No audio derived from a commercial pack enters this repository (ruling 2).
3. The bless waits for the owner's listening approval (ruling 4) — a conforming file is not a
   musical pass, and a grid match proves timing and never proves quality (ruling 3).

## 1. Project setup — disposable, one tempo, dead silent everywhere else

| Setting | Value | Why it is a rule |
|---|---|---|
| Live set | new empty set, saved as e.g. `SliceForge-takes-25.als` **outside** this repo | Ruling 1: no existing working session is opened, edited or overwritten. |
| Tempo | 200 BPM, one tempo map, no automation | The grids are beat×0.300 s. A tempo change rescales all 257 references silently. |
| Time signature | 4/4 | The .mid has no time-signature event; Live's default must match the authoring. |
| Swing / groove | **off** globally and per track | `swing-16ths` is authored as a 1/3-beat note position, not from Live's swing knob. A global swing would double it. |
| Master chain | empty — no limiter, no utility, no EQ, no glue | The level targets below are the measurement. A master limiter rewrites them. |
| Sends / returns | none created; no reverb or delay anywhere | A tail that is not the source's own decay is unattributable in the ring-out bar. |
| Metronome | off, and no count-in | A leaked click in the first 200 ms is a hard red (`pre-roll` rule) and a phantom onset. |
| Monitor | Off | Monitoring through an armed track can add delay compensation at the render head. |
| Arrangement region | bar 1.1.000 → bar 7.1.000 | 24 beats = **7.200 s**. This is the file, not the clip. |
| Clip length / position | 4 bars, starting at bar 2.1.000 | First downbeat = beat 4 = **1.200 s** into the file. Bar 1 = lead-in silence, bar 6 = ring-out. |

Geometry that the verifier depends on, measured off the grids this session: every take's first onset
is 1.200 s, the latest onset in the corpus is 5.925 s (`tom-fill`), so the narrowest ring-out is
7.200 − 5.925 = **1.275 s** — inside the 1.000 s the reference owes itself, with 0.275 s to spare.

## 2. Import

Drag `fixtures/marks-src/<take>.mid` into the arrangement at **bar 1**. It is SMF format 1, 480 TPQ,
track 0 = tempo map, track 1 = notes, and **the +4-beat offset is already in the note positions** —
the .mid timeline is the file timeline, which is what makes the scorer's mid-vs-JSON cross-check mean
something. Dropping it at bar 2 shifts every onset by 1.200 s and the render is refused.

After import, map the GM pitches to your kit:

| Pitch | Role | Used by |
|---|---|---|
| 36 | kick | kick-quad, hat-16th, backbeat-8ths, toploop-a/b |
| 38 | snare | kick-quad, backbeat-8ths, swing-16ths (i%4==3), toploop-a/b |
| 42 | closed hat | hat-16th, backbeat-8ths, swing-16ths, toploop-a/b |
| 45 | tom low | crash-tail, tom-fill |
| 47 | tom high | crash-tail, tom-fill |
| 49 | crash | crash-tail |
| 51 | ride | ride-32nd |

## 3. The nine clips

`refs` = distinct onset times a detector is graded against, which is **not** the note count: two
instruments on one beat are one onset. `IOI` = tightest inter-onset interval. All numbers re-derived
this session by `python fixtures/tools/make-takes.py --check` and independently by
`verify-takes.py`'s own `grid_onsets_s`; the two agree on all nine.

| # | take | notes | refs | IOI | onsets (s) | pitches (velocities) | what it stresses |
|---|---|---|---|---|---|---|---|
| 1 | kick-quad | 18 | 16 | 300 ms | 1.200→5.700 | KICK 110, SNARE 100 | sparse floor, 4-on-the-floor |
| 2 | hat-16th | 51 | **48** | 75 ms | 1.200→4.725 | HAT 96, KICK 112 | densest case in the corpus, 1 slice under the 49 ceiling |
| 3 | backbeat-8ths | 36 | 32 | 150 ms | 1.200→5.850 | HAT 92, KICK 115, SNARE 105 | the common case |
| 4 | crash-tail | 9 | 9 | 150 ms | 1.200→5.100 | CRASH 104, TOM_HI 92–100, TOM_LO 88–96 | long ring under re-triggered hits |
| 5 | tom-fill | 20 | 20 | 75 ms | 1.200→5.925 | TOM_HI/TOM_LO 88,92,96,100 | pitched run, 3 tom pitches |
| 6 | ride-32nd | 32 | 32 | **37.5 ms** | 1.200→2.362 | RIDE 98 | below `kMinGapSeconds` (115 ms): the wash case |
| 7 | swing-16ths | 32 | 32 | 50 ms | 1.200→3.550 | HAT 94, SNARE 88 | 50/25 ms swing pair + ghost note at −6 velocity |
| 8 | toploop-a | 44 | 34 | 75 ms | 1.200→5.850 | HAT 90, KICK 114, SNARE 104 | arrangement with doubles (44 notes → 34 onsets) |
| 9 | toploop-b | 44 | 34 | 75 ms | 1.200→5.850 | identical to #8 | the level twin: **−12 dB**, same grid |

#8 and #9 are byte-identical grids (same 417-byte .mid, same notes list). The pair is therefore one
performance at two gains, and the only thing that may differ between the two renders is level.

## 4. Gain

| Take | Peak target | How |
|---|---|---|
| all except toploop-b | **−6.0 dBFS ± 0.75** | clip gain / track fader, set to the meter then confirmed by `--measure` |
| toploop-b | **−18.0 dBFS ± 0.75** | duplicate #8's track, set **−12 dB** on the fader. Never by re-authoring velocities — that changes the performance, which is the variable the pair exists to hold constant. |

Hard rules the verifier enforces: a full-scale sample anywhere is its own red (a take that hit the
ceiling is a distorted take, not a loud one), DC offset above 5.0e-4 is a red, and the two-level
difference must be the *only* difference. Reference point: the six in-repo factory sources all peak at
exactly −3.09 dBFS (measured, 48 kHz / 16-bit / 10.6–13.8 s), so a uniform ≈ −3 dB trim lands near
target from any of them.

## 5. Export — exact settings

File → **Render and Export**, once per take, 9 files.

| Control | Value | Consequence of getting it wrong |
|---|---|---|
| Rendered Track | **Master** | Per-track rendering yields dry stems and no summed doubles. |
| Export Range | **Selected** (the 1.1.000→7.1.000 loop) | `All` leaks the arrangement length in. |
| Loop | **off** | Region repeats and the tail rule reports a rise. |
| **Tail** | **Never** | The single most important setting. `3 Beats` adds 0.900 s → 132 300 extra frames and the duration rule reddens. The ring-out is inside the region on purpose; it is not a rendered tail. |
| Sample rate | 44100 | A rate change silently rescales every reference. |
| Bit depth | **24-bit PCM** | 16-bit fails; the tier-1 synthetic fixtures are 16-bit and are correctly refused by this spec. |
| Channels | stereo, Convert to Mono **off** | Mono fails. |
| Normalize | **off** | Normalising equalises the pair and destroys the −12 dB differential. |
| Create Analysis Files | off | No `.asd` beside the fixtures. |
| File type | WAV | — |
| Directory / names | `fixtures/` at the repo root, exactly `backbeat-8ths.wav`, `crash-tail.wav`, `hat-16th.wav`, `kick-quad.wav`, `ride-32nd.wav`, `swing-16ths.wav`, `tom-fill.wav`, `toploop-a.wav`, `toploop-b.wav` | `make-takes.py --publish` derives the marks file's `wav` field from the take name. |

## 6. Which material may be committed, and which may not

Ruling 2 separates *reference grid* (ours, committable) from *rendered sound*. Two independent gates,
not one:

- **Commercial sample pack** → private tier. Not committed to `github.com/vermeiresebastien/skeer-vrac-plus`,
  which is public and AGPL-3.0; a bounce through it is derivative third-party audio.
- **Ableton factory / Library content** → treat as private tier too, until the current EULA is read
  and *filed in this repo*. Live's licence permits use in your music; redistribution of its samples is
  a different question, and a single-instrument 7.2 s loop is close enough to the source that I would
  not commit it on a recollection. AGENTS.md rule: a licence is a file in the repo, not a memory.
- **Original recordings you made, and synthesised material** → public tier, committable.

**Which clips require commercial material: strictly none.** What three of them require is *real
decaying acoustic sources*, and a commercial pack is only the fastest way to get one — a recording of
your own kit serves identically and would be committable. The distinction matters because it changes
the ask from "approve publishing derivative audio" to "choose a sound source".

| Take | Fair test from in-repo synthesised sources? | Verdict |
|---|---|---|
| kick-quad, backbeat-8ths, toploop-a, toploop-b | yes — timing-only stress, decay irrelevant | **public tier** |
| hat-16th, tom-fill | yes for the grid; a real hat/tom adds partial overlap the synth lacks | **public tier**, private confirmation |
| crash-tail | partially — a filtered-noise crash has no inharmonic partials, so the ring-under-retrigger case is understated | private confirmation |
| ride-32nd | **no** — 37.5 ms on a synthetic ping gives clean valleys a real ride bell does not have; a good score here overstates the detector | **private tier** |
| swing-16ths | **no** — the ghost is only a −6 velocity step, and a synthesised hit has no velocity layers, so the floor (which is peak-*relative*) cancels exactly the difference being tested | **private tier** |

The public-tier sound palette is already in the repo and already licence-cleared: the six factory
sources are generated by seeded recipes in `src/audio/sampleLibrary.ts:597-635`
(`THIRD-PARTY-NOTICES.txt` §6 states no third-party sample licence applies), and the per-event
primitives in that file can voice all seven pitch roles deterministically: `thud` / `chestThump` (36),
`metalClank` + `cutlery` (38), `glassPing` / `relayClick` / `circuitBlip` (42), `potHit` / `woodKnock`
(45 and 47, at two tunings), `chainRattle` + `metalClank` (49 and 51 — the closest thing in the repo to
a wash). Note the six shipped wavs are 48 kHz multi-event *takes*, so a role needs either a re-render
from those primitives or a slice — not the whole factory wav.

### Structural rule: private audio must never bless the tracked baseline

`SlicerScoreTest.cpp:36` fixes the baseline path inside `fixtures/`, `loadTakes` enumerates
`fixtures/marks/*.json`, and a marks file whose wav is absent is a **named red**. So a bless run while
only private wavs sit in this tree writes a `fixtures/slicer-baseline.json` that passes on this machine
and reddens a fresh clone. Therefore:

- The tracked tree is blessed with **public-tier audio only**.
- Private-tier scoring happens in a **copy of the repo outside the working tree**, with the private wavs
  and their marks published there. Same scripts, same scorer, nothing tracked.

### `fixtures/user-groove.wav`: provenance audit

First, a correction to what this section said when the sheet was written. It claimed the file was
"referenced by no code, no doc, no test and no marks file". That was wrong: the grep that produced it
omitted `.mjs`. **`scripts/e2e.mjs` loads it as the browser suite's custom-sample leg** (the `fixture`
constant, uploaded through the file input), and `README.md` documented that dependency until commit
`9659b78` deleted the sentence while the code kept using the file. The clause that was right: it is
outside the slicer gate, because `loadTakes` reaches a wav only through `fixtures/marks/*.json` and no
marks file names it. So it cannot move any score - but it is load-bearing for `npm run test:browser`.

What the audit could and could not establish.

- **Git cannot establish it.** One commit ever (`192df10`, the initial 131-file import), one blob ever
  (`493a3d05379a…`, 768 044 B; sha256 `3040b1fd70cff830…`), never modified. That commit's message reasons explicitly about audio provenance - it names the six
  `native/Resources/library` WAVs as synthesised with deterministic seeds and distinguishes them from
  the commercial VRAC+ recordings - and it says nothing about this seventh audio file. The one commit
  that thought about licensing covered 6 of 7 and passed this one through undocumented.
- **The file cannot establish it.** The RIFF holds `fmt ` (16 B) and `data` and **zero bytes outside
  those chunks** - no LIST/INFO, no BWF, no encoder tag. That header shape is what a hand-rolled writer
  emits; ffmpeg, Audacity, Sound Forge and every DAW master page add a metadata chunk.
- **It is a construction, not a recording.** Measured: 48 kHz / 2 ch / 16-bit, exactly 4.0000 s, first
  non-silent frame 9600 (0.2000 s), last 187233 (3.9007 s), 14 non-silent runs separated by 13 gaps of
  **exact digital zero**, hit envelopes of 350.0 ms (and one 700.4 ms overlap), DC -73 dBFS, true
  stereo (energy(R-L)/energy(L) = 1.66). A room never prints 13 blocks of exact zero.
- **Not this project's own output.** Peak 27851 = floor(0.85 x 32767). `encodeWav` scales with
  `Math.round` (would give 27852) and the shipped kit export writes 24-bit; the factory recipe path
  normalises to 0.88 (would give 28835). drumgen's two WAV writers emit mono 44.1 kHz, and all 33 pack
  WAVs tracked there are 44.1 kHz (mono 16-bit or stereo 24-bit) - no rate, depth or channel count in
  that set matches. All 42 WAVs tracked across the family were hash-compared: only the file itself
  matches, and no tracked script in any of the thirteen git repos names it.
- **Not a paste of the factory library either.** Gain-invariant FFT correlation of each hit run against
  both channels of all six library WAVs plus the two synth fixtures peaked at 0.64 - a verbatim copy
  scores about 1.0, and chance for windows this long is about 0.04.

So: origin and redistribution rights are **not establishable from anything on disk**, and the residual
risk is not "a raw recording" but "hits rendered out of some third-party pack and gated onto a grid",
which the construction evidence cannot exclude. The option that closes the question without a licence
judgement: regenerate the fixture in-repo, byte-reproducibly, the way `fixtures/tools/synth-takes.cjs`
already does - the e2e leg asserts only `sourceId` starts with `user-` and `slices >= 4`, so a
replacement moves no acceptance threshold. The file was untouched when this was written, per your
ruling; what was done with it is below.

### Resolution: the fixture is generated (approved 2026-11-22)

The audit could not clear the old file and could not convict it either, so it is gone from the working
tree by replacement rather than by judgement.

- **`fixtures/tools/make-user-groove.cjs`** is the only source of `fixtures/user-groove.wav`.
  `node fixtures/tools/make-user-groove.cjs` writes it, `--check` re-renders in RAM, compares the bytes
  and audits the contract, and `--sha` prints the digest of the render. Constants, not material: five
  synthesised voices (kick, snare, hat, clank, pad), one xorshift32 PRNG seeded at `0x5eed51c0`, and the
  arrangement in the `SCORE` table. Nothing is read from a sample pack, and nothing is derived from the
  file it replaced.
- **Measured properties of the new bytes**: 1 728 044 B, sha256 `1998499da87776b9aafc76fd3e0b86e2…`,
  48 kHz / 2 ch / 16-bit PCM, exactly 9.0000 s, first non-silent frame 16800 (0.3500 s lead), int16 peak
  26214 = 0.8000 full scale with no sample at full scale, DC -73.4 dBFS per channel, true stereo
  (energy(R-L)/energy(L) = 0.174, `L != R`), and the same bare `RIFF/WAVE/fmt /data` container with zero
  bytes outside the two chunks - now because this writer writes nothing else, which is a claim the last
  file could not make.
- **What is gated**: `scripts/e2e.mjs` runs the render and compares it against the tracked file's hash
  as its own check (`custom-sample fixture re-renders byte-identical`), so the byte-reproducibility
  claim is exercised by the suite rather than asserted in prose. The census in `--check` reads
  `MIN_GAP_SECONDS` and `FLOOR_RATIO` out of `src/audio/slicerTuning.ts` instead of restating them. The
  existing acceptance thresholds are unchanged; this added a check, it did not move one.
- **The old bytes are still in the repository.** They live in commit `192df10` as blob
  `493a3d05379a…` and in every commit since; replacing the working file does not remove them from Git
  history, and no history was rewritten to do this. Recoverable with
  `git show 192df10:fixtures/user-groove.wav`.
- **Still outside the slicer gate.** No `fixtures/marks/*.json` names it, so `loadTakes` never sees it
  and `fixtures/slicer-baseline.json` cannot move because of it. The nine Step 25 renders are unaffected
  by this substitution.

## 7. Order of operations, and what each step can still refuse

```bash
# 1. the renders land in fixtures/ … then:
python fixtures/tools/verify-takes.py            # exit 0 required to go further
python fixtures/tools/make-takes.py --publish    # writes marks/<take>.marks.json, refuses any take whose wav is missing
cmake --build native/build --config Release --target SliceForgeSmokeTest
native/build/SliceForgeSmokeTest_artefacts/Release/SliceForgeSmokeTest.exe   # scores the nine vs the baseline
# 2. owner listening pass, section 8 -> owner approval
# 3. only then:
native/build/SliceForgeSmokeTest_artefacts/Release/SliceForgeSmokeTest.exe --bless-slicer-baseline
```

What the bless will do, so it is not a surprise: `fixtures/slicer-baseline.json` goes from **6 rows to
33** (nine takes × sensitivities 0.20 / 0.55 / 0.90, plus the two existing synthetic takes). The band is
symmetric, so an *improvement* reddens until blessed — that is the gate working, and it is why the bless
is argv-gated and ctest never passes it. Expect `capped = True` on seven of the nine at sens 0.20,
because `targetAt(0.20)` returns 17 slices and only `kick-quad` (16) and `crash-tail` (9) have fewer
references than that. `capped` is the ceiling, not a miss; the one that would be a genuine problem is a
take with `refN > 49`, and none of the nine has it — `hat-16th` at 48 is admissible by one slice.

`verify-takes.py` is self-tested against six synthesised controls, which also defines what it will *not*
tell you:

| Control | Result | Meaning |
|---|---|---|
| correct render, last hit decaying 40 dB/s → tail window −43.9 dBFS | **pass** | ring-down in the tail is legal (this would have failed an earlier absolute −48 dBFS rule, which is why the tail rules are change-based) |
| correct render, 60 dB/s decay → tail −46.5 dBFS | **pass** | same, deeper |
| stray hit at 6.600 s | **fail** — "peak rises 20.2 dB after 6.100 s" | material past the authored end is caught |
| clip shifted 1.1 s early | **fail** — "−8.0 dBFS in the 0.000-0.200 s pre-roll" | misplaced clip is caught |
| tail cut off at the last sample | **fail** — "−13.1 dBFS in the last 5 ms" | an end-of-file click is caught, and caught by that rule alone (a non-decaying tail raises no onset, so the rise test stays silent) |

Status of this sheet right now: the verifier is self-tested and the nine are **absent**, so
`verify-takes.py` exits 1 with all nine named. `--publish` has not been run. `--bless-slicer-baseline`
has not been run. No audio has been added to the repo. No binary was rebuilt or modified.

## 8. Listening checklist (ruling 3)

Two columns, deliberately: the left is already measured by the bench, so do not spend ears on it; the
right is only answerable by listening, and its answer is what the bless waits for. Musical quality is
not a proxy for the timing numbers and the timing numbers are not proof of musical quality.

Method: load each rendered wav into SliceForge standalone (or the browser edition), leave
`slice sensitivity` at each of 0.20 / 0.55 / 0.90, and audition the resulting pads. Judge the *kit*,
not the loop.

| Topic | Measured, don't re-listen | Ears only — record pass/fail and a word |
|---|---|---|
| **Transient placement** | `medAbsMs` / `maxAbsMs` / `biasMs` per take×sensitivity vs the .mid. Detector bias is currently ≈ −10 ms on the synthetic tier and the 6 ms pull-back is a known constant. | Do the slices *feel* early when you play them back, even inside the measured window? A −10 ms bias is measurable; whether it is audible at 37.5 ms spacing is not. |
| **Slice boundaries** | slice count vs `refN`, and `capped` rows prove the ceiling not the detector | Does a pad end where the note musically ends, or mid-word of a decay? Boundary count is not boundary sense. |
| **Clicks** | end-of-file level rule; DC rule | Clicks *inside* a slice, at pad starts, at zero-crossing cuts. The verifier only sees the last 5 ms of the file. |
| **Missing attacks** | `recall` / `recall25` against the reference onsets | An attack the detector kept but that arrived without its transient — a numerically matched onset can still sound truncated. |
| **Audible tails** | tail-window rise rule (no material begins past the authored end) | Whether the ring you hear is the source's decay or a cut artifact; whether `crash-tail`'s re-triggered toms still sit *on* the crash. |
| **Timing consistency** | `biasMs` sign stability across the nine takes | Drift that survives the numbers: the ghost in `swing-16ths` feeling late against the hat, `ride-32nd`'s wash turning into a smear at 0.90. |
| **Artifacts** | clipping rule, DC rule, level-pair equality | Ringing that is not the source (spectral smearing from the analysis window), pumping, and whether `toploop-b` at −18 dBFS loses quiet hits it keeps at −6. |

Then the two judgement calls no check makes: does the A/B pair sound like the *same* kit at two
levels (if the low one sounds like a different kit, the floor is doing something musical we did not
ask for), and would you ship a beat built from each kit? Record both against the take name.

The residual this sheet cannot close: a ±50 ms recall window on references 300 ms apart pays ~0.500
recall to a detector firing on nothing (AGENTS.md), so `recall` must always be read beside `recall25`,
`precision` and `fpPerMin` — and the dimension the public tier cannot test at all, real decay under
re-trigger, is exactly why `ride-32nd`, `swing-16ths` and `crash-tail` get a private confirmation run.
Whether that difference is audible is a Step 7 product question, not a render question.
