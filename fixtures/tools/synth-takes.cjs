/*  Tier 1 of the slicer fixture corpus: synthetic takes whose reference onsets are exact by
    construction, so `fixtures/slicer-baseline.json` can never be a table of nothing.
    Every wav and every marks file under fixtures/ that this script owns is re-derivable by
    running it, which is the same standard a `midi-note-times` take is held to by the marks-src
    rule - a list of times somebody typed cannot be re-checked, these bytes can.

    Deterministic by design: fixed amplitudes, fixed positions, no randomness and no clock.

        node fixtures/tools/synth-takes.cjs            # writes into fixtures/
        node fixtures/tools/synth-takes.cjs --check    # re-render in RAM and compare against disk
*/
const fs = require ('fs');
const path = require ('path');
const crypto = require ('crypto');

const rate = 44100;
const fixturesDir = path.join (__dirname, '..');
const marksDir = path.join (fixturesDir, 'marks');
const checkOnly = process.argv.includes ('--check');

function render ({ widthMs = 2.5, hz = 3800, amp = 0.8 }, t)
{
    // One band-limited-ish decaying click. The envelope is what makes it a fair test: the detector
    // sees a rise, not a discontinuity, so a fixture cannot be solved by reading a sample jump.
    const len = Math.floor (rate * widthMs / 1000);
    const start = Math.floor (rate * t);
    return { start, len };
}

function click (sample, { start, len }, amp, hz)
{
    for (let i = 0; i < len; ++i)
    {
        const env = Math.exp (-i / (len * 0.35));
        const v = amp * env * Math.sin (2 * Math.PI * hz * i / rate);
        sample[start + i] = v;
    }
}

function wav (seconds, onsets, shape)
{
    const n = Math.floor (rate * seconds);
    const left = new Float64Array (n);
    const right = new Float64Array (n);
    for (const t of onsets)
    {
        const geo = render (shape, t);
        click (left, geo, shape.amp, shape.hz);
        click (right, geo, shape.amp, shape.hz);
    }
    const data = Buffer.alloc (n * 4);
    for (let i = 0; i < n; ++i)
    {
        const l = Math.max (-1, Math.min (1, left[i]));
        const r = Math.max (-1, Math.min (1, right[i]));
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
    header.writeUInt16LE (2, 22);
    header.writeUInt32LE (rate, 24);
    header.writeUInt32LE (rate * 4, 28);
    header.writeUInt16LE (4, 32);
    header.writeUInt16LE (16, 34);
    header.write ('data', 36);
    header.writeUInt32LE (data.length, 40);
    return Buffer.concat ([header, data]);
}

const grid = (first, step, count) => Array.from ({ length: count }, (_, i) => first + step * i);
const flat = { amp: 0.8, widthMs: 2.5, hz: 3800 };

// `refN` is what each take asks the grid to cover, and the comment next to it is the reason the
// take is in the corpus: one that every knob can reach, and one that only the upper knobs can.
const takes = [
    {
        name: 'synth-clean',
        seconds: 8.0,
        onsets: grid (0.5, 0.5, 12), // refN 12: under 17, so coverable at all three sensitivities
        shape: flat,
        note: 'twelve evenly spaced hits, the simplest thing the detector can be asked to find',
    },
    {
        name: 'synth-16ths',
        seconds: 7.0,
        onsets: grid (0.3, 0.2, 30), // refN 30: over targetAt(0.20)=17, so capped at the low knob only
        shape: { amp: 0.65, widthMs: 3.0, hz: 6200 },
        note: 'a thirty-event line at 200 ms spacing, denser than the lowest knob can return',
    },
];

if (! checkOnly)
    fs.mkdirSync (marksDir, { recursive: true });

let bad = 0;
for (const take of takes)
{
    const bytes = wav (take.seconds, take.onsets, take.shape);
    const digest = crypto.createHash ('sha256').update (bytes).digest ('hex');
    const wavPath = path.join (fixturesDir, take.name + '.wav');
    const marksPath = path.join (marksDir, take.name + '.marks.json');
    const body = JSON.stringify ({
        schema: 'sliceforge-slicer-marks/1',
        take: take.name,
        markProvenance: 'synthetic click train, re-derived by fixtures/tools/synth-takes.cjs',
        note: take.note,
        sampleRate: rate,
        wav: take.name + '.wav',
        wavSha256: digest,
        onsets: take.onsets.map (t => Number (t.toFixed (6))),
    }, null, 2) + '\n';

    if (checkOnly)
    {
        const onDisk = fs.readFileSync (wavPath);
        const same = onDisk.equals (bytes);
        // The wav is binary to Git, so its bytes are the bytes a clone gets. The marks file is LF text
        // with no .gitattributes, and core.autocrlf=true hands a checkout the same file with CRLF pairs -
        // so that one translation is admitted, named in the output, and nothing else is.
        const raw = fs.readFileSync (marksPath, 'utf8');
        const marksSame = raw === body ? 'exact'
                        : (raw.replace (/\r\n/g, '\n') === body ? 'crlf-checkout' : 'differ');
        console.log (`${same && marksSame !== 'differ' ? 'ok  ' : 'DIFF'} ${take.name}.wav sha256 ${digest.slice (0, 16)} bytes ${bytes.length} marks ${marksSame}`);
        if (! same || marksSame === 'differ')
            ++bad;
        continue;
    }

    fs.writeFileSync (wavPath, bytes);
    fs.writeFileSync (marksPath, body);
    console.log (`wrote ${take.name}.wav sha256 ${digest.slice (0, 16)} bytes ${bytes.length} refN=${take.onsets.length}`);
}

process.exit (bad === 0 ? 0 : 1);
