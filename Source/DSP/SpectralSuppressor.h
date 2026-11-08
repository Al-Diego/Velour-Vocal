#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

#include "DisplayFifo.h"

/**
    STFT-based dynamic resonance suppressor.

    Signal flow per FFT frame (Hann window, 75 % overlap):
      1. Power spectrum of all channels (summed, so stereo is processed with one
         gain curve and the image stays intact).
      2. "Detail" curve: the spectrum lightly smoothed across frequency, wide enough
         to blur individual voice harmonics (width depends on the voice profile).
      3. "Envelope" curve: a much wider log-frequency moving average of the detail
         curve. It is computed twice, with peaks clamped on the second pass so a
         big resonance doesn't drag its own reference upwards.
      4. Anything where detail > envelope is a resonance. The reduction for that
         bin is (excess dB) * (band amount) * (band strength) * (dynamics scale).
      5. Per-bin attack/release smoothing over time, then light smoothing across
         frequency, to avoid fluttering and musical-noise artefacts.
      6. Gains (plus delta / solo masks) are applied to every channel's spectrum,
         then inverse FFT and overlap-add.

    Latency is exactly getLatencySamples() (== FFT size).
    Everything is allocated in prepare(); process() is real-time safe.
*/
class SpectralSuppressor
{
public:
    enum Band { Low = 0, Mid, High, NumBands };
    enum Profile { Male = 0, Female, Neutral };

    struct Settings
    {
        std::array<float, NumBands> amount  { 0.25f, 0.4f, 0.35f }; // 0..1
        std::array<bool,  NumBands> enabled { true, true, true };
        std::array<bool,  NumBands> solo    { false, false, false };
        float xoverLowMid    = 300.0f;
        float xoverMidHigh   = 4000.0f;
        bool  highBroad      = false;   // false = focused/bell, true = broad/shelf
        int   profile        = Neutral;
        bool  dynEnabled     = false;
        float dynThresholdDb = -12.0f;
        bool  delta          = false;
        bool  bypass         = false;   // keeps latency, ramps processing out
    };

    SpectralSuppressor() = default;

    /** Allocates everything. Call from prepareToPlay (not real-time). */
    void prepare (double sampleRate, int maxNumChannels);
    void reset() noexcept;

    /** FFT size is chosen so frequency resolution stays similar at high sample rates. */
    static int fftOrderForSampleRate (double sampleRate) noexcept;
    int getLatencySamples() const noexcept  { return fftSize; }
    int getFftSize() const noexcept         { return fftSize; }

    /** Call once per block from the audio thread, before process(). */
    void setSettings (const Settings& newSettings) noexcept;

    /** In-place processing. Any block size; channels beyond the prepared count pass through. */
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    // --- metering (safe to read from any thread) ---------------------------
    float getOverallReductionDb() const noexcept       { return overallReductionDb.load (std::memory_order_relaxed); }
    float getBandReductionDb (int band) const noexcept { return bandReductionDb[(size_t) band].load (std::memory_order_relaxed); }
    float getDetectedLevelDb() const noexcept          { return detectedLevelDb.load (std::memory_order_relaxed); }
    DisplayFifo& getDisplayFifo() noexcept             { return displayFifo; }

private:
    struct ProfileTuning
    {
        float detailWidthHz;                    // blur width to hide harmonics
        std::array<float, NumBands> envOctaves; // full width of the reference envelope
        float attackMs, releaseMs;
    };
    static ProfileTuning tuningFor (int profile) noexcept;

    void processFrame (int numChannels) noexcept;
    void updateBandLayoutIfNeeded() noexcept;
    void boxAverage (const float* src, float* dst, const int* lo, const int* hi) noexcept;
    void pushDisplayFrame() noexcept;
    float dynamicsScale (float levelDb) const noexcept;

    // --- configuration -------------------------------------------------------
    double sampleRate = 44100.0;
    int fftOrder = 11, fftSize = 2048, hopSize = 512, numBins = 1025, numChannelsPrepared = 0;
    float binHz = 0.0f, hopSeconds = 0.0f;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;

    // --- streaming buffers (per channel) --------------------------------------
    std::vector<std::vector<float>> inputRing, outputRing, fftBuffer;
    int ringPos = 0, hopCounter = 0;

    // --- per-bin analysis arrays ------------------------------------------------
    std::vector<float> powerSpec, magDb, detailDb, envDb, scratch, targetDb, reductionDb,
                       smoothedReductionDb, binGain, outFactor, soloMask, bandStrength;
    std::vector<double> prefix;
    std::array<std::vector<float>, NumBands> bandWeight;
    std::vector<int> detailLo, detailHi, envLo, envHi, gainLo, gainHi;

    // --- parameters -----------------------------------------------------------
    Settings settings;
    std::array<juce::SmoothedValue<float>, NumBands> amountSmoothed;
    juce::SmoothedValue<float> bypassSmoothed;
    float deltaMix = 0.0f;
    float attackCoef = 0.0f, releaseCoef = 0.0f, maskCoef = 0.0f;
    float levelAttackCoef = 0.0f, levelReleaseCoef = 0.0f, levelDbState = -120.0f;

    // cache key for the band layout (recomputed only when these change)
    float layoutLowMid = -1.0f, layoutMidHigh = -1.0f;
    int layoutProfile = -1;
    bool layoutHighBroad = false;

    // --- metering / display ---------------------------------------------------
    std::atomic<float> overallReductionDb { 0.0f }, detectedLevelDb { -120.0f };
    std::array<std::atomic<float>, NumBands> bandReductionDb {};
    DisplayFifo displayFifo;
    DisplayFrame displayScratch;
    std::array<int, DisplayFrame::numPoints> displayLo {}, displayHi {};
    int samplesSinceDisplay = 0, displayInterval = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralSuppressor)
};
