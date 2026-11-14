#include "Params.h"

namespace sliceforge
{
    namespace
    {
        constexpr std::array<const char*, kNumPads> kPadNotes {
            "C1", "C#1", "D1", "D#1", "E1", "F1", "F#1", "G1",
            "G#1", "A1", "A#1", "B1", "C2", "C#2", "D2", "D#2"
        };

        constexpr std::array<const char*, kNumBuses> kBusNames {
            "Output 1", "Output 2", "Output 3", "Output 4",
            "Output 5", "Output 6", "Output 7", "Output 8"
        };

        const std::array<std::array<ModelInfo, kModelsPerFamily>, kNumFamilies> kModels {{
            { { { "ROUND", "RND", "Long, round body with a soft beater." },
                { "PUNCH", "PCH", "Fast pitch drop, tight and aggressive." },
                { "808", "808", "Deep sub tail that carries the note." },
                { "BROKEN", "BRK", "Driven, torn speaker texture." } } },
            { { { "CRACK", "CRK", "Wide noise crack with a woody tone." },
                { "TIGHT", "TGT", "Short, dry and close to the mic." },
                { "BRUSH", "BRS", "Loose brush sweep and soft transients." },
                { "RIM", "RIM", "Hard rim click, almost a woodblock." } } },
            { { { "CLOSED", "CLS", "Clipped, bright closed hat." },
                { "OPEN", "OPN", "Open choke-ready hat with air." },
                { "METAL", "MTL", "Six-square metallic machine hat." },
                { "AIR", "AIR", "Wide, breathy top end." } } },
            { { { "CLASSIC", "CLP", "Four-burst hand clap with tail." },
                { "TIGHT", "TGT", "Snappy, close and dry." },
                { "BIG", "BIG", "Room-sized crowd clap." },
                { "SNAP", "SNP", "Two-burst finger snap." } } }
        }};

        constexpr std::array<const char*, 4> kFamilyNames { "KICK", "SNARE", "HAT", "CLAP" };
        constexpr std::array<const char*, 4> kFamilyPlurals { "KICKS", "SNARES", "HATS", "CLAPS" };

        struct PadDefault
        {
            int family;
            int model;
            float drumify;
            float length;
            int choke;
            float pan;
        };

        constexpr std::array<PadDefault, kNumPads> kPadDefaults {{
            { kick,  0, 0.72f, 0.42f, 1, 0.0f },
            { kick,  1, 0.58f, 0.26f, 1, 0.0f },
            { snare, 0, 0.68f, 0.24f, 0, 0.0f },
            { clap,  0, 0.62f, 0.30f, 0, 0.0f },
            { snare, 1, 0.76f, 0.22f, 0, 0.0f },
            { snare, 2, 0.48f, 0.34f, 0, 0.0f },
            { hat,   0, 0.80f, 0.09f, 2, -0.12f },
            { hat,   0, 0.64f, 0.08f, 2, 0.12f },
            { hat,   1, 0.85f, 0.30f, 2, 0.0f },
            { hat,   2, 0.55f, 0.14f, 2, -0.2f },
            { clap,  3, 0.70f, 0.15f, 0, 0.0f },
            { clap,  2, 0.52f, 0.38f, 0, 0.0f },
            { hat,   3, 0.60f, 0.20f, 2, 0.2f },
            { hat,   0, 0.90f, 0.06f, 2, 0.0f },
            { snare, 3, 0.66f, 0.12f, 0, 0.0f },
            { clap,  0, 0.74f, 0.28f, 0, 0.0f }
        }};
    } // namespace

    const std::array<std::array<ModelInfo, kModelsPerFamily>, kNumFamilies>& modelBank() { return kModels; }
    const std::array<const char*, kNumPads>& padNotes() { return kPadNotes; }
    const std::array<const char*, kNumBuses>& busNames() { return kBusNames; }

    const char* familyName (int family)
    {
        return kFamilyNames[(size_t) juce::jlimit (0, 3, family)];
    }

    const char* familyPlural (int family)
    {
        return kFamilyPlurals[(size_t) juce::jlimit (0, 3, family)];
    }

    juce::Colour familyColour (int family)
    {
        static const juce::Colour colours[] {
            juce::Colour (0xffff6500), juce::Colour (0xff716c63),
            juce::Colour (0xff00cc66), juce::Colour (0xffd9382e)
        };
        return colours[(size_t) juce::jlimit (0, 3, family)];
    }

    juce::Colour familyColourDim (int family)
    {
        static const juce::Colour colours[] {
            juce::Colour (0xffcc5100), juce::Colour (0xff5a554d),
            juce::Colour (0xff00994d), juce::Colour (0xffa92a22)
        };
        return colours[(size_t) juce::jlimit (0, 3, family)];
    }

    juce::String padParamId (int pad, const char* name)
    {
        return "p" + juce::String (pad + 1).paddedLeft ('0', 2) + "_" + name;
    }

    juce::String modelNameForIndex (int modelIndex)
    {
        const auto family = familyForModel (modelIndex);
        const auto local = localModelForIndex (modelIndex);
        return juce::String (familyName (family)) + " " + kModels[(size_t) family][(size_t) local].name;
    }

    int familyForModel (int modelIndex) { return juce::jlimit (0, kNumFamilies - 1, modelIndex / kModelsPerFamily); }
    int localModelForIndex (int modelIndex) { return juce::jlimit (0, kModelsPerFamily - 1, modelIndex % kModelsPerFamily); }

    int defaultFamilyForPad (int index)
    {
        return kPadDefaults[(size_t) juce::jlimit (0, kNumPads - 1, index)].family;
    }

    PadSettings makeDefaultPad (int index)
    {
        const auto& d = kPadDefaults[(size_t) juce::jlimit (0, kNumPads - 1, index)];
        PadSettings pad;
        pad.modelIndex = d.family * kModelsPerFamily + d.model;
        pad.drumify = d.drumify;
        pad.length = d.length;
        pad.choke = d.choke;
        pad.pan = d.pan;
        return pad;
    }

    std::array<PadSettings, kNumPads> makeDefaultPads()
    {
        std::array<PadSettings, kNumPads> pads;
        for (int i = 0; i < kNumPads; ++i)
            pads[(size_t) i] = makeDefaultPad (i);
        return pads;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add (std::make_unique<AudioParameterFloat> ("global.fine", "Fine Tune", -50.0f, 50.0f, 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> ("global.drumify", "Global Drumify", 0.0f, 200.0f, 100.0f));
        layout.add (std::make_unique<AudioParameterFloat> ("global.volume", "Master Volume", -24.0f, 6.0f, -2.4f));
        layout.add (std::make_unique<AudioParameterFloat> ("global.transpose", "Transpose", -24.0f, 24.0f, 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> ("global.length", "Global Length", 0.25f, 2.0f, 1.0f));
        layout.add (std::make_unique<AudioParameterFloat> ("global.tone", "Global Tone", -12.0f, 12.0f, 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> ("global.filter", "Global Filter", -1.0f, 1.0f, 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> ("global.width", "Stereo Width", 0.0f, 1.0f, 0.65f));
        // The output stage keeps its two parameters at the very end of the layout; see below.
        layout.add (std::make_unique<AudioParameterFloat> ("source.sensitivity", "Auto Slice Sensitivity", 0.0f, 1.0f, 0.55f));

        StringArray modelChoices;
        for (int i = 0; i < kNumModels; ++i)
            modelChoices.add (modelNameForIndex (i));

        for (int pad = 0; pad < kNumPads; ++pad)
        {
            const auto prefix = "P" + String (pad + 1).paddedLeft ('0', 2) + " ";
            const auto defaults = makeDefaultPad (pad);
            layout.add (std::make_unique<AudioParameterChoice> (padParamId (pad, "model"), prefix + "Model", modelChoices, defaults.modelIndex));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "drumify"), prefix + "Drumify", 0.0f, 1.0f, defaults.drumify));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "original"), prefix + "Original Level", -24.0f, 12.0f, defaults.originalGain));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "drumified"), prefix + "Drumified Level", -24.0f, 12.0f, defaults.drumifiedGain));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "pitch"), prefix + "Pitch", -24.0f, 24.0f, defaults.pitch));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "length"), prefix + "Length", 0.02f, 2.0f, defaults.length));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "volume"), prefix + "Volume", -60.0f, 6.0f, defaults.volume));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "pan"), prefix + "Pan", -1.0f, 1.0f, defaults.pan));
            layout.add (std::make_unique<AudioParameterInt> (padParamId (pad, "choke"), prefix + "Choke Group", 0, 4, defaults.choke));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "velocity"), prefix + "Velocity Amount", 0.0f, 1.0f, defaults.velocityAmount));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "warp"), prefix + "Warp", 0.0f, 1.0f, defaults.warp));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "impact"), prefix + "Impact", 0.0f, 1.0f, defaults.impact));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "instability"), prefix + "Instability", 0.0f, 1.0f, defaults.instability));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "tone"), prefix + "Tone Shift", -12.0f, 12.0f, defaults.toneShift));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "super"), prefix + "Super", 0.0f, 1.0f, defaults.super));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "eqLow"), prefix + "EQ Low", -12.0f, 12.0f, defaults.eqLow));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "eqMid"), prefix + "EQ Mid", -12.0f, 12.0f, defaults.eqMid));
            layout.add (std::make_unique<AudioParameterFloat> (padParamId (pad, "eqHigh"), prefix + "EQ High", -12.0f, 12.0f, defaults.eqHigh));
            layout.add (std::make_unique<AudioParameterInt> (padParamId (pad, "bus"), prefix + "Output", 1, 8, defaults.bus));
            layout.add (std::make_unique<AudioParameterBool> (padParamId (pad, "mute"), prefix + "Mute", false));
            layout.add (std::make_unique<AudioParameterBool> (padParamId (pad, "solo"), prefix + "Solo", false));
        }

        // The output stage is added last, after every pad parameter, so a host session that
        // indexes parameters by number keeps pointing at the same control it always did.
        layout.add (std::make_unique<AudioParameterFloat> ("global.clip", "Soft Clip", 0.0f, 1.0f, 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> ("global.out", "Out Gain", -24.0f, 12.0f, 0.0f));
        return layout;
    }

    void ParameterCache::connect (juce::AudioProcessorValueTreeState& state)
    {
        apvts = &state;
        auto raw = [&state] (const juce::String& id) { return state.getRawParameterValue (id); };
        for (int i = 0; i < kNumPads; ++i)
        {
            auto& p = pads[(size_t) i];
            p.model = raw (padParamId (i, "model"));
            p.drumify = raw (padParamId (i, "drumify"));
            p.originalGain = raw (padParamId (i, "original"));
            p.drumifiedGain = raw (padParamId (i, "drumified"));
            p.pitch = raw (padParamId (i, "pitch"));
            p.length = raw (padParamId (i, "length"));
            p.volume = raw (padParamId (i, "volume"));
            p.pan = raw (padParamId (i, "pan"));
            p.choke = raw (padParamId (i, "choke"));
            p.velocityAmount = raw (padParamId (i, "velocity"));
            p.warp = raw (padParamId (i, "warp"));
            p.impact = raw (padParamId (i, "impact"));
            p.instability = raw (padParamId (i, "instability"));
            p.toneShift = raw (padParamId (i, "tone"));
            p.super = raw (padParamId (i, "super"));
            p.eqLow = raw (padParamId (i, "eqLow"));
            p.eqMid = raw (padParamId (i, "eqMid"));
            p.eqHigh = raw (padParamId (i, "eqHigh"));
            p.bus = raw (padParamId (i, "bus"));
            p.mute = raw (padParamId (i, "mute"));
            p.solo = raw (padParamId (i, "solo"));
        }
        global.fineTune = raw ("global.fine");
        global.drumify = raw ("global.drumify");
        global.volume = raw ("global.volume");
        global.transpose = raw ("global.transpose");
        global.length = raw ("global.length");
        global.tone = raw ("global.tone");
        global.filter = raw ("global.filter");
        global.width = raw ("global.width");
        global.sensitivity = raw ("source.sensitivity");
        global.clip = raw ("global.clip");
        global.outGain = raw ("global.out");
    }

    PadSettings ParameterCache::readPad (int index) const
    {
        const auto& p = pads[(size_t) index];
        PadSettings s;
        s.modelIndex = (int) p.model->load();
        s.drumify = p.drumify->load();
        s.originalGain = p.originalGain->load();
        s.drumifiedGain = p.drumifiedGain->load();
        s.pitch = p.pitch->load();
        s.length = p.length->load();
        s.volume = p.volume->load();
        s.pan = p.pan->load();
        s.choke = (int) p.choke->load();
        s.velocityAmount = p.velocityAmount->load();
        s.warp = p.warp->load();
        s.impact = p.impact->load();
        s.instability = p.instability->load();
        s.toneShift = p.toneShift->load();
        s.super = p.super->load();
        s.eqLow = p.eqLow->load();
        s.eqMid = p.eqMid->load();
        s.eqHigh = p.eqHigh->load();
        s.bus = (int) p.bus->load();
        s.mute = p.mute->load() > 0.5f;
        s.solo = p.solo->load() > 0.5f;
        return s;
    }

    GlobalSettings ParameterCache::readGlobal() const
    {
        GlobalSettings s;
        s.fineTune = global.fineTune->load();
        s.drumify = global.drumify->load();
        s.volume = global.volume->load();
        s.transpose = global.transpose->load();
        s.length = global.length->load();
        s.tone = global.tone->load();
        s.filter = global.filter->load();
        s.width = global.width->load();
        s.clip = global.clip->load();
        s.outGain = global.outGain->load();
        return s;
    }

    float ParameterCache::readSensitivity() const { return global.sensitivity->load(); }

    void ParameterCache::writePad (int index, const PadSettings& s)
    {
        auto set = [this] (const juce::String& id, float value)
        {
            if (auto* p = apvts->getParameter (id))
            {
                const auto range = p->getNormalisableRange();
                p->setValueNotifyingHost (range.convertTo0to1 (value));
            }
        };
        set (padParamId (index, "model"), (float) s.modelIndex);
        set (padParamId (index, "drumify"), s.drumify);
        set (padParamId (index, "original"), s.originalGain);
        set (padParamId (index, "drumified"), s.drumifiedGain);
        set (padParamId (index, "pitch"), s.pitch);
        set (padParamId (index, "length"), s.length);
        set (padParamId (index, "volume"), s.volume);
        set (padParamId (index, "pan"), s.pan);
        set (padParamId (index, "choke"), (float) s.choke);
        set (padParamId (index, "velocity"), s.velocityAmount);
        set (padParamId (index, "warp"), s.warp);
        set (padParamId (index, "impact"), s.impact);
        set (padParamId (index, "instability"), s.instability);
        set (padParamId (index, "tone"), s.toneShift);
        set (padParamId (index, "super"), s.super);
        set (padParamId (index, "eqLow"), s.eqLow);
        set (padParamId (index, "eqMid"), s.eqMid);
        set (padParamId (index, "eqHigh"), s.eqHigh);
        set (padParamId (index, "bus"), (float) s.bus);
        set (padParamId (index, "mute"), s.mute ? 1.0f : 0.0f);
        set (padParamId (index, "solo"), s.solo ? 1.0f : 0.0f);
    }

    void ParameterCache::writeGlobal (const GlobalSettings& s)
    {
        auto set = [this] (const juce::String& id, float value)
        {
            if (auto* p = apvts->getParameter (id))
            {
                const auto range = p->getNormalisableRange();
                p->setValueNotifyingHost (range.convertTo0to1 (value));
            }
        };
        set ("global.fine", s.fineTune);
        set ("global.drumify", s.drumify);
        set ("global.volume", s.volume);
        set ("global.transpose", s.transpose);
        set ("global.length", s.length);
        set ("global.tone", s.tone);
        set ("global.filter", s.filter);
        set ("global.width", s.width);
        set ("global.clip", s.clip);
        set ("global.out", s.outGain);
    }
} // namespace sliceforge
