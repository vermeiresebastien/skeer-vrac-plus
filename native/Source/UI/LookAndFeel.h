#pragma once

#include <JuceHeader.h>

namespace vrac::ui
{
    class VracLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        VracLookAndFeel();
        void refreshTheme();

        juce::Font getTextButtonFont (juce::TextButton&, int) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getLabelFont (juce::Label&) override;
        juce::Font getPopupMenuFont() override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                   bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override;
        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;
        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    };
} // namespace vrac::ui

