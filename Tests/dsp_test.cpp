// Test autonome du moteur DSP : g++ -O2 -std=c++17 -I../Source/DSP dsp_test.cpp -o dsp_test
#include "SatEngine.h"
#include <cstdio>
#include <chrono>
#include <random>

using namespace satlab;

static double goertzel (const std::vector<float>& x, double f, double fs)
{
    const double w = 2.0 * 3.14159265358979 * f / fs; double re = 0, im = 0;
    for (size_t i = 0; i < x.size(); ++i) { re += x[i] * std::cos (w * (double) i); im += x[i] * std::sin (w * (double) i); }
    return 2.0 * std::sqrt (re * re + im * im) / (double) x.size();
}

int main()
{
    const float fs = 192000.0f; const int N = 16384 * 2; const double f0 = 1000.0;
    bool allOk = true;
    std::printf ("%-26s %7s %7s | %6s %6s %6s %6s %6s | %s\n", "type", "peak", "gain dB", "H1", "H2", "H3", "H4", "H5", "rt-load%");
    for (int t = 0; t < NumSatTypes; ++t)
    {
        SatEngine eng; eng.prepare (fs, 4, 4096);
        EngineParams ep; ep.slots[0].on = true; ep.slots[0].type = t; ep.slots[0].driveDb = 12.0f;
        const auto& ti = getTypeInfo (t);
        for (int i = 0; i < 4; ++i) ep.slots[0].s[i] = ti.def[i];
        eng.setParams (ep);
        std::vector<float> L (N), R (N), out; out.reserve (N);
        for (int i = 0; i < N; ++i) L[i] = R[i] = 0.25f * (float) std::sin (2.0 * 3.14159265358979 * f0 * i / fs);
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int pos = 0; pos < N; pos += 4096) eng.process (L.data() + pos, R.data() + pos, 4096);
        auto t1 = std::chrono::high_resolution_clock::now();
        const double sec = std::chrono::duration<double> (t1 - t0).count();
        const double load = 100.0 * sec / ((double) N / fs);
        std::vector<float> seg (L.begin() + N / 2, L.end());   // apres stabilisation
        float peak = 0; bool fin = true;
        for (float v : seg) { peak = std::max (peak, std::abs (v)); if (! std::isfinite (v)) fin = false; }
        const double h1 = goertzel (seg, f0, fs), h2 = goertzel (seg, 2 * f0, fs), h3 = goertzel (seg, 3 * f0, fs);
        const double h4 = goertzel (seg, 4 * f0, fs), h5 = goertzel (seg, 5 * f0, fs);
        auto db = [] (double v) { return 20.0 * std::log10 (v + 1e-12); };
        std::printf ("%-26s %7.3f %7.2f | %6.1f %6.1f %6.1f %6.1f %6.1f | %.1f %s\n", ti.name, peak, db (h1 / 0.25), db (h1), db (h2), db (h3), db (h4), db (h5), load, fin ? "" : "NON-FINI");
        if (! fin || peak > 20.0f) allOk = false;
    }

    // Test de stress : parametres aleatoires extremes, changements a chaque bloc
    std::mt19937 rng (1234); std::uniform_real_distribution<float> U (0.0f, 1.0f);
    float worst = 0; int bad = 0;
    for (int trial = 0; trial < 6; ++trial)
    {
        SatEngine eng; eng.prepare (96000.0f, 2, 2048);
        std::vector<float> L (2048), R (2048);
        for (int blk = 0; blk < 120; ++blk)
        {
            EngineParams ep; ep.parallel = (trial & 1); ep.mix = U (rng); ep.safetyClip = true;
            for (auto& s : ep.slots)
            {
                s.on = U (rng) > 0.2f; s.type = (int) (U (rng) * NumSatTypes) % NumSatTypes;
                s.rangeLo = 20.0f * std::pow (1000.0f, U (rng) * 0.9f); s.rangeHi = std::max (s.rangeLo * 1.2f, 20.0f * std::pow (1000.0f, 0.5f + U (rng) * 0.5f));
                s.stereoMode = (int) (U (rng) * 5) % 5;
                s.driveDb = U (rng) * 48.0f; s.bias = U (rng) * 2 - 1; s.asym = U (rng) * 2 - 1;
                s.focusFreq = 20.0f * std::pow (1000.0f, U (rng)); s.focusGainDb = (U (rng) * 2 - 1) * 18.0f; s.focusQ = 0.3f + U (rng) * 7.7f; s.deemph = U (rng);
                s.sag = U (rng); s.sagTimeMs = 5.0f + U (rng) * 495.0f;
                for (auto& v : s.s) v = U (rng);
                s.warmth = U (rng); s.warmFreq = 100.0f * std::pow (80.0f, U (rng));
                s.harsh = U (rng) * 2 - 1; s.harshFreq = 500.0f * std::pow (30.0f, U (rng));
                s.lsFreq = 30.0f * std::pow (30.0f, U (rng)); s.lsGainDb = (U (rng) * 2 - 1) * 18.0f;
                s.midFreq = 100.0f * std::pow (100.0f, U (rng)); s.midGainDb = (U (rng) * 2 - 1) * 18.0f; s.midQ = 0.2f + U (rng) * 9.8f;
                s.hsFreq = 1000.0f * std::pow (20.0f, U (rng)); s.hsGainDb = (U (rng) * 2 - 1) * 18.0f;
                s.mix = U (rng); s.outDb = (U (rng) * 2 - 1) * 12.0f; s.autoGain = U (rng) > 0.5f;
            }
            eng.setParams (ep);
            for (int i = 0; i < 2048; ++i)
            {
                const float ph = (float) (blk * 2048 + i);
                L[i] = 0.5f * std::sin (ph * 0.05f) + 0.2f * (U (rng) - 0.5f);
                R[i] = 0.5f * std::sin (ph * 0.051f) + 0.2f * (U (rng) - 0.5f);
            }
            eng.process (L.data(), R.data(), 2048);
            for (int i = 0; i < 2048; ++i)
            {
                if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) ++bad;
                worst = std::max ({ worst, std::abs (L[i]), std::abs (R[i]) });
            }
        }
    }
    std::printf ("\nStress: non-finis=%d, crete max=%.2f\n", bad, worst);
    if (bad > 0 || worst > 4.0f) allOk = false;
    std::printf (allOk ? "\nTOUT OK\n" : "\nPROBLEME\n");
    return allOk ? 0 : 1;
}
