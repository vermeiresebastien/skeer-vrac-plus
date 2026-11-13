#pragma once

#include <JuceHeader.h>

namespace vrac::ui
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
} // namespace vrac::ui

