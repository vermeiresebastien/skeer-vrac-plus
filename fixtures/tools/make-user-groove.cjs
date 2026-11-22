/*  fixtures/user-groove.wav - the browser suite's custom-sample leg, generated instead of inherited.

    The file this replaces arrived in the initial import with no generator, no note and no metadata
    chunk, so neither its origin nor its redistribution rights could be established from anything on
    disk (fixtures/tools/render-sheet.md section 6 records the audit). This removes the question rather
    than answering it: every sample here is computed from the constants below by this file alone.

        node fixtures/tools/make-user-groove.cjs         # writes fixtures/user-groove.wav
        node fixtures/tools/make-user-groove.cjs --check # re-render, compare, and audit the contract
        node fixtures/tools/make-user-groove.cjs --sha   # print the sha256 of the bytes it renders

    Reproducibility is the same contract as synth-takes.cjs holds: fixed seed, fixed positions, no
    clock, and `--check` comparing the fresh render against the tracked bytes instead of trusting this
    sentence. It does rest on the engine's transcendentals (Math.sin / Math.exp), so the honest claim is
    "this engine re-renders these bytes" - and `--check` is what turns a future engine that disagrees
    into a red run rather than a silent one. `scripts/e2e.mjs` runs it as a check of its own.

    Deliberately NOT part of the slicer corpus: SlicerScoreTest enumerates fixtures/marks/*.json, and
    this take is a browser-test input, not a reference. Writing a marks file for it would pull it into
    fixtures/slicer-baseline.json.
*/
const fs = require ('fs');
const path = require ('path');
const crypto = require ('crypto');

const RATE = 48000;            // the browser AudioContext's own rate (src/audio/audioEngine.ts)
const CHANNELS = 2;            // true stereo: the mix is panned, not duplicated
const BITS = 16;               // PCM, the same container the kit export uses for 16-bit
const LEAD = 0.35;             // silence before the downbeat, so a slice edge is not at frame 0
const BEAT = 0.5;              // 120 BPM
const BARS = 4;                // 16 beats of groove
const TAIL = 0.65;             // ring-out after the last beat, so the file does not end on a hit
const SECONDS = LEAD + BEAT * BARS * 4 + TAIL;
const FRAMES = Math.round (RATE * SECONDS);
const PEAK_TARGET = 0.80;
const SEED = 0x5eed51c0;

/** The pinned digest of the tracked bytes. `--check` fails if a render disagrees with it, which is the
 *  half that a byte comparison alone cannot prove: two different renders can agree with each other. */
const EXPECTED_SHA256 = '1998499da87776b9aafc76fd3e0b86e28900d87a688902907b6aa17bbc42c983';

const outPath = path.join (__dirname, '..', 'user-groove.wav');
const tuningPath = path.join (__dirname, '..', '..', 'src', 'audio', 'slicerTuning.ts');

/*  The detector's own thresholds, read out of the module rather than restated here. A fixture that is
 *  "obviously sliceable" by eye is worth nothing if the tuning moves under it, and this repo already
 *  parses a constants module as text to keep two copies honest (src/lib/__tests__/importLimits.test.ts
 *  does exactly this to the C++ header). */
function tuning ()
{
    const src = fs.readFileSync (tuningPath, 'utf8');
    const one = (name) => {
        const found = new RegExp (`export const ${name}\\s*=\\s*([0-9.]+)`).exec (src);
        if (found == null) throw new Error (`${name} is not stated in src/audio/slicerTuning.ts`);
        return Number (found[1]);
    };
    return { minGap: one ('MIN_GAP_SECONDS'), floor: one ('FLOOR_RATIO') };
}

/** xorshift32, so "noise" means the same numbers on every run. */
function noise (seed)
{
    let s = (seed >>> 0) || 1;
    return () => {
        s ^= s << 13; s >>>= 0;
        s ^= s >>> 17;
        s ^= s << 5; s >>>= 0;
        return s / 0x80000000 - 1;
    };
}

/*  Five voices, each a closed form over its own sample window. Amplitude shape is always
 *  (1 - e^-t/attack) * e^-t/decay, so every event starts at exact zero and nothing depends on a
 *  previous event's filter state. */

function kick (n, amp)
{
    const rnd = noise (SEED ^ 0x1a2b3c);
    const out = new Float64Array (n);
    let phase = 0;
    for (let i = 0; i < n; ++i) {
        const t = i / RATE;
        const hz = 46 + 88 * Math.exp (-t / 0.020);          // 134 Hz dropping to 46 Hz
        phase += 2 * Math.PI * hz / RATE;
        if (phase > 2 * Math.PI) phase -= 2 * Math.PI;
        const env = (1 - Math.exp (-t / 0.0011)) * Math.exp (-t / 0.085);
        out[i] = amp * env * Math.sin (phase) + rnd () * amp * Math.exp (-t / 0.0016) * 0.18;
    }
    return out;
}

function snare (n, amp)
{
    const rnd = noise (SEED ^ 0x2b3c4d);
    const out = new Float64Array (n);
    let p1 = 0, p2 = 0, hp = 0, hx = 0;
    for (let i = 0; i < n; ++i) {
        const t = i / RATE;
        const body = 0.55 * Math.sin (p1) + 0.45 * Math.sin (p2);
        p1 += 2 * Math.PI * 186 / RATE;
        p2 += 2 * Math.PI * 213 / RATE;
        const x = rnd ();
        hp = 0.82 * (hp + x - hx);                            // one-pole highpass, twice
        hx = x;
        const snap = hp * (1 - Math.exp (-t / 0.0006)) * Math.exp (-t / 0.036);
        out[i] = amp * (0.5 * body * Math.exp (-t / 0.055) + snap * 1.5);
    }
    return out;
}

function hat (n, amp)
{
    const rnd = noise (SEED ^ 0x3c4d5e);
    const out = new Float64Array (n);
    let a = 0, b = 0, xa = 0, xb = 0;
    for (let i = 0; i < n; ++i) {
        const t = i / RATE;
        const x = rnd ();
        a = 0.72 * (a + x - xa); xa = x;                      // cascaded highpass = metallic fizz
        b = 0.72 * (b + a - xb); xb = a;
        out[i] = amp * b * (1 - Math.exp (-t / 0.0004)) * Math.exp (-t / 0.024);
    }
    return out;
}

function clank (n, amp)
{
    const rnd = noise (SEED ^ 0x4d5e6f);
    const out = new Float64Array (n);
    let p1 = 0, p2 = 0, hp = 0, hx = 0;
    for (let i = 0; i < n; ++i) {
        const t = i / RATE;
        p1 += 2 * Math.PI * 1180 / RATE;
        p2 += 2 * Math.PI * 2637 / RATE;                      // inharmonic second partial
        const x = rnd ();
        hp = 0.9 * (hp + x - hx); hx = x;
        out[i] = amp * ((0.6 * Math.sin (p1) + 0.4 * Math.sin (p2)) * Math.exp (-t / 0.045)
                        + hp * Math.exp (-t / 0.004) * 0.5);
    }
    return out;
}

/** The sustained voice: three low-passed partials of an open-fifth chord, held for `hold` seconds.
 *  This is what makes the take a mix of transient and sustained material rather than a click train, and
 *  it is what keeps a level on the two-beat rest: the file holds above -60 dBFS from 1.35 to 6.10 s and
 *  again from 6.35 to the end. It does not fill every frame - a quiet 110 Hz partial rounds to zero at
 *  16-bit on most crossings, so 9.7% of the frames are exact zero in 1213 short runs, and the only long
 *  silences are the 0.35 s lead and a 278 ms breather at 6.07 s. */
function pad (n, amp, hold)
{
    const out = new Float64Array (n);
    let p1 = 0, p2 = 0, p3 = 0, l1 = 0, l2 = 0, l3 = 0;
    const release = 0.45;
    for (let i = 0; i < n; ++i) {
        const t = i / RATE;
        p1 += 2 * Math.PI * 110.00 / RATE;
        p2 += 2 * Math.PI * 164.81 / RATE;
        p3 += 2 * Math.PI * 219.00 / RATE;
        const s = 0.45 * Math.sin (p1) + 0.33 * Math.sin (p2) + 0.22 * Math.sin (p3);
        l1 += 0.22 * (s - l1);                                // two one-pole stages: no edge to key on
        l2 += 0.22 * (l1 - l2);
        l3 = l2 * (1 + 0.06 * Math.sin (2 * Math.PI * 0.18 * t));   // slow, shallow, almost stationary
        const end = hold - release;
        const env = (1 - Math.exp (-t / 0.22))
                    * (t < end ? 1 : Math.max (0, 1 - (t - end) / release));
        if (t > hold) break;
        out[i] = amp * l3 * env;
    }
    return out;
}

const VOICES = { kick, snare, hat, clank };

/*  One 4-bar line. Beats 6 and 7 are left empty on purpose: a bar-and-a-bit of pad with nothing struck on
 *  it is the difference between a groove and a grid, it is what the sustained-material check below
 *  measures, and it gives the detector a stretch where the right answer is "no slice here".
 *  `every` is the ring-out length asked of the voice, in seconds. */
const SCORE = [
    { b: 0,  v: 'kick',  amp: 0.95, pan: 0.50 },
    { b: 1,  v: 'hat',   amp: 0.34, pan: 0.66 },
    { b: 2,  v: 'pad',   amp: 0.30, pan: 0.30, every: 4.0 },
    { b: 3,  v: 'snare', amp: 0.62, pan: 0.42 },
    { b: 4,  v: 'kick',  amp: 0.90, pan: 0.50 },
    { b: 4.5,v: 'clank', amp: 0.40, pan: 0.60 },
    { b: 5,  v: 'hat',   amp: 0.30, pan: 0.36 },
    { b: 8,  v: 'kick',  amp: 0.95, pan: 0.52 },
    { b: 9,  v: 'hat',   amp: 0.34, pan: 0.64 },
    { b: 10, v: 'snare', amp: 0.66, pan: 0.44 },
    { b: 11, v: 'hat',   amp: 0.28, pan: 0.40 },
    { b: 12, v: 'kick',  amp: 0.92, pan: 0.48 },
    { b: 13, v: 'pad',   amp: 0.26, pan: 0.72, every: 3.4 },
    { b: 14, v: 'snare', amp: 0.60, pan: 0.46 },
    { b: 15, v: 'clank', amp: 0.36, pan: 0.58 },
];

function render ()
{
    const left = new Float64Array (FRAMES);
    const right = new Float64Array (FRAMES);
    const span = (v, every) => Math.ceil (RATE * (v === 'pad' ? every : (every || 1.2)));
    for (const ev of SCORE) {
        const start = Math.round (RATE * (LEAD + ev.b * BEAT));
        const len = Math.min (span (ev.v, ev.every), FRAMES - start);
        const voice = ev.v === 'pad'
            ? pad (len, ev.amp, ev.every)
            : VOICES[ev.v] (len, ev.amp);
        const gl = Math.cos (ev.pan * Math.PI / 2);            // constant-power pan
        const gr = Math.sin (ev.pan * Math.PI / 2);
        for (let i = 0; i < len; ++i) {
            left[start + i] += voice[i] * gl;
            right[start + i] += voice[i] * gr;
        }
    }
    let peak = 0;
    for (let i = 0; i < FRAMES; ++i)
        peak = Math.max (peak, Math.abs (left[i]), Math.abs (right[i]));
    const scale = peak > 0 ? PEAK_TARGET / peak : 1;

    const data = Buffer.alloc (FRAMES * CHANNELS * 2);
    for (let i = 0; i < FRAMES; ++i) {
        const l = Math.max (-1, Math.min (1, left[i] * scale));
        const r = Math.max (-1, Math.min (1, right[i] * scale));
        data.writeInt16LE (Math.round (l * 32767), i * 4);
        data.writeInt16LE (Math.round (r * 32767), i * 4 + 2);
    }
    const header = Buffer.alloc (44);
    header.write ('RIFF', 0);
    header.writeUInt32LE (36 + data.length, 4);
    header.write ('WAVE', 8);
    header.write ('fmt ', 12);
    header.writeUInt32LE (16, 16);
    header.writeUInt16LE (1, 20);
    header.writeUInt16LE (CHANNELS, 22);
    header.writeUInt32LE (RATE, 24);
    header.writeUInt32LE (RATE * CHANNELS * 2, 28);
    header.writeUInt16LE (CHANNELS * 2, 32);
    header.writeUInt16LE (BITS, 34);
    header.write ('data', 36);
    header.writeUInt32LE (data.length, 40);
    return { bytes: Buffer.concat ([header, data]), left, right, scale, peak };
}

/*  Read the artifact back rather than trusting the score, so "at least four meaningful, independently
 *  detectable events" is a property of the bytes - a voice that stops ringing before it can be heard
 *  reddens here.
 *
 *  One envelope, at 50 ms, and it is not the obvious choice. An 8 ms window was tried first and is
 *  shorter than one period of the pad's 110 Hz fundamental (9.1 ms), so its short-time RMS flickers on
 *  material that is perfectly steady: the 3.00-4.00 s pad window measured 10.45 dB of spread at an 8 ms
 *  hop against 0.73 dB at 50 ms, and no 1.0 s window anywhere in the file got under 7.27 dB at 8 ms - the
 *  bound was unsatisfiable rather than strict. The 8 ms onset detector also reported 21 "events" for a
 *  pad-only render of two pads. Both of this file's sustained claims need a window that can resolve the
 *  lowest voice in it.
 *
 *  Onsets are a Schmitt trigger: armed from the running trough, fires 10 dB above it, and only re-arms
 *  once the level has fallen 6 dB from that peak - so one long pad entry counts as one event. What it is
 *  NOT is a model of the product's slicer. It is deliberately coarse, and it is knowingly blind: on the
 *  shipped file it finds 9 of the 15 scored events (the ones riding on the held pad never sit 10 dB over
 *  the pad under them), and a probe that moved the clank to 50 ms after the kick still reported the
 *  tightest gap as 0.500 s. That blindness is why no spacing claim is made here - see `authoredGap`. */

function envelope (left, right, scale, hopSeconds)
{
    const hop = Math.round (RATE * hopSeconds);
    const env = [];
    for (let i = 0; i + hop <= FRAMES; i += hop) {
        let e = 0;
        for (let k = 0; k < hop; ++k) {
            const l = left[i + k] * scale, r = right[i + k] * scale;
            e += l * l + r * r;
        }
        env.push (10 * Math.log10 (e / hop / 2 + 1e-12));
    }
    return env;
}

function census (left, right, tun)
{
    const HOP = 0.050;
    const env = envelope (left, right, tun.scale, HOP);
    // FLOOR_RATIO is "this fraction of the loudest rise in the file", stated on the envelope it gates.
    const floor = Math.max (...env) + 20 * Math.log10 (tun.floor);
    const onsets = [];
    let armed = true, trough = env[0], peak = env[0];
    for (let i = 1; i < env.length; ++i) {
        if (armed) {
            if (env[i] - trough >= 10 && env[i] >= floor) {
                onsets.push (i * HOP);
                armed = false;
                peak = env[i];
            } else {
                trough = Math.min (trough, env[i]);
            }
        } else {
            peak = Math.max (peak, env[i]);
            if (env[i] <= peak - 6) { armed = true; trough = env[i]; }
        }
    }
    let transient = false;
    for (const t of onsets) {
        const at = (s) => env[Math.min (env.length - 1, Math.round ((t + s) / HOP))];
        if (at (0.05) - at (0.30) >= 12) transient = true;     // falls away: a hit, not a hum
    }
    // Sustained material, measured as a whole second that holds its level. Deliberately not tied to an
    // onset: the pad in this take runs under a rest, and a soft entry is not itself a slice point.
    const hold = Math.round (1.0 / HOP);
    let sustained = false, held = 0;
    for (let i = 0; i + hold < env.length; ++i) {
        const slice = env.slice (i, i + hold);
        const hi = Math.max (...slice), lo = Math.min (...slice);
        if (hi >= floor && hi - lo <= 6) { sustained = true; held = Math.max (held, hi); }
    }
    return { onsets, transient, sustained, floorDb: floor };
}

/*  The one spacing claim this file can honestly make, and it belongs to the arrangement rather than to a
 *  detector: no two events the take asks to be heard sit closer than the detector's own MIN_GAP_SECONDS,
 *  so nothing in the fixture depends on the slicer resolving sub-gap material. Measured off SCORE because
 *  any detector with a refractory window - including the one above - reports the tightest gap as its own
 *  suppression constant, which is arithmetic and not evidence. */
function authoredGap ()
{
    const t = SCORE.map (ev => ev.b * BEAT).sort ((a, b) => a - b);
    let gap = Infinity;
    for (let i = 1; i < t.length; ++i)
        gap = Math.min (gap, t[i] - t[i - 1]);
    return gap;
}

function report (label, bytes)
{
    const digest = crypto.createHash ('sha256').update (bytes).digest ('hex');
    console.log (`${label} sha256 ${digest.slice (0, 16)}… bytes ${bytes.length} `
                 + `${RATE} Hz ${CHANNELS} ch ${BITS} bit ${(FRAMES / RATE).toFixed (3)} s`);
    return digest;
}

const mode = process.argv.includes ('--check') ? 'check'
           : process.argv.includes ('--sha') ? 'sha' : 'write';

const tun = { ...tuning (), scale: 1 };
const { bytes, left, right, scale, peak } = render ();
tun.scale = scale;

if (mode === 'sha') {
    console.log (crypto.createHash ('sha256').update (bytes).digest ('hex'));
    process.exit (0);
}

if (mode === 'write') {
    report ('wrote', bytes);
    fs.writeFileSync (outPath, bytes);
    console.log (`      ${SCORE.length} scored events, pre-normalise peak ${peak.toFixed (4)} -> `
                 + `${PEAK_TARGET.toFixed (2)} (x${scale.toFixed (4)})`);
    process.exit (0);
}

let bad = 0;
const onDisk = fs.readFileSync (outPath);
const fresh = crypto.createHash ('sha256').update (bytes).digest ('hex');
const bytesSame = onDisk.equals (bytes);
const diskSha = crypto.createHash ('sha256').update (onDisk).digest ('hex');
console.log (`${bytesSame ? 'ok  ' : 'DIFF'} re-render matches the tracked bytes (${onDisk.length} B)`);
console.log (`${diskSha === EXPECTED_SHA256 ? 'ok  ' : 'DIFF'} tracked sha256 ${diskSha.slice (0, 16)}… `
             + `against the pinned ${EXPECTED_SHA256.slice (0, 16)}…`);
if (! bytesSame || diskSha !== EXPECTED_SHA256) ++bad;

const c = census (left, right, tun);
const say = (cond, msg) => { console.log (`${cond ? 'ok  ' : 'FAIL'} ${msg}`); if (! cond) ++bad; };
const gap = authoredGap ();
say (c.onsets.length >= 4, `${c.onsets.length} events found in the bytes by a coarse 50 ms backstop (the e2e leg needs the app to find >= 4)`);
say (gap >= tun.minGap, `tightest authored spacing ${gap.toFixed (3)} s >= MIN_GAP_SECONDS ${tun.minGap}`);
say (c.transient, 'transient material present (a rise that falls >= 12 dB within 250 ms)');
say (c.sustained, 'sustained material present (energy that holds within 6 dB for a second, 50 ms window)');
say (onDisk.readUInt32LE (24) === RATE && onDisk.readUInt16LE (34) === BITS && onDisk.readUInt16LE (22) === CHANNELS,
     `header says ${onDisk.readUInt32LE (24)} Hz / ${onDisk.readUInt16LE (22)} ch / ${onDisk.readUInt16LE (34)} bit`);
console.log (`      ${SCORE.length} scored events, pre-normalise peak ${peak.toFixed (4)} -> `
             + `${PEAK_TARGET.toFixed (2)}, detection floor ${c.floorDb.toFixed (1)} dBFS`);
process.exit (bad === 0 ? 0 : 1);
