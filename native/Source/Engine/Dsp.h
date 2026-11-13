#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

namespace vrac
{
    constexpr float kTwoPi = 6.283185307179586f;
    constexpr float kHalfPi = 1.5707963267948966f;
    constexpr int kMaxVoices = 32;

    class Rng
    {
    public:
        explicit Rng (uint32_t seed = 0x5eed1234u) : state (seed == 0 ? 0x9e3779b9u : seed) {}

        uint32_t nextUInt()
        {
            auto x = state;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            state = x;
            return state;
        }

        float next01() { return (float) nextUInt() / 4294967296.0f; }
        float bipolar() { return next01() * 2.0f - 1.0f; }
        float range (float lo, float hi) { return lo + next01() * (hi - lo); }
        int rangeInt (int lo, int hi) { return lo + (int) (next01() * (float) (hi - lo + 1)); }
        bool chance (float probability) { return next01() < probability; }

    private:
        uint32_t state;
    };

    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

        void reset()
        {
            x1 = x2 = y1 = y2 = 0.0f;
        }

        void configureLowpass (float frequency, float sampleRate, float q = 0.7071068f)
        {
            configure (0, frequency, sampleRate, q, 0.0f);
        }

        void configureHighpass (float frequency, float sampleRate, float q = 0.7071068f)
        {
            configure (1, frequency, sampleRate, q, 0.0f);
        }

        void configureBandpass (float frequency, float sampleRate, float q = 1.0f)
        {
            configure (2, frequency, sampleRate, q, 0.0f);
        }

        void configurePeaking (float frequency, float sampleRate, float q, float gainDb)
        {
            configure (3, frequency, sampleRate, q, gainDb);
        }

        void configureLowshelf (float frequency, float sampleRate, float gainDb)
        {
            configure (4, frequency, sampleRate, 0.7071068f, gainDb);
        }

        void configureHighshelf (float frequency, float sampleRate, float gainDb)
        {
            configure (5, frequency, sampleRate, 0.7071068f, gainDb);
        }

        void configure (int kind, float frequency, float sampleRate, float q, float gainDb)
        {
            const auto nyquist = sampleRate * 0.5f;
            const auto f = juce::jlimit (10.0f, nyquist * 0.95f, frequency);
            const auto w0 = kTwoPi * f / sampleRate;
            const auto cosw = std::cos (w0);
            const auto sinw = std::sin (w0);
            const auto alpha = sinw / (2.0f * juce::jmax (0.05f, q));
            const auto A = std::pow (10.0f, gainDb / 40.0f);

            float nb0 = 1.0f, nb1 = 0.0f, nb2 = 0.0f, na0 = 1.0f, na1 = 0.0f, na2 = 0.0f;

            switch (kind)
            {
                case 0:
                    nb0 = (1.0f - cosw) * 0.5f;
                    nb1 = 1.0f - cosw;
                    nb2 = nb0;
                    na0 = 1.0f + alpha;
                    na1 = -2.0f * cosw;
                    na2 = 1.0f - alpha;
                    break;
                case 1:
                    nb0 = (1.0f + cosw) * 0.5f;
                    nb1 = -(1.0f + cosw);
                    nb2 = nb0;
                    na0 = 1.0f + alpha;
                    na1 = -2.0f * cosw;
                    na2 = 1.0f - alpha;
                    break;
                case 2:
                    nb0 = alpha;
                    nb1 = 0.0f;
                    nb2 = -alpha;
                    na0 = 1.0f + alpha;
                    na1 = -2.0f * cosw;
                    na2 = 1.0f - alpha;
                    break;
                case 3:
                    nb0 = 1.0f + alpha * A;
                    nb1 = -2.0f * cosw;
                    nb2 = 1.0f - alpha * A;
                    na0 = 1.0f + alpha / A;
                    na1 = -2.0f * cosw;
                    na2 = 1.0f - alpha / A;
                    break;
                case 4:
                {
                    const auto beta = 2.0f * std::sqrt (A) * alpha;
                    nb0 = A * (A + 1.0f - (A - 1.0f) * cosw + beta);
                    nb1 = 2.0f * A * (A - 1.0f - (A + 1.0f) * cosw);
                    nb2 = A * (A + 1.0f - (A - 1.0f) * cosw - beta);
                    na0 = A + 1.0f + (A - 1.0f) * cosw + beta;
                    na1 = -2.0f * (A - 1.0f + (A + 1.0f) * cosw);
                    na2 = A + 1.0f + (A - 1.0f) * cosw - beta;
                    break;
                }
                case 5:
                {
                    const auto beta = 2.0f * std::sqrt (A) * alpha;
                    nb0 = A * (A + 1.0f + (A - 1.0f) * cosw + beta);
                    nb1 = -2.0f * A * (A - 1.0f + (A + 1.0f) * cosw);
                    nb2 = A * (A + 1.0f + (A - 1.0f) * cosw - beta);
                    na0 = A + 1.0f - (A - 1.0f) * cosw + beta;
                    na1 = 2.0f * (A - 1.0f - (A + 1.0f) * cosw);
                    na2 = A + 1.0f - (A - 1.0f) * cosw - beta;
                    break;
                }
                default:
                    break;
            }

            b0 = nb0 / na0;
            b1 = nb1 / na0;
            b2 = nb2 / na0;
            a1 = na1 / na0;
            a2 = na2 / na0;
        }

        float process (float x) noexcept
        {
            const auto y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            return y;
        }
    };

    struct MasterChain
    {
        Biquad lowpassL, lowpassR, highpassL, highpassR;
        Biquad toneLowL, toneLowR, toneHighL, toneHighR;
        float sampleRate = 48000.0f;
        float volume = 0.7f;
        float lastFilter = 999.0f;
        float lastTone = 999.0f;
        float lastVolume = 999.0f;

        void prepare (double sr)
        {
            sampleRate = (float) sr;
            lastFilter = lastTone = lastVolume = 999.0f;
            reset();
            setParameters (0.0f, 0.0f, -2.4f, true);
        }

        void reset()
        {
            lowpassL.reset(); lowpassR.reset();
            highpassL.reset(); highpassR.reset();
            toneLowL.reset(); toneLowR.reset();
            toneHighL.reset(); toneHighR.reset();
        }

        void setParameters (float filter, float tone, float volumeDb, bool force = false)
        {
            if (! force && std::abs (filter - lastFilter) < 1.0e-5f
                && std::abs (tone - lastTone) < 1.0e-4f
                && std::abs (volumeDb - lastVolume) < 1.0e-4f)
                return;
            lastFilter = filter;
            lastTone = tone;
            lastVolume = volumeDb;

            const auto f = juce::jlimit (-1.0f, 1.0f, filter);
            float lp = 20000.0f, hp = 20.0f;
            if (f < 0.0f)
            {
                lp = 20000.0f * std::pow (120.0f / 20000.0f, -f);
                if (lp < 15000.0f)
                    hp = juce::jmax (20.0f, lp * 0.18f);
            }
            else if (f > 0.0f)
            {
                hp = 20.0f * std::pow (8200.0f / 20.0f, f);
                lp = juce::jmin (20000.0f, hp * 12.0f);
            }
            lp = juce::jlimit (80.0f, 20500.0f, lp);
            hp = juce::jlimit (18.0f, 12000.0f, hp);
            lowpassL.configureLowpass (lp, sampleRate); lowpassR.configureLowpass (lp, sampleRate);
            highpassL.configureHighpass (hp, sampleRate); highpassR.configureHighpass (hp, sampleRate);
            const auto t = juce::jlimit (-12.0f, 12.0f, tone);
            toneLowL.configureLowshelf (280.0f, sampleRate, -t * 0.36f);
            toneLowR.configureLowshelf (280.0f, sampleRate, -t * 0.36f);
            toneHighL.configureHighshelf (3400.0f, sampleRate, t * 0.58f);
            toneHighR.configureHighshelf (3400.0f, sampleRate, t * 0.58f);
            volume = std::pow (10.0f, juce::jlimit (-40.0f, 6.0f, volumeDb) * 0.05f);
        }

        void process (float* left, float* right, int numSamples)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                auto l = toneLowL.process (highpassL.process (lowpassL.process (left[i])));
                auto r = toneLowR.process (highpassR.process (lowpassR.process (right[i])));
                l = toneHighL.process (l);
                r = toneHighR.process (r);
                l = std::tanh (l * 1.1f) / 0.800499f;
                r = std::tanh (r * 1.1f) / 0.800499f;
                left[i] = l * volume;
                right[i] = r * volume;
            }
        }
    };
} // namespace vrac

