#include "SlicerScoreTest.h"

#include "Engine/SampleData.h"
#include "Engine/Slicer.h"
#include "Engine/SlicerTuning.h"
#include "Params.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace sliceforge::test
{
namespace
{
    using ReportFn = std::function<void (bool, const juce::String&, const juce::String&)>;

    /*  The judge's constants. These belong to the scorer and deliberately NOT to
        Engine/SlicerTuning.h: tuning the product and tuning the ruler are two decisions, and a run
        that moves both at once proves nothing about either.
        50 ms is the MIREX default hit window, so the headline recall is citable outside this repo;
        the 25 ms second recall exists because a 50 ms window happily scores a detector that is
        consistently 40 ms late as near-perfect. */
    constexpr double kHitWindowMs = 50.0;
    constexpr double kFineWindowMs = 25.0;

    /*  0.55 is the product default sensitivity (Source/Params.cpp); the other two probe the ends a
        customer can actually reach. Sensitivity is swept, not tuned: a detector that only works at
        its own default is not working. */
    constexpr double kSensitivities[] { 0.20, 0.55, 0.90 };

    const juce::String kMarksSchema = "sliceforge-slicer-marks/1";
    const juce::String kBaselineSchema = "sliceforge-slicer-baseline/1";
    const juce::String kBaselineName = "slicer-baseline.json";

    /** The one provenance string that makes a *claim* rather than describes a source: "these onsets
        are the clip's own note times". That claim is checkable, because the clip can be shipped next
        to the marks (`fixtures/marks-src/<take>.mid`) and re-read. Any other provenance - a one-shot
        bag, a fixed grid, a human tapping - is recorded honestly and left unchecked, because there is
        nothing next to it to check it against. */
    const juce::String kProvenanceNoteTimes = "midi-note-times";

    /** Marks are serialised to six decimal places, so a note time and its own marks file can only
        disagree by rounding. Anything bigger means the two files describe different notes. */
    constexpr double kMarksSrcToleranceSeconds = 1.0e-6;

    /** Note-onset times in seconds, read back out of a standard MIDI file image.
        Ticks are not seconds until the file says how long a quarter note is, so an image with no tempo
        map is refused rather than scored at JUCE's own default: a silently assumed 120 BPM would make
        every onset disagree and blame the render for the reader's guess. */
    bool midNoteOnsets (const juce::MemoryBlock& image, std::vector<double>& out, juce::String& problem)
    {
        juce::MemoryInputStream in (image, false);
        juce::MidiFile mf;
        if (! mf.readFrom (in))
        {
            problem = "midi does not parse";
            return false;
        }
        if (mf.getNumTracks() == 0) // readFrom returns true for a file whose chunks it did not recognise
        {
            problem = "midi parses but holds no MTrk chunks, so every track in it was skipped";
            return false;
        }
        juce::MidiMessageSequence tempos;
        mf.findAllTempoEvents (tempos);
        if (tempos.getNumEvents() == 0)
        {
            problem = "midi carries no tempo map, so its ticks cannot be read as seconds";
            return false;
        }
        mf.convertTimestampTicksToSeconds();
        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            const auto* seq = mf.getTrack (t);
            if (seq == nullptr)
                continue;
            for (int i = 0; i < seq->getNumEvents(); ++i)
            {
                const auto* event = seq->getEventPointer (i);
                if (event != nullptr && event->message.isNoteOn()) // isNoteOn() excludes velocity 0, which is a note-off
                    out.push_back (event->message.getTimeStamp());
            }
        }
        std::sort (out.begin(), out.end());
        out.erase (std::unique (out.begin(), out.end(),
                                [] (double a, double b) { return std::abs (a - b) < 1.0e-9; }),
                   out.end());
        return true;
    }

    bool readMarksSrcOnsets (const juce::File& fixturesDir, const juce::String& take,
                             std::vector<double>& out, juce::String& problem)
    {
        const auto mid = fixturesDir.getChildFile ("marks-src").getChildFile (take + ".mid");
        juce::MemoryBlock image;
        if (! mid.existsAsFile())
        {
            problem = "claims note times but marks-src/" + mid.getFileName()
                + " is not there, so the reference is a transcription rather than a measurement";
            return false;
        }
        if (! mid.loadFileAsData (image))
        {
            problem = "marks-src/" + mid.getFileName() + " could not be read";
            return false;
        }
        return midNoteOnsets (image, out, problem);
    }

    double undefined() { return std::numeric_limits<double>::quiet_NaN(); }
    bool isSet (double v) { return std::isfinite (v); }

    /** A number for the table, or `-` in the log and `null` in the JSON. Never a fabricated 0. */
    juce::String fmt (double v, int dp) { return isSet (v) ? juce::String (v, dp) : juce::String ("-"); }

    double medianOf (std::vector<double> values)
    {
        if (values.empty())
            return undefined();
        std::sort (values.begin(), values.end());
        const auto n = values.size();
        return n % 2 == 1 ? values[n / 2] : 0.5 * (values[n / 2 - 1] + values[n / 2]);
    }

    int targetAt (double sensitivity)
    {
        return (int) std::lround ((double) kTargetBase + (double) kTargetSpan * sensitivity);
    }

    struct MatchResult
    {
        int tp = 0;
        std::vector<double> signedMs; // pred - ref, for matched pairs only
    };

    /** Greedy one-to-one matching: every pair inside the window is a candidate, the closest is
     *  taken first, and a prediction or a reference already spent cannot be spent again. One-to-one
     *  is the property that stops a detector firing twice on one hit from being paid for twice. */
    MatchResult matchOnsets (const std::vector<double>& pred, const std::vector<double>& ref, double windowMs)
    {
        struct Cand { double deltaMs; int p; int r; };
        std::vector<Cand> cands;
        for (size_t p = 0; p < pred.size(); ++p)
            for (size_t r = 0; r < ref.size(); ++r)
            {
                const auto delta = std::abs (pred[p] - ref[r]) * 1000.0;
                if (delta <= windowMs)
                    cands.push_back ({ delta, (int) p, (int) r });
            }
        std::sort (cands.begin(), cands.end(),
                   [] (const Cand& a, const Cand& b) { return a.deltaMs < b.deltaMs; });

        std::vector<char> usedP (pred.size(), 0), usedR (ref.size(), 0);
        MatchResult m;
        for (const auto& c : cands)
        {
            if (usedP[c.p] != 0 || usedR[c.r] != 0)
                continue;
            usedP[c.p] = 1;
            usedR[c.r] = 1;
            ++m.tp;
            m.signedMs.push_back ((pred[c.p] - ref[c.r]) * 1000.0);
        }
        return m;
    }

    struct Row
    {
        juce::String take;
        double sens = 0.0;
        int refN = 0;
        int detN = 0;
        int tp = 0;
        int tpFine = 0;
        double seconds = 0.0;
        double recall = undefined();      // NaN when there is no reference to score against
        double recallFine = undefined();
        double precision = undefined();   // tp/detN - undefined when the detector found nothing, not 0
        double fpPerMin = 0.0;
        double medAbsMs = undefined();    // NaN when nothing matched
        double maxAbsMs = undefined();
        double biasMs = undefined();      // signed: pred - ref. Where the -6 ms correction is judged.
        bool capped = false;              // the reference outruns what this sensitivity asks for

        juce::String key() const { return take + "@" + juce::String (sens, 2); }

        juce::String line() const
        {
            return "SLICERSCORE " + take + " sens=" + juce::String (sens, 2)
                + " refN=" + juce::String (refN) + " detN=" + juce::String (detN)
                + " tp=" + juce::String (tp) + " recall=" + fmt (recall, 3)
                + " recall25=" + fmt (recallFine, 3) + " p=" + fmt (precision, 3)
                + " fpmin=" + fmt (fpPerMin, 2)
                + " medAbsMs=" + fmt (medAbsMs, 2) + " maxAbsMs=" + fmt (maxAbsMs, 2)
                + " biasMs=" + fmt (biasMs, 2) + " capped=" + (capped ? "yes" : "no");
        }
    };

    Row scorePair (const juce::String& take, double sens, const std::vector<double>& pred,
                   const std::vector<double>& ref, double seconds)
    {
        Row row;
        row.take = take;
        row.sens = sens;
        row.detN = (int) pred.size();
        row.refN = (int) ref.size();
        row.seconds = seconds;

        const auto coarse = matchOnsets (pred, ref, kHitWindowMs);
        const auto fine = matchOnsets (pred, ref, kFineWindowMs);
        row.tp = coarse.tp;
        row.tpFine = fine.tp;

        if (row.refN > 0) // A zero-reference take is undefined, never a vacuous 1.000.
        {
            row.recall = (double) row.tp / (double) row.refN;
            row.recallFine = (double) row.tpFine / (double) row.refN;
        }
        const int fp = row.detN - row.tp;
        row.fpPerMin = seconds > 0.0 ? (double) fp * 60.0 / seconds : 0.0;
        if (row.detN > 0) // A detector that stayed silent has no precision to report.
            row.precision = (double) row.tp / (double) row.detN;

        std::vector<double> absMs;
        for (const auto d : coarse.signedMs)
            absMs.push_back (std::abs (d));
        row.medAbsMs = medianOf (absMs);
        row.maxAbsMs = absMs.empty() ? undefined() : *std::max_element (absMs.begin(), absMs.end());
        row.biasMs = medianOf (coarse.signedMs);

        row.capped = row.refN > std::min (targetAt (sens), (int) kMaxSlices);
        return row;
    }

    // ---------------------------------------------------------------- fixtures

    juce::File findFixturesDir()
    {
        auto dir = juce::File::getCurrentWorkingDirectory();
        for (int i = 0; i < 6; ++i) // ctest runs from the repo root; a manual run may not
        {
            const auto candidate = dir.getChildFile ("fixtures");
            if (candidate.isDirectory())
                return candidate;
            dir = dir.getParentDirectory();
        }
        dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
        for (int i = 0; i < 12; ++i) // ... and the exe sits several levels below the root
        {
            const auto candidate = dir.getChildFile ("fixtures");
            if (candidate.isDirectory())
                return candidate;
            const auto parent = dir.getParentDirectory();
            if (parent == dir)
                break;
            dir = parent;
        }
        return {};
    }

    struct Take
    {
        juce::String name;
        juce::String provenance;
        std::vector<double> refs;
        sliceforge::SamplePtr sample;
    };

    struct Inventory
    {
        std::vector<Take> takes;
        juce::String problems; // empty when every marks file and every wav it names came through
        juce::String notes;    // states that are worth reading but are not failures
    };

    /** A JSON number, and nothing that merely converts to one. `null` is void, `true` is bool, and a
        string that looks like a timestamp must not be read as an onset. JUCE 8 has no isNum(). */
    bool isNumeric (const juce::var& v) { return v.isInt() || v.isDouble(); }

    double numOr (const juce::var& v, double fallback) { return isNumeric (v) ? (double) v : fallback; }

    Inventory loadTakes (const juce::File& fixturesDir)
    {
        Inventory inv;
        const auto marksDir = fixturesDir.getChildFile ("marks");
        if (! marksDir.isDirectory())
            return inv; // No ground truth in this checkout yet - the caller says so out loud.

        juce::Array<juce::File> files;
        marksDir.findChildFiles (files, juce::File::findFiles, false, "*.marks.json");
        std::sort (files.begin(), files.end(),
                   [] (const juce::File& a, const juce::File& b) { return a.getFileName() < b.getFileName(); });

        for (const auto& file : files)
        {
            const auto expectProblem = [&] (const juce::String& why)
            { inv.problems << file.getFileName() << ": " << why << "; "; };

            const auto withoutSuffix = file.getFileName().upToLastOccurrenceOf (".marks.json", false, false);

            juce::var parsed;
            try { parsed = juce::JSON::parse (file.loadFileAsString()); }
            catch (...) { expectProblem ("is not valid JSON"); continue; }
            if (! parsed.isObject()) { expectProblem ("is not a JSON object"); continue; }

            if (parsed.getProperty ("schema", juce::var()).toString() != kMarksSchema)
            {
                expectProblem ("schema is not " + kMarksSchema);
                continue;
            }
            const auto internalName = parsed.getProperty ("take", juce::var()).toString();
            if (internalName.isEmpty() || internalName != withoutSuffix)
            {
                expectProblem ("take \"" + internalName + "\" does not match the file name");
                continue;
            }
            const auto provenance = parsed.getProperty ("markProvenance", juce::var()).toString();
            if (provenance.isEmpty())
            {
                expectProblem ("states no markProvenance, so nobody can tell what its truth is");
                continue;
            }
            const auto wavRel = parsed.getProperty ("wav", juce::var()).toString();
            if (wavRel.isEmpty() || wavRel.containsChar ('"') || wavRel.contains (".."))
            {
                expectProblem ("wav path \"" + wavRel + "\" is not usable");
                continue;
            }

            const auto onsetsVar = parsed.getProperty ("onsets", juce::var());
            std::vector<double> refs;
            if (! onsetsVar.isArray()) { expectProblem ("onsets is not an array"); continue; }
            auto ok = true;
            double previous = -1.0;
            for (const auto& entry : *onsetsVar.getArray())
            {
                if (! isNumeric (entry)) { ok = false; break; }
                const double t = (double) entry;
                if (! std::isfinite (t) || t < 0.0 || t < previous)
                { ok = false; break; } // ascending, finite, non-negative: the matcher's contract
                refs.push_back (t);
                previous = t;
            }
            if (! ok) { expectProblem ("onsets are not a finite ascending list of seconds"); continue; }
            if (refs.empty())
                inv.notes << file.getFileName() << ": has zero reference onsets, so its rows print as -; ";

            const auto wavFile = fixturesDir.getChildFile (wavRel);
            auto sample = sliceforge::loadAudioFile (wavFile);
            if (sample == nullptr || sample->getNumSamples() <= 0)
            {
                expectProblem ("wav \"" + wavRel + "\" did not decode");
                continue;
            }

            if (provenance == kProvenanceNoteTimes)
            {
                std::vector<double> fromMid;
                juce::String why;
                if (! readMarksSrcOnsets (fixturesDir, internalName, fromMid, why))
                {
                    expectProblem (why);
                    continue;
                }
                if (fromMid.size() != refs.size())
                {
                    expectProblem ("lists " + juce::String ((int) refs.size()) + " onsets but its midi holds "
                                   + juce::String ((int) fromMid.size()));
                    continue;
                }
                double worst = 0.0;
                for (size_t i = 0; i < refs.size(); ++i)
                    worst = juce::jmax (worst, std::abs (refs[i] - fromMid[i]));
                if (! (worst <= kMarksSrcToleranceSeconds)) // also catches a NaN from a corrupt read
                {
                    expectProblem ("disagrees with its own midi by " + juce::String (worst * 1000.0, 3)
                                   + " ms, so one of the two files was edited after the render");
                    continue;
                }
            }

            Take take;
            take.name = internalName;
            take.provenance = provenance;
            take.refs = std::move (refs);
            take.sample = std::move (sample);
            inv.takes.push_back (std::move (take));
        }
        return inv;
    }

    std::vector<Row> scoreInventory (const Inventory& inv)
    {
        std::vector<Row> rows;
        for (const auto& take : inv.takes)
        {
            const auto seconds = take.sample->getDurationSeconds();
            for (const auto sens : kSensitivities)
            {
                std::vector<double> pred;
                for (const auto& slice : sliceforge::detectSlices (*take.sample, (float) sens))
                    pred.push_back (slice.time);
                rows.push_back (scorePair (take.name, sens, pred, take.refs, seconds));
            }
        }
        std::sort (rows.begin(), rows.end(),
                   [] (const Row& a, const Row& b) { return a.key() < b.key(); });
        return rows;
    }

    /** A take whose every row is pinned by the slice budget scores the budget rather than the
        detector, so it has no business in the table. One capped row is the ordinary case of a knob
        set below what a dense line asks for - the ceiling at 0.20 is seventeen slices and a 16th-note
        hat line has more events than that - which is why this is a per-take test and not a per-row one. */
    juce::String uncoverableTakes (const std::vector<Row>& rows, const std::vector<Take>& takes)
    {
        juce::String out;
        for (const auto& take : takes)
        {
            int coverable = 0;
            for (const auto& r : rows)
                if (r.take == take.name && ! r.capped)
                    ++coverable;
            if (coverable == 0)
                out << "[" << take.name << " refN " << juce::String ((int) take.refs.size())
                    << " asks more than the grid's ceiling of "
                    << juce::String (targetAt (kSensitivities[2])) << "] ";
        }
        return out;
    }

    // ---------------------------------------------------------------- baseline

    struct Tol
    {
        double recall = 0.02;
        double recallFine = 0.02;
        double medAbsMs = 1.5;
        double biasMs = 2.0;
        double fpFloor = 1.0;  // absolute slack, so a blessed 0 fp/min is not a division by zero
        double fpRatio = 2.0;
    };

    struct Baseline
    {
        bool present = false;
        juce::String problem;
        juce::String tuningMismatch;
        Tol tol;
        std::vector<Row> rows;
    };

    const char* kTuningNames[] { "kTargetBase", "kTargetSpan", "kMinGapSeconds",
                                 "kFloorRatio", "kOnsetBiasSeconds", "kDedupGateSeconds" };

    double compiledTuning (int i)
    {
        switch (i)
        {
            case 0: return kTargetBase;
            case 1: return kTargetSpan;
            case 2: return kMinGapSeconds;
            case 3: return kFloorRatio;
            case 4: return kOnsetBiasSeconds;
            default: return kDedupGateSeconds;
        }
    }

    juce::String readTuningMismatch (const juce::var& tunings)
    {
        if (! tunings.isObject())
            return "the file states no tunings block, so it predates the tuning extraction";
        juce::String mismatch;
        for (int i = 0; i < 6; ++i)
        {
            const auto v = tunings.getProperty (juce::Identifier (kTuningNames[i]), juce::var());
            if (! isNumeric (v)) { mismatch << kTuningNames[i] << " absent; "; continue; }
            if (std::abs ((double) v - compiledTuning (i)) > 1.0e-9)
                mismatch << kTuningNames[i] << " blessed " << juce::String ((double) v, 6)
                         << " but built " << juce::String (compiledTuning (i), 6) << "; ";
        }
        return mismatch;
    }

    Baseline readBaseline (const juce::File& fixturesDir)
    {
        Baseline b;
        const auto file = fixturesDir.getChildFile (kBaselineName);
        if (! file.existsAsFile())
            return b;
        b.present = true;

        juce::var parsed;
        try { parsed = juce::JSON::parse (file.loadFileAsString()); }
        catch (...) { b.problem = "is not valid JSON"; return b; }
        if (! parsed.isObject()) { b.problem = "is not a JSON object"; return b; }
        if (parsed.getProperty ("schema", juce::var()).toString() != kBaselineSchema)
        { b.problem = "schema is not " + kBaselineSchema; return b; }

        b.tuningMismatch = readTuningMismatch (parsed.getProperty ("tunings", juce::var()));

        const auto tol = parsed.getProperty ("tol", juce::var());
        if (tol.isObject())
        {
            b.tol.recall = numOr (tol.getProperty ("recall", juce::var()), b.tol.recall);
            b.tol.recallFine = numOr (tol.getProperty ("recallFine", juce::var()), b.tol.recallFine);
            b.tol.medAbsMs = numOr (tol.getProperty ("medAbsMs", juce::var()), b.tol.medAbsMs);
            b.tol.biasMs = numOr (tol.getProperty ("biasMs", juce::var()), b.tol.biasMs);
            b.tol.fpFloor = numOr (tol.getProperty ("fpFloor", juce::var()), b.tol.fpFloor);
            b.tol.fpRatio = numOr (tol.getProperty ("fpRatio", juce::var()), b.tol.fpRatio);
        }

        const auto rows = parsed.getProperty ("rows", juce::var());
        if (! rows.isArray()) { b.problem = "rows is not an array"; return b; }
        for (const auto& entry : *rows.getArray())
        {
            if (! entry.isObject()) continue;
            Row r;
            r.take = entry.getProperty ("take", juce::var()).toString();
            r.sens = numOr (entry.getProperty ("sens", juce::var()), undefined());
            r.refN = (int) numOr (entry.getProperty ("refN", juce::var()), 0);
            r.detN = (int) numOr (entry.getProperty ("detN", juce::var()), 0);
            r.tp = (int) numOr (entry.getProperty ("tp", juce::var()), 0);
            r.tpFine = (int) numOr (entry.getProperty ("tpFine", juce::var()), 0);
            r.seconds = numOr (entry.getProperty ("seconds", juce::var()), undefined());
            r.recall = numOr (entry.getProperty ("recall", juce::var()), undefined());
            r.recallFine = numOr (entry.getProperty ("recallFine", juce::var()), undefined());
            r.precision = numOr (entry.getProperty ("precision", juce::var()), undefined());
            r.fpPerMin = numOr (entry.getProperty ("fpPerMin", juce::var()), undefined());
            r.medAbsMs = numOr (entry.getProperty ("medAbsMs", juce::var()), undefined());
            r.maxAbsMs = numOr (entry.getProperty ("maxAbsMs", juce::var()), undefined());
            r.biasMs = numOr (entry.getProperty ("biasMs", juce::var()), undefined());
            r.capped = entry.getProperty ("capped", juce::var()) == juce::var (true);
            b.rows.push_back (std::move (r));
        }
        return b;
    }

    const Row* findRow (const std::vector<Row>& rows, const juce::String& key)
    {
        for (const auto& r : rows)
            if (r.key() == key)
                return &r;
        return nullptr;
    }

    /*  False positives get an asymmetric band because a symmetric one cannot exist. The metric bottoms
        out at zero, so a relative slack of (fpRatio - 1) * blessed below the blessed value is the whole
        distance to silence and never trips: the first version of this gate let a blessed 40/min against
        a scored 0/min pass as a 100% improvement. Downward the slack is fpFloor per minute - a bigger
        drop than that is a real change in what the detector fires on. Upward it may be relative, because
        a dense kit legitimately starts at thirty per minute and 40 -> 42 is not a finding. */
    bool fpViolates (double blessed, double got, const Tol& tol)
    {
        return got < blessed - tol.fpFloor
            || got - blessed > std::max (tol.fpFloor, blessed * (tol.fpRatio - 1.0));
    }

    /** One field, one band. Symmetric on purpose: a blessed number that moves is a claim that needs
     *  reading, whether it moved toward perfect or away from it. */
    void compareField (juce::String& out, const juce::String& what, double got, double base, double tol)
    {
        const auto g = isSet (got);
        const auto b = isSet (base);
        if (g != b)
        {
            out << what << (g ? " is now defined" : " is now undefined")
                << " (blessed " << (b ? juce::String (base, 3) : juce::String ("-")) << ") ";
            return;
        }
        if (! g)
            return; // neither side scored it: nothing to call broken
        if (std::abs (got - base) > tol)
            out << what << " " << juce::String (got, 3) << " vs blessed " << juce::String (base, 3)
                << " (tol " << juce::String (tol, 3) << ") ";
    }

    juce::String bandViolations (const std::vector<Row>& rows, const Baseline& b)
    {
        juce::String out;
        for (const auto& r : rows)
        {
            const auto* base = findRow (b.rows, r.key());
            if (base == nullptr)
                continue; // inventory parity owns that problem, and names it once
            juce::String here;
            compareField (here, "recall", r.recall, base->recall, b.tol.recall);
            compareField (here, "recall25", r.recallFine, base->recallFine, b.tol.recallFine);
            compareField (here, "medAbsMs", r.medAbsMs, base->medAbsMs, b.tol.medAbsMs);
            compareField (here, "biasMs", r.biasMs, base->biasMs, b.tol.biasMs);
            if (isSet (r.fpPerMin) != isSet (base->fpPerMin))
                here << "fpmin defined on one side only ";
            else if (isSet (r.fpPerMin) && fpViolates (base->fpPerMin, r.fpPerMin, b.tol))
                here << "fpmin " << juce::String (r.fpPerMin, 2) << " vs blessed "
                     << juce::String (base->fpPerMin, 2) << " ";
            if (r.capped != base->capped)
                here << "capped flipped to " << (r.capped ? "yes" : "no") << " ";
            if (here.isNotEmpty())
                out << "[" << r.key() << " " << here << "] ";
        }
        return out;
    }

    juce::String inventoryDiff (const std::vector<Row>& rows, const Baseline& b)
    {
        juce::String out;
        for (const auto& r : rows)
            if (findRow (b.rows, r.key()) == nullptr)
                out << "[" << r.key() << " scored but not blessed] ";
        for (const auto& w : b.rows)
            if (findRow (rows, w.key()) == nullptr)
                out << "[" << w.key() << " blessed but not on disk] ";
        return out;
    }

    // ---------------------------------------------------------------- serialise

    juce::String jsonNum (double v, int dp) { return isSet (v) ? juce::String (v, dp) : juce::String ("null"); }

    juce::String baselineText (const std::vector<Row>& rows)
    {
        juce::String t;
        t << "{\n";
        t << "  \"schema\": \"" << kBaselineSchema << "\",\n";
        t << "  \"generatedBy\": \"SliceForgeSmokeTest --bless-slicer-baseline\",\n";
        t << "  \"tunings\": {\n";
        for (int i = 0; i < 6; ++i)
            t << "    \"" << kTuningNames[i] << "\": " << juce::String (compiledTuning (i), 6)
              << (i == 5 ? "\n" : ",\n");
        t << "  },\n";
        const Tol tol;
        t << "  \"tol\": { \"recall\": " << juce::String (tol.recall, 3)
          << ", \"recallFine\": " << juce::String (tol.recallFine, 3)
          << ", \"medAbsMs\": " << juce::String (tol.medAbsMs, 3)
          << ", \"biasMs\": " << juce::String (tol.biasMs, 3)
          << ", \"fpFloor\": " << juce::String (tol.fpFloor, 3)
          << ", \"fpRatio\": " << juce::String (tol.fpRatio, 3) << " },\n";
        t << "  \"rows\": [\n";
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto& r = rows[i];
            t << "    { \"take\": \"" << r.take << "\", \"sens\": " << juce::String (r.sens, 2)
              << ", \"refN\": " << r.refN << ", \"detN\": " << r.detN
              << ", \"tp\": " << r.tp << ", \"tpFine\": " << r.tpFine
              << ", \"seconds\": " << juce::String (r.seconds, 6)
              << ", \"recall\": " << jsonNum (r.recall, 4)
              << ", \"recallFine\": " << jsonNum (r.recallFine, 4)
              << ", \"precision\": " << jsonNum (r.precision, 4)
              << ", \"fpPerMin\": " << jsonNum (r.fpPerMin, 3)
              << ", \"medAbsMs\": " << jsonNum (r.medAbsMs, 3)
              << ", \"maxAbsMs\": " << jsonNum (r.maxAbsMs, 3)
              << ", \"biasMs\": " << jsonNum (r.biasMs, 3)
              << ", \"capped\": " << (r.capped ? "true" : "false") << " }"
              << (i + 1 == rows.size() ? "\n" : ",\n");
        }
        t << "  ]\n}\n";
        return t;
    }

    // ---------------------------------------------------------------- controls

    std::vector<double> grid (double first, double step, int count)
    {
        std::vector<double> v;
        for (int i = 0; i < count; ++i)
            v.push_back (first + step * i);
        return v;
    }

    /** The matcher's and the metric's own arithmetic, exercised without any audio, so it is gated
     *  from the day it is written rather than the day a render lands. */
    void runControls (const ReportFn& report)
    {
        const double sens = 0.55;
        const auto refs = grid (0.5, 0.5, 12); // 12 onsets, 0.5 s apart, over 6.5 s
        const double seconds = 7.0;

        // A shift larger than the window must score nothing and must charge every detection.
        std::vector<double> shifted;
        for (const auto t : refs)
            shifted.push_back (t + 0.20);
        const auto late = scorePair ("ctl-shifted", sens, shifted, refs, seconds);
        report (late.tp == 0 && late.recall == 0.0
                    && std::abs (late.fpPerMin - (double) shifted.size() * 60.0 / seconds) < 1.0e-9,
                "the scorer charges a 200 ms-shifted detector nothing and counts it all as false",
                late.line());

        const auto none = scorePair ("ctl-empty", sens, {}, refs, seconds);
        report (none.recall == 0.0 && none.tp == 0 && none.fpPerMin == 0.0 && ! isSet (none.medAbsMs),
                "a detector that finds nothing scores 0.000 and not a dash",
                none.line());

        const auto bare = scorePair ("ctl-noref", sens, refs, {}, seconds);
        report (! isSet (bare.recall) && ! isSet (bare.recallFine) && bare.tp == 0
                    && std::abs (bare.fpPerMin - (double) refs.size() * 60.0 / seconds) < 1.0e-9,
                "a take with no reference onsets is reported undefined, never as a perfect score",
                bare.line());

        std::vector<double> oneExtra = refs;
        oneExtra.push_back (0.75); // midway between two references: it cannot steal a match
        const auto extra = scorePair ("ctl-injected", sens, oneExtra, refs, seconds);
        const auto clean = scorePair ("ctl-clean", sens, refs, refs, seconds);
        report (clean.tp == (int) refs.size() && extra.tp == (int) refs.size()
                    && extra.fpPerMin > clean.fpPerMin,
                "one extra mid-take slice raises false positives per minute and costs no recall",
                clean.line() + " | " + extra.line());

        const auto doubled = scorePair ("ctl-doubled", sens, { 1.0, 1.005 }, { 1.0 }, seconds);
        report (doubled.tp == 1 && doubled.detN == 2 && doubled.fpPerMin > 0.0,
                "two slices 5 ms apart on one hit are one match and one false positive",
                doubled.line());

        // The precision column, including the two cases where a naive `tp / detN` would divide by
        // zero or report 1.000 for a detector that never fired.
        report (! isSet (none.precision) && std::abs (bare.precision) < 1.0e-12
                    && std::abs (clean.precision - 1.0) < 1.0e-12
                    && std::abs (doubled.precision - 0.5) < 1.0e-12
                    && std::abs (extra.precision - (double) refs.size() / (double) (refs.size() + 1)) < 1.0e-12,
                "precision is undefined for a silent detector, 0.000 for one that fires at nothing, and exact otherwise",
                "silent=" + fmt (none.precision, 3) + " noref=" + fmt (bare.precision, 3)
                    + " clean=" + fmt (clean.precision, 3) + " doubled=" + fmt (doubled.precision, 3));

        const auto earlyIn = matchOnsets ({ 0.951 }, { 1.0 }, kHitWindowMs).tp;
        const auto earlyOut = matchOnsets ({ 0.949 }, { 1.0 }, kHitWindowMs).tp;
        const auto lateIn = matchOnsets ({ 1.049 }, { 1.0 }, kHitWindowMs).tp;
        const auto lateOut = matchOnsets ({ 1.051 }, { 1.0 }, kHitWindowMs).tp;
        report (earlyIn == 1 && earlyOut == 0 && lateIn == 1 && lateOut == 0,
                "the hit window is 50 ms on both sides, so 49 ms matches early or late and 51 ms does not",
                juce::String (earlyIn) + "/" + juce::String (earlyOut) + " "
                    + juce::String (lateIn) + "/" + juce::String (lateOut));

        // The false-positive band, pinned in code so it cannot silently become one-sided again.
        {
            const Tol t;
            const auto ok = ! fpViolates (0.0, 0.0, t) && fpViolates (40.0, 0.0, t)
                              && ! fpViolates (40.0, 45.0, t) && fpViolates (40.0, 81.0, t);
            report (ok, "the false-positive band reddens for a drop to silence as well as for a rise",
                    "40->0=" + juce::String (fpViolates (40.0, 0.0, t) ? 1 : 0)
                        + " 40->45=" + juce::String (fpViolates (40.0, 45.0, t) ? 1 : 0)
                        + " 40->81=" + juce::String (fpViolates (40.0, 81.0, t) ? 1 : 0)
                        + " (floor " + juce::String (t.fpFloor, 1) + ", ratio " + juce::String (t.fpRatio, 1) + ")");
        }

        // 80 clicks at 0.15 s: more onsets than any sensitivity asks for, and more than the kit holds.
        {
            constexpr double rate = 44100.0;
            constexpr int count = 80;
            const int length = (int) (rate * (count * 0.15 + 0.5));
            sliceforge::SampleData busy;
            busy.sampleRate = rate;
            busy.buffer.setSize (2, length, false, true);
            busy.buffer.clear();
            for (int i = 0; i < count; ++i)
            {
                const auto start = (int) ((0.25 + i * 0.15) * rate);
                for (int n = 0; n < 240 && start + n < length; ++n)
                {
                    const auto v = 0.8f * std::sin (juce::MathConstants<float>::twoPi * 3800.0f * (float) n / (float) rate);
                    busy.buffer.setSample (0, start + n, v);
                    busy.buffer.setSample (1, start + n, v);
                }
            }
            std::vector<double> truth;
            for (int i = 0; i < count; ++i)
                truth.push_back (0.25 + i * 0.15);
            std::vector<double> pred;
            for (const auto& slice : sliceforge::detectSlices (busy, (float) kSensitivities[2]))
                pred.push_back (slice.time);
            const auto row = scorePair ("ctl-busy", kSensitivities[2], pred, truth, length / rate);
            report (row.detN <= (int) kMaxSlices && row.detN <= targetAt (kSensitivities[2]) && row.capped,
                    "an 80-onset take stays inside the slice budget and is flagged as capped",
                    row.line());
        }

        // A ramped sine sweep: energy that only ever rises, so it contains no impulsive onset and a
        // reference set of zero is the honest truth about it. This is where `refN = 0` is exercised on
        // real detector output rather than on hand-written vectors - the case that would print a
        // vacuous 1.000 if the scorer ever divided by what it found instead of by what it was given.
        // The detection count is reported, not asserted: a swell the detector reads as hits is a
        // finding for the tuning, not a scorer bug.
        {
            constexpr double rate = 44100.0;
            constexpr double dur = 6.0;
            const int length = (int) (rate * dur);
            sliceforge::SampleData sweep;
            sweep.sampleRate = rate;
            sweep.buffer.setSize (2, length, false, true);
            sweep.buffer.clear();
            double phase = 0.0;
            for (int i = 0; i < length; ++i)
            {
                const auto t = (double) i / rate;
                const auto hz = 100.0 + (4000.0 - 100.0) * (t / dur);
                phase += juce::MathConstants<double>::twoPi * hz / rate;
                const auto v = (float) ((t / dur) * std::sin (phase)); // amplitude ramps 0 -> 1 with the sweep
                sweep.buffer.setSample (0, i, v);
                sweep.buffer.setSample (1, i, v);
            }
            std::vector<double> fired;
            for (const auto& slice : sliceforge::detectSlices (sweep, (float) kSensitivities[1]))
                fired.push_back (slice.time);
            const auto row = scorePair ("ctl-sweep", kSensitivities[1], fired, {}, dur);
            const auto finiteFp = ! std::isnan (row.fpPerMin) && ! std::isinf (row.fpPerMin);
            std::cout << "  info  " << row.line() << '\n'; // printed even when the check below is green
            report (row.refN == 0 && ! isSet (row.recall) && ! isSet (row.recallFine)
                        && ! isSet (row.medAbsMs) && ! isSet (row.biasMs) && finiteFp
                        && (fired.empty() ? ! isSet (row.precision) : row.precision == 0.0)
                        && ! row.capped,
                    "a swell with no impulsive onset scores against zero references without a NaN or a free 1.000",
                    row.line());
        }

        /*  A characterisation test, not an approval. On a buffer of exact zeros the detector still hands
            back the whole target count, because `kFloorRatio` is a fraction of the file's own peak and a
            peak of 0 lets every candidate through (`0 >= 0`). So for a dead take the slice count is a
            function of the sensitivity knob and of nothing else. Pinned here on purpose: whoever gives
            silence a real answer will watch this name go red and move it deliberately, instead of
            learning about the degeneracy from a customer's first empty take. */
        {
            constexpr double rate = 44100.0;
            const int length = (int) (rate * 6.0);
            sliceforge::SampleData dead;
            dead.sampleRate = rate;
            dead.buffer.setSize (2, length, false, true);
            dead.buffer.clear();
            int followed = 0;
            for (const auto knob : kSensitivities)
            {
                const auto slices = sliceforge::detectSlices (dead, (float) knob);
                const int fired = (int) slices.size();
                const auto asked = targetAt (knob);
                followed += fired == asked ? 1 : 0;
                std::cout << "  info  SLICERSCORE ctl-silence sens=" << knob
                          << " detN=" << fired << " target=" << asked << '\n';
            }
            report (followed == (int) (sizeof (kSensitivities) / sizeof (kSensitivities[0])),
                    "a dead take still yields the knob's target count: the relative floor is degenerate at peak 0",
                    "silence no longer follows the sensitivity knob - three detN/target lines above");
        }

        /*  The marks-src reader, run over a standard MIDI file built here rather than over a hand-
            written vector. Ticks-per-quarter and the tempo meta event are the two things that turn a
            note number into a wall-clock time, so a reader that guessed either would still find the
            right *count* of onsets while scoring every take against a shifted truth - which is exactly
            the failure a marks file cannot see from the inside. The second half is the falsifier: the
            same bytes compared against a list with one onset 20 ms late must be refused. */
        {
            constexpr double ticksPerQuarter = 480.0;
            juce::MidiMessageSequence seq;
            seq.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0); // 120 BPM: a quarter is 0.5 s
            const double quarters[] { 2.0, 3.0, 4.0 };                      // so 1.0 s, 1.5 s, 2.0 s
            for (const auto q : quarters)
            {
                seq.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), q * ticksPerQuarter);
                seq.addEvent (juce::MidiMessage::noteOff (1, 60), q * ticksPerQuarter + ticksPerQuarter * 0.25);
            }
            juce::MidiFile mf;
            mf.setTicksPerQuarterNote ((int) ticksPerQuarter);
            mf.addTrack (seq);
            juce::MemoryBlock image;
            juce::MemoryOutputStream stream (image, false);
            const bool written = mf.writeTo (stream);
            stream.flush();

            std::vector<double> back;
            juce::String why;
            const bool reads = written && midNoteOnsets (image, back, why);
            std::vector<double> wanted;
            for (const auto q : quarters)
                wanted.push_back (q * 0.5);

            double worst = 0.0;
            if (reads && back.size() == wanted.size())
                for (size_t i = 0; i < wanted.size(); ++i)
                    worst = juce::jmax (worst, std::abs (back[i] - wanted[i]));
            else
                worst = 1.0;

            auto edited = wanted;
            if (! edited.empty())
                edited.back() += 0.02;
            double worstEdited = 0.0;
            if (back.size() == edited.size())
                for (size_t i = 0; i < edited.size(); ++i)
                    worstEdited = juce::jmax (worstEdited, std::abs (edited[i] - back[i]));

            report (reads && worst <= kMarksSrcToleranceSeconds
                        && worstEdited > kMarksSrcToleranceSeconds && back.size() == 3,
                    "note times are read out of a real midi, and an onset edited 20 ms late is refused by it",
                    (reads ? juce::String ((int) back.size()) + " onsets, worst "
                                 + juce::String (worst * 1000.0, 6) + " ms, edited worst "
                                 + juce::String (worstEdited * 1000.0, 3) + " ms"
                           : "read failed: " + (why.isEmpty() ? juce::String ("write failed") : why)));
        }

        /*  The zero-track guard, exercised on the bytes that motivated it. `MidiFile::readFrom` compares a
            chunk tag against "MTrk" exactly, skips anything else without a word and still returns true, so
            a generator that emits the wrong case hands the reader a file that parses as empty. Scored
            against an empty truth every take would agree with itself, which is why the refusal has to be
            named rather than merely non-zero. The tag is tampered here instead of authored because JUCE's
            own writer is the thing under test's input. */
        {
            juce::MidiFile mf;
            mf.setTicksPerQuarterNote (480);
            juce::MidiMessageSequence seq;
            seq.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);
            seq.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 960.0);
            seq.addEvent (juce::MidiMessage::noteOff (1, 60), 1080.0);
            mf.addTrack (seq);
            juce::MemoryBlock intact;
            {
                juce::MemoryOutputStream out (intact, false);
                mf.writeTo (out);
                out.flush();
            }
            auto broken = intact;
            bool tampered = false;
            if (auto* b = static_cast<char*> (broken.getData()))
                for (size_t i = 0; i + 3 < broken.getSize(); ++i)
                    if (b[i] == 'M' && b[i + 1] == 'T' && b[i + 2] == 'r' && b[i + 3] == 'k')
                    {
                        b[i + 1] = 't';
                        tampered = true;
                    }

            std::vector<double> back;
            juce::String why;
            const bool readClean = midNoteOnsets (intact, back, why);
            std::vector<double> nothing;
            juce::String guard;
            const bool readBroken = midNoteOnsets (broken, nothing, guard);
            const auto named = guard.contains ("no MTrk chunks");
            std::cout << "  info  SLICERSCORE ctl-miditag tampered=" << (tampered ? "yes" : "no")
                      << " cleanReads=" << (readClean ? "yes" : "no")
                      << " brokenReads=" << (readBroken ? "yes" : "no")
                      << " guardNamed=" << (named ? "yes" : "no") << "  [" << guard << "]\n";
            report (tampered && readClean && ! readBroken && named,
                    "a midi whose track tag is not MTrk is refused by name instead of parsing as empty",
                    "the four tamper flags above carry the reason");
        }
    }
} // namespace

void runSlicerScoreChecks (const ReportFn& report)
{
    const auto fixtures = findFixturesDir();
    report (fixtures.isDirectory(), "the slicer fixture directory is reachable from this run",
            "cwd " + juce::File::getCurrentWorkingDirectory().getFullPathName());
    if (! fixtures.isDirectory())
    {
        std::cout << "  info  SLICERSCORE no fixture directory: the table below is controls only\n";
        std::cout.flush();
        runControls (report);
        return;
    }

    const auto inventory = loadTakes (fixtures);
    const auto rows = scoreInventory (inventory);
    const auto baseline = readBaseline (fixtures);

    for (const auto& r : rows)
    {
        std::cout << "  info  " << r.line() << '\n';
        std::cout.flush();
    }
    if (rows.empty())
    {
        std::cout << "  info  SLICERSCORE takes=0 baseline="
                  << (baseline.present ? "present" : "absent")
                  << " - the fixture gate is idle until the rendered takes land\n";
        std::cout.flush();
    }
    if (inventory.problems.isNotEmpty())
        std::cout << "  info  SLICERSCORE load problems: " << inventory.problems << '\n';
    if (inventory.notes.isNotEmpty())
        std::cout << "  info  SLICERSCORE notes: " << inventory.notes << '\n';
    std::cout.flush();

    report (inventory.problems.isEmpty(), "every slicer marks file parses and every wav it names decodes",
            inventory.problems);

    const auto noTakesYet = rows.empty() && inventory.problems.isEmpty();
    report (baseline.present || noTakesYet,
            "the slicer baseline is present for the takes on disk (bless it with --bless-slicer-baseline)",
            noTakesYet ? "no takes on disk yet" : "fixtures/slicer-baseline.json is missing");
    report (! baseline.present || baseline.problem.isEmpty(), "the slicer baseline file is readable",
            baseline.problem);
    report (! baseline.present || baseline.tuningMismatch.isEmpty(),
            "the slicer baseline was blessed under the tuning this binary was built with",
            baseline.tuningMismatch);
    report (! baseline.present || rows.empty() || inventoryDiff (rows, baseline).isEmpty(),
            "the baseline lists exactly the fixture take/sensitivity pairs on disk",
            inventoryDiff (rows, baseline));
    report (! baseline.present || rows.empty() || bandViolations (rows, baseline).isEmpty(),
            "every scored fixture take stays inside its blessed band (a move either way needs reading)",
            bandViolations (rows, baseline));

    // A dense fixture is *expected* to outrun the low knobs: the ceiling at 0.20 is seventeen slices
    // and a 16th-note hat line has more events than that in six seconds, so gating on a capped row
    // would gate out the material the ruler exists to measure. The pathology worth the failure is a
    // take no knob can cover, because then every one of its rows measures the slice budget rather
    // than the detector - which is the case `ctl-busy` above builds deliberately, in memory.
    juce::String cappedRows;
    for (const auto& r : rows)
        if (r.capped)
            cappedRows << r.key() << " ";
    if (cappedRows.isNotEmpty())
    {
        std::cout << "  info  SLICERSCORE rows pinned by the slice budget, where recall is a ceiling "
                     "and not a measurement: " << cappedRows << '\n';
        std::cout.flush();
    }
    report (uncoverableTakes (rows, inventory.takes).isEmpty(),
            "every fixture take is coverable by at least one sensitivity in the grid",
            uncoverableTakes (rows, inventory.takes));

    runControls (report);
}

juce::String writeSlicerBaseline()
{
    const auto fixtures = findFixturesDir();
    if (! fixtures.isDirectory())
        return "no fixtures/ directory is reachable from this run";

    const auto inventory = loadTakes (fixtures);
    if (inventory.problems.isNotEmpty())
        return "fixtures did not load cleanly: " + inventory.problems;

    const auto rows = scoreInventory (inventory);
    if (rows.empty())
        return "there are no takes in fixtures/marks yet, so there is nothing to bless";

    if (const auto uncoverable = uncoverableTakes (rows, inventory.takes); uncoverable.isNotEmpty())
        return "refusing to bless a table that cannot pass: no sensitivity in the grid covers these takes: "
             + uncoverable;

    const auto file = fixtures.getChildFile (kBaselineName);
    file.replaceWithText (baselineText (rows));
    if (! file.existsAsFile())
        return "the table was built but " + file.getFullPathName() + " could not be written";
    return {};
}

} // namespace sliceforge::test
