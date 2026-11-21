#include "GrooveBankTest.h"

#include <cmath>
#include <cstring>

namespace sliceforge::test
{
    namespace
    {
        /*  Sixty rows share one invariant each, so a check that says "the bank is wrong" is not
            actionable. An aggregate walks the bank, stops at the first trip and names that groove. */
        struct Offender
        {
            int index = -1;
            juce::String what;

            bool failed() const { return index >= 0; }

            juce::String detail() const
            {
                const auto bank = grooveBank();
                if (index < 0 || index >= (int) bank.size())
                    return what;
                return grooveLabel (bank[(size_t) index]) + "  -  " + what;
            }
        };

        int litReach (const Groove& groove)
        {
            auto reach = -1;
            for (const auto& lane : groove.lanes)
            {
                const auto length = (int) std::strlen (lane.steps);
                for (int step = 0; step < length; ++step)
                    if (lane.steps[step] != '.' && step > reach)
                        reach = step;
            }
            return reach;
        }

        bool lightsBarTwo (const Groove& groove)
        {
            for (const auto& lane : groove.lanes)
                for (int step = kSeqSteps / 2; step < (int) std::strlen (lane.steps); ++step)
                    if (lane.steps[step] != '.')
                        return true;
            return false;
        }
    } // namespace

    void runGrooveBankChecks (const Report& check)
    {
        const auto bank = grooveBank();

        /*  The count is pinned on purpose: the product page names twenty genres and three variations
            each, and this suite is the only place that number can be contradicted. Adding a genre
            means deciding to move this row, which is the whole point. */
        check ((int) bank.size() == 60, "the groove bank holds the 20 genres x 3 variations the product names",
               juce::String ((int) bank.size ()));

        {
            Offender bad;
            for (int i = 0; i < (int) bank.size() && ! bad.failed(); )
            {
                /*  grooveGenreRun, not a local string walk: this is the same grouping the GROOVES
                    menu builds its submenus from, so a run length here is a submenu length there. */
                const auto end = grooveGenreRun (i).second;
                if (end - i + 1 != 3)
                {
                    bad.index = i;
                    bad.what = "genre run is " + juce::String (end - i + 1) + " rows, want 3";
                    break;
                }
                for (int earlier = 0; earlier < i && ! bad.failed(); ++earlier)
                    if (juce::String (bank[(size_t) earlier].genre)
                        == juce::String (bank[(size_t) i].genre))
                    {
                        bad.index = i;
                        bad.what = "genre resumes after another genre was listed";
                    }
                i = end + 1;
            }
            check (! bad.failed(), "every genre carries three variations and one contiguous run",
                   bad.failed() ? bad.detail() : juce::String());
        }

        {
            Offender bad;
            for (int i = 0; i < (int) bank.size() && ! bad.failed(); ++i)
            {
                const auto label = grooveLabel (bank[(size_t) i]);
                if (! bank[(size_t) i].genre[0] || ! bank[(size_t) i].name[0])
                {
                    bad.index = i;
                    bad.what = "an empty genre or variation name";
                    break;
                }
                for (int j = 0; j < i; ++j)
                    if (label == grooveLabel (bank[(size_t) j]))
                    {
                        bad.index = i;
                        bad.what = "shares its label with another row";
                        break;
                    }
            }
            check (! bad.failed(), "no two grooves share a display label",
                   bad.failed() ? bad.detail() : juce::String());
        }

        {
            Offender bad;
            for (int i = 0; i < (int) bank.size() && ! bad.failed(); ++i)
                for (const auto& lane : bank[(size_t) i].lanes)
                {
                    if (lane.steps == nullptr)
                    {
                        bad.index = i;
                        bad.what = "pad " + juce::String (lane.pad) + " has no mask";
                        break;
                    }
                    for (const char* c = lane.steps; *c != '\0'; ++c)
                        if (! isGrooveStepChar (*c))
                        {
                            bad.index = i;
                            bad.what = "pad " + juce::String (lane.pad) + " uses '"
                                + juce::String::charToString ((juce::juce_wchar) (unsigned char) *c)
                                + "', outside the groove alphabet";
                            break;
                        }
                }
            check (! bad.failed(), "every lane character is in the groove alphabet",
                   bad.failed() ? bad.detail() : juce::String());
        }

        {
            Offender bad;
            for (int i = 0; i < (int) bank.size() && ! bad.failed(); ++i)
                for (const auto& lane : bank[(size_t) i].lanes)
                {
                    const auto length = (int) std::strlen (lane.steps);
                    if (lane.loop > 0)
                    {
                        /*  A looped lane states one cycle: the scheduler wraps it, so a mask longer
                            than the loop is data that can never be read and a loop longer than the
                            mask reads past the string. */
                        if (length != lane.loop)
                        {
                            bad.index = i;
                            bad.what = "pad " + juce::String (lane.pad) + " loops every "
                                + juce::String (lane.loop) + " steps but its mask is "
                                + juce::String (length);
                            break;
                        }
                    }
                    else if (length != kSeqSteps)
                    {
                        bad.index = i;
                        bad.what = "pad " + juce::String (lane.pad) + " mask is "
                            + juce::String (length) + " steps, want the full " + juce::String (kSeqSteps);
                        break;
                    }
                }
            check (! bad.failed(), "every mask spans the grid, or exactly one loop of its lane",
                   bad.failed() ? bad.detail() : juce::String());
        }

        {
            Offender bad;
            for (int i = 0; i < (int) bank.size() && ! bad.failed(); ++i)
            {
                std::array<int, kNumPads> uses {};
                auto lanes = 0;
                for (const auto& lane : bank[(size_t) i].lanes)
                {
                    ++lanes;
                    if (lane.pad < 0 || lane.pad >= kNumPads)
                    {
                        bad.index = i;
                        bad.what = "a lane names pad " + juce::String (lane.pad);
                        break;
                    }
                    if (++uses[(size_t) lane.pad] == 2)
                    {
                        bad.index = i;
                        bad.what = "pad " + juce::String (lane.pad) + " is written by two lanes";
                        break;
                    }
                }
                if (! bad.failed() && lanes < 4)
                {
                    bad.index = i;
                    bad.what = "only " + juce::String (lanes) + " lanes";
                }
            }
            check (! bad.failed(), "every groove writes 4+ distinct pads",
                   bad.failed() ? bad.detail() : juce::String());
        }

        /*  The owner's requirement, in the data: a preset that stops at step 15 is a one-bar groove
            wearing a two-bar label. The reach test is the strict half - the last hit of every lane
            has to sit in the second half of bar two, not merely past step 16. */
        {
            Offender bad;
            for (int i = 0; i < (int) bank.size() && ! bad.failed(); ++i)
            {
                const auto& groove = bank[(size_t) i];
                if (! lightsBarTwo (groove))
                {
                    bad.index = i;
                    bad.what = "nothing is lit in bar two";
                    break;
                }
                if (litReach (groove) < 24)
                {
                    bad.index = i;
                    bad.what = "its last hit is at step " + juce::String (litReach (groove));
                    break;
                }
                auto openLanes = 0;
                auto repeated = 0;
                for (const auto& lane : groove.lanes)
                {
                    if (lane.loop > 0)
                        continue;
                    ++openLanes;
                    if (std::strncmp (lane.steps, lane.steps + (kSeqSteps / 2), kSeqSteps / 2) == 0)
                        ++repeated;
                }
                if (openLanes > 0 && repeated == openLanes)
                {
                    bad.index = i;
                    bad.what = "bar two repeats bar one in every lane";
                    break;
                }
            }
            check (! bad.failed(), "every groove plays a real second bar, not a copy of the first",
                   bad.failed() ? bad.detail() : juce::String());
        }

        {
            Offender bad;
            for (int i = 0; i < (int) bank.size() && ! bad.failed(); ++i)
            {
                const auto& groove = bank[(size_t) i];
                if ((double) groove.bpm < kSeqMinBpm || (double) groove.bpm > kSeqMaxBpm)
                {
                    bad.index = i;
                    bad.what = juce::String (groove.bpm) + " BPM is outside the tempo the clock accepts";
                    break;
                }
                /*  32 steps is two bars at four steps per quarter and nothing else. A groove that
                    wanted a triplet grid would have to be one bar long, so the shuffle lives in
                    swing and the '<'/'>' pushes instead. */
                if (groove.division != 4)
                {
                    bad.index = i;
                    bad.what = "division " + juce::String (groove.division)
                        + " cannot make two bars out of " + juce::String (kSeqSteps) + " steps";
                    break;
                }
                if (groove.swing < 0.0f || groove.swing > 0.66f)
                {
                    bad.index = i;
                    bad.what = "swing " + juce::String (groove.swing, 2) + " is outside the control";
                    break;
                }
            }
            check (! bad.failed(), "every groove's tempo, division and swing fit the instrument",
                   bad.failed() ? bad.detail() : juce::String());
        }

        {
            Offender bad;
            for (int i = 0; i < (int) bank.size() && ! bad.failed(); ++i)
                for (const auto& voice : bank[(size_t) i].voicing)
                {
                    if (voice.pad < 0 || voice.pad >= kNumPads || voice.model < 0
                        || voice.model >= kNumModels)
                    {
                        bad.index = i;
                        bad.what = "a voice names pad " + juce::String (voice.pad) + " model "
                            + juce::String (voice.model);
                        break;
                    }
                    if (familyForModel (voice.model) != defaultFamilyForPad (voice.pad))
                    {
                        bad.index = i;
                        bad.what = "pad " + juce::String (voice.pad) + " is a "
                            + familyName (defaultFamilyForPad (voice.pad)) + " slot but is voiced "
                            + modelNameForIndex (voice.model);
                        break;
                    }
                }
            check (! bad.failed(), "MATCH SOUNDS never puts a genre's snare in a kick's pad",
                   bad.failed() ? bad.detail() : juce::String());
        }

        {
            Offender bad;
            for (int i = 0; i < (int) bank.size() && ! bad.failed(); ++i)
            {
                const auto named = grooveVoicing (i);
                for (const auto& voice : bank[(size_t) i].voicing)
                    if (named[(size_t) voice.pad] != voice.model)
                    {
                        bad.index = i;
                        bad.what = "pad " + juce::String (voice.pad) + " asks for model "
                            + juce::String (voice.model) + " but the lookup answers "
                            + juce::String (named[(size_t) voice.pad]);
                        break;
                    }
            }
            const auto none = grooveVoicing (-1);
            auto silent = true;
            for (const auto model : none)
                silent = silent && model < 0;
            check (! bad.failed() && silent, "the voicing lookup answers every pad the genre names",
                   bad.failed() ? bad.detail() : juce::String());
        }

        {
            Offender bad;
            for (int i = 0; i < (int) bank.size() && ! bad.failed(); ++i)
            {
                const auto& groove = bank[(size_t) i];
                SequencerData data;
                data.length = 16;
                data.bpm = 120.0;
                data.division = 8;
                data.swing = 0.0f;
                if (! applyGroove (data, i))
                {
                    bad.index = i;
                    bad.what = "refused its own index";
                    break;
                }
                if (data.length != kSeqSteps)
                {
                    bad.index = i;
                    bad.what = "left the grid at " + juce::String (data.length)
                        + " steps, so bar two would be unreachable";
                    break;
                }
                if (data.bpm != (double) groove.bpm || data.division != groove.division
                    || data.swing != groove.swing)
                {
                    bad.index = i;
                    bad.what = "applied a different feel than the row carries";
                    break;
                }
            }
            check (! bad.failed(), "a groove takes the grid with it: 32 steps, tempo, division, swing",
                   bad.failed() ? bad.detail() : juce::String());
        }

        {
            SequencerData untouched;
            const auto before = serialiseSequencer (untouched);
            const auto refusedLow = applyGroove (untouched, -1);
            const auto refusedHigh = applyGroove (untouched, (int) bank.size());
            const auto leftAlone = serialiseSequencer (untouched) == before;
            check (! refusedLow && ! refusedHigh && leftAlone,
                   "an index outside the bank is refused and changes nothing",
                   leftAlone ? juce::String ()
                             : "the refused call wrote into the document");

            const auto emptyOutside = makeGroovePattern (-1);
            auto blank = true;
            for (const auto& row : emptyOutside.rows)
                for (const auto& step : row)
                    blank = blank && ! step.on;
            for (const auto loop : emptyOutside.laneLength)
                blank = blank && loop == 0;
            check (blank, "a pattern for an unknown groove is silent, not garbage", juce::String ());
        }

        for (int arm = 0; arm < kSeqPatterns; ++arm)
        {
            SequencerData data;
            data.activePattern = arm;
            for (int p = 0; p < kSeqPatterns; ++p)
                data.patterns[(size_t) p] = makeGroovePattern (p * 5 + 2);
            const auto before = data;

            const auto picked = arm * 7 + 3;
            applyGroove (data, picked);

            const auto moved = serialisePattern (data.patterns[(size_t) arm])
                            != serialisePattern (before.patterns[(size_t) arm]);
            auto others = true;
            for (int p = 0; p < kSeqPatterns; ++p)
                if (p != arm && serialisePattern (data.patterns[(size_t) p])
                                  != serialisePattern (before.patterns[(size_t) p]))
                    others = false;
            /*  Arm A is the slot a fresh document plays, so a groove written into slot 0 whatever the
                selector says would pass arm A and break the other three - which is exactly the bug
                the per-arm split is here to catch. */
            check (moved && others && data.activePattern == arm,
                   "a groove lands in the selected pattern slot " + juce::String (arm + 1)
                       + " and leaves the other three",
                   moved ? (others ? juce::String() : "another slot changed too")
                         : "the selected slot did not change");
        }

        {
            Offender bad;
            for (int i = 0; i < (int) bank.size() && ! bad.failed(); ++i)
            {
                const auto pattern = makeGroovePattern (i);
                const auto block = serialisePattern (pattern);
                const auto restored = deserialisePattern (block);
                if (restored == nullptr || serialisePattern (*restored) != block)
                {
                    bad.index = i;
                    bad.what = restored == nullptr ? "the saved pattern would not load"
                                                   : "it loads as something else";
                }
            }
            check (! bad.failed(), "every groove survives a pattern save and reload",
                   bad.failed() ? bad.detail() : juce::String());
        }

        {
            auto looped = false;
            for (const auto& groove : bank)
                for (const auto& lane : groove.lanes)
                    if (lane.loop > 0 && lane.loop < kSeqSteps)
                        looped = true;
            check (looped, "the bank still contains a lane that cycles short of the grid",
                   juce::String());
        }

        {
            Offender bad;
            for (int start = 0; start < (int) bank.size() && ! bad.failed(); )
            {
                const auto end = grooveGenreRun (start).second;

                auto walked = 0;
                auto at = start;
                while (true)
                {
                    if (at < start || at > end)
                    {
                        bad.index = start;
                        bad.what = "cycling left its genre";
                        break;
                    }
                    at = nextGrooveVariation (at);
                    ++walked;
                    if (at == start || walked > 4)
                        break;
                }
                if (! bad.failed() && walked != end - start + 1)
                {
                    bad.index = start;
                    bad.what = "a full cycle visited " + juce::String (walked) + " of "
                        + juce::String (end - start + 1) + " variations";
                }
                start = end + 1;
            }
            check (! bad.failed() && nextGrooveVariation (-1) == -1
                       && nextGrooveVariation ((int) bank.size()) == (int) bank.size(),
                   "NEXT VARIATION walks its own genre and wraps",
                   bad.failed() ? bad.detail() : juce::String());
        }

        /*  The span claim, heard rather than read. A two-bar mask behind a 16-step grid is not a
            longer pattern, it is invisible data - wrapStep folds every absolute step back into
            `length`. So the check drives the real scheduler and asks whether a hit ever arrives in
            the second bar, and the control arm proves that the answer is not "always yes regardless
            of the grid": the same data with the length put back to 16 must never reach step 16.

            Two things decide what counts as a bar-two block, and a hand-erased second bar taught me
            both. The feel is taken out first - swing and every step nudge - because a bar-one hit
            that merely sounds late is still bar-one material, and this check is about material. With
            the feel gone a block can only fire the step its playhead points at or the one before it,
            so the playhead has to clear the boundary by a whole step: `>= 17`, not `>= 16`. The top
            boundary is excluded for the mirror-image reason: the block that ends the loop is where
            the NEXT loop's step 0 lands a hair early when `8.0 ppq` rounds down instead of up, so a
            playhead of 31 can carry a bar-one hit. The first version of this check asked only for
            `>= 16` and stayed green on a bank whose second bar had been emptied by hand. */
        {
            Offender bad;
            auto controlReachedBarTwo = false;
            juce::Random random (0x5EED);
            constexpr int blockSamples = 512;
            constexpr double sampleRate = 48000.0;
            // One whole 32-step loop: eight quarters at four steps per quarter. The horizon has to
            // cover the second bar end to end, because a groove is free to leave steps 16-20 empty.
            constexpr double horizonPpq = 8.0;

            for (int i = 0; i < (int) bank.size() && ! bad.failed(); ++i)
            {
                SequencerData data;
                applyGroove (data, i);
                data.swing = 0.0f;
                for (auto& row : data.patterns[(size_t) data.activePattern].rows)
                    for (auto& step : row)
                        step.nudge = 0.0f;
                const auto blockPpq = (double) blockSamples * (data.bpm / 60.0) / sampleRate;
                const auto blocks = (int) std::ceil (horizonPpq / blockPpq);

                SeqPlaybackState state;
                auto heard = false;
                for (int b = 0; b < blocks && ! heard; ++b)
                {
                    const SeqHostInfo host { true, true, blockPpq * (double) b, data.bpm };
                    SeqBlockOutput out;
                    processSequencerBlock (data, host, false, sampleRate, blockSamples, random, state, out);
                    heard = out.playheadStep >= kSeqSteps / 2 + 1
                               && out.playheadStep < kSeqSteps - 1
                               && out.triggerCount > 0;
                }
                if (! heard)
                {
                    bad.index = i;
                    bad.what = "the scheduler never fired a hit in bar two";
                    break;
                }

                SequencerData clipped = data;
                clipped.length = kSeqSteps / 2;
                SeqPlaybackState clipState;
                for (int b = 0; b < blocks && ! controlReachedBarTwo; ++b)
                {
                    const SeqHostInfo host { true, true, blockPpq * (double) b, data.bpm };
                    SeqBlockOutput out;
                    processSequencerBlock (clipped, host, false, sampleRate, blockSamples, random,
                                           clipState, out);
                    if (out.playheadStep >= kSeqSteps / 2)
                        controlReachedBarTwo = true;
                }
            }
            check (! bad.failed(),
                   "every groove puts real hits in bar two once the feel is taken out",
                   bad.failed() ? bad.detail() : juce::String());
            check (! controlReachedBarTwo, "the same grid at 16 steps cannot reach bar two at all",
                   controlReachedBarTwo ? "a 16-step window still scheduled step 16 or later"
                                        : juce::String());
        }
    }
} // namespace sliceforge::test
