// Offline sanity test for the Jimmi Carcosa Fuzz DSP core (no GUI needed).
#include "CarcosaDSP.h"
#include <cstdio>

static double goertzel (const std::vector<float>& x, double f, double fs)
{
    const double w = 2.0 * M_PI * f / fs;
    double s1 = 0, s2 = 0, c = 2.0 * std::cos (w);
    for (float v : x) { double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
    const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
    return 2.0 * std::sqrt (std::max (0.0, p)) / (double) x.size();
}

struct Result { double rms, peak, dc, h1, h2, h3; bool finite; std::vector<float> tail; };

static Result run (carcosa::Params p, double amp, double f0 = 330.0, double fs = 48000.0, double seconds = 1.0)
{
    carcosa::Engine eng;
    eng.prepare (fs);
    const int N = (int) (fs * seconds), block = 512;
    juce::AudioBuffer<float> buf (2, block);
    std::vector<float> out;
    for (int pos = 0; pos < N; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            float v = (float) (amp * std::sin (2.0 * M_PI * f0 * (pos + i) / fs));
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        eng.process (buf, p);
        for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
    }
    Result r { 0, 0, 0, 0, 0, 0, true, { out.begin() + out.size() / 2, out.end() } };
    for (float v : r.tail)
    {
        if (! std::isfinite (v)) r.finite = false;
        r.rms += v * v; r.peak = std::max (r.peak, (double) std::fabs (v)); r.dc += v;
    }
    r.rms = std::sqrt (r.rms / r.tail.size()); r.dc /= r.tail.size();
    r.h1 = goertzel (r.tail, f0, fs); r.h2 = goertzel (r.tail, 2 * f0, fs); r.h3 = goertzel (r.tail, 3 * f0, fs);
    return r;
}

static void show (const char* name, const Result& r)
{
    std::printf ("%-40s rms=%.3f peak=%.3f dc=%+.4f  f0=%.3f 2f0=%.3f 3f0=%.3f  %s\n", name, r.rms, r.peak, r.dc,
                 r.h1, r.h2, r.h3, r.finite ? "ok" : "NaN/Inf!");
}
static double dbr (double a, double b) { return 20.0 * std::log10 (a / b); }

int main()
{
    int failures = 0;
    auto check = [&failures] (bool cond, const char* msg) { if (! cond) { std::printf ("FAIL: %s\n", msg); ++failures; } };

    // --- bypass is transparent
    carcosa::Params off; off.on = false;
    auto rb = run (off, 0.1);
    show ("bypassed (on = false)", rb);
    check (std::fabs (rb.rms - 0.1 / std::sqrt (2.0)) < 0.004, "bypass passes the signal at unity gain");

    // --- defaults
    carcosa::Params def;
    auto rd = run (def, 0.1);
    show ("defaults, -20 dBFS in", rd);
    check (rd.finite, "finite output with defaults");
    check (rd.rms > 0.05 && rd.rms < 1.5, "default level is usable");
    check (rd.h3 > 0.01, "odd harmonics present");
    check (std::fabs (rd.dc) < 0.01, "no DC offset");

    // --- AFTER = 0 symmetric, AFTER = 1 asymmetric (even harmonics rise)
    carcosa::Params a0 = def, a1 = def; a0.after = 0.0f; a1.after = 1.0f;
    auto ra0 = run (a0, 0.1), ra1 = run (a1, 0.1);
    show ("AFTER 0 (woody)", ra0);
    show ("AFTER 1 (starved)", ra1);
    check (ra0.h2 < 0.05 * ra0.h1, "AFTER 0: nearly symmetric, even harmonics small");
    check (ra1.h2 > 3.0 * ra0.h2 + 0.002, "AFTER 1: even harmonics grow (broken, splattery)");

    // --- gating: with AFTER high a quiet signal is attenuated much more than a loud one (guitar volume cleans it up differently)
    carcosa::Params g0 = def, g1 = def; g0.after = 0.0f; g1.after = 1.0f; g0.before = g1.before = 0.4f;
    const double qa = 0.004, la = 0.1;
    const double gainQuiet0 = run (g0, qa).rms / (qa / std::sqrt (2.0)), gainLoud0 = run (g0, la).rms / (la / std::sqrt (2.0));
    const double gainQuiet1 = run (g1, qa).rms / (qa / std::sqrt (2.0)), gainLoud1 = run (g1, la).rms / (la / std::sqrt (2.0));
    std::printf ("gain quiet/loud: AFTER 0 -> %.1f/%.1f   AFTER 1 -> %.1f/%.1f\n", gainQuiet0, gainLoud0, gainQuiet1, gainLoud1);
    check (gainQuiet1 / gainLoud1 < 0.6 * (gainQuiet0 / gainLoud0), "AFTER high: quiet notes lose gain relative to loud ones (gating)");

    // --- BEFORE adds gain
    carcosa::Params b0 = def, b1 = def; b0.before = 0.1f; b1.before = 0.9f;
    auto rbl = run (b0, 0.01), rbh = run (b1, 0.01);
    show ("BEFORE 0.1 (-40 dBFS in)", rbl);
    show ("BEFORE 0.9 (-40 dBFS in)", rbh);
    check (rbh.rms > 3.0 * rbl.rms, "BEFORE raises the level of a quiet signal");
    check (rbh.rms > 0.05, "BEFORE high gives a strong fuzz from a quiet signal");

    // --- OUTPUT is a level control, and with BEFORE/AFTER at zero the pedal acts as a boost
    carcosa::Params o1 = def, o2 = def; o1.output = 0.3f; o2.output = 0.9f;
    check (run (o2, 0.1).rms > 4.0 * run (o1, 0.1).rms, "OUTPUT controls the level");
    carcosa::Params boost = def; boost.before = 0.0f; boost.after = 0.0f; boost.output = 0.8f;
    auto rboost = run (boost, 0.02);
    show ("boost use (BEFORE 0, AFTER 0, OUT 0.8)", rboost);
    check (rboost.rms > 0.02 / std::sqrt (2.0) * 1.5, "BEFORE and AFTER at zero: usable clean boost");

    // --- DEMHE adds bass relative to HALI, HI-CUT darkens
    carcosa::Params dem = def, hali = def; dem.demhe = true; hali.demhe = false; dem.before = hali.before = 0.1f;
    const double bass = dbr (run (dem, 0.005, 90.0).rms, run (hali, 0.005, 90.0).rms);
    const double mid = dbr (run (dem, 0.005, 1500.0).rms, run (hali, 0.005, 1500.0).rms);
    std::printf ("DEMHE vs HALI: 90 Hz %+.1f dB, 1.5 kHz %+.1f dB\n", bass, mid);
    check (bass > 6.0, "DEMHE has clearly more bass than HALI");
    check (bass > mid + 4.0, "the DEMHE/HALI difference is mostly in the bass");
    carcosa::Params bright = def, dark = def; bright.hiCut = 0.0f; dark.hiCut = 1.0f; bright.before = dark.before = 0.1f;
    const double cut = dbr (run (dark, 0.005, 5000.0).rms, run (bright, 0.005, 5000.0).rms);
    std::printf ("HI-CUT 1 vs 0: 5 kHz %+.1f dB\n", cut);
    check (cut < -12.0, "HI-CUT turned up removes the treble");


    // --- OCTAVE down: a note of f0 gains a strong component at f0/2 (flip-flop divider), none when the knob is at zero
    {
        carcosa::Params od = def; od.before = 0.4f; od.octUp = false;
        carcosa::Params o0 = od; o0.octave = 0.0f;
        carcosa::Params o1 = od; o1.octave = 1.0f;
        for (double f0 : { 110.0, 165.0, 220.0, 330.0 })
        {
            auto r0 = run (o0, 0.1, f0, 48000.0, 2.0), r1 = run (o1, 0.1, f0, 48000.0, 2.0);
            const double half0 = goertzel (r0.tail, f0 / 2, 48000.0), half1 = goertzel (r1.tail, f0 / 2, 48000.0);
            std::printf ("octave down @%.0f Hz: component at f0/2 %.4f -> %.4f (fundamental %.3f)\n", f0, half0, half1, r1.h1);
            check (half1 > 0.2 * r1.h1, "octave down: strong sub-octave component");
            check (half1 > 10.0 * half0 + 0.005, "octave down: sub-octave only appears when the knob is up");
            check (r1.finite && std::fabs (r1.dc) < 0.02, "octave down: finite, no DC");
        }
        for (float amt : { 0.5f, 1.0f })
        {
            carcosa::Params oa = od; oa.octave = amt;
            auto r = run (oa, 0.1, 220.0, 48000.0, 2.0);
            std::printf ("octave down knob %.1f @220 Hz: f0 %.3f, f0/2 %.3f (ratio %.2f)\n", amt, r.h1, goertzel (r.tail, 110.0, 48000.0), goertzel (r.tail, 110.0, 48000.0) / r.h1);
        }
        // decays with the note: a note that dies away must not leave the divider running
        carcosa::Engine eng; eng.prepare (48000.0);
        juce::AudioBuffer<float> buf (2, 512);
        double tailRms = 0;
        for (int blk = 0; blk < 200; ++blk)
        {
            for (int i = 0; i < 512; ++i)
            {
                const double t = (blk * 512 + i) / 48000.0;
                const float v = t < 0.3 ? (float) (0.1 * std::sin (2.0 * M_PI * 220.0 * t)) : 0.0f;
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            eng.process (buf, o1);
            if (blk >= 150) for (int i = 0; i < 512; ++i) tailRms += buf.getSample (0, i) * buf.getSample (0, i);
        }
        tailRms = std::sqrt (tailRms / (50 * 512));
        std::printf ("octave down: output 1.5 s after the note ended: rms %.6f\n", tailRms);
        check (tailRms < 1.0e-3, "octave down: silence after the note ends (no stuck divider)");
    }

    // --- OCTAVE up (rectifier, as on the FZ-2): energy at 2*f0 grows
    {
        carcosa::Params ou = def; ou.before = 0.4f; ou.octUp = true;
        carcosa::Params u0 = ou, u1 = ou; u0.octave = 0.0f; u1.octave = 1.0f;
        auto r0 = run (u0, 0.1, 220.0, 48000.0, 2.0), r1 = run (u1, 0.1, 220.0, 48000.0, 2.0);
        const double two0 = goertzel (r0.tail, 440.0, 48000.0), two1 = goertzel (r1.tail, 440.0, 48000.0);
        std::printf ("octave up @220 Hz: component at 2*f0 %.4f -> %.4f\n", two0, two1);
        check (two1 > 3.0 * two0, "octave up: strong 2*f0 component");
    }

    // --- tone controls (measured in the linear region: BEFORE and AFTER at zero, HI-CUT open, tiny signal)
    {
        carcosa::Params t = def; t.before = 0.0f; t.after = 0.0f; t.hiCut = 0.0f; t.demhe = true;
        auto gainAt = [&] (carcosa::Params q, double f) { return run (q, 0.0005, f, 48000.0, 1.0).rms; };
        auto flatLo = gainAt (t, 60.0), flatMid = gainAt (t, 750.0), flatHi = gainAt (t, 7000.0);
        carcosa::Params bu = t, bd = t, mu = t, md = t, tu = t, td = t;
        bu.bass = 1.0f; bd.bass = 0.0f; mu.mid = 1.0f; md.mid = 0.0f; tu.treble = 1.0f; td.treble = 0.0f;
        const double b1 = dbr (gainAt (bu, 60.0), flatLo), b0 = dbr (gainAt (bd, 60.0), flatLo);
        const double m1 = dbr (gainAt (mu, 750.0), flatMid), m0 = dbr (gainAt (md, 750.0), flatMid);
        const double t1 = dbr (gainAt (tu, 7000.0), flatHi), t0 = dbr (gainAt (td, 7000.0), flatHi);
        std::printf ("tone: bass %+.1f/%+.1f dB @60 Hz, mid %+.1f/%+.1f dB @750 Hz, treble %+.1f/%+.1f dB @7 kHz (max/min)\n", b1, b0, m1, m0, t1, t0);
        check (b1 > 8.0 && b0 < -8.0, "BASS boosts and cuts the low end");
        check (m1 > 8.0 && m0 < -8.0, "MID boosts and cuts the middle");
        check (t1 > 8.0 && t0 < -8.0, "TREBLE boosts and cuts the top");
        // the controls are independent: moving one hardly changes the other regions
        check (std::fabs (dbr (gainAt (bu, 7000.0), flatHi)) < 1.5, "BASS does not change the treble");
        check (std::fabs (dbr (gainAt (tu, 60.0), flatLo)) < 1.5, "TREBLE does not change the bass");
    }

    // --- sustain: the output level hardly follows the input level over a 30 dB range
    {
        const double lo = run (def, 0.01).rms, hi = run (def, 0.3).rms;
        std::printf ("sustain: 30 dB more input gives %+.1f dB more output\n", dbr (hi, lo));
        check (dbr (hi, lo) < 12.0, "heavy compression: 30 dB of input range gives under 12 dB of output range");
    }

    // --- stability at extremes
    bool allFinite = true; double worstDc = 0, maxPeak = 0;
    for (int demhe = 0; demhe < 2; ++demhe)
        for (float be : { 0.0f, 1.0f })
            for (float af : { 0.0f, 0.5f, 1.0f })
                for (float hc : { 0.0f, 1.0f })
                {
                    carcosa::Params e; e.demhe = demhe; e.before = be; e.after = af; e.hiCut = hc; e.output = 1.0f; e.inputDb = 24.0f; e.trimDb = 0.0f;
                    e.octave = 1.0f; e.octUp = (be > 0.5f); e.bass = e.mid = e.treble = (af > 0.4f ? 1.0f : 0.0f);
                    for (double a : { 1.0, 0.001 })
                    {
                        auto r = run (e, a, 110.0);
                        allFinite = allFinite && r.finite;
                        worstDc = std::max (worstDc, std::fabs (r.dc) / std::max (r.peak, 1.0e-3));
                        maxPeak = std::max (maxPeak, r.peak);
                    }
                }
    std::printf ("extremes: finite=%d worst |dc|/peak=%.4f max peak=%.2f\n", allFinite, worstDc, maxPeak);
    {
        carcosa::Params e; e.before = 1.0f; e.after = 1.0f; e.output = 1.0f; e.inputDb = 24.0f; e.trimDb = 12.0f; e.octave = 1.0f;
        e.bass = e.mid = e.treble = 1.0f;
        auto r = run (e, 1.0, 110.0);
        allFinite = allFinite && r.finite;
        maxPeak = std::max (maxPeak, r.peak);
    }
    check (allFinite, "all extreme settings stay finite");
    check (maxPeak <= 8.0 * 1.3, "output stays within the clamp plus oversampler overshoot");
    check (worstDc < 0.03, "DC stays below 3% of the peak even at extreme settings");

    // --- steady state is periodic (375 Hz at 48 kHz repeats every 128 samples): no unwanted noise from the model
    {
        auto r = run (def, 0.05, 375.0, 48000.0, 2.0);
        double err = 0, en = 0;
        for (size_t i = 0; i + 128 < r.tail.size(); ++i) { const double d = r.tail[i + 128] - r.tail[i]; err += d * d; en += (double) r.tail[i] * r.tail[i]; }
        std::printf ("periodicity error: %.2e relative\n", std::sqrt (err / en));
        check (std::sqrt (err / en) < 1.0e-3, "steady state is periodic (no model noise)");
    }

    {
        carcosa::Params po = def; po.octave = 0.7f;
        auto r = run (po, 0.05, 375.0, 48000.0, 2.0);
        double err = 0, en = 0;
        for (size_t i = 0; i + 256 < r.tail.size(); ++i) { const double d = r.tail[i + 256] - r.tail[i]; err += d * d; en += (double) r.tail[i] * r.tail[i]; }
        std::printf ("periodicity error with octave down: %.2e relative\n", std::sqrt (err / en));
        check (std::sqrt (err / en) < 1.0e-3, "steady state with octave down is periodic over two cycles");
    }

    carcosa::Engine e; e.prepare (48000.0);
    std::printf ("reported latency: %d samples\n", e.getLatencySamples());
    check (e.getLatencySamples() > 0, "latency is reported");

    std::printf (failures ? "\n%d FAILURES\n" : "\nALL CHECKS PASSED\n", failures);
    return failures ? 1 : 0;
}
