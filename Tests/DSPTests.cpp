// Offline checks for the SpectralSuppressor. Run: ./VelourVocalTests
// Exit code 0 = all passed.

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/SpectralSuppressor.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
int failures = 0;

void check (bool ok, const juce::String& name, const juce::String& detail)
{
    std::printf ("[%s] %s  (%s)\n", ok ? "PASS" : "FAIL", name.toRawUTF8(), detail.toRawUTF8());
    if (! ok) ++failures;
}

using Signal = std::vector<std::vector<float>>; // [channel][sample]

Signal makeNoise (int chans, int len, float level, int seed)
{
    juce::Random r (seed);
    Signal s ((size_t) chans, std::vector<float> ((size_t) len));
    for (auto& c : s) for (auto& x : c) x = (r.nextFloat() * 2.0f - 1.0f) * level;
    return s;
}

void addSine (Signal& s, double sr, double hz, float amp)
{
    for (auto& c : s)
        for (size_t i = 0; i < c.size(); ++i)
            c[i] += amp * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / sr);
}

/** Runs the signal through the engine using random block sizes (1..1024). */
Signal run (SpectralSuppressor& e, Signal in, const SpectralSuppressor::Settings& st, int seed = 1)
{
    juce::Random r (seed);
    const int len = (int) in[0].size();
    std::vector<float*> ptrs (in.size());
    e.setSettings (st);
    for (int pos = 0; pos < len;)
    {
        const int n = std::min (len - pos, 1 + r.nextInt (1024));
        for (size_t c = 0; c < in.size(); ++c) ptrs[c] = in[c].data() + pos;
        e.setSettings (st);
        e.process (ptrs.data(), (int) in.size(), n);
        pos += n;
    }
    return in;
}

/** Amplitude of a sine component via correlation over [start, end). */
double sineAmplitude (const std::vector<float>& x, double sr, double hz, int start, int end)
{
    double re = 0, im = 0;
    for (int i = start; i < end; ++i)
    {
        const double ph = juce::MathConstants<double>::twoPi * hz * i / sr;
        re += x[(size_t) i] * std::cos (ph);
        im += x[(size_t) i] * std::sin (ph);
    }
    return 2.0 * std::sqrt (re * re + im * im) / (end - start);
}

double rms (const std::vector<float>& x, int start, int end)
{
    double s = 0; for (int i = start; i < end; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
    return std::sqrt (s / (end - start));
}

/** Average power (dB) between f1 and f2, from one large windowed FFT starting at 'start'. */
double bandPowerDb (const std::vector<float>& x, int start, double sr, double f1, double f2)
{
    constexpr int order = 15, n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> buf (2 * n, 0.0f);
    for (int i = 0; i < n; ++i)
        buf[(size_t) i] = x[(size_t) (start + i)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) n));
    fft.performFrequencyOnlyForwardTransform (buf.data(), true);
    double sum = 0; int count = 0;
    for (int k = (int) (f1 * n / sr); k <= (int) (f2 * n / sr); ++k) { sum += (double) buf[(size_t) k] * buf[(size_t) k]; ++count; }
    return 10.0 * std::log10 (sum / count + 1e-30);
}

SpectralSuppressor::Settings zeroAmount()
{
    SpectralSuppressor::Settings s;
    s.amount = { 0.0f, 0.0f, 0.0f };
    return s;
}
} // namespace

int main()
{
    // ---- 1. transparency: zero amount == delayed input, every rate, random blocks
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        SpectralSuppressor e;
        auto st = zeroAmount();
        e.setSettings (st);
        e.prepare (sr, 2);
        const int lat = e.getLatencySamples();
        const int len = (int) sr; // 1 s
        auto in = makeNoise (2, len, 0.5f, 7);
        auto out = run (e, in, st);

        double maxErr = 0;
        for (size_t c = 0; c < 2; ++c)
            for (int i = lat; i < len; ++i)
                maxErr = std::max (maxErr, (double) std::abs (out[c][(size_t) i] - in[c][(size_t) (i - lat)]));

        check (maxErr < 1.0e-4, "null test @ " + juce::String (sr, 0) + " Hz",
               "latency " + juce::String (lat) + ", max error " + juce::String (maxErr, 8));
    }

    const double sr = 48000.0;
    const int len = (int) (sr * 3);
    const int tail0 = len / 2; // measure after things settle

    // ---- 2. resonance suppression: narrow peak at 2.5 kHz in noise
    {
        auto in = makeNoise (2, len, 0.05f, 3);
        addSine (in, sr, 2500.0, 0.3f);

        SpectralSuppressor e;
        SpectralSuppressor::Settings st;
        st.amount = { 0.0f, 1.0f, 0.0f };
        e.setSettings (st);
        e.prepare (sr, 2);
        auto out = run (e, in, st);
        const int lat = e.getLatencySamples();

        const double before = sineAmplitude (in[0],  sr, 2500.0, tail0 - lat, len - lat);
        const double after  = sineAmplitude (out[0], sr, 2500.0, tail0, len);
        const double cutDb  = 20.0 * std::log10 (before / after);
        check (cutDb > 6.0, "resonance at 2.5 kHz is reduced", juce::String (cutDb, 1) + " dB");

        // Away from the peak (one octave below / above) the noise should be mostly untouched.
        for (auto range : { std::pair<double, double> { 800.0, 1200.0 }, std::pair<double, double> { 5000.0, 7000.0 } })
        {
            const double d = bandPowerDb (out[0], tail0, sr, range.first, range.second)
                           - bandPowerDb (in[0], tail0 - lat, sr, range.first, range.second);
            check (std::abs (d) < 3.0, "noise at " + juce::String (range.first, 0) + "-" + juce::String (range.second, 0)
                                          + " Hz is mostly untouched", juce::String (d, 2) + " dB");
        }
        check (e.getBandReductionDb (SpectralSuppressor::Mid) > 1.0f, "mid band meter shows reduction",
               juce::String (e.getBandReductionDb (SpectralSuppressor::Mid), 1) + " dB");
    }

    // ---- 3. bands: same peak with only the high band active is left alone
    {
        auto in = makeNoise (1, len, 0.05f, 3);
        addSine (in, sr, 1000.0, 0.3f);
        SpectralSuppressor e;
        SpectralSuppressor::Settings st;
        st.amount = { 0.0f, 0.0f, 1.0f };
        e.setSettings (st);
        e.prepare (sr, 1);
        auto out = run (e, in, st);
        const int lat = e.getLatencySamples();
        const double cutDb = 20.0 * std::log10 (sineAmplitude (in[0], sr, 1000.0, tail0 - lat, len - lat)
                                                / sineAmplitude (out[0], sr, 1000.0, tail0, len));
        check (std::abs (cutDb) < 0.5, "high band ignores a 1 kHz peak", juce::String (cutDb, 2) + " dB");
    }

    // ---- 4. delta: processed + delta == dry
    {
        auto in = makeNoise (2, len, 0.05f, 11);
        addSine (in, sr, 3000.0, 0.3f);
        SpectralSuppressor::Settings st;
        st.amount = { 0.5f, 0.8f, 0.6f };
        SpectralSuppressor a, b;
        a.setSettings (st); a.prepare (sr, 2);
        auto wet = run (a, in, st);
        st.delta = true;
        b.setSettings (st); b.prepare (sr, 2);
        auto del = run (b, in, st);
        const int lat = a.getLatencySamples();

        double maxErr = 0;
        for (int i = tail0; i < len; ++i)
            maxErr = std::max (maxErr, (double) std::abs (wet[0][(size_t) i] + del[0][(size_t) i] - in[0][(size_t) (i - lat)]));
        check (maxErr < 1.0e-3, "processed + delta == dry", "max error " + juce::String (maxErr, 6));
        check (rms (del[0], tail0, len) > 1.0e-3, "delta is not silent", "rms " + juce::String (rms (del[0], tail0, len), 5));
    }

    // ---- 5. solo: soloing Low removes a 5 kHz tone
    {
        auto in = makeNoise (1, len, 0.0f, 1);
        addSine (in, sr, 5000.0, 0.5f);
        SpectralSuppressor e;
        auto st = zeroAmount();
        st.solo = { true, false, false };
        e.setSettings (st); e.prepare (sr, 1);
        auto out = run (e, in, st);
        const double db = 20.0 * std::log10 (rms (out[0], tail0, len) / rms (in[0], tail0, len) + 1e-12);
        check (db < -60.0, "low solo removes 5 kHz", juce::String (db, 1) + " dB");
    }

    // ---- 6. bypass with full amount is transparent (after the ramp)
    {
        auto in = makeNoise (2, len, 0.3f, 5);
        addSine (in, sr, 2500.0, 0.3f);
        SpectralSuppressor e;
        SpectralSuppressor::Settings st;
        st.amount = { 1.0f, 1.0f, 1.0f };
        st.delta = true; st.solo = { false, true, false };
        st.bypass = true;
        e.setSettings (st); e.prepare (sr, 2);
        auto out = run (e, in, st);
        const int lat = e.getLatencySamples();
        double maxErr = 0;
        for (int i = tail0; i < len; ++i)
            maxErr = std::max (maxErr, (double) std::abs (out[1][(size_t) i] - in[1][(size_t) (i - lat)]));
        check (maxErr < 1.0e-4, "bypass is transparent and latency-matched", "max error " + juce::String (maxErr, 8));
    }

    // ---- 7. dynamics: quiet input gets less processing than loud input
    {
        auto measure = [&] (float scale)
        {
            auto in = makeNoise (1, len, 0.05f * scale, 3);
            addSine (in, sr, 2500.0, 0.3f * scale);
            SpectralSuppressor e;
            SpectralSuppressor::Settings st;
            st.amount = { 0.0f, 0.6f, 0.0f };
            st.dynEnabled = true; st.dynThresholdDb = -12.0f;
            e.setSettings (st); e.prepare (sr, 1);
            auto out = run (e, in, st);
            const int lat = e.getLatencySamples();
            return 20.0 * std::log10 (sineAmplitude (in[0], sr, 2500.0, tail0 - lat, len - lat)
                                      / sineAmplitude (out[0], sr, 2500.0, tail0, len));
        };
        const double loud = measure (1.0f), quiet = measure (0.03f);
        check (loud > quiet + 3.0, "dyn: louder passages are processed harder",
               "loud " + juce::String (loud, 1) + " dB, quiet " + juce::String (quiet, 1) + " dB");
    }

    // ---- 8. stability: silence, DC, full-scale clipping noise -> finite output
    {
        SpectralSuppressor e;
        SpectralSuppressor::Settings st;
        st.amount = { 1.0f, 1.0f, 1.0f };
        st.dynEnabled = true;
        e.setSettings (st); e.prepare (192000.0, 2);
        Signal in (2, std::vector<float> (192000, 0.0f));
        juce::Random r (9);
        for (size_t i = 64000; i < 128000; ++i) in[0][i] = in[1][i] = 1.0f;          // DC
        for (size_t i = 128000; i < 192000; ++i) in[0][i] = in[1][i] = r.nextBool() ? 1.0f : -1.0f;
        auto out = run (e, in, st);
        bool finite = true;
        for (auto& c : out) for (float x : c) finite &= std::isfinite (x);
        check (finite, "no NaN/Inf on silence, DC and full-scale noise", "192 kHz");
    }

    // ---- 9. CPU: realtime factor, stereo 48 kHz
    {
        SpectralSuppressor e;
        SpectralSuppressor::Settings st;
        e.setSettings (st); e.prepare (48000.0, 2);
        auto in = makeNoise (2, 48000 * 10, 0.3f, 2);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        run (e, in, st);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        check (ms < 10000.0, "faster than realtime", juce::String (10000.0 / ms, 1) + "x realtime (10 s stereo in "
                                                         + juce::String (ms, 0) + " ms)");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "SOME TESTS FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
