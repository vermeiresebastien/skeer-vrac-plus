#pragma once

#include <JuceHeader.h>

#include <functional>

namespace sliceforge::test
{
    /*  Ground truth for the auto-slicer, in its own translation unit for the same reason the groove
        bank is: decoding a fixture does not belong in main()'s already 1 MB stack frame, and every
        check here reports through the smoke harness's own check(), so the suite total stays the suite
        total instead of becoming a second number nobody reads.

        Report's signature is SmokeTest.cpp's check(). */
    void runSlicerScoreChecks (const std::function<void (bool, const juce::String&, const juce::String&)>& report);

    /** Recomputes the table and overwrites `fixtures/slicer-baseline.json`.
     *  Only reachable from `SliceForgeSmokeTest --bless-slicer-baseline`, which ctest never passes,
     *  so a green run can never re-bless itself. Returns an empty string on success and the reason
     *  it refused otherwise: a bless that wrote nothing must not look like a bless that wrote an
     *  empty table. */
    juce::String writeSlicerBaseline();
} // namespace sliceforge::test
