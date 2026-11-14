#include "VoiceEngine.h"

namespace sliceforge
{
    namespace
    {
        struct KickModel { float fStart, fEnd, pitchTau, bodyTauMul, click, drive, sub; };
        struct SnareModel { float tone1, tone2, toneMix, noiseMix, noiseTauMul, bodyTauMul, center, q, snap, softAttack; };
        struct HatModel { float decayMul, metal, noise, metalFreq, hp; };
        struct ClapModel { float bursts, spacing, tailMul, center, q, wide; };

        constexpr std::array<KickModel, 4> kickModels {{
            { 158.0f, 47.0f, 0.042f, 0.50f, 0.10f, 1.40f, 0.14f },
            { 248.0f, 53.0f, 0.016f, 0.30f, 0.50f, 2.10f, 0.06f },
            { 93.0f, 41.0f, 0.120f, 0.86f, 0.05f, 1.05f, 0.24f },
            { 184.0f, 55.0f, 0.028f, 0.42f, 0.34f, 3.40f, 0.12f }
        }};

        constexpr std::array<SnareModel, 4> snareModels {{
            { 183.0f, 327.0f, 0.45f, 1.00f, 0.34f, 0.25f, 2400.0f, 0.85f, 0.65f, 0.0f },
            { 228.0f, 386.0f, 0.62f, 0.86f, 0.20f, 0.16f, 3300.0f, 0.95f, 0.80f, 0.0f },
            { 156.0f, 271.0f, 0.18f, 1.00f, 0.55f, 0.40f, 1500.0f, 0.50f, 0.18f, 0.018f },
            { 427.0f, 691.0f, 0.90f, 0.25f, 0.09f, 0.09f, 4600.0f, 1.60f, 0.55f, 0.0f }
        }};

        constexpr std::array<HatModel, 4> hatModels {{
            { 0.52f, 0.62f, 0.50f, 6200.0f, 7000.0f },
            { 2.70f, 0.55f, 0.40f, 4800.0f, 4800.0f },
            { 1.15f, 0.95f, 0.18f, 8200.0f, 6200.0f },
            { 1.70f, 0.16f, 1.00f, 3800.0f, 3800.0f }
        }};

        constexpr std::array<ClapModel, 4> clapModels {{
            { 4.0f, 0.0115f, 0.62f, 1350.0f, 1.10f, 0.60f },
            { 3.0f, 0.0080f, 0.40f, 1850.0f, 1.30f, 0.40f },
            { 5.0f, 0.0170f, 1.20f, 920.0f, 0.85f, 1.00f },
            { 2.0f, 0.0055f, 0.30f, 2800.0f, 1.50f, 0.25f }
        }};

        constexpr std::array<float, 4> familyGain { 0.92f, 0.58f, 0.40f, 0.62f };
        constexpr std::array<std::array<float, 4>, 4> modelTrim {{
            { 1.0f, 1.0f, 1.0f, 1.0f },
            { 1.0f, 1.02f, 1.06f, 0.9f },
            { 1.22f, 1.16f, 1.50f, 0.60f },
            { 1.04f, 1.0f, 1.12f, 0.92f }
        }};

        const std::array<float, 4096> metalTable = []
        {
            std::array<float, 4096> table {};
            constexpr float ratios[] { 1.0f, 1.4471f, 1.6170f, 1.9265f, 2.5028f, 2.6637f };
            constexpr float phases[] { 0.0f, 1.7f, 3.1f, 4.4f, 5.2f, 2.2f };
            for (int i = 0; i < 4096; ++i)
            {
                const auto p = (float) i / 4096.0f * kTwoPi;
                float sum = 0.0f;
                for (int k = 0; k < 6; ++k)
                    sum += std::sin (p * ratios[k] + phases[k]);
                table[(size_t) i] = sum / 6.0f;
            }
            return table;
        }();

        inline float readMetalTable (float phase)
        {
            auto p = phase - std::floor (phase / 4096.0f) * 4096.0f;
            if (p < 0.0f)
                p += 4096.0f;
            const auto i0 = (int) p;
            const auto i1 = (i0 + 1) & 4095;
            const auto f = p - (float) i0;
            return metalTable[(size_t) i0] * (1.0f - f) + metalTable[(size_t) i1] * f;
        }

        inline float fastTanh (float x)
        {
            if (x < -3.0f) return -1.0f;
            if (x > 3.0f) return 1.0f;
            const auto x2 = x * x;
            return x * (27.0f + x2) / (27.0f + 9.0f * x2);
        }
    } // namespace

    class Voice
    {
    public:
        Voice() = default;

        void setSampleRate (double sr) { sampleRate = (float) sr; }

        bool isActive() const noexcept { return active; }
        double getAge() const noexcept { return t; }
        int getBusIndex() const noexcept { return busIndex; }
        int getPadIndex() const noexcept { return padId; }
        int getChokeGroup() const noexcept { return chokeGroup; }

        void stopImmediately() noexcept
        {
            active = false;
            choking = false;
            chokeGain = 0.0f;
        }

        void choke (bool fast)
        {
            if (! active)
                return;
            choking = true;
            killRate = 1.0f / (juce::jmax (0.001f, sampleRate * (fast ? 0.004f : 0.012f)));
        }

        void setMuted (bool shouldMute) { muted = shouldMute; }

        void start (const PadSettings& settings,
                    const GlobalSettings& globals,
                    float startSeconds,
                    float velocity,
                    int padIndex,
                    SamplePtr source,
                    double engineSampleRate,
                    uint32_t seedValue)
        {
            active = true;
            muted = false;
            choking = false;
            chokeGain = 1.0f;
            killRate = 0.0f;
            t = 0.0;
            pad = settings;
            global = globals;
            padId = padIndex;
            busIndex = juce::jlimit (0, kNumBuses - 1, settings.bus - 1);
            chokeGroup = settings.choke;
            sample = source;
            sampleRate = (float) engineSampleRate;
            rng = Rng (seedValue);
            resampleRatio = 1.0;

            const auto inst = juce::jlimit (0.0f, 1.0f, pad.instability);
            const auto vel = juce::jlimit (0.0f, 1.0f, velocity);
            velocityValue = vel;
            const auto velAmt = juce::jlimit (0.0f, 1.0f, pad.velocityAmount);
            velGain = 1.0f + (std::pow (vel, 1.35f) - 1.0f) * velAmt;
            const auto velocityLength = 1.0f + (vel - 0.5f) * 0.38f * velAmt;
            lengthSeconds = juce::jlimit (0.012f, 4.0f, pad.length * global.length * velocityLength);

            const auto cents = pad.pitch + global.transpose + global.fineTune * 0.01f;
            const auto pitchJitter = rng.bipolar() * inst * 0.55f;
            pitchRatio = std::pow (2.0f, (cents + pitchJitter) / 12.0f);

            impact = juce::jlimit (0.0f, 1.0f, pad.impact);
            warp = juce::jlimit (0.0f, 1.0f, pad.warp);
            superAmount = juce::jlimit (0.0f, 1.0f, pad.super);
            toneShift = juce::jlimit (-24.0f, 24.0f, pad.toneShift + (vel - 0.5f) * 9.0f * velAmt);
            width = 0.35f + juce::jlimit (0.0f, 1.0f, global.width) * 1.3f;

            const auto mixAmount = juce::jlimit (0.0f, 1.0f, pad.drumify * global.drumify * 0.01f);
            const auto modelIndex = juce::jlimit (0, kNumModels - 1, pad.modelIndex);
            family = familyForModel (modelIndex);
            model = localModelForIndex (modelIndex);
            const auto trim = modelTrim[(size_t) family][(size_t) model];
            srcMix = 0.0f;
            modelMix = 0.0f;
            if (sample != nullptr && sample->getNumSamples() > 1)
            {
                srcMix = std::cos (mixAmount * kHalfPi) * dbToGain (pad.originalGain) * 0.92f;
                modelMix = std::sin (mixAmount * kHalfPi) * dbToGain (pad.drumifiedGain) * familyGain[(size_t) family] * trim;
                hasSample = true;
                const auto srcRate = sample->sampleRate > 0.0 ? sample->sampleRate : engineSampleRate;
                resampleRatio = srcRate / engineSampleRate;
                srcInc = (double) pitchRatio * resampleRatio;
                const auto sStart = juce::jlimit (0.0, sample->getDurationSeconds() - 0.002,
                                                  (double) startSeconds + (double) rng.bipolar() * 0.003 * inst);
                srcPos = sStart * srcRate;
                sampleLeft = sample->buffer.getReadPointer (0);
                sampleRight = sample->buffer.getNumChannels() > 1 ? sample->buffer.getReadPointer (1) : sampleLeft;
                sampleLength = sample->getNumSamples();
            }
            else
            {
                modelMix = dbToGain (pad.drumifiedGain) * familyGain[(size_t) family] * trim;
                hasSample = false;
                srcInc = 1.0;
                srcPos = 0.0;
                sampleLeft = sampleRight = nullptr;
                sampleLength = 0;
            }

            const auto pan = juce::jlimit (-1.0f, 1.0f, pad.pan + rng.bipolar() * 0.16f * inst);
            const auto angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            panL = std::cos (angle);
            panR = std::sin (angle);
            gain = dbToGain (pad.volume + rng.bipolar() * 1.4f * inst);

            maxLife = juce::jlimit (0.08, 6.0, (double) (lengthSeconds
                * (family == kick ? 5.4f : family == hat ? 5.2f : family == clap ? 3.6f : 3.6f) + 0.06f));

            kickPhase = tone1Phase = tone2Phase = metalPhase = 0.0f;
            warpLowL = warpLowR = warpHighL = warpHighR = 0.0f;
            mL = mR = 0.0f;
            configureFilters();
        }

        void renderTo (float* busL, float* busR, int start, int end)
        {
            if (! active)
                return;
            for (int i = start; i < end && active; ++i)
            {
                float srcL = 0.0f, srcR = 0.0f;
                if (hasSample && srcMix > 1.0e-5f)
                {
                    const auto i0 = (int) srcPos;
                    if (i0 >= 0 && i0 + 1 < sampleLength)
                    {
                        const auto f = (float) (srcPos - i0);
                        const auto l = sampleLeft[i0] * (1.0f - f) + sampleLeft[i0 + 1] * f;
                        const auto r = sampleRight[i0] * (1.0f - f) + sampleRight[i0 + 1] * f;
                        const auto attack = juce::jmin (1.0f, (float) t / 0.0012f);
                        const auto decay = std::exp (-(float) t / juce::jmax (0.008f, lengthSeconds * 0.62f));
                        const auto release = 1.0f - smoothstep (lengthSeconds * 0.95f, lengthSeconds * 1.35f, (float) t);
                        const auto env = attack * decay * release;
                        srcL = l * env * srcMix;
                        srcR = r * env * srcMix;
                        srcPos += srcInc;
                    }
                }

                float modelL = 0.0f, modelR = 0.0f;
                if (modelMix > 1.0e-5f)
                {
                    renderModel();
                    modelL = mL * modelMix;
                    modelR = mR * modelMix;
                }

                auto xL = srcL + modelL;
                auto xR = srcR + modelR;

                if (impact > 0.001f)
                {
                    const auto boost = 1.0f + impact * 1.8f * std::exp (-(float) t / 0.0045f);
                    xL *= boost;
                    xR *= boost;
                }

                if (warp > 0.001f)
                {
                    const auto aLow = warpLowCoef;
                    const auto aHigh = warpHighCoef;
                    warpLowL += aLow * (xL - warpLowL);
                    warpLowR += aLow * (xR - warpLowR);
                    warpHighL += aHigh * (xL - warpHighL);
                    warpHighR += aHigh * (xR - warpHighR);
                    const auto lowL = warpLowL, lowR = warpLowR;
                    const auto highL = xL - warpHighL, highR = xR - warpHighR;
                    const auto midL = warpHighL - lowL, midR = warpHighR - lowR;
                    const auto age = juce::jlimit (0.0f, 1.0f, (float) t / juce::jmax (0.02f, lengthSeconds * 1.5f));
                    const auto lowGain = 1.0f + warp * 0.95f * (1.0f - age);
                    const auto highGain = 1.0f + warp * 1.05f * age;
                    xL = lowL * lowGain + midL + highL * highGain;
                    xR = lowR * lowGain + midR + highR * highGain;
                }

                xL = eqLowL.process (toneL.process (xL));
                xR = eqLowR.process (toneR.process (xR));
                xL = eqMidL.process (xL);
                xR = eqMidR.process (xR);
                xL = eqHighL.process (xL);
                xR = eqHighR.process (xR);

                if (superAmount > 0.001f)
                {
                    const auto drive = 1.0f + superAmount * 3.2f;
                    const auto norm = fastTanh (drive);
                    xL = fastTanh (xL * drive) / norm;
                    xR = fastTanh (xR * drive) / norm;
                    const auto mid = (xL + xR) * 0.5f;
                    const auto side = (xL - xR) * 0.5f * (1.0f + superAmount * 0.9f);
                    xL = mid + side;
                    xR = mid - side;
                }

                if (std::abs (width - 1.0f) > 0.001f)
                {
                    const auto mid = (xL + xR) * 0.5f;
                    const auto side = (xL - xR) * 0.5f * width;
                    xL = mid + side;
                    xR = mid - side;
                }

                if (choking)
                {
                    chokeGain -= killRate;
                    if (chokeGain <= 0.0f)
                    {
                        chokeGain = 0.0f;
                        active = false;
                        return;
                    }
                }

                if (! muted && chokeGain > 0.0f)
                {
                    const auto g = gain * velGain * chokeGain;
                    busL[i] += xL * g * panL;
                    busR[i] += xR * g * panR;
                }

                t += 1.0 / sampleRate;
                if (t >= maxLife)
                    active = false;
            }
        }

    private:
        void configureFilters()
        {
            const auto sr = sampleRate;
            const auto toneCenter = juce::jlimit (90.0f, 13000.0f, 880.0f * std::pow (2.0f, toneShift / 12.0f));
            const auto toneGain = toneShift == 0.0f ? 0.0f : 4.5f * std::copysign (1.0f, toneShift) * juce::jmin (1.0f, std::abs (toneShift) / 4.0f);
            toneL.configurePeaking (toneCenter, sr, 1.15f, toneGain);
            toneR.configurePeaking (toneCenter, sr, 1.15f, toneGain);
            eqLowL.configureLowshelf (120.0f, sr, pad.eqLow);
            eqLowR.configureLowshelf (120.0f, sr, pad.eqLow);
            eqMidL.configurePeaking (1200.0f, sr, 0.8f, pad.eqMid);
            eqMidR.configurePeaking (1200.0f, sr, 0.8f, pad.eqMid);
            eqHighL.configureHighshelf (7000.0f, sr, pad.eqHigh);
            eqHighR.configureHighshelf (7000.0f, sr, pad.eqHigh);

            switch (family)
            {
                case kick:
                    clickL.configureHighpass (1500.0f, sr);
                    clickR.configureHighpass (1500.0f, sr);
                    colorL.configureBandpass (2200.0f, sr, 1.0f);
                    colorR.configureBandpass (2200.0f, sr, 1.0f);
                    break;
                case snare:
                {
                    const auto& m = snareModels[(size_t) model];
                    colorL.configureBandpass (m.center, sr, m.q);
                    colorR.configureBandpass (m.center, sr, m.q);
                    clickL.configureHighpass (2500.0f, sr);
                    clickR.configureHighpass (2500.0f, sr);
                    break;
                }
                case hat:
                {
                    const auto& m = hatModels[(size_t) model];
                    colorL.configureHighpass (m.hp, sr);
                    colorR.configureHighpass (m.hp, sr);
                    clickL.configureHighpass (1500.0f, sr);
                    clickR.configureHighpass (1500.0f, sr);
                    break;
                }
                case clap:
                {
                    const auto& m = clapModels[(size_t) model];
                    colorL.configureBandpass (m.center, sr, m.q);
                    colorR.configureBandpass (m.center, sr, m.q);
                    clickL.configureHighpass (1500.0f, sr);
                    clickR.configureHighpass (1500.0f, sr);
                    break;
                }
                default:
                    break;
            }
            toneL.reset(); toneR.reset();
            eqLowL.reset(); eqLowR.reset();
            eqMidL.reset(); eqMidR.reset();
            eqHighL.reset(); eqHighR.reset();
            colorL.reset(); colorR.reset();
            clickL.reset(); clickR.reset();
            warpLowL = warpLowR = warpHighL = warpHighR = 0.0f;
        }

        void renderModel()
        {
            const auto tt = (float) t;
            const auto sr = sampleRate;
            const auto len = lengthSeconds;
            const auto ratio = pitchRatio;
            const auto noiseL = rng.bipolar();
            const auto noiseR = rng.bipolar();
            float outL = 0.0f, outR = 0.0f;

            switch (family)
            {
                case kick:
                {
                    const auto& m = kickModels[(size_t) model];
                    const auto f = (m.fEnd + (m.fStart - m.fEnd) * std::exp (-tt / m.pitchTau)) * ratio;
                    kickPhase += kTwoPi * f / sr;
                    if (kickPhase > kTwoPi * 64.0f)
                        kickPhase = std::fmod (kickPhase, kTwoPi * 4.0f);
                    const auto tau = juce::jmax (0.008f, len * m.bodyTauMul);
                    const auto env = std::exp (-tt / tau);
                    auto body = std::sin (kickPhase) * env;
                    body += std::sin (kickPhase * 2.0f + 0.3f) * env * 0.09f;
                    body += std::sin (kickPhase * 3.02f) * env * 0.04f;
                    const auto sub = std::sin (kickPhase * 0.5f) * std::exp (-tt / (tau * 1.6f)) * m.sub;
                    const auto clickAmt = m.click + impact * 0.55f;
                    const auto click = clickL.process (noiseL) * std::exp (-tt / 0.0028f) * clickAmt;
                    const auto click2 = clickR.process (noiseR) * std::exp (-tt / 0.0028f) * clickAmt;
                    outL = fastTanh ((body + sub + click) * m.drive) / fastTanh (m.drive);
                    outR = fastTanh ((body + sub + click2) * m.drive) / fastTanh (m.drive);
                    break;
                }
                case snare:
                {
                    const auto& m = snareModels[(size_t) model];
                    const auto noiseTau = juce::jmax (0.008f, len * m.noiseTauMul);
                    const auto bodyTau = juce::jmax (0.006f, len * m.bodyTauMul);
                    const auto noiseEnv = std::exp (-tt / noiseTau);
                    const auto bodyEnv = std::exp (-tt / bodyTau);
                    const auto attack = m.softAttack > 0.0f ? smoothstep (0.0f, m.softAttack, tt) : 1.0f;
                    const auto nl = colorL.process (noiseL) * 2.6f * m.noiseMix * noiseEnv * attack;
                    const auto nr = colorR.process (noiseR) * 2.6f * m.noiseMix * noiseEnv * attack;
                    tone1Phase += kTwoPi * m.tone1 * ratio / sr;
                    tone2Phase += kTwoPi * m.tone2 * ratio / sr;
                    const auto body = (std::sin (tone1Phase) * 0.7f + std::sin (tone2Phase) * 0.42f) * bodyEnv * m.toneMix * attack;
                    const auto snapAmt = m.snap + impact * 0.45f;
                    const auto snapEnv = std::exp (-tt / 0.0045f) * snapAmt;
                    const auto snapL = clickL.process (noiseL) * snapEnv;
                    const auto snapR = clickR.process (noiseR) * snapEnv;
                    const auto widthMix = superAmount * 0.45f;
                    const auto center = body * 0.6f + snapL * 0.26f;
                    outL = nl + center + (nr - nl) * widthMix * 0.5f + snapL * 0.35f;
                    outR = nr + center + (nl - nr) * widthMix * 0.5f + snapR * 0.35f;
                    break;
                }
                case hat:
                {
                    const auto& m = hatModels[(size_t) model];
                    const auto tau = juce::jmax (0.006f, len * 0.45f * m.decayMul);
                    const auto env = std::exp (-tt / tau) * juce::jmin (1.0f, tt / 0.0007f + 0.15f);
                    metalPhase += (m.metalFreq * ratio / sr) * 4096.0f;
                    const auto metal = readMetalTable (metalPhase);
                    const auto nl = colorL.process (noiseL) * 2.2f;
                    const auto nr = colorR.process (noiseR) * 2.2f;
                    const auto widthMix = m.noise * (0.35f + superAmount * 0.6f);
                    outL = (metal * m.metal + nl * m.noise + (nr - nl) * 0.18f) * env;
                    outR = (metal * m.metal + nr * m.noise + (nl - nr) * 0.18f) * env;
                    outL *= 1.0f + widthMix * 0.15f;
                    outR *= 1.0f + widthMix * 0.15f;
                    break;
                }
                case clap:
                {
                    const auto& m = clapModels[(size_t) model];
                    const auto spacing = m.spacing * (0.9f + impact * 0.15f);
                    float burst = 0.0f;
                    const int bursts = (int) m.bursts;
                    for (int b = 0; b < bursts; ++b)
                    {
                        const auto bt = tt - (float) b * spacing;
                        if (bt >= 0.0f)
                            burst += std::exp (-bt / 0.0045f);
                    }
                    burst = juce::jmin (1.7f, burst);
                    const auto tail = std::exp (-tt / juce::jmax (0.01f, len * m.tailMul));
                    const auto env = burst * 0.95f + tail * 0.24f;
                    const auto nl = colorL.process (noiseL) * 2.8f * env;
                    const auto nr = colorR.process (noiseR) * 2.8f * env;
                    const auto widthMix = m.wide * (0.4f + superAmount * 0.4f);
                    outL = nl + (nr - nl) * widthMix * 0.4f;
                    outR = nr + (nl - nr) * widthMix * 0.4f;
                    break;
                }
                default:
                    break;
            }
            mL = outL;
            mR = outR;
        }

    private:
        bool active = false;
        bool muted = false;
        bool choking = false;
        bool hasSample = false;
        int padId = 0;
        int busIndex = 0;
        int chokeGroup = 0;
        int family = 0;
        int model = 0;
        float sampleRate = 48000.0f;
        float lengthSeconds = 0.25f;
        float impact = 0.0f;
        float warp = 0.0f;
        float superAmount = 0.0f;
        float toneShift = 0.0f;
        float width = 1.0f;
        float velocityValue = 0.8f;
        float velGain = 1.0f;
        float pitchRatio = 1.0f;
        float srcMix = 0.0f;
        float modelMix = 0.0f;
        float gain = 1.0f;
        float panL = 0.7071068f;
        float panR = 0.7071068f;
        float chokeGain = 1.0f;
        float killRate = 0.0f;
        double t = 0.0;
        double maxLife = 1.0;
        double srcPos = 0.0;
        double srcInc = 1.0;
        double resampleRatio = 1.0;
        const float* sampleLeft = nullptr;
        const float* sampleRight = nullptr;
        int sampleLength = 0;
        SamplePtr sample;
        PadSettings pad;
        GlobalSettings global;

        uint32_t seed = 1;
        Rng rng { 1 };

        float kickPhase = 0.0f;
        float tone1Phase = 0.0f;
        float tone2Phase = 0.0f;
        float metalPhase = 0.0f;
        float warpLowL = 0.0f, warpLowR = 0.0f, warpHighL = 0.0f, warpHighR = 0.0f;
        float warpLowCoef = 1.0f - std::exp (-kTwoPi * 200.0f / 48000.0f);
        float warpHighCoef = 1.0f - std::exp (-kTwoPi * 4200.0f / 48000.0f);
        float mL = 0.0f, mR = 0.0f;

        Biquad toneL, toneR, eqLowL, eqLowR, eqMidL, eqMidR, eqHighL, eqHighR;
        Biquad colorL, colorR, clickL, clickR;
    };

    DrumEngine::DrumEngine()
    {
        for (auto& meter : busPeaks)
            meter.store (0.0f, std::memory_order_relaxed);
        for (auto& bus : busBuffers)
            bus.setSize (2, 512);
        for (auto& s : runtime.audible)
            s = true;
    }

    DrumEngine::~DrumEngine() = default;

    void DrumEngine::prepare (double sampleRate, int maximumBlockSize)
    {
        currentSampleRate = sampleRate;
        maximumBlock = juce::jmax (32, maximumBlockSize);
        for (auto& bus : busBuffers)
            bus.setSize (2, maximumBlock, false, true, false);
        for (int i = 0; i < kMaxVoices; ++i)
        {
            if (voices[(size_t) i] == nullptr)
                voices[(size_t) i] = std::make_unique<Voice>();
            voices[(size_t) i]->setSampleRate (sampleRate);
        }
        for (auto& chain : masterChains)
            chain.prepare (sampleRate);
        reset();
    }

    void DrumEngine::reset()
    {
        for (auto& voice : voices)
            if (voice != nullptr)
                voice->stopImmediately();
        for (auto& bus : busBuffers)
            bus.clear();
        for (auto& meter : busPeaks)
            meter.store (0.0f, std::memory_order_relaxed);
    }

    void DrumEngine::setSample (SamplePtr newSample)
    {
        samplePtr.store (std::move (newSample), std::memory_order_release);
    }

    SamplePtr DrumEngine::getSample() const
    {
        return samplePtr.load (std::memory_order_acquire);
    }

    void DrumEngine::setRuntime (const EngineRuntime& newRuntime)
    {
        runtime = newRuntime;
        updateGating();
    }

    void DrumEngine::updateGating()
    {
        for (auto& voice : voices)
            if (voice != nullptr && voice->isActive())
            {
                const auto pad = juce::jlimit (0, kNumPads - 1, voice->getPadIndex());
                voice->setMuted (! runtime.audible[(size_t) pad]);
            }
    }

    Voice* DrumEngine::findFreeVoice()
    {
        Voice* oldest = nullptr;
        for (auto& voice : voices)
        {
            if (voice == nullptr)
                continue;
            if (! voice->isActive())
                return voice.get();
            if (oldest == nullptr || voice->getAge() > oldest->getAge())
                oldest = voice.get();
        }
        return oldest;
    }

    void DrumEngine::trigger (int pad, float velocity, const PadSettings* overrideSettings)
    {
        if (pad < 0 || pad >= kNumPads || ! runtime.audible[(size_t) pad])
            return;
        const auto settings = overrideSettings != nullptr ? *overrideSettings
                                                          : runtime.pads[(size_t) pad];
        if (settings.choke > 0)
        {
            for (auto& voice : voices)
                if (voice != nullptr && voice->isActive() && voice->getChokeGroup() == settings.choke)
                    voice->choke (false);
        }
        auto* voice = findFreeVoice();
        if (voice == nullptr)
            return;
        const auto seed = rng.nextUInt() + 1u;
        const auto source = samplePtr.load (std::memory_order_acquire);
        voice->start (settings, runtime.global, runtime.padStarts[(size_t) pad], velocity, pad,
                      source, currentSampleRate, seed);
    }

    void DrumEngine::chokeAll()
    {
        for (auto& voice : voices)
            if (voice != nullptr && voice->isActive())
                voice->choke (true);
    }

    void DrumEngine::clearBusBuffers (int numSamples)
    {
        for (auto& bus : busBuffers)
            bus.clear (0, juce::jlimit (0, bus.getNumSamples(), numSamples));
    }

    void DrumEngine::processSegment (int startSample, int numSamples)
    {
        const int endSample = juce::jmin (startSample + numSamples, maximumBlock);
        for (auto& voicePtr : voices)
        {
            if (voicePtr == nullptr || ! voicePtr->isActive())
                continue;
            auto& bus = busBuffers[(size_t) juce::jlimit (0, kNumBuses - 1, voicePtr->getBusIndex())];
            voicePtr->renderTo (bus.getWritePointer (0), bus.getWritePointer (1), startSample, endSample);
        }
    }

    void DrumEngine::processMaster (int numSamples)
    {
        const auto& g = runtime.global;
        for (int busIndex = 0; busIndex < kNumBuses; ++busIndex)
        {
            auto& chain = masterChains[(size_t) busIndex];
            chain.setParameters (g.filter, g.tone, g.volume);
            auto& bus = busBuffers[(size_t) busIndex];
            chain.process (bus.getWritePointer (0), bus.getWritePointer (1), numSamples);
            float peak = 0.0f;
            for (int i = 0; i < numSamples; ++i)
                peak = juce::jmax (peak, std::abs (bus.getSample (0, i)), std::abs (bus.getSample (1, i)));
            const auto previous = busPeaks[(size_t) busIndex].load (std::memory_order_relaxed);
            busPeaks[(size_t) busIndex].store (juce::jmax (peak, previous * 0.82f), std::memory_order_relaxed);
        }
    }

    int DrumEngine::getActiveVoiceCount() const
    {
        int count = 0;
        for (const auto& voice : voices)
            if (voice != nullptr && voice->isActive())
                ++count;
        return count;
    }

    float DrumEngine::getBusPeak (int busIndex) const
    {
        if (busIndex < 0 || busIndex >= kNumBuses)
            return 0.0f;
        return busPeaks[(size_t) busIndex].load (std::memory_order_relaxed);
    }
} // namespace sliceforge
