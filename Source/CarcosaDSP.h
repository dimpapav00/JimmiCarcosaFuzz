#pragma once
// Jimmi Carcosa Fuzz - DSP core.
//
// Behavioural model of a two-stage silicon fuzz, started from the public description of the DOD Carcosa
// (BEFORE, AFTER = bias, OUTPUT, HI-CUT, DEMHE/HALI bass switch) and extended with more gain, an octave
// section (octave down with a flip-flop divider, or octave up with a rectifier) and a 3-band tone control.
// The real circuit is not published, so the internals below are educated guesses, not a schematic-exact copy:
//
//   input -> DEMHE (adds bass / low mids) or HALI (cuts bass) -> OCTAVE (down: divider, up: rectifier)
//   -> stage 1: gain set by BEFORE (asymmetric, bandwidth falls with gain)
//   -> stage 2: starved-bias hard-clipping stage set by AFTER (gating, sag, splatter)
//   -> mid/treble emphasis -> BASS / MID / TREBLE -> HI-CUT low-pass -> OUTPUT level

#include <juce_dsp/juce_dsp.h>
#include "NoiseGate.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace carcosa
{
constexpr float kTwoPi = 6.283185307179586f;
constexpr double kPi = 3.14159265358979323846; // the Windows compiler does not define M_PI

struct Params
{
    float before   = 0.65f;  // 0..1  fuzz amount (stage 1 gain, up to +54 dB)
    float after    = 0.30f;  // 0..1  bias of stage 2: 0 = loud and woody, 1 = starved, gated and splattery
    float output   = 0.5f;   // 0..1  level (0.5 ~ unity-ish, 1 = about +12 dB)
    float hiCut    = 0.3f;   // 0..1  0 = open, 1 = dark
    float octave   = 0.0f;   // 0..1  octave amount
    bool  octUp    = false;  // false = octave down (flip-flop divider), true = octave up (rectifier, as on the FZ-2)
    float bass     = 0.5f;   // 0..1  0.5 = flat, +-12 dB
    float mid      = 0.5f;
    float treble   = 0.5f;
    float inputDb  = 0.0f;   // host-side trim before the gate
    float trimDb   = 0.0f;   // host-side output trim
    float gateDb   = -80.0f; // input noise gate threshold, <= -79.9 means off
    bool  on       = true;
    bool  demhe    = true;   // true = DEMHE (adds bass, for clean amps), false = HALI (cuts bass, for dirty amps)
};

struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (float fc, float fs) { a = 1.0f - std::exp (-kTwoPi * fc / fs); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { return x - lp (x); }
    void reset() { z = 0.0f; }
};

// RBJ biquad in double precision (low shelves at 100 Hz on a 176 kHz rate are not safe in float)
struct BiquadCoef
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    static BiquadCoef lowShelf (double fs, double f0, double gainDb, double S = 0.8)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * kPi * f0 / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0), k = 2.0 * std::sqrt (A) * al;
        const double a0 = (A + 1) + (A - 1) * c + k;
        return norm ({ A * ((A + 1) - (A - 1) * c + k), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - k),
                       -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - k }, a0);
    }
    static BiquadCoef highShelf (double fs, double f0, double gainDb, double S = 0.8)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * kPi * f0 / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0), k = 2.0 * std::sqrt (A) * al;
        const double a0 = (A + 1) - (A - 1) * c + k;
        return norm ({ A * ((A + 1) + (A - 1) * c + k), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - k),
                       2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - k }, a0);
    }
    static BiquadCoef peak (double fs, double f0, double Q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * kPi * f0 / fs, c = std::cos (w), al = std::sin (w) / (2.0 * Q);
        return norm ({ 1 + al * A, -2 * c, 1 - al * A, -2 * c, 1 - al / A }, 1 + al / A);
    }

private:
    static BiquadCoef norm (BiquadCoef c, double a0)
    {
        return { c.b0 / a0, c.b1 / a0, c.b2 / a0, c.a1 / a0, c.a2 / a0 };
    }
};

struct Biquad
{
    BiquadCoef k;
    double z1 = 0, z2 = 0;
    float process (float x)
    {
        const double y = k.b0 * x + z1;
        z1 = k.b1 * x - k.a1 * y + z2;
        z2 = k.b2 * x - k.a2 * y;
        return (float) y;
    }
    void reset() { z1 = z2 = 0; }
};

struct Coeffs
{
    float onMix = 1, modeMix = 1, g1 = 1, fc1a = 0.5f, bias = 0, neg = 1, sag = 0, rest = 0, level = 1, hiA = 0.5f;
    float octAmt = 0, octUpMix = 0;
    BiquadCoef bass, mid, treble;
};

class Channel
{
public:
    void prepare (float fsOS)
    {
        dcIn.setCutoff (15.0f, fsOS);
        bassLow.setCutoff (220.0f, fsOS);
        haliHP.setCutoff (300.0f, fsOS);
        coupling1.setCutoff (35.0f, fsOS);
        coupling2.setCutoff (20.0f, fsOS);
        presLo.setCutoff (1000.0f, fsOS);
        presHi.setCutoff (6000.0f, fsOS);
        envLP.setCutoff (10.0f, fsOS);
        sense1.setCutoff (650.0f, fsOS);
        sense2.setCutoff (650.0f, fsOS);
        sub1.setCutoff (500.0f, fsOS);
        sub2.setCutoff (500.0f, fsOS);
        upHP.setCutoff (50.0f, fsOS);
        upLP.setCutoff (4000.0f, fsOS);
        envRel = std::exp (-1.0f / (0.04f * fsOS));
        stage1Rest = std::tanh (0.2f);
        reset();
    }

    void reset()
    {
        for (auto* f : { &dcIn, &bassLow, &haliHP, &miller, &coupling1, &coupling2, &presLo, &presHi, &hi1, &hi2, &envLP,
                         &sense1, &sense2, &sub1, &sub2, &upHP, &upLP })
            f->reset();
        for (auto* b : { &bassQ, &midQ, &trebleQ })
            b->reset();
        env = 0.0f;
        octEnv = 0.0f;
        flip = false;
        armed = true;
    }

    float process (float x, const Coeffs& c)
    {
        const float v = dcIn.hp (x);

        // DEMHE: bass and low-mid boost.  HALI: bass cut.
        const float demhe = v + 0.9f * bassLow.lp (v);
        const float hali = haliHP.hp (v) * 1.1f;
        float y = hali + c.modeMix * (demhe - hali);

        // OCTAVE. Down: Schmitt trigger + flip-flop (divide by two) on the low-passed signal, with the level of the note.
        //          Up: soft full-wave rectifier (the differential-pair trick used in fuzz-octave pedals).
        {
            const float s = sense2.lp (sense1.lp (y));
            const float a = std::fabs (s);
            octEnv = a > octEnv ? a : octEnv * envRel;
            const float thr = 0.3f * octEnv + 2.0e-4f;
            if (armed && s > thr) { flip = ! flip; armed = false; }
            else if (! armed && s < -thr) armed = true;
            const float down = sub2.lp (sub1.lp ((flip ? 1.0f : -1.0f) * octEnv)) * 0.9f;

            constexpr float eps = 0.03f;
            const float rect = std::sqrt (y * y + eps * eps) - eps;
            const float up = upLP.lp (upHP.hp (rect)) * 2.0f;

            const float oct = down + c.octUpMix * (up - down);
            y = y * (1.0f - 0.1f * c.octAmt) + oct * c.octAmt;
        }

        // stage 1: gain from BEFORE, bandwidth falls as the gain rises (Miller effect), asymmetry
        miller.a = c.fc1a;
        y = miller.lp (y * c.g1);
        y = std::tanh (y + 0.2f) - stage1Rest;
        y = coupling1.hp (y);

        // stage 2: AFTER starves the bias. The coupling capacitor charges with the signal (envelope), so loud
        // passages shift the bias further: the stage sags and gates on decays instead of sustaining.
        const float beff = c.bias + c.sag * env;
        const float u = y - beff;
        constexpr float g2 = 8.0f;
        float z;
        if (u >= 0.0f)
            z = std::tanh (g2 * u);
        else
            z = -c.neg * std::tanh (g2 * (-u) / c.neg);
        z -= c.rest;                       // idle level of the stage (removed so nothing thumps when AFTER moves)
        env = envLP.lp (std::fabs (z));
        z = coupling2.hp (z);

        // mid / treble emphasis ("velcro"), then the tone controls and HI-CUT (two poles)
        z += 0.4f * (presHi.lp (z) - presLo.lp (z));
        bassQ.k = c.bass; midQ.k = c.mid; trebleQ.k = c.treble;
        z = trebleQ.process (midQ.process (bassQ.process (z)));
        hi1.a = hi2.a = c.hiA;
        z = hi2.lp (hi1.lp (z));

        const float out = v + c.onMix * (z * c.level - v);
        if (! std::isfinite (out))
        {
            reset();
            return 0.0f;
        }
        return std::clamp (out, -8.0f, 8.0f);
    }

private:
    OnePole dcIn, bassLow, haliHP, miller, coupling1, coupling2, presLo, presHi, hi1, hi2, envLP;
    OnePole sense1, sense2, sub1, sub2, upHP, upLP;
    Biquad bassQ, midQ, trebleQ;
    float env = 0.0f, octEnv = 0.0f, envRel = 0.999f, stage1Rest = 0.197375f;
    bool flip = false, armed = true;
};

class Engine
{
public:
    static constexpr int kChunk = 32;

    void prepare (double sampleRate)
    {
        fsOS = (float) sampleRate * 4.0f;
        os.reset();
        os.initProcessing ((size_t) kChunk);
        gate.prepare (sampleRate);
        for (auto& c : channels)
            c.prepare (fsOS);
        for (auto* s : { &smBefore, &smAfter, &smOut, &smHi, &smMode, &smOn, &smTrim, &smOct, &smOctUp, &smBass, &smMid, &smTreble })
            s->reset (sampleRate, 0.03);
        smIn.reset (sampleRate, 0.03);
        snap = true;
    }

    void reset()
    {
        os.reset();
        gate.reset();
        for (auto& c : channels) c.reset();
        snap = true;
    }

    int getLatencySamples() const { return (int) std::lround (os.getLatencyInSamples()) + gate.getLatencySamples(); }

    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        const int numCh = std::min (buffer.getNumChannels(), 2);
        const int n = buffer.getNumSamples();
        if (numCh == 0 || n == 0) return;

        auto set = [this] (juce::SmoothedValue<float>& s, float v)
        {
            if (snap) s.setCurrentAndTargetValue (v); else s.setTargetValue (v);
        };
        set (smBefore, p.before); set (smAfter, p.after); set (smOut, p.output); set (smHi, p.hiCut);
        set (smMode, p.demhe ? 1.0f : 0.0f);
        set (smOn, p.on ? 1.0f : 0.0f);
        set (smIn, p.inputDb); set (smTrim, p.trimDb);
        set (smOct, p.octave); set (smOctUp, p.octUp ? 1.0f : 0.0f);
        set (smBass, p.bass); set (smMid, p.mid); set (smTreble, p.treble);
        snap = false;

        // input trim, then the gate (the threshold is measured after the trim; bypassed pedal = no gate)
        for (int i = 0; i < n; ++i)
        {
            const float g = std::pow (10.0f, smIn.getNextValue() / 20.0f);
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
        gate.process (buffer, numCh, p.on ? p.gateDb : NoiseGate::kOffDb);

        for (int pos = 0; pos < n; pos += kChunk)
        {
            const int len = std::min (kChunk, n - pos);

            Coeffs c;
            const float before = smBefore.skip (len), after = smAfter.skip (len);
            const float out = smOut.skip (len), hi = smHi.skip (len);
            c.onMix = smOn.skip (len);
            c.modeMix = smMode.skip (len);
            c.octAmt = smOct.skip (len);
            c.octUpMix = smOctUp.skip (len);

            c.g1 = std::pow (10.0f, before * 54.0f / 20.0f);                 // 0 .. +54 dB
            const float fc1 = std::clamp (300.0e3f / c.g1, 3000.0f, 0.45f * fsOS);
            c.fc1a = 1.0f - std::exp (-kTwoPi * fc1 / fsOS);
            c.bias = after * 0.55f;
            c.neg = 1.0f - 0.6f * after;
            c.sag = after * 0.35f;
            c.rest = -c.neg * std::tanh (8.0f * c.bias / c.neg);   // stage 2 output at rest (no signal, no sag)
            const float lv = out * 2.0f;
            c.level = lv * lv * 0.5f * std::pow (10.0f, smTrim.skip (len) / 20.0f);
            const float fcHi = std::min (14000.0f * std::pow (1000.0f / 14000.0f, hi), 0.45f * fsOS);
            c.hiA = 1.0f - std::exp (-kTwoPi * fcHi / fsOS);

            // tone controls: +-12 dB around the middle position
            const double bassDb = (smBass.skip (len) - 0.5f) * 24.0, midDb = (smMid.skip (len) - 0.5f) * 24.0,
                         trebDb = (smTreble.skip (len) - 0.5f) * 24.0;
            c.bass   = BiquadCoef::lowShelf (fsOS, 100.0, bassDb);
            c.mid    = BiquadCoef::peak (fsOS, 750.0, 0.8, midDb);
            c.treble = BiquadCoef::highShelf (fsOS, 3500.0, trebDb);

            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) pos, (size_t) len);
            auto up = os.processSamplesUp (block);
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = up.getChannelPointer ((size_t) ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    d[i] = channels[(size_t) ch].process (d[i], c);
            }
            os.processSamplesDown (block);
        }
    }

private:
    float fsOS = 176400.0f;
    bool snap = true;
    std::array<Channel, 2> channels;
    NoiseGate gate;
    juce::dsp::Oversampling<float> os { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    juce::SmoothedValue<float> smBefore, smAfter, smOut, smHi, smMode, smOn, smTrim, smOct, smOctUp, smBass, smMid, smTreble;
    juce::SmoothedValue<float> smIn;
};
} // namespace carcosa
