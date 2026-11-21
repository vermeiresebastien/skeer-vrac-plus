#pragma once

#include <JuceHeader.h>
#include "Sequencer.h"

#include <functional>

namespace sliceforge::test
{
    /** The smoke suite owns the counters and the printing, so a check that lives in another
        translation unit reports through the same harness and the suite's total stays the suite's
        total. Signature is SmokeTest.cpp's own check(). */
    using Report = std::function<void (bool, const juce::String&, const juce::String&)>;

    void runGrooveBankChecks (const Report&);
} // namespace sliceforge::test
