#pragma once

#include <JuceHeader.h>

namespace sliceforge::ui
{
    struct Palette
    {
        juce::Colour bg;
        juce::Colour bgDeep;
        juce::Colour panel;
        juce::Colour panel2;
        juce::Colour inset;
        juce::Colour ink;
        juce::Colour inkSoft;
        juce::Colour muted;
        juce::Colour line;
        juce::Colour lineStrong;
        juce::Colour accent;
        juce::Colour accentSoft;
        juce::Colour selection;
        juce::Colour green;
        juce::Colour red;
        juce::Colour pad;
        juce::Colour padHi;
        juce::Colour padInk;
    };

    enum class ThemeMode
    {
        light,
        dark
    };

    ThemeMode getThemeMode() noexcept;
    void setThemeMode (ThemeMode mode, bool persist = true);
    ThemeMode toggleThemeMode (bool persist = true);
    bool isDarkTheme() noexcept;
    juce::String themeModeName();

    const Palette& palette();
    juce::Font font (float height, bool bold = false);
    juce::Font narrow (float height, bool bold = false);
    juce::Font mono (float height);
    juce::Font display (float height, bool bold = true);

    juce::Colour familyColour (int family);
    juce::Colour familyColourDim (int family);

    void drawPanelBackground (juce::Graphics&, juce::Rectangle<float> bounds, bool raised = false);
    void drawInsetBackground (juce::Graphics&, juce::Rectangle<float> bounds);
    void drawSectionLabel (juce::Graphics&, juce::Point<int> position, const juce::String& label,
                           const juce::String& meta = {});
    juce::String formatTime (double seconds, bool ms = true);
    juce::String formatDb (float db, int decimals = 1);
    juce::String formatPercent (float value, int decimals = 0);
    juce::String formatSemitones (float value);
    juce::String formatCents (float value);

    /** Whether this component is actually laid out where a user could reach it: itself and every
        ancestor visible, with a non-empty box all the way up. Component::isShowing() cannot answer
        that headlessly—it also demands a peer—so the layout test asks this instead. */
    bool isPlacedOnScreen (const juce::Component&) noexcept;
} // namespace sliceforge::ui

