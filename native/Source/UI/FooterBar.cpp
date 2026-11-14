#include "FooterBar.h"
#include "Theme.h"

namespace sliceforge::ui
{
    FooterBar::FooterBar (SliceForgeAudioProcessor& processor) : processorRef (processor)
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
                    getWidth() - 300, 0, 150, getHeight(), juce::Justification::centredRight, false);
        g.setColour (c.ink);
        g.setFont (display (14.0f));
        g.drawText ("SLICEFORGE", getWidth() - 140, 0, 126, getHeight(), juce::Justification::centredRight, false);
    }
} // namespace sliceforge::ui

