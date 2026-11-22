"""Step 25 phase 3 - the nine rendered takes, authored as note grids.

One global tempo (200 BPM) because Live has one project tempo and the export is a manual pass;
what the detector cares about is the inter-onset interval in seconds, and 200 BPM puts every
regime the tuning has to survive on a standard grid: 8th = 150 ms, 16th = 75 ms, 32nd = 37.5 ms,
swung 16th = 50/25 ms - i.e. both sides of kMinGapSeconds (115 ms) inside one take.

Region geometry per take: 24 beats (7.2 s). The 4-bar clip sits at clip-arrangement beat +4, so
the first downbeat lands at 1.2 s into the file and every onset is >= 1.2 s from the end.
`fixtures/marks-src/<take>.mid` therefore carries the +4 offset in its own note positions: the
.mid timeline IS the file timeline, which is what makes the scorer's midi-vs-JSON agreement
check mean something.

What this script writes, and where, is a licensing decision rather than a file-layout preference.
A `midi-note-times` marks file commits its audio with it: `SlicerScoreTest.cpp` turns a marks file
whose wav is missing or undecodable into a named red, so committing a reference for a take nobody
has rendered takes the suite down. So the two things that are pure authorship - the .mid and the
notes grid - live in the tracked tree, and the marks JSON is not written at all until the rendered
wav is already on disk beside it:

    python fixtures/tools/make-takes.py              # write fixtures/marks-src/*.mid + notes/*.json
    python fixtures/tools/make-takes.py --check      # prove the tracked files are re-derivable
    python fixtures/tools/make-takes.py --publish    # also write fixtures/marks/*.marks.json

The .mid is the durable half: the marks JSON is a projection of it, so it needs no staging directory
and cannot go stale. The rendered audio is deliberately not this script's business - a bounce of a
third-party sample pack through a commercial DAW is the owner's call, not a step to automate past him.

Coverability is not computed here either. How many slices a knob can reach belongs to
`kTargetBase`/`kTargetSpan` plus `kMaxSlices`, and the gated authority on it is the scorer's
"every fixture take is coverable by at least one sensitivity in the grid" check - which refuses the
bless, not just the run. This script prints `onsets=` (the reference count) and the tightest
inter-onset interval; the suite decides whether the grid can cover them.
"""
import json, os, sys

BPM = 200.0
TPQ = 480
OFFSET_BEATS = 4.0
HERE = os.path.dirname (os.path.abspath (__file__))
FIXTURES = os.path.abspath (os.path.join (HERE, ".."))
PUBLISH = "--publish" in sys.argv
CHECK = "--check" in sys.argv

KICK, SNARE, HAT, TOM_LO, TOM_HI, CRASH, RIDE = 36, 38, 42, 45, 47, 49, 51

def arange (a, n, step):
    return [a + step * i for i in range (n)]

TAKES = {}
TAKES["kick-quad"]    = ([(KICK, b, 0.25, 110) for b in arange (0, 16, 1.0)]
                       + [(SNARE, b, 0.25, 100) for b in (4, 12)])
TAKES["hat-16th"]     = ([(HAT, b, 0.20, 96) for b in arange (0, 48, 0.25)]
                       + [(KICK, b, 0.25, 112) for b in (0, 4, 8)])
TAKES["backbeat-8ths"]= ([(HAT, b, 0.20, 92) for b in arange (0, 32, 0.5)]
                       + [(KICK, b, 0.25, 115) for b in (0, 8)]
                       + [(SNARE, b, 0.25, 105) for b in (4, 12)])
TAKES["crash-tail"]   = ([(CRASH, 0.0, 3.0, 104), (TOM_HI, 2.0, 0.5, 100), (TOM_HI, 3.0, 0.4, 96),
                          (TOM_LO, 3.5, 0.4, 92), (TOM_HI, 8.0, 0.5, 100), (TOM_LO, 10.0, 0.5, 96),
                          (TOM_HI, 11.0, 0.4, 92), (TOM_LO, 11.5, 0.4, 88), (TOM_HI, 13.0, 0.5, 96)])
TAKES["tom-fill"]     = ([(TOM_LO, b, 0.4, 100) for b in (0, 2, 4, 6)]
                       + [(TOM_HI if i % 2 else TOM_LO, 12.0 + i * 0.25, 0.2, 88 + 4 * (i % 4))
                          for i in range (16)])
TAKES["ride-32nd"]    = [(RIDE, b, 0.10, 98) for b in arange (0, 32, 0.125)]
_toploop = ([(HAT, b, 0.18, 90) for b in arange (0, 32, 0.5)]
          + [(KICK, b, 0.25, 114) for b in (0, 2.75, 4, 6.5, 8, 10.75, 12, 14.5)]
          + [(SNARE, b, 0.25, 104) for b in (2, 6, 10, 14)])
TAKES["toploop-a"]    = _toploop
TAKES["toploop-b"]    = _toploop                      # the level-matched pair: same notes, -12 dB trim
_swing = []
for _i in range (16):                                  # swung 16ths over 8 beats: 50 / 25 ms apart
    _swing.append ((HAT, _i * 0.5, 0.10, 94))
    _swing.append ((SNARE if _i % 4 == 3 else HAT, _i * 0.5 + 1.0 / 3.0, 0.10, 88))
TAKES["swing-16ths"]  = _swing

KIT = {"kick-quad": "kick + snare", "hat-16th": "closed hat + kick", "backbeat-8ths": "full kit 8ths",
       "crash-tail": "crash + toms, long ring", "tom-fill": "toms, 16th fill", "ride-32nd": "ride 32nds",
       "toploop-a": "top loop, reference gain", "toploop-b": "top loop, -12 dB trim", "swing-16ths": "swung 16ths"}

# The render level per take. toploop-b is toploop-a's grid 12 dB lower, so one literal for all nine
# would publish a false claim about it - and nothing gates this field: SlicerScoreTest.cpp reads
# schema, take, markProvenance, wav and onsets only. verify-takes.py is what refuses a render that
# disagrees with what is written here, so the two must stay in step.
PEAK_DBFS = {t: (-18.0 if t == "toploop-b" else -6.0) for t in TAKES}

def onsets_seconds (notes):
    seen = set()
    for _, start, _, _ in notes:
        for v in (start,):
            seen.add (round (v, 9))
    return sorted ((o + OFFSET_BEATS) * 60.0 / BPM for o in seen)

def varlen (v):
    if v < 0:
        raise SystemExit ("negative delta time: %d" % v)
    out = bytearray ([v & 0x7F]); v >>= 7
    while v:
        out.insert (0, (v & 0x7F) | 0x80)
        v >>= 7
    return bytes (out)

def mid_bytes (notes):
    """Format-1 SMF: track 0 = tempo map only, track 1 = the notes. Ticks are absolute positions
    converted to deltas, which is what JUCE's readFrom expects before convertTimestampTicksToSeconds."""
    trk0 = varlen (0) + bytes ([0xFF, 0x51, 0x03]) + (int (round (60_000_000 / BPM))).to_bytes (3, "big")
    trk0 += varlen (0) + bytes ([0xFF, 0x2F, 0x00])
    ev = []
    for pitch, start, dur, vel in notes:
        t0 = int (round ((OFFSET_BEATS + start) * TPQ))
        t1 = int (round ((OFFSET_BEATS + start + max (dur, 0.05)) * TPQ))
        ev.append ((t0, [0x90, pitch, vel])); ev.append ((t1, [0x80, pitch, 0x00]))
    ev.sort (key = lambda e: (e[0], e[1][0] & 0xF0, e[1][1]))
    out = bytearray (); last = 0
    for t, msg in ev:
        out += varlen (t - last); out += bytes (msg); last = t
    out += varlen (0) + bytes ([0xFF, 0x2F, 0x00])
    def chunk (tag, body):
        return tag + len (body).to_bytes (4, "big") + body
    # The tag is MTrk with a capital T. JUCE's reader skips any chunk whose tag is not exactly
    # "MTrk" and readFrom still returns true, so a lowercase typo yields a file that "parses"
    # with zero tracks - see the guard in SlicerScoreTest.cpp's midNoteOnsets.
    return b"MThd" + (6).to_bytes (4, "big") + (1).to_bytes (2, "big") + (2).to_bytes (2, "big") \
         + TPQ.to_bytes (2, "big") + chunk (b"MTrk", bytes (trk0)) + chunk (b"MTrk", bytes (out))

def notes_json (notes):
    return json.dumps ([{"pitch": p, "start_time": s, "duration": d, "velocity": v, "mute": False}
                        for p, s, d, v in notes])

def marks_json (take, ons):
    doc = {"schema": "sliceforge-slicer-marks/1", "take": take, "wav": take + ".wav",
           "sampleRate": 44100, "bpm": BPM, "clipBars": 4, "regionBeats": 24, "kit": KIT[take],
           "chain": "dry: no master limiter, no sends, monitor off", "peakDbfs": PEAK_DBFS[take],
           "markProvenance": "midi-note-times",
           "onsets": [round (o, 6) for o in ons]}
    return json.dumps (doc, indent = 2) + "\n"

if PUBLISH and CHECK:
    raise SystemExit ("--publish writes, --check does not; pick one")

blobs = {}
summary = []
missing = []
for take, notes in TAKES.items ():
    ons = onsets_seconds (notes)
    blobs[os.path.join (FIXTURES, "marks-src", take + ".mid")] = mid_bytes (notes)
    blobs[os.path.join (HERE, "notes", take + ".notes.json")] = (notes_json (notes) + "\n").encode ("utf-8")
    if PUBLISH:
        if not os.path.exists (os.path.join (FIXTURES, take + ".wav")):
            missing.append (take)
            continue
        blobs[os.path.join (FIXTURES, "marks", take + ".marks.json")] = marks_json (take, ons).encode ("utf-8")
    gaps = [(ons[i + 1] - ons[i]) * 1000.0 for i in range (len (ons) - 1)]
    summary.append ("%s notes=%d onsets=%d first=%.3fs last=%.3fs minIOI=%.1fms"
                    % (take, len (notes), len (ons), ons[0], ons[-1], min (gaps)))

if missing:
    raise SystemExit ("refusing to publish marks with no audio beside them: " + ", ".join (missing)
                      + " - a marks file whose wav is missing is a named red in SliceForgeSmokeTest."
                      + " Bounce the take into fixtures/ first, or drop --publish.")

if CHECK:
    # A clone of this repo is not byte-identical to this worktree: core.autocrlf=true with no
    # .gitattributes means Git hands out the LF text payloads with CRLF pairs (verified with
    # `git checkout-index`), while the .mid files are binary to Git and arrive exactly as stored.
    # So the text half gets that one translation undone before comparing, and the count is printed -
    # a drift nobody has to discover by finding the check red on a fresh machine.
    bad = []
    exact = crlf = 0
    for path, want in sorted (blobs.items ()):
        have = open (path, "rb").read () if os.path.exists (path) else None
        if have == want:
            exact += 1
        elif path.endswith (".json") and have is not None and have.replace (b"\r\n", b"\n") == want:
            crlf += 1
        else:
            bad.append ("%s %s" % ("absent " if have is None else "differs", path))
    print ("\n".join (summary))
    if bad:
        raise SystemExit ("TAKE GENERATOR CHECK FAILED - re-derived bytes do not match the tree:\n"
                          + "\n".join (bad))
    print ("OK - every tracked .mid and notes grid is re-derived by this script (%d files: %d byte-exact,"
           " %d identical once the checkout's LF->CRLF is undone)" % (len (blobs), exact, crlf))
    raise SystemExit (0)

for path, data in sorted (blobs.items ()):
    os.makedirs (os.path.dirname (path), exist_ok = True)
    with open (path, "wb") as f:
        f.write (data)
print ("\n".join (summary))
print ("wrote %d files%s" % (len (blobs), " (marks published into fixtures/marks)" if PUBLISH else ""))
