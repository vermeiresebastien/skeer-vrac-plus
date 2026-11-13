#pragma once

#include <JuceHeader.h>
#include <array>

namespace vrac
{
    constexpr int kNumPads = 16;
    constexpr int kNumBuses = 8;
    constexpr int kNumFamilies = 4;
    constexpr int kModelsPerFamily = 4;
    constexpr int kNumModels = kNumFamilies * kModelsPerFamily;
    constexpr int kMaxSlices = 64;
    constexpr int kPadMidiBase = 36; // C1

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
} // namespace vrac
