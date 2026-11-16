#include "Theme.h"
#include "../Params.h"

#include <BinaryData.h>

namespace sliceforge::ui
{
    namespace
    {
        juce::Typeface::Ptr typeface (const void* data, int size)
        {
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        }

        juce::Typeface::Ptr plexRegular()
        {
            static const auto face = typeface (SliceForgeAssets::IBMPlexSansRegular_ttf,
                                               SliceForgeAssets::IBMPlexSansRegular_ttfSize);
            return face;
        }

        juce::Typeface::Ptr plexBold()
        {
            static const auto face = typeface (SliceForgeAssets::IBMPlexSansBold_ttf,
                                               SliceForgeAssets::IBMPlexSansBold_ttfSize);
            return face;
        }

        juce::Typeface::Ptr plexMonoRegular()
        {
            static const auto face = typeface (SliceForgeAssets::IBMPlexMonoRegular_ttf,
                                               SliceForgeAssets::IBMPlexMonoRegular_ttfSize);
            return face;
        }

        juce::Typeface::Ptr barlowBold()
        {
            static const auto face = typeface (SliceForgeAssets::BarlowCondensedBold_ttf,
                                               SliceForgeAssets::BarlowCondensedBold_ttfSize);
            return face;
        }

        juce::Font makeFont (juce::Typeface::Ptr face, float height, int flags)
        {
            auto options = juce::FontOptions (face).withHeight (height);
            juce::Font result (options);
            result.setStyleFlags (flags);
            return result;
        }
    } // namespace

    namespace
    {
        const Palette lightPalette {
            juce::Colour (0xfff5f2eb), juce::Colour (0xffe7e2d8), juce::Colour (0xfffcfaf6),
            juce::Colour (0xfff5f2eb), juce::Colour (0xffe7e2d8), juce::Colour (0xff292821),
            juce::Colour (0xff4a463e), juce::Colour (0xff716c63), juce::Colour (0xffcfc8bc),
            juce::Colour (0xffb8b0a2), juce::Colour (0xffff6500), juce::Colour (0x1aff6500),
            juce::Colour (0xffff6500), juce::Colour (0xff00cc66), juce::Colour (0xffd9382e),
            juce::Colour (0xffe7e2d8), juce::Colour (0xfff0ebe1), juce::Colour (0xff292821)
        };

        const Palette darkPalette {
            juce::Colour (0xff151412), juce::Colour (0xff0e0d0b), juce::Colour (0xff211f1b),
            juce::Colour (0xff1a1815), juce::Colour (0xff27231e), juce::Colour (0xfff5f2eb),
            juce::Colour (0xffd8d1c5), juce::Colour (0xff8f8779), juce::Colour (0xff39342c),
            juce::Colour (0xff514a3f), juce::Colour (0xffff6500), juce::Colour (0x33ff6500),
            juce::Colour (0xffff6500), juce::Colour (0xff00d26a), juce::Colour (0xffff5a4e),
            juce::Colour (0xff2a2621), juce::Colour (0xff332e27), juce::Colour (0xfff5f2eb)
        };

        juce::File themeSettingsFile()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                .getChildFile ("SliceForge")
                .getChildFile ("theme.txt");
        }

        ThemeMode loadThemeMode()
        {
            const auto text = themeSettingsFile().loadFileAsString().trim().toLowerCase();
            if (text == "dark")
                return ThemeMode::dark;
            if (text == "light")
                return ThemeMode::light;
            return ThemeMode::light;
        }

        ThemeMode& currentThemeMode()
        {
            static ThemeMode mode = loadThemeMode();
            return mode;
        }
    } // namespace

    ThemeMode getThemeMode() noexcept
    {
        return currentThemeMode();
    }

    void setThemeMode (ThemeMode mode, bool persist)
    {
        currentThemeMode() = mode;
        if (! persist)
            return;
        auto file = themeSettingsFile();
        file.getParentDirectory().createDirectory();
        file.replaceWithText (mode == ThemeMode::dark ? "dark" : "light");
    }

    ThemeMode toggleThemeMode (bool persist)
    {
        const auto next = isDarkTheme() ? ThemeMode::light : ThemeMode::dark;
        setThemeMode (next, persist);
        return next;
    }

    bool isDarkTheme() noexcept
    {
        return currentThemeMode() == ThemeMode::dark;
    }

    juce::String themeModeName()
    {
        return isDarkTheme() ? "DARK" : "LIGHT";
    }

    const Palette& palette()
    {
        return isDarkTheme() ? darkPalette : lightPalette;
    }

    juce::Font font (float height, bool bold)
    {
        return makeFont (bold ? plexBold() : plexRegular(), height,
                         bold ? juce::Font::bold : juce::Font::plain);
    }

    juce::Font narrow (float height, bool)
    {
        return makeFont (barlowBold(), height, juce::Font::bold);
    }

    juce::Font display (float height, bool)
    {
        return makeFont (barlowBold(), height, juce::Font::bold);
    }

    juce::Font mono (float height)
    {
        return makeFont (plexMonoRegular(), height, juce::Font::plain);
    }

    juce::Colour familyColour (int family) { return sliceforge::familyColour (family); }
    juce::Colour familyColourDim (int family) { return sliceforge::familyColourDim (family); }

    void drawPanelBackground (juce::Graphics& g, juce::Rectangle<float> bounds, bool raised)
    {
        const auto& c = palette();
        auto colour = raised ? c.panel : c.panel2;
        g.setColour (colour);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (c.line);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);
    }

    void drawInsetBackground (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        const auto& c = palette();
        g.setColour (c.panel);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (c.line);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
    }

    void drawSectionLabel (juce::Graphics& g, juce::Point<int> position, const juce::String& label,
                           const juce::String& meta)
    {
        const auto& c = palette();
        g.setColour (c.accent);
        g.fillEllipse ((float) position.x, (float) position.y + 4.0f, 4.0f, 4.0f);
        g.setColour (c.inkSoft);
        g.setFont (narrow (9.0f, true));
        g.drawText (label, position.x + 9, position.y, 260, 14, juce::Justification::centredLeft);
        if (meta.isNotEmpty())
        {
            g.setColour (c.muted);
            g.setFont (narrow (8.0f, true));
            g.drawText (meta, position.x + 260, position.y, 240, 14, juce::Justification::centredRight);
        }
    }

    juce::String formatTime (double seconds, bool withMs)
    {
        seconds = juce::jmax (0.0, seconds);
        const auto minutes = (int) std::floor (seconds / 60.0);
        const auto rest = seconds - minutes * 60.0;
        const auto whole = (int) std::floor (rest);
        if (! withMs)
            return juce::String (minutes).paddedLeft ('0', 2) + ":" + juce::String (whole).paddedLeft ('0', 2);
        const auto ms = (int) std::round ((rest - whole) * 1000.0);
        return juce::String (minutes).paddedLeft ('0', 2) + ":" + juce::String (whole).paddedLeft ('0', 2)
             + "." + juce::String (ms).paddedLeft ('0', 3);
    }

    juce::String formatDb (float db, int decimals)
    {
        if (db <= -59.95f)
            return "-inf dB";
        return juce::String (db, decimals) + " dB";
    }

    juce::String formatPercent (float value, int decimals)
    {
        return juce::String (value * 100.0f, decimals) + "%";
    }

    juce::String formatSemitones (float value)
    {
        return (value > 0.0f ? "+" : "") + juce::String (value, std::abs (value - std::round (value)) < 0.01f ? 0 : 1) + " st";
    }

    juce::String formatCents (float value)
    {
        return (value > 0.0f ? "+" : "") + juce::String (value, 0) + " ct";
    }

    bool isPlacedOnScreen (const juce::Component& component) noexcept
    {
        for (auto* c = &component; c != nullptr; c = c->getParentComponent())
            if (! c->isVisible() || c->getWidth() <= 0 || c->getHeight() <= 0)
                return false;
        return true;
    }
} // namespace sliceforge::ui


