#pragma once

#include <JuceHeader.h>
#include <array>

namespace sliceforge
{
    constexpr int kNumPads = 16;
    constexpr int kNumBuses = 8;
    constexpr int kNumFamilies = 4;
    constexpr int kModelsPerFamily = 4;
    constexpr int kNumModels = kNumFamilies * kModelsPerFamily;
    constexpr int kMaxSlices = 64;
    constexpr int kPadMidiBase = 36; // C1
    /*  The drum channel, 1-based as every juce::MidiMessage channel argument is. One map, three
        users: the incoming notes the plugin decodes at PluginProcessor.cpp, the live MIDI-out bus
        (Step 28) and the exported clip (Sequencer.cpp's makePatternMidiFile). A second constant here
        would be a second truth, which is exactly how a clip and a performance stop agreeing. */
    constexpr int kPadMidiChannel = 10;

    /*  The pattern bank lives here rather than in Sequencer.h because both sides need it: the
        parameter layout declares four choices, and ParameterCache::readActivePattern clamps the host
        into that bank. Sequencer.h includes this header, so moving them the other way would leave the
        cache guessing its own ceiling. */
    constexpr int kSeqPatterns = 4;

    /** Value for the live pattern selector that means "nobody is driving it from the parameter
        surface, use the document's own copy". The scheduler's authority is the `seq.pattern`
        parameter; `SequencerData::activePattern` is the mirror that offline callers - the MIDI
        exporter, a preset blob - read when they have no access to a running processor. */
    constexpr int kSeqPatternFromDocument = -1;

    enum Family
    {
        kick = 0,
        snare = 1,
        hat = 2,
        clap = 3
    };

    struct ModelInfo
    {
        const char* name;
        const char* code;
        const char* blurb;
    };

    const std::array<std::array<ModelInfo, kModelsPerFamily>, kNumFamilies>& modelBank();
    const std::array<const char*, kNumPads>& padNotes();
    const std::array<const char*, kNumBuses>& busNames();
    const char* familyName (int family);
    const char* familyPlural (int family);
    juce::Colour familyColour (int family);
    juce::Colour familyColourDim (int family);

    juce::String padParamId (int pad, const char* name);
    juce::String modelNameForIndex (int modelIndex);
    int familyForModel (int modelIndex);
    int localModelForIndex (int modelIndex);

    struct GlobalSettings
    {
        float fineTune = 0.0f;
        float drumify = 100.0f;
        float volume = -2.4f;
        float transpose = 0.0f;
        float length = 1.0f;
        float tone = 0.0f;
        float filter = 0.0f;
        float width = 0.65f;
        // Appended last: the factory presets and saved kits initialise the eight fields
        // above positionally, so anything new must go after them and keep its own default.
        float clip = 0.0f;      // 0 = soft clipper bypassed, 1 = knee at 0.35
        float outGain = 0.0f;   // dB, applied after the clipper
    };

    struct PadSettings
    {
        int modelIndex = 0;
        float drumify = 0.65f;
        float originalGain = -4.4f;
        float drumifiedGain = -4.5f;
        float pitch = 0.0f;
        float length = 0.25f;
        float volume = 0.0f;
        float pan = 0.0f;
        int choke = 0;
        float velocityAmount = 0.65f;
        float warp = 0.2f;
        float impact = 0.35f;
        float instability = 0.08f;
        float toneShift = 0.0f;
        float super = 0.18f;
        float eqLow = 0.0f;
        float eqMid = 0.0f;
        float eqHigh = 0.0f;
        int bus = 1;
        bool mute = false;
        bool solo = false;
    };

    struct PadParamPtrs
    {
        std::atomic<float>* model = nullptr;
        std::atomic<float>* drumify = nullptr;
        std::atomic<float>* originalGain = nullptr;
        std::atomic<float>* drumifiedGain = nullptr;
        std::atomic<float>* pitch = nullptr;
        std::atomic<float>* length = nullptr;
        std::atomic<float>* volume = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* choke = nullptr;
        std::atomic<float>* velocityAmount = nullptr;
        std::atomic<float>* warp = nullptr;
        std::atomic<float>* impact = nullptr;
        std::atomic<float>* instability = nullptr;
        std::atomic<float>* toneShift = nullptr;
        std::atomic<float>* super = nullptr;
        std::atomic<float>* eqLow = nullptr;
        std::atomic<float>* eqMid = nullptr;
        std::atomic<float>* eqHigh = nullptr;
        std::atomic<float>* bus = nullptr;
        std::atomic<float>* mute = nullptr;
        std::atomic<float>* solo = nullptr;
    };

    struct GlobalParamPtrs
    {
        std::atomic<float>* fineTune = nullptr;
        std::atomic<float>* drumify = nullptr;
        std::atomic<float>* volume = nullptr;
        std::atomic<float>* transpose = nullptr;
        std::atomic<float>* length = nullptr;
        std::atomic<float>* tone = nullptr;
        std::atomic<float>* filter = nullptr;
        std::atomic<float>* width = nullptr;
        std::atomic<float>* sensitivity = nullptr;
        std::atomic<float>* clip = nullptr;
        std::atomic<float>* outGain = nullptr;
        // Appended after them all, because seq.pattern is the newest parameter on the surface.
        std::atomic<float>* pattern = nullptr;
    };

    struct ParameterCache
    {
        std::array<PadParamPtrs, kNumPads> pads;
        GlobalParamPtrs global;
        juce::AudioProcessorValueTreeState* apvts = nullptr;

        void connect (juce::AudioProcessorValueTreeState& state);
        PadSettings readPad (int index) const;
        GlobalSettings readGlobal() const;
        float readSensitivity() const;
        /** The live pattern choice as the audio path should use it: the raw (denormalised, so
            already an index) value of `seq.pattern`, rounded and clamped into 0..3. Safe to call
            from the render thread - one atomic load, no allocation. */
        int readActivePattern() const;
        void writePad (int index, const PadSettings& settings);
        void writeGlobal (const GlobalSettings& settings);
    };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    std::array<PadSettings, kNumPads> makeDefaultPads();
    PadSettings makeDefaultPad (int index);
    int defaultFamilyForPad (int index);

    inline float clampValue (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
    inline double clampValue (double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
    inline float dbToGain (float db) { return db <= -96.0f ? 0.0f : std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float gain) { return gain <= 1.0e-8f ? -120.0f : 20.0f * std::log10 (gain); }
    inline float smoothstep (float a, float b, float x)
    {
        const auto t = clampValue ((x - a) / (b - a), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
} // namespace sliceforge
