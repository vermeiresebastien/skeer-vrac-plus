"""Step 25 phase 3 - verify a rendered take before it is allowed near the baseline.

The bless is not the place to discover that an export came out at 16 bit, mono, with a limiter on
the master, or with the clip sitting where the grid does not claim it sits. So the render sheet
(`fixtures/tools/render-sheet.md`) names a spec, and this script is the only thing that decides
whether a wav satisfies it. Nothing here writes into `fixtures/`; it measures, prints and refuses.

    python fixtures/tools/verify-takes.py                 # verify the nine against the spec
    python fixtures/tools/verify-takes.py --report-only   # same census, always exit 0
    python fixtures/tools/verify-takes.py --measure a.wav # census arbitrary files, no verdict

What it checks, and why each one is a rule rather than a note:

  identity    sha256 of the exact bytes. A bless records a number for a file, so the file has to be
              nameable afterwards; a name and a date are not that.
  geometry    44100 Hz, 2 channels, 24-bit PCM, 317520 frames (7.200 s). The marks are wall-clock
              times derived from one tempo map, so a rate change silently rescales every reference.
  level       peak dBFS, and the two-level rule for the matched pair (toploop-b is the same grid 12 dB
              lower). A take that hit the ceiling is not a loud take, it is a distorted one.
  DC          a mean offset the detector's floor arithmetic would inherit.
  content     nothing audible before 0.200 s (the first authored onset is at 1.200 s), the region
              keeping 1.000 s of ring-out after the last authored onset, no rise in level past that
              guarantee, and no live tail cut off at the end of the file. The pre-roll catches a
              misplaced clip; the rise test catches an export wider than the region, a second loop
              pass or a stray hit - the material the marks file does not describe. Long ring-down is
              expected and must not be a red, so the tail rules are about change, not absolute level.
  pairing     toploop-a and toploop-b must be the same grid, so their onset times must be identical;
              if the two grids disagree the pair measures the arrangement and not the level.
"""
import argparse
import hashlib
import json
import math
import os
import struct
import sys
import wave

BPM = 200.0
BEAT = 60.0 / BPM
OFFSET_BEATS = 4.0
RATE = 44100
CHANNELS = 2
WIDTH_BITS = 24
DURATION_S = 7.200
FRAMES = int (round (DURATION_S * RATE))
FRAME_TOL = 22  # 0.5 ms: a locator render rounds to the sample, this only catches a wrong bar
PEAK_TARGET_DBFS = -6.0
PEAK_TOL_DBFS = 0.75
PAIR_LOW_TARGET_DBFS = -18.0
SILENCE_DBFS = -48.0
RISE_LOOKAHEAD_S = 0.25
RISE_TOL_DBFS = 3.0
END_WINDOW_S = 0.005
CLICK_DBFS = -40.0
FIRST_ONSET_S = OFFSET_BEATS * BEAT  # 1.200 s
PREROLL_S = FIRST_ONSET_S - 1.0      # 0.200 s
TAIL_GUARANTEE_S = 1.0

HERE = os.path.dirname (os.path.abspath (__file__))
FIXTURES = os.path.abspath (os.path.join (HERE, ".."))
NOTES = os.path.join (HERE, "notes")

TAKES = ["backbeat-8ths", "crash-tail", "hat-16th", "kick-quad", "ride-32nd",
         "swing-16ths", "tom-fill", "toploop-a", "toploop-b"]


def dbfs (x):
    return float ("-inf") if x <= 0.0 else 20.0 * math.log10 (x)


def sample_at (raw, i, width, full_scale):
    if width == 3:
        v = int.from_bytes (raw[i:i + 3], "little", signed = True)
    elif width == 4:
        v = int.from_bytes (raw[i:i + 4], "little", signed = True)
    elif width == 2:
        v = struct.unpack_from ("<h", raw, i)[0]
    else:
        v = raw[i] - 128
    return v / full_scale


def scan (raw, width, ch, from_s, to_s, rate):
    """One pass over [from_s, to_s) returning (peak, sum_squares, sum, n) in linear terms."""
    lo = max (0, int (from_s * rate)) * width * ch
    hi = min (to_s * rate, len (raw) // (width * ch)) * width * ch
    full_scale = float (1 << (width * 8 - 1))
    step = width * ch
    peak = 0.0
    sq = 0.0
    tot = 0.0
    n = 0
    for i in range (lo, hi, step):
        for c in range (ch):
            f = sample_at (raw, i + c * width, width, full_scale)
            a = abs (f)
            if a > peak:
                peak = a
            sq += f * f
            tot += f
            n += 1
    return peak, sq, tot, n


def measure (path):
    with wave.open (path, "rb") as w:
        rate, ch, width, frames = w.getframerate (), w.getnchannels (), w.getsampwidth (), w.getnframes ()
        raw = w.readframes (frames)
    peak, sq, tot, n = scan (raw, width, ch, 0.0, 1e9, rate)
    return dict (rate = rate, channels = ch, bits = width * 8, frames = frames, path = path,
                 peak = peak, rms = math.sqrt (sq / n) if n else 0.0, dc = tot / n if n else 0.0)


def window_peak_dbfs (path, from_s, to_s):
    with wave.open (path, "rb") as w:
        rate, ch, width = w.getframerate (), w.getnchannels (), w.getsampwidth ()
        frames = w.getnframes ()
        lo = max (0, int (from_s * rate))
        hi = min (frames, int (to_s * rate))
        if hi <= lo:
            return float ("-inf")
        w.setpos (lo)
        raw = w.readframes (hi - lo)
    peak, _, _, _ = scan (raw, width, ch, 0.0, 1e9, rate)
    return dbfs (peak)


def grid_onsets_s (take):
    with open (os.path.join (NOTES, take + ".notes.json")) as f:
        notes = json.load (f)
    return sorted ({round (n["start_time"] * BEAT + OFFSET_BEATS * BEAT, 6) for n in notes})


def sha256 (path):
    h = hashlib.sha256 ()
    with open (path, "rb") as f:
        for chunk in iter (lambda: f.read (1 << 20), b""):
            h.update (chunk)
    return h.hexdigest ()


def check (take, path, expect_peak):
    if not os.path.exists (path):
        return ["absent: " + os.path.relpath (path, FIXTURES).replace (os.sep, "/")], None
    fails = []
    m = measure (path)
    dur = m["frames"] / float (m["rate"]) if m["rate"] else 0.0
    if m["rate"] != RATE:
        fails.append ("sample rate %d, expected %d" % (m["rate"], RATE))
    if m["channels"] != CHANNELS:
        fails.append ("%d channel(s), expected %d" % (m["channels"], CHANNELS))
    if m["bits"] != WIDTH_BITS:
        fails.append ("%d-bit, expected %d" % (m["bits"], WIDTH_BITS))
    if abs (m["frames"] - FRAMES) > FRAME_TOL:
        fails.append ("%d frames (%.3f s), expected %d (%.3f s) - check the locator end and Tail = None"
                      % (m["frames"], dur, FRAMES, DURATION_S))
    pk = dbfs (m["peak"])
    if abs (pk - expect_peak) > PEAK_TOL_DBFS:
        fails.append ("peak %.2f dBFS, expected %.2f +/- %.2f" % (pk, expect_peak, PEAK_TOL_DBFS))
    if m["peak"] >= 1.0:
        fails.append ("full-scale sample present (clipping)")
    if abs (m["dc"]) > 5.0e-4:
        fails.append ("DC offset %.2e" % m["dc"])
    pre = window_peak_dbfs (path, 0.0, PREROLL_S)
    if pre > SILENCE_DBFS:
        fails.append ("%.1f dBFS in the 0.000-%.3f s pre-roll - the clip is not at bar 2.1"
                      % (pre, PREROLL_S))
    onsets = grid_onsets_s (take)
    last = onsets[-1]
    if not m["rate"]:
        return fails, None
    if dur - last < TAIL_GUARANTEE_S:
        fails.append ("file ends %.3f s after the last authored onset (%.3f s) - the region owes "
                      "%.1f s of ring-out, so the marks cannot describe what is still playing"
                      % (dur - last, last, TAIL_GUARANTEE_S))
    # A decaying ring is not a new onset, so the post-authored window cannot be an absolute level
    # rule: crash-tail has 2.100 s left after its last onset and a real crash fills it quietly.
    # What the reference cannot survive is audio that STARTS past the authored end - a second loop
    # pass, a stray hit, an export range wider than the region - so the rule is a rise test.
    tail_start = last + TAIL_GUARANTEE_S
    if dur > tail_start + RISE_LOOKAHEAD_S:
        before = window_peak_dbfs (path, tail_start - RISE_LOOKAHEAD_S, tail_start)
        after = window_peak_dbfs (path, tail_start, dur)
        if after > before + RISE_TOL_DBFS:
            fails.append ("peak rises %.1f dB after %.3f s (last onset %.3f s + %.1f s) - material "
                          "beginning past the authored end is not in the reference: %s"
                          % (after - before, tail_start, last, TAIL_GUARANTEE_S, take))
    end_peak = window_peak_dbfs (path, dur - END_WINDOW_S, dur)
    if end_peak > CLICK_DBFS:
        fails.append ("%.1f dBFS in the last %.0f ms - the render cut a live tail, which is a click "
                      "at the end of the file" % (end_peak, END_WINDOW_S * 1000.0))
    info = dict (sha = sha256 (path), dur = dur, peak = pk, rms = dbfs (m["rms"]), dc = m["dc"],
                 pre = pre, refs = len (onsets), last = last, tail = tail_start, end = end_peak)
    return fails, info


def main (argv):
    ap = argparse.ArgumentParser ()
    ap.add_argument ("--report-only", action = "store_true")
    ap.add_argument ("--measure", nargs = "*", default = [])
    ap.add_argument ("--dir", default = FIXTURES)
    a = ap.parse_args (argv)

    if a.measure:
        for p in a.measure:
            if not os.path.exists (p):
                print ("absent  %s" % p)
                continue
            m = measure (p)
            print ("%s  sha256 %s  %d Hz  %d ch  %d bit  %d frames  %.3f s  peak %.2f dBFS  "
                   "rms %.2f dBFS  dc %.2e"
                   % (os.path.basename (p), sha256 (p)[:16], m["rate"], m["channels"], m["bits"],
                      m["frames"], m["frames"] / float (m["rate"]), dbfs (m["peak"]),
                      dbfs (m["rms"]), m["dc"]))
        return 0

    bad = []
    print ("take            refs  dur(s)   peak     rms      dc       sha256[0:16]      verdict")
    for take in TAKES:
        expect = PAIR_LOW_TARGET_DBFS if take == "toploop-b" else PEAK_TARGET_DBFS
        fails, info = check (take, os.path.join (a.dir, take + ".wav"), expect)
        if fails:
            bad.append (take)
            print ("%-14s  %s%s" % (take, " " * 47, "FAIL: " + fails[0]))
            for extra in fails[1:]:
                print (" " * 64 + "- " + extra)
            continue
        print ("%-14s %4d  %6.3f  %6.2f  %6.2f  %6.1e  %s  ok (last onset %.3f s, pre-roll %.1f dBFS, "
               "tail from %.3f s, end %.1f dBFS)"
               % (take, info["refs"], info["dur"], info["peak"], info["rms"], info["dc"],
                  info["sha"][:16], info["last"], info["pre"], info["tail"], info["end"]))

    if os.path.exists (os.path.join (a.dir, "toploop-a.wav")) and os.path.exists (os.path.join (a.dir, "toploop-b.wav")):
        same = grid_onsets_s ("toploop-a") == grid_onsets_s ("toploop-b")
        print ("\nmatched pair: grids identical = %s, %d references on both"
               % (same, len (grid_onsets_s ("toploop-a"))))
        if not same:
            print ("FAIL: the two grids differ, so the pair is not level-only")
            return 1

    if bad and not a.report_only:
        print ("\nVERIFY FAILED for %d take(s): %s" % (len (bad), ", ".join (bad)))
        print ("No marks publish and no bless until this is green.")
        return 1
    if bad:
        print ("\n%d take(s) not yet conforming (report-only, exit 0)" % len (bad))
    else:
        print ("\nAll nine conform. Publishing marks and blessing are now allowed, in that order.")
    return 0


if __name__ == "__main__":
    sys.exit (main (sys.argv[1:]))
