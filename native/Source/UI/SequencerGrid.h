#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../Sequencer.h"

namespace vrac::ui
{
    class SequencerGrid final : public juce::Component
    {
    public:
        enum class EditMode
        {
            Draw,
            Velocity,
            Probability,
            Ratchet,
            Nudge,
            Flam
        };

        explicit SequencerGrid (VRACPlusAudioProcessor& processor);

        void setData (std::shared_ptr<const vrac::SequencerData> newData);
        void setPlayheadStep (int step);
        void setEditMode (EditMode mode);
        void setSelectedLane (int lane);
        int getSelectedLane() const noexcept { return selectedLane; }

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void()> onGestureStart;
        std::function<void (int, int, const vrac::SeqStep&)> onStepChanged;
        std::function<void (int, int, juce::Point<int>)> onStepInspectorRequested;
        std::function<void (int)> onLaneMuteClicked;
        std::function<void (int)> onLaneSoloClicked;
        std::function<void (int)> onLaneSelected;

    private:
        void updateLayout();
        int padAtY (float y) const;
        int stepAtX (float x) const;
        juce::Rectangle<float> cellBounds (int pad, int step) const;
        int dataStepForLane (int pad, int displayStep) const;
        int laneLengthForPad (int pad) const;
        void applyCell (int pad, int displayStep, float relativeY);

        VRACPlusAudioProcessor& processorRef;
        std::shared_ptr<const vrac::SequencerData> data;
        EditMode mode = EditMode::Draw;
        int playheadStep = -1;
        int selectedLane = 0;
        int dragPad = -1;
        int dragStep = -1;
        bool dragTarget = true;

        static constexpr int headerHeight = 22;
        static constexpr int labelWidth = 200;
        float cellWidth = 1.0f;
        float rowHeight = 1.0f;
        std::array<juce::Rectangle<float>, vrac::kNumPads> rowRects;
        std::array<juce::Rectangle<float>, vrac::kNumPads> muteRects;
        std::array<juce::Rectangle<float>, vrac::kNumPads> soloRects;
    };
} // namespace vrac::ui
