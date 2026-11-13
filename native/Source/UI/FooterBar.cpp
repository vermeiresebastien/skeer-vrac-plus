#include "FooterBar.h"
#include "Theme.h"

namespace vrac::ui
{
    FooterBar::FooterBar (VRACPlusAudioProcessor& processor) : processorRef (processor)
    {
    }

    void FooterBar::paint (juce::Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.panel);
        g.fillAll();
        g.setColour (c.line);
        g.fillRect (0, 0, getWidth(), 1);
        g.setColour (c.muted);
        g.setFont (font (7.0f, true));
        g.drawText ("RIGHT-CLICK A KNOB TO MIDI LEARN  /  LOAD ANY SOUND AND AUTO-SLICE IT  /  8 STEREO OUTPUTS",
                    14, 0, getWidth() - 280, getHeight(), juce::Justification::centredLeft, false);
        g.setFont (mono (7.5f));
        g.setColour (c.muted);
        g.drawText (juce::String (processorRef.getActiveVoiceCount()) + " VOICES  /  8 STEREO OUTS",
                    getWidth() - 280, 0, 160, getHeight(), juce::Justification::centredRight, false);
        g.setColour (c.ink);
        g.setFont (display (14.0f));
        g.drawText ("VRAC", getWidth() - 104, 0, 60, getHeight(), juce::Justification::centredRight, false);
        g.setColour (c.accent);
        g.drawText ("+", getWidth() - 44, 0, 20, getHeight(), juce::Justification::centredLeft, false);
    }
} // namespace vrac::ui

