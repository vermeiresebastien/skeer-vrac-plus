#include "LookAndFeel.h"
#include "Theme.h"

namespace sliceforge::ui
{
    SliceForgeLookAndFeel::SliceForgeLookAndFeel()
    {
        refreshTheme();
    }

    void SliceForgeLookAndFeel::refreshTheme()
    {
        const auto& c = palette();
        setColour (juce::ResizableWindow::backgroundColourId, c.bg);
        setColour (juce::PopupMenu::backgroundColourId, c.panel);
        setColour (juce::PopupMenu::textColourId, c.ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c.accent);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xfffff9f2));
        setColour (juce::TextButton::buttonColourId, c.panel);
        setColour (juce::TextButton::buttonOnColourId, c.accent);
        setColour (juce::TextButton::textColourOffId, c.ink);
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xfffff9f2));
        setColour (juce::ComboBox::backgroundColourId, c.panel);
        setColour (juce::ComboBox::outlineColourId, c.line);
        setColour (juce::ComboBox::textColourId, c.ink);
        setColour (juce::ComboBox::arrowColourId, c.muted);
        setColour (juce::Label::textColourId, c.ink);
        setColour (juce::TextEditor::backgroundColourId, c.panel);
        setColour (juce::TextEditor::textColourId, c.ink);
        setColour (juce::TextEditor::outlineColourId, c.line);
        setColour (juce::TextEditor::focusedOutlineColourId, c.accent);
        setColour (juce::TextEditor::highlightColourId, c.accent.withAlpha (0.22f));
        setColour (juce::TextEditor::highlightedTextColourId, c.ink);
        setColour (juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
        setColour (juce::ListBox::backgroundColourId, c.panel);
        setColour (juce::ListBox::outlineColourId, c.line);
        setColour (juce::ListBox::textColourId, c.ink);
        setColour (juce::ScrollBar::backgroundColourId, c.bgDeep);
        setColour (juce::ScrollBar::thumbColourId, c.lineStrong);
        setColour (juce::ScrollBar::trackColourId, c.bgDeep);
        setColour (juce::Slider::trackColourId, c.accent);
        setColour (juce::Slider::backgroundColourId, c.line);
        setColour (juce::Slider::thumbColourId, c.accent);
        setColour (juce::Slider::rotarySliderFillColourId, c.accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, c.line);
        setColour (juce::Slider::textBoxTextColourId, c.ink);
        setColour (juce::Slider::textBoxBackgroundColourId, c.panel);
        setColour (juce::Slider::textBoxOutlineColourId, c.line);
        setColour (juce::TooltipWindow::backgroundColourId, c.panel);
        setColour (juce::TooltipWindow::textColourId, c.ink);
        setColour (juce::TooltipWindow::outlineColourId, c.line);
    }

    juce::Font SliceForgeLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return font (9.5f, true); }
    juce::Font SliceForgeLookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (11.0f); }
    juce::Font SliceForgeLookAndFeel::getLabelFont (juce::Label&) { return font (11.0f); }
    juce::Font SliceForgeLookAndFeel::getPopupMenuFont() { return font (11.0f); }

    void SliceForgeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                 const juce::Colour& backgroundColour,
                                                 bool highlighted, bool down)
    {
        const auto& c = palette();
        juce::ignoreUnused (backgroundColour);
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = button.getToggleState();
        const bool primary = (bool) button.getProperties().getWithDefault ("sliceforgePrimary", false);
        auto fill = primary ? c.accent
                            : (on ? button.findColour (juce::TextButton::buttonOnColourId) : c.panel);
        if (highlighted)
            fill = fill.brighter (isDarkTheme() ? 0.08f : 0.03f);
        if (down)
            fill = fill.darker (0.07f);
        if (! button.isEnabled())
            fill = fill.withAlpha (0.38f);
        const bool coloured = fill.getSaturation() > 0.35f && fill.getBrightness() < 0.98f;
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (coloured ? fill.darker (0.12f) : c.line);
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }

    void SliceForgeLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool down)
    {
        const auto& c = palette();
        const bool on = button.getToggleState();
        const bool primary = (bool) button.getProperties().getWithDefault ("sliceforgePrimary", false);
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.setColour ((primary || on ? juce::Colour (0xfffff9f2) : c.ink)
                         .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.4f));
        auto area = button.getLocalBounds().reduced (5, 1);
        if (down)
            area.translate (0, 1);
        g.drawFittedText (button.getButtonText().toUpperCase(), area, juce::Justification::centred, 1);
    }

    void SliceForgeLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box)
    {
        const auto& c = palette();
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        g.setColour (c.panel);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (box.hasKeyboardFocus (true) ? c.accent.withAlpha (0.8f) : c.line);
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
        auto arrow = juce::Path();
        const auto x = (float) width - 16.0f;
        const auto y = (float) height * 0.5f;
        arrow.startNewSubPath (x - 4.0f, y - 2.0f);
        arrow.lineTo (x, y + 2.5f);
        arrow.lineTo (x + 4.0f, y - 2.0f);
        g.setColour (c.muted);
        g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void SliceForgeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPosProportional, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider& slider)
    {
        const auto& c = palette();
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (3.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

        g.setColour (c.line);
        g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);
        g.setColour (c.panel);
        g.fillEllipse (centre.x - radius + 2.5f, centre.y - radius + 2.5f,
                       radius * 2.0f - 5.0f, radius * 2.0f - 5.0f);

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        const auto pointer = radius - 6.0f;
        g.setColour (c.ink);
        g.drawLine (centre.x, centre.y,
                    centre.x + std::sin (angle) * pointer,
                    centre.y - std::cos (angle) * pointer, 1.7f);
        g.setColour (accent);
        g.fillEllipse (centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f);
    }

    void SliceForgeLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float, float,
                                            juce::Slider::SliderStyle style, juce::Slider& slider)
    {
        const auto& c = palette();
        if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar)
        {
            const auto cy = (float) y + (float) height * 0.5f;
            const auto startX = (float) x + 3.0f;
            const auto endX = (float) x + (float) width - 3.0f;
            const auto pos = juce::jlimit (startX, endX, sliderPos);
            g.setColour (c.line);
            g.drawLine (startX, cy, endX, cy, 2.0f);
            g.setColour (slider.findColour (juce::Slider::trackColourId));
            g.drawLine (startX, cy, pos, cy, 2.0f);
            g.fillEllipse (pos - 5.0f, cy - 5.0f, 10.0f, 10.0f);
            g.setColour (c.panel);
            g.fillEllipse (pos - 2.0f, cy - 2.0f, 4.0f, 4.0f);
            return;
        }
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
    }

    void SliceForgeLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
    {
        const auto& c = palette();
        g.setColour (c.panel);
        g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height), 4.0f);
        g.setColour (c.line);
        g.drawRoundedRectangle (juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f), 4.0f, 1.0f);
    }
} // namespace sliceforge::ui

