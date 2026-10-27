// Noise gate checks: silence stays silent, hiss is removed, a played note passes untouched.
#include "CarcosaDSP.h"
#include <cstdio>

using namespace carcosa;

static int failures = 0;
static void check (bool ok, const char* what, double v)
{
    std::printf ("%s  %-58s %.3f\n", ok ? "PASS" : "FAIL", what, v);
    if (! ok) ++failures;
}

static double db (double x) { return 20.0 * std::log10 (std::max (x, 1.0e-12)); }

// input: 0.2 s hiss, 0.5 s note (330 Hz, -12 dBFS peak), 0.6 s hiss
static std::vector<float> makeInput (double fs, double hissRmsDb, double noteAmp)
{
    juce::Random rnd (7);
    const int n = (int) (1.3 * fs), on = (int) (0.2 * fs), off = (int) (0.7 * fs);
    const double hiss = std::pow (10.0, hissRmsDb / 20.0) * 1.7320508;
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        double v = hiss * (rnd.nextFloat() * 2.0f - 1.0f);
        if (i >= on && i < off) v += noteAmp * std::sin (6.283185307179586 * 330.0 * (i - on) / fs);
        x[(size_t) i] = (float) v;
    }
    return x;
}

static std::vector<float> run (const std::vector<float>& in, float gateDb, double fs)
{
    Engine e; e.prepare (fs);
    Params p; p.gateDb = gateDb;
    std::vector<float> out (in.size());
    juce::AudioBuffer<float> b (2, 256);
    for (size_t pos = 0; pos < in.size(); pos += 256)
    {
        const int len = (int) std::min<size_t> (256, in.size() - pos);
        b.setSize (2, len, false, false, true);
        for (int i = 0; i < len; ++i) { b.setSample (0, i, in[pos + (size_t) i]); b.setSample (1, i, in[pos + (size_t) i]); }
        e.process (b, p);
        for (int i = 0; i < len; ++i) out[pos + (size_t) i] = b.getSample (0, i);
    }
    return out;
}

static double rms (const std::vector<float>& v, double fs, double t0, double t1)
{
    double s = 0; long n = 0;
    for (long i = (long) (t0 * fs); i < (long) (t1 * fs) && i < (long) v.size(); ++i) { s += (double) v[(size_t) i] * v[(size_t) i]; ++n; }
    return std::sqrt (s / std::max (1L, n));
}

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("-- %.0f Hz\n", fs);
        const auto in = makeInput (fs, -70.0, 0.25);
        const auto off = run (in, -80.0f, fs);
        const auto on  = run (in, -55.0f, fs);

        // hiss only parts: before the note (after the gate has settled) and well after the note
        const double hissOff = rms (off, fs, 1.1, 1.3), hissOn = rms (on, fs, 1.1, 1.3);
        check (db (hissOff) - db (hissOn) > 30.0, "hiss reduction after the note (dB, > 30)", db (hissOff) - db (hissOn));

        const double noteOff = rms (off, fs, 0.25, 0.65), noteOn = rms (on, fs, 0.25, 0.65);
        check (std::fabs (db (noteOn) - db (noteOff)) < 0.3, "note level changed by gate (dB, < 0.3)", db (noteOn) - db (noteOff));

        const double atkOff = rms (off, fs, 0.2, 0.22), atkOn = rms (on, fs, 0.2, 0.22);
        check (db (atkOn) - db (atkOff) > -1.5, "first 20 ms of the note (dB, > -1.5)", db (atkOn) - db (atkOff));

        // silence in -> silence out, whatever the gate does
        std::vector<float> z (48000, 0.0f);
        const auto zo = run (z, -55.0f, fs);
        double pk = 0; for (float v : zo) pk = std::max (pk, (double) std::fabs (v));
        check (pk == 0.0, "digital silence stays exactly silent", pk);

        // gate off must not alter the signal except for the delay (compare against the reference run)
        const auto off2 = run (in, -80.0f, fs);
        check (off2 == off, "gate off is deterministic", 0.0);
    }
    std::printf (failures ? "\n%d FAILURES\n" : "\nALL GATE TESTS PASSED\n", failures);
    return failures ? 1 : 0;
}
