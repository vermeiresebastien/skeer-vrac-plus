#pragma once

#include <JuceHeader.h>
#include "../Engine/SampleData.h"
#include "../Engine/Slicer.h"

namespace vrac::ui
{
    class WaveformDisplay final : public juce::Component
    {
    public:
        WaveformDisplay();

        void setSample (SamplePtr newSample);
        void setSlices (const std::vector<Slice>& newSlices);
        void setStart (double seconds);
        void setFocusSeconds (double seconds);
        void setInteractive (bool shouldBeInteractive);
        void setCompact (bool shouldBeCompact);
        void setAccent (juce::Colour colour);
        double getStart() const noexcept { return startSeconds; }

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void (double)> onStartChanged;
        std::function<void()> onGestureStart;

    private:
        double timeAt (float x) const;
        float xForTime (double time) const;
        juce::Rectangle<float> boundsForWave() const;

        SamplePtr sample;
        std::vector<Slice> slices;
        double startSeconds = 0.0;
        double focusSeconds = 0.0;
        bool interactive = true;
        bool compact = false;
        juce::Colour accent;
        bool dragging = false;
    };
} // namespace vrac::ui

