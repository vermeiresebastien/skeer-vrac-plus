#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

namespace vrac::ui
{
    class LearningSlider final : public juce::Slider
    {
    public:
        std::function<void()> onContextMenu;
        void mouseDown (const juce::MouseEvent& event) override
        {
            if (event.mods.isPopupMenu())
            {
                if (onContextMenu)
                    onContextMenu();
                return;
            }
            juce::Slider::mouseDown (event);
        }
    };

    class ParamKnob final : public juce::Component
    {
    public:
        ParamKnob (VRACPlusAudioProcessor& processor, juce::String label, juce::String parameterId,
                   int dialSize = 48);
        ~ParamKnob() override = default;

        void setParameterId (const juce::String& newId);
        void setLabel (const juce::String& newLabel);
        void setDialSize (int size);
        void setAccent (juce::Colour colour);
        void refresh();
        juce::String getParameterId() const { return parameterId; }
        LearningSlider& getSlider() { return slider; }

        void paint (juce::Graphics&) override;
        void resized() override;

        std::function<void()> onValueChanged;

    private:
        void showContextMenu();
        void attach();
        juce::String formatValue() const;

        VRACPlusAudioProcessor& processorRef;
        juce::String parameterId;
        juce::String labelText;
        int dialSize = 48;
        LearningSlider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::Colour accent;
    };

    class ParamSlider final : public juce::Component
    {
    public:
        ParamSlider (VRACPlusAudioProcessor& processor, juce::String label, juce::String parameterId);
        ~ParamSlider() override = default;
        void setParameterId (const juce::String& newId);
        void setLabel (const juce::String& newLabel);
        void refresh();
        void paint (juce::Graphics&) override;
        void resized() override;
        void setAccent (juce::Colour colour);
        LearningSlider& getSlider() { return slider; }
        std::function<void()> onValueChanged;

    private:
        void attach();
        void showContextMenu();
        juce::String formatValue() const;
        VRACPlusAudioProcessor& processorRef;
        juce::String parameterId;
        juce::String labelText;
        LearningSlider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::Colour accent;
    };

    juce::String formatParamValue (const juce::String& id, double value);
    void styleChip (juce::TextButton&, bool active = false);
    void stylePrimaryButton (juce::TextButton&);
    void styleSmallPanelButton (juce::TextButton&, juce::Colour accent);
} // namespace vrac::ui

