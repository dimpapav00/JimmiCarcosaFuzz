#pragma once
// Input noise gate shared by all the pedal sims.
//
// Why it exists: a distortion pedal amplifies everything at its input by 20-40 dB, including the hiss of
// the audio interface and pickup/cable hum. A real player puts a gate in front of the pedal (or in the
// effects loop); this does the same inside the plugin.
//
// Design: peak detector shared by both channels (so the stereo image never moves), 1 ms lookahead so the
// pick attack is never clipped, hysteresis (closes 4 dB below the opening level), 50 ms hold and a slow
// 80 ms release so note tails fade out instead of being chopped. When closed it attenuates by 50 dB
// rather than muting completely.

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

class NoiseGate
{
public:
    static constexpr float kOffDb = -79.9f; // at or below this threshold the gate is bypassed

    void prepare (double sampleRate)
    {
        const float sr = (float) sampleRate;
        look = std::max (1, (int) std::lround (sampleRate * 0.001));
        for (auto& d : delay)
            d.assign ((size_t) look, 0.0f);
        holdMax  = (int) std::lround (sampleRate * 0.05);
        relCoef  = std::exp (-1.0f / (0.020f * sr));
        gainUp   = 1.0f - std::exp (-1.0f / (0.0005f * sr));
        gainDown = 1.0f - std::exp (-1.0f / (0.080f * sr));
        reset();
    }

    void reset()
    {
        for (auto& d : delay)
            std::fill (d.begin(), d.end(), 0.0f);
        pos = 0;
        env = 0.0f;
        hold = 0;
        open = false;
        gain = kFloor;
    }

    int getLatencySamples() const { return look; }

    void process (juce::AudioBuffer<float>& buffer, int numCh, float thresholdDb)
    {
        const int n = buffer.getNumSamples();
        const bool enabled = thresholdDb > kOffDb;
        const float thrOpen  = std::pow (10.0f, thresholdDb / 20.0f);
        const float thrClose = std::pow (10.0f, (thresholdDb - 4.0f) / 20.0f);

        float* ptr[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };

        for (int i = 0; i < n; ++i)
        {
            float det = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                det = std::max (det, std::fabs (ptr[ch][i]));

            env = det > env ? det : env * relCoef;
            if (env < 1.0e-9f) env = 0.0f;

            if (! enabled)
            {
                open = true;
            }
            else if (env > thrOpen)
            {
                open = true;
                hold = holdMax;
            }
            else if (open)
            {
                if (env > thrClose)
                    hold = holdMax;
                else if (--hold <= 0)
                    open = false;
            }

            const float target = open ? 1.0f : kFloor;
            gain += (target > gain ? gainUp : gainDown) * (target - gain);

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto& d = delay[(size_t) ch];
                const float out = d[(size_t) pos];
                d[(size_t) pos] = ptr[ch][i];
                ptr[ch][i] = out * gain;
            }
            if (++pos >= look) pos = 0;
        }
    }

private:
    static constexpr float kFloor = 0.0031623f; // -50 dB
    std::array<std::vector<float>, 2> delay;
    int look = 48, pos = 0, holdMax = 2400, hold = 0;
    float env = 0.0f, gain = kFloor, relCoef = 0.999f, gainUp = 0.04f, gainDown = 0.0003f;
    bool open = false;
};
