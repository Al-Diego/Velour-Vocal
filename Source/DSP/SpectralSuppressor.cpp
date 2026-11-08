#include "SpectralSuppressor.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kMarginDb      = 1.0f;    // ignore bumps smaller than this
    constexpr float kPeakClampDb   = 3.0f;    // second envelope pass: limit peak influence
    constexpr float kGateFloorDb   = -90.0f;  // below this nothing is processed (noise floor)
    constexpr float kGateRangeDb   = 20.0f;   // fade-in range above the floor
    constexpr float kMaskTimeMs    = 30.0f;   // solo / delta crossfade time
    constexpr float kXoverFadeOct  = 1.0f / 3.0f; // crossover transition width
    constexpr float kMinFreqHz     = 20.0f;

    inline float coefForTime (float periodSeconds, float timeMs) noexcept
    {
        return std::exp (-periodSeconds / std::max (1.0e-4f, timeMs * 0.001f));
    }

    /** 0 below (x - fade/2), 1 above (x + fade/2), linear in log-frequency between. */
    inline float crossoverRamp (float hz, float xoverHz) noexcept
    {
        const float octaves = std::log2 (hz / xoverHz);
        return juce::jlimit (0.0f, 1.0f, (octaves + 0.5f * kXoverFadeOct) / kXoverFadeOct);
    }

    inline float dbToGain (float db) noexcept
    {
        return std::exp (db * 0.11512925464970229f); // ln(10) / 20
    }
}

//==============================================================================
SpectralSuppressor::ProfileTuning SpectralSuppressor::tuningFor (int profile) noexcept
{
    switch (profile)
    {
        // Lower voices: closer harmonics, so a narrower blur; slightly slower release.
        case Male:   return { 110.0f, { 0.90f, 0.75f, 0.70f }, 8.0f, 90.0f };
        // Higher voices: harmonics further apart, so blur wider so they aren't flagged.
        case Female: return { 210.0f, { 1.00f, 0.90f, 0.80f }, 6.0f, 70.0f };
        default:     return { 160.0f, { 0.95f, 0.80f, 0.75f }, 7.0f, 80.0f };
    }
}

int SpectralSuppressor::fftOrderForSampleRate (double sr) noexcept
{
    if (sr <= 50000.0)  return 11; // 2048
    if (sr <= 100000.0) return 12; // 4096
    return 13;                     // 8192
}

//==============================================================================
void SpectralSuppressor::prepare (double newSampleRate, int maxNumChannels)
{
    sampleRate          = newSampleRate;
    numChannelsPrepared = std::max (1, maxNumChannels);
    fftOrder            = fftOrderForSampleRate (sampleRate);
    fftSize             = 1 << fftOrder;
    hopSize             = fftSize / 4;
    numBins             = fftSize / 2 + 1;
    binHz               = (float) (sampleRate / fftSize);
    hopSeconds          = (float) hopSize / (float) sampleRate;

    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    // Periodic Hann: Hann^2 at 75 % overlap sums to a constant 1.5.
    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);

    auto resizeAll = [] (auto& vecs, size_t count, size_t size)
    {
        vecs.resize (count);
        for (auto& v : vecs) v.assign (size, 0.0f);
    };
    resizeAll (inputRing,  (size_t) numChannelsPrepared, (size_t) fftSize);
    resizeAll (outputRing, (size_t) numChannelsPrepared, (size_t) fftSize);
    resizeAll (fftBuffer,  (size_t) numChannelsPrepared, (size_t) fftSize * 2);

    for (auto* v : { &powerSpec, &magDb, &detailDb, &envDb, &scratch, &targetDb, &reductionDb,
                     &smoothedReductionDb, &binGain, &outFactor, &soloMask, &bandStrength })
        v->assign ((size_t) numBins, 0.0f);

    for (auto& w : bandWeight)
        w.assign ((size_t) numBins, 0.0f);

    prefix.assign ((size_t) numBins + 1, 0.0);
    for (auto* v : { &detailLo, &detailHi, &envLo, &envHi, &gainLo, &gainHi })
        v->assign ((size_t) numBins, 0);

    // Map each display point onto a range of FFT bins.
    for (int p = 0; p < DisplayFrame::numPoints; ++p)
    {
        const float fLo = DisplayFrame::frequencyForPoint ((float) p - 0.5f);
        const float fHi = DisplayFrame::frequencyForPoint ((float) p + 0.5f);
        int lo = (int) std::ceil (fLo / binHz);
        int hi = (int) std::floor (fHi / binHz);
        if (lo > hi)
            lo = hi = (int) std::lround (DisplayFrame::frequencyForPoint ((float) p) / binHz);
        displayLo[(size_t) p] = juce::jlimit (0, numBins - 1, lo);
        displayHi[(size_t) p] = juce::jlimit (0, numBins - 1, hi);
    }
    displayInterval = std::max (1, (int) (sampleRate / 40.0)); // ~40 updates per second

    for (auto& s : amountSmoothed)
        s.reset (sampleRate, 0.05);
    bypassSmoothed.reset (sampleRate, 0.05);

    maskCoef         = coefForTime (hopSeconds, kMaskTimeMs);
    levelAttackCoef  = coefForTime (hopSeconds, 5.0f);
    levelReleaseCoef = coefForTime (hopSeconds, 120.0f);

    layoutProfile = -1; // force band layout rebuild
    updateBandLayoutIfNeeded();
    reset();
}

void SpectralSuppressor::reset() noexcept
{
    for (auto& v : inputRing)  std::fill (v.begin(), v.end(), 0.0f);
    for (auto& v : outputRing) std::fill (v.begin(), v.end(), 0.0f);
    std::fill (reductionDb.begin(), reductionDb.end(), 0.0f);
    std::fill (smoothedReductionDb.begin(), smoothedReductionDb.end(), 0.0f);
    std::fill (soloMask.begin(), soloMask.end(), 1.0f);

    ringPos = hopCounter = samplesSinceDisplay = 0;
    deltaMix = settings.delta && ! settings.bypass ? 1.0f : 0.0f;
    levelDbState = -120.0f;

    for (int b = 0; b < NumBands; ++b)
    {
        amountSmoothed[(size_t) b].setCurrentAndTargetValue (settings.amount[(size_t) b]);
        bandReductionDb[(size_t) b].store (0.0f);
    }
    bypassSmoothed.setCurrentAndTargetValue (settings.bypass ? 0.0f : 1.0f);
    overallReductionDb.store (0.0f);
    detectedLevelDb.store (-120.0f);
}

void SpectralSuppressor::setSettings (const Settings& s) noexcept
{
    settings = s;
    for (int b = 0; b < NumBands; ++b)
        amountSmoothed[(size_t) b].setTargetValue (juce::jlimit (0.0f, 1.0f, s.amount[(size_t) b]));
    bypassSmoothed.setTargetValue (s.bypass ? 0.0f : 1.0f);
}

//==============================================================================
void SpectralSuppressor::updateBandLayoutIfNeeded() noexcept
{
    if (layoutProfile == settings.profile && layoutHighBroad == settings.highBroad
        && juce::exactlyEqual (layoutLowMid, settings.xoverLowMid)
        && juce::exactlyEqual (layoutMidHigh, settings.xoverMidHigh))
        return;

    layoutProfile   = settings.profile;
    layoutHighBroad = settings.highBroad;
    layoutLowMid    = settings.xoverLowMid;
    layoutMidHigh   = settings.xoverMidHigh;

    const auto tune  = tuningFor (settings.profile);
    const bool broad = settings.highBroad;

    attackCoef  = coefForTime (hopSeconds, tune.attackMs);
    releaseCoef = coefForTime (hopSeconds, broad ? tune.releaseMs * 1.25f : tune.releaseMs);

    // Half-widths in octaves for each band. Focused highs: tight detail + narrow
    // reference (catches narrow peaks). Broad highs: wider everything, gentler.
    const std::array<float, NumBands> detailHalfOct { 1.0f / 24.0f, 1.0f / 24.0f, broad ? 1.0f / 12.0f : 1.0f / 48.0f };
    const std::array<float, NumBands> envHalfOct    { tune.envOctaves[0] * 0.5f, tune.envOctaves[1] * 0.5f,
                                                      broad ? 0.75f : tune.envOctaves[2] * 0.5f };
    const std::array<float, NumBands> gainHalfOct   { 1.0f / 48.0f, 1.0f / 48.0f, broad ? 1.0f / 8.0f : 1.0f / 48.0f };
    const std::array<float, NumBands> strength      { 0.9f, 1.0f, broad ? 0.75f : 1.15f };
    const float detailHalfHz = tune.detailWidthHz * 0.5f;

    const int last = numBins - 1;
    auto toBin = [this, last] (float hz) { return juce::jlimit (1, last, (int) std::lround (hz / binHz)); };

    for (int k = 0; k < numBins; ++k)
    {
        const float f  = (float) std::max (k, 1) * binHz;
        const float wl = 1.0f - crossoverRamp (f, settings.xoverLowMid);
        const float wh = crossoverRamp (f, settings.xoverMidHigh);
        const float wm = std::max (0.0f, 1.0f - wl - wh);

        bandWeight[Low][(size_t) k]  = wl;
        bandWeight[Mid][(size_t) k]  = wm;
        bandWeight[High][(size_t) k] = wh;

        auto blend = [wl, wm, wh] (const std::array<float, NumBands>& a) { return wl * a[0] + wm * a[1] + wh * a[2]; };

        // Below 20 Hz we don't touch anything.
        const float lowFloor = juce::jlimit (0.0f, 1.0f, (f - (kMinFreqHz - 5.0f)) / 10.0f);
        bandStrength[(size_t) k] = k == 0 ? 0.0f : blend (strength) * lowFloor;

        if (k == 0)
        {
            detailLo[0] = detailHi[0] = envLo[0] = envHi[0] = gainLo[0] = gainHi[0] = 0;
            continue;
        }

        const float hd = blend (detailHalfOct);
        detailLo[(size_t) k] = toBin (std::min (f * std::exp2 (-hd), f - detailHalfHz));
        detailHi[(size_t) k] = toBin (std::max (f * std::exp2 (hd),  f + detailHalfHz));

        const float he = blend (envHalfOct);
        envLo[(size_t) k] = juce::jlimit (1, last, std::min (toBin (f * std::exp2 (-he)), k - 3));
        envHi[(size_t) k] = juce::jlimit (1, last, std::max (toBin (f * std::exp2 (he)),  k + 3));

        const float hg = blend (gainHalfOct);
        gainLo[(size_t) k] = juce::jlimit (1, last, std::min (toBin (f * std::exp2 (-hg)), k - 1));
        gainHi[(size_t) k] = juce::jlimit (1, last, std::max (toBin (f * std::exp2 (hg)),  k + 1));
    }
}

void SpectralSuppressor::boxAverage (const float* src, float* dst, const int* lo, const int* hi) noexcept
{
    prefix[0] = 0.0;
    for (int k = 0; k < numBins; ++k)
        prefix[(size_t) k + 1] = prefix[(size_t) k] + (double) src[k];

    for (int k = 0; k < numBins; ++k)
    {
        const int a = lo[k], b = hi[k];
        dst[k] = (float) ((prefix[(size_t) b + 1] - prefix[(size_t) a]) / (double) (b - a + 1));
    }
}

float SpectralSuppressor::dynamicsScale (float levelDb) const noexcept
{
    // Quiet passages get only a little processing; at the threshold it is at full
    // strength, and above the threshold it keeps increasing (up to +60 %).
    const float t = settings.dynThresholdDb;
    if (levelDb >= t)
        return 1.0f + 0.6f * std::min (levelDb - t, 12.0f) / 12.0f;

    const float x = juce::jlimit (0.0f, 1.0f, (levelDb - (t - 18.0f)) / 18.0f);
    return 0.1f + 0.9f * x;
}

//==============================================================================
void SpectralSuppressor::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const int chans = std::min (numChannels, numChannelsPrepared);
    if (chans <= 0 || fft == nullptr)
        return;

    updateBandLayoutIfNeeded();

    const int mask = fftSize - 1;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < chans; ++ch)
        {
            auto& out = outputRing[(size_t) ch];
            inputRing[(size_t) ch][(size_t) ringPos] = channels[ch][i];
            channels[ch][i] = out[(size_t) ringPos];
            out[(size_t) ringPos] = 0.0f;
        }

        ringPos = (ringPos + 1) & mask;

        if (++hopCounter == hopSize)
        {
            hopCounter = 0;
            processFrame (chans);
        }
    }
}

void SpectralSuppressor::processFrame (int numChannels) noexcept
{
    const int N = fftSize, mask = N - 1;

    // ---- 1. analysis --------------------------------------------------------
    std::fill (powerSpec.begin(), powerSpec.end(), 0.0f);
    double sumSquares = 0.0;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* buf = fftBuffer[(size_t) ch].data();
        const auto* in = inputRing[(size_t) ch].data();

        for (int j = 0; j < N; ++j)
        {
            const float x = in[(ringPos + j) & mask]; // ringPos is the oldest sample
            sumSquares += (double) x * x;
            buf[j] = x * window[(size_t) j];
        }
        std::fill (buf + N, buf + 2 * N, 0.0f);

        fft->performRealOnlyForwardTransform (buf, true);

        for (int k = 0; k < numBins; ++k)
            powerSpec[(size_t) k] += buf[2 * k] * buf[2 * k] + buf[2 * k + 1] * buf[2 * k + 1];
    }

    // A full-scale sine reads 0 dB in its bin.
    const float norm = (4.0f / (float) N) * (4.0f / (float) N) / (float) numChannels;
    for (int k = 0; k < numBins; ++k)
    {
        powerSpec[(size_t) k] *= norm;
        magDb[(size_t) k] = 10.0f * std::log10 (powerSpec[(size_t) k] + 1.0e-12f);
    }

    // ---- 2. level detection for the dynamics section ------------------------
    const float frameLevelDb = 10.0f * (float) std::log10 (sumSquares / (double) (N * numChannels) + 1.0e-12);
    const float lc = frameLevelDb > levelDbState ? levelAttackCoef : levelReleaseCoef;
    levelDbState = frameLevelDb + lc * (levelDbState - frameLevelDb);
    detectedLevelDb.store (levelDbState, std::memory_order_relaxed);

    const float dynScale = settings.dynEnabled ? dynamicsScale (levelDbState) : 1.0f;
    const float active   = bypassSmoothed.skip (hopSize);

    std::array<float, NumBands> amt {};
    for (int b = 0; b < NumBands; ++b)
        amt[(size_t) b] = amountSmoothed[(size_t) b].skip (hopSize)
                        * (settings.enabled[(size_t) b] ? 1.0f : 0.0f) * active;

    // ---- 3. detail curve and reference envelope ----------------------------
    boxAverage (magDb.data(),    detailDb.data(), detailLo.data(), detailHi.data());
    boxAverage (detailDb.data(), envDb.data(),    envLo.data(),    envHi.data());
    for (int k = 0; k < numBins; ++k)
        scratch[(size_t) k] = std::min (detailDb[(size_t) k], envDb[(size_t) k] + kPeakClampDb);
    boxAverage (scratch.data(),  envDb.data(),    envLo.data(),    envHi.data());

    // ---- 4. target reduction + attack/release per bin ----------------------
    const auto& wL = bandWeight[Low];
    const auto& wM = bandWeight[Mid];
    const auto& wH = bandWeight[High];

    for (int k = 0; k < numBins; ++k)
    {
        const size_t i = (size_t) k;
        const float a = (wL[i] * amt[0] + wM[i] * amt[1] + wH[i] * amt[2]) * bandStrength[i] * dynScale;
        const float excess = detailDb[i] - envDb[i] - kMarginDb;

        float target = 0.0f;
        if (excess > 0.0f && a > 0.0f)
        {
            const float gate   = juce::jlimit (0.0f, 1.0f, (detailDb[i] - kGateFloorDb) / kGateRangeDb);
            // Works like a compressor above the envelope: ratio 1:1 at 0 %, 5:1 at 100 %
            // (higher with band strength / dynamics), so peaks are pulled toward the envelope.
            const float ratio  = 1.0f + 4.0f * a;
            const float maxCut = 3.0f + 21.0f * std::min (a, 1.0f);
            target = std::min (excess * (1.0f - 1.0f / ratio), maxCut) * gate;
        }

        const float coef = target > reductionDb[i] ? attackCoef : releaseCoef;
        reductionDb[i] = target + coef * (reductionDb[i] - target);
    }

    // ---- 5. smooth across frequency, build output gains ---------------------
    boxAverage (reductionDb.data(), smoothedReductionDb.data(), gainLo.data(), gainHi.data());
    smoothedReductionDb[0] = 0.0f;

    const bool anySolo = (settings.solo[0] || settings.solo[1] || settings.solo[2]) && ! settings.bypass;
    const float deltaTarget = settings.delta && ! settings.bypass ? 1.0f : 0.0f;
    deltaMix = deltaTarget + maskCoef * (deltaMix - deltaTarget);

    std::array<double, NumBands> bandIn {}, bandOut {};
    double totalIn = 0.0, totalOut = 0.0;

    for (int k = 0; k < numBins; ++k)
    {
        const size_t i = (size_t) k;
        const float g = dbToGain (-smoothedReductionDb[i]);
        binGain[i] = g;

        const float maskTarget = anySolo ? (settings.solo[0] ? wL[i] : 0.0f)
                                         + (settings.solo[1] ? wM[i] : 0.0f)
                                         + (settings.solo[2] ? wH[i] : 0.0f)
                                         : 1.0f;
        soloMask[i] = maskTarget + maskCoef * (soloMask[i] - maskTarget);

        // processed * (1 - delta) + removed * delta
        outFactor[i] = soloMask[i] * (g + deltaMix * (1.0f - 2.0f * g));

        const double p = powerSpec[i], pg = p * (double) (g * g);
        bandIn[0] += wL[i] * p;  bandOut[0] += wL[i] * pg;
        bandIn[1] += wM[i] * p;  bandOut[1] += wM[i] * pg;
        bandIn[2] += wH[i] * p;  bandOut[2] += wH[i] * pg;
        totalIn += p; totalOut += pg;
    }

    // ---- 6. resynthesis + overlap-add ----------------------------------------
    const float synthScale = 1.0f / 1.5f; // Hann^2 overlap sum at 75 %
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* buf = fftBuffer[(size_t) ch].data();
        for (int k = 0; k < numBins; ++k)
        {
            buf[2 * k]     *= outFactor[(size_t) k];
            buf[2 * k + 1] *= outFactor[(size_t) k];
        }

        fft->performRealOnlyInverseTransform (buf);

        auto* out = outputRing[(size_t) ch].data();
        for (int j = 0; j < N; ++j)
            out[(ringPos + j) & mask] += buf[j] * window[(size_t) j] * synthScale;
    }

    // ---- 7. metering ---------------------------------------------------------
    auto reductionFor = [] (double in, double out)
    {
        return in > 1.0e-10 ? (float) (-10.0 * std::log10 (std::max (out, 1.0e-20) / in)) : 0.0f;
    };
    overallReductionDb.store (reductionFor (totalIn, totalOut), std::memory_order_relaxed);
    for (int b = 0; b < NumBands; ++b)
        bandReductionDb[(size_t) b].store (reductionFor (bandIn[(size_t) b], bandOut[(size_t) b]), std::memory_order_relaxed);

    samplesSinceDisplay += hopSize;
    if (samplesSinceDisplay >= displayInterval)
    {
        samplesSinceDisplay = 0;
        pushDisplayFrame();
    }
}

void SpectralSuppressor::pushDisplayFrame() noexcept
{
    for (size_t p = 0; p < (size_t) DisplayFrame::numPoints; ++p)
    {
        float red = 0.0f, spec = -120.0f;
        for (int k = displayLo[p]; k <= displayHi[p]; ++k)
        {
            red  = std::max (red,  smoothedReductionDb[(size_t) k]);
            spec = std::max (spec, magDb[(size_t) k]);
        }
        displayScratch.reductionDb[p] = red;
        displayScratch.spectrumDb[p]  = spec;
    }
    displayFifo.push (displayScratch);
}
