#include "Controls.h"
#include "Theme.h"

namespace sliceforge::ui
{
    namespace
    {
        bool isGainParam (const juce::String& id)
        {
            return id.endsWith ("_original") || id.endsWith ("_drumified") || id.endsWith ("_volume")
                || id == "global.volume" || id == "global.out";
        }

        bool isPitchParam (const juce::String& id)
        {
            return id.endsWith ("_pitch") || id.endsWith ("_tone") || id == "global.transpose";
        }

        bool isPercentParam (const juce::String& id)
        {
            return id.endsWith ("_drumify") || id.endsWith ("_velocity") || id.endsWith ("_warp")
                || id.endsWith ("_impact") || id.endsWith ("_instability") || id.endsWith ("_super")
                || id == "global.width" || id == "global.clip" || id == "source.sensitivity";
        }

        /** The pad controls drawn as button rows: their value selects a name or a toggle, so they
            never need a text value. Anything else that is not covered by a branch below is a
            parameter that was added without being given a unit. */
        bool isButtonParam (const juce::String& id)
        {
            return id.endsWith ("_model") || id.endsWith ("_choke") || id.endsWith ("_bus")
                || id.endsWith ("_mute") || id.endsWith ("_solo");
        }
    } // namespace

    bool isKnownParamId (const juce::String& id)
    {
        return isGainParam (id) || isPitchParam (id) || isPercentParam (id) || isButtonParam (id)
            || id == "global.fine" || id == "global.drumify" || id == "global.length"
            || id == "global.filter" || id == "global.tone"
            || id.endsWith ("_length") || id.endsWith ("_pan")
            || id.endsWith ("_eqLow") || id.endsWith ("_eqMid") || id.endsWith ("_eqHigh");
    }

    juce::String formatParamValue (const juce::String& id, double value)
    {
        const auto v = (float) value;
        if (isGainParam (id))
            return formatDb (v, 1);
        if (isPitchParam (id))
            return formatSemitones (v);
        if (id == "global.fine")
            return formatCents (v);
        // Global DRUMIFY is already a percentage of the pad amount, and its range is 0..200, so it
        // takes the value as-is; formatPercent would multiply a 0..1 knob and this one would read
        // "10000%".
        if (id == "global.drumify")
            return juce::String ((int) std::round (v)) + "%";
        // Global TONE is a spectral tilt, not a pitch move: the value drives two shelf gains
        // (VoiceEngine's master chain scales it by 0.36 and 0.58 dB), so it reads as a signed
        // amount with no unit. The per-pad tone shift, by contrast, moves a filter centre in
        // semitones and keeps the "st" suffix.
        if (id == "global.tone")
            return std::abs (v) < 0.05f ? "0.0" : (v > 0.0f ? "+" : "") + juce::String (v, 1);
        if (id.endsWith ("_length"))
            return juce::String ((int) std::round (v * 1000.0f)) + " ms";
        if (id == "global.length")
            return juce::String ((int) std::round (v * 100.0f)) + "%";
        if (id.endsWith ("_pan"))
        {
            if (std::abs (v) < 0.02f) return "C";
            return juce::String (v < 0 ? "L" : "R") + juce::String ((int) std::round (std::abs (v) * 100.0f));
        }
        if (isPercentParam (id))
            return formatPercent (v, 0);
        if (id.endsWith ("_eqLow") || id.endsWith ("_eqMid") || id.endsWith ("_eqHigh"))
            return (v > 0 ? "+" : "") + juce::String (v, 1);
        if (id == "global.filter")
        {
            if (std::abs (v) < 0.015f) return "OPEN";
            if (v < 0.0f)
            {
                const auto cutoff = 20000.0f * std::pow (120.0f / 20000.0f, -v);
                return "LP " + juce::String (cutoff >= 1000.0f ? cutoff / 1000.0f : cutoff, 1)
                     + (cutoff >= 1000.0f ? "k" : "");
            }
            const auto cutoff = 20.0f * std::pow (8200.0f / 20.0f, v);
            return "HP " + juce::String (cutoff >= 1000.0f ? cutoff / 1000.0f : cutoff, 1)
                 + (cutoff >= 1000.0f ? "k" : "");
        }
        return juce::String (v, 2);
    }

    ParamKnob::ParamKnob (SliceForgeAudioProcessor& processor, juce::String label, juce::String id, int size)
        : processorRef (processor), parameterId (std::move (id)), labelText (std::move (label)), dialSize (size)
    {
        accent = palette().accent;
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        slider.setPopupMenuEnabled (false);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.onContextMenu = [this] { showContextMenu(); };
        slider.onDragStart = [this] { processorRef.pushUndoSnapshot(); };
        slider.onValueChange = [this]
        {
            repaint();
            if (onValueChanged)
                onValueChanged();
        };
        addAndMakeVisible (slider);
        attach();
    }

    void ParamKnob::attach()
    {
        attachment.reset();
        if (parameterId.isEmpty())
        {
            slider.setEnabled (false);
            return;
        }
        if (auto* parameter = processorRef.getValueTreeState().getParameter (parameterId))
        {
            slider.setEnabled (true);
            auto range = parameter->getNormalisableRange();
            slider.setRange (range.start, range.end, 0.0);
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                processorRef.getValueTreeState(), parameterId, slider);
            slider.setDoubleClickReturnValue (true, range.convertFrom0to1 (parameter->getDefaultValue()));
        }
        repaint();
    }

    void ParamKnob::setParameterId (const juce::String& newId)
    {
        if (parameterId == newId)
            return;
        parameterId = newId;
        attach();
    }

    void ParamKnob::setLabel (const juce::String& newLabel)
    {
        labelText = newLabel;
        repaint();
    }

    void ParamKnob::setDialSize (int size)
    {
        dialSize = juce::jmax (24, size);
        resized();
    }

    void ParamKnob::setAccent (juce::Colour colour)
    {
        accent = colour;
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        repaint();
    }

    void ParamKnob::refresh()
    {
        repaint();
    }

    void ParamKnob::resized()
    {
        auto area = getLocalBounds();
        area.removeFromTop (13);
        area.removeFromBottom (15);
        const auto side = juce::jmin (area.getWidth(), area.getHeight());
        slider.setBounds (area.withSizeKeepingCentre (side, side));
    }

    void ParamKnob::paint (juce::Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.muted);
        g.setFont (font (8.0f, true));
        g.drawText (labelText.toUpperCase(), getLocalBounds().removeFromTop (13),
                    juce::Justification::centred, false);
        auto tag = getLocalBounds().removeFromBottom (14).reduced (4, 1);
        g.setColour (c.panel);
        g.fillRoundedRectangle (tag.toFloat(), 3.0f);
        g.setColour (c.line);
        g.drawRoundedRectangle (tag.toFloat().reduced (0.5f), 3.0f, 1.0f);
        g.setColour (c.ink);
        g.setFont (mono (8.5f));
        g.drawText (formatValue(), tag, juce::Justification::centred, false);
    }

    juce::String ParamKnob::formatValue() const
    {
        if (auto* parameter = processorRef.getValueTreeState().getParameter (parameterId))
        {
            const auto range = parameter->getNormalisableRange();
            const auto real = range.convertFrom0to1 (parameter->getValue());
            return formatParamValue (parameterId, real);
        }
        return "--";
    }

    void ParamKnob::showContextMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "MIDI Learn");
        menu.addItem (2, "Cancel MIDI Learn");
        menu.addSeparator();
        menu.addItem (3, "Reset to Default");
        auto safe = juce::Component::SafePointer<ParamKnob> (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider),
                            [safe] (int result)
                            {
                                if (safe == nullptr)
                                    return;
                                if (result == 1)
                                    safe->processorRef.armMidiLearn (safe->parameterId);
                                else if (result == 2)
                                    safe->processorRef.cancelMidiLearn();
                                else if (result == 3)
                                {
                                    if (auto* parameter = safe->processorRef.getValueTreeState().getParameter (safe->parameterId))
                                    {
                                        safe->processorRef.pushUndoSnapshot();
                                        parameter->setValueNotifyingHost (parameter->getDefaultValue());
                                        safe->refresh();
                                    }
                                }
                            });
    }

    ParamSlider::ParamSlider (SliceForgeAudioProcessor& processor, juce::String label, juce::String id)
        : processorRef (processor), parameterId (std::move (id)), labelText (std::move (label))
    {
        accent = palette().accent;
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setPopupMenuEnabled (false);
        slider.setColour (juce::Slider::trackColourId, accent);
        slider.onContextMenu = [this] { showContextMenu(); };
        slider.onDragStart = [this] { processorRef.pushUndoSnapshot(); };
        slider.onValueChange = [this]
        {
            repaint();
            if (onValueChanged)
                onValueChanged();
        };
        addAndMakeVisible (slider);
        attach();
    }

    void ParamSlider::attach()
    {
        attachment.reset();
        if (parameterId.isEmpty())
            return;
        if (auto* parameter = processorRef.getValueTreeState().getParameter (parameterId))
        {
            auto range = parameter->getNormalisableRange();
            slider.setRange (range.start, range.end, 0.0);
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                processorRef.getValueTreeState(), parameterId, slider);
            slider.setDoubleClickReturnValue (true, range.convertFrom0to1 (parameter->getDefaultValue()));
        }
        repaint();
    }

    void ParamSlider::setParameterId (const juce::String& newId)
    {
        if (parameterId == newId)
            return;
        parameterId = newId;
        attach();
    }

    void ParamSlider::setLabel (const juce::String& newLabel)
    {
        labelText = newLabel;
        repaint();
    }

    void ParamSlider::setAccent (juce::Colour colour)
    {
        accent = colour;
        slider.setColour (juce::Slider::trackColourId, accent);
        repaint();
    }

    void ParamSlider::refresh()
    {
        repaint();
    }

    void ParamSlider::resized()
    {
        auto area = getLocalBounds();
        area.removeFromTop (15);
        slider.setBounds (area.reduced (0, 2));
    }

    void ParamSlider::paint (juce::Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.muted);
        g.setFont (font (8.0f, true));
        g.drawText (labelText.toUpperCase(), 0, 0, getWidth() - 58, 14, juce::Justification::centredLeft, false);
        auto tag = juce::Rectangle<int> (getWidth() - 56, 1, 56, 13);
        g.setColour (c.panel);
        g.fillRoundedRectangle (tag.toFloat(), 3.0f);
        g.setColour (c.line);
        g.drawRoundedRectangle (tag.toFloat().reduced (0.5f), 3.0f, 1.0f);
        g.setColour (c.ink);
        g.setFont (mono (8.5f));
        g.drawText (formatValue(), tag, juce::Justification::centred, false);
    }

    juce::String ParamSlider::formatValue() const
    {
        if (auto* parameter = processorRef.getValueTreeState().getParameter (parameterId))
        {
            const auto range = parameter->getNormalisableRange();
            return formatParamValue (parameterId, range.convertFrom0to1 (parameter->getValue()));
        }
        return "--";
    }

    void ParamSlider::showContextMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "MIDI Learn");
        menu.addItem (2, "Cancel MIDI Learn");
        auto safe = juce::Component::SafePointer<ParamSlider> (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider),
                            [safe] (int result)
                            {
                                if (safe == nullptr)
                                    return;
                                if (result == 1)
                                    safe->processorRef.armMidiLearn (safe->parameterId);
                                else if (result == 2)
                                    safe->processorRef.cancelMidiLearn();
                            });
    }

    void styleChip (juce::TextButton& button, bool active)
    {
        // Colours are resolved by SliceForgeLookAndFeel from the active palette so a
        // light/dark switch updates every chip without restyling the component tree.
        button.setToggleState (active, juce::dontSendNotification);
    }

    void stylePrimaryButton (juce::TextButton& button)
    {
        button.getProperties().set ("sliceforgePrimary", true);
    }

    void styleSmallPanelButton (juce::TextButton& button, juce::Colour accentColour)
    {
        button.setColour (juce::TextButton::buttonOnColourId, accentColour);
    }
} // namespace sliceforge::ui

