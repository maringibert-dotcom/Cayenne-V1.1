#pragma once
// SatLab - filtres de base (header-only, sans JUCE)
// Tous les filtres sont de type TPT / "trapezoidal" (Zavalishin / Cytomic) :
// stables et sans "zipper noise" quand on les modulera ou automatise.
#include <cmath>
#include <algorithm>

namespace satlab
{
constexpr float kPi = 3.14159265358979323846f;

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float dbToLin (float db) { return std::pow (10.0f, db * 0.05f); }

// Evite les denormaux / NaN qui se propagent
inline float sanitize (float v)
{
    if (! std::isfinite (v)) return 0.0f;
    return v;
}

inline float tanTpt (float freq, float fs)
{
    const float f = clampf (freq, 1.0f, fs * 0.45f);
    return std::tan (kPi * f / fs);
}

//------------------------------------------------------------------------------
// Filtre 1 pole TPT (LP / HP)
struct OnePole
{
    float G = 0.0f, s = 0.0f;
    void setFreq (float f, float fs) { const float g = tanTpt (f, fs); G = g / (1.0f + g); }
    void reset() { s = 0.0f; }
    inline float lp (float x) { const float v = (x - s) * G; const float y = v + s; s = y + v; return y; }
    inline float hp (float x) { return x - lp (x); }
};

//------------------------------------------------------------------------------
// SVF TPT de Cytomic (A. Simper) - gère LP/HP/BP/cloche/shelf avec les memes etats
struct Svf
{
    float a1 = 0, a2 = 0, a3 = 0, m0 = 1, m1 = 0, m2 = 0;
    float ic1 = 0, ic2 = 0;
    bool  bypass = true;

    void reset() { ic1 = ic2 = 0.0f; }

    void setLowpass (float f, float q, float fs)
    {
        const float g = tanTpt (f, fs), k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        m0 = 0; m1 = 0; m2 = 1; bypass = false;
    }
    void setHighpass (float f, float q, float fs)
    {
        const float g = tanTpt (f, fs), k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        m0 = 1; m1 = -k; m2 = -1; bypass = false;
    }
    void setBell (float f, float q, float gainDb, float fs)
    {
        if (std::abs (gainDb) < 0.02f) { bypass = true; return; }
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float g = tanTpt (f, fs), k = 1.0f / (q * A);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        m0 = 1; m1 = k * (A * A - 1.0f); m2 = 0; bypass = false;
    }
    void setLowShelf (float f, float q, float gainDb, float fs)
    {
        if (std::abs (gainDb) < 0.02f) { bypass = true; return; }
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float g = tanTpt (f, fs) / std::sqrt (A), k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        m0 = 1; m1 = k * (A - 1.0f); m2 = A * A - 1.0f; bypass = false;
    }
    void setHighShelf (float f, float q, float gainDb, float fs)
    {
        if (std::abs (gainDb) < 0.02f) { bypass = true; return; }
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float g = tanTpt (f, fs) * std::sqrt (A), k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        m0 = A * A; m1 = k * (1.0f - A) * A; m2 = 1.0f - A * A; bypass = false;
    }

    inline float process (float v0)
    {
        if (bypass) return v0;
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return m0 * v0 + m1 * v1 + m2 * v2;
    }
};

//------------------------------------------------------------------------------
// Crossover Linkwitz-Riley 4 (2 Butterworth en cascade)
struct LR4
{
    Svf a, b;
    void reset() { a.reset(); b.reset(); }
    void setLowpass (float f, float fs)  { a.setLowpass (f, 0.70710678f, fs);  b.setLowpass (f, 0.70710678f, fs); }
    void setHighpass (float f, float fs) { a.setHighpass (f, 0.70710678f, fs); b.setHighpass (f, 0.70710678f, fs); }
    inline float process (float x) { return b.process (a.process (x)); }
};

//------------------------------------------------------------------------------
// Bloqueur de DC
struct DcBlocker
{
    OnePole hpf;
    void prepare (float fs, float f = 8.0f) { hpf.setFreq (f, fs); hpf.reset(); }
    void reset() { hpf.reset(); }
    inline float process (float x) { return hpf.hp (x); }
};

//------------------------------------------------------------------------------
// Suiveur d'enveloppe (attaque/relache en ms)
struct EnvFollower
{
    float env = 0, ca = 0, cr = 0;
    void setTimes (float attackMs, float releaseMs, float fs)
    {
        ca = 1.0f - std::exp (-1.0f / (0.001f * std::max (0.01f, attackMs) * fs));
        cr = 1.0f - std::exp (-1.0f / (0.001f * std::max (0.01f, releaseMs) * fs));
    }
    void reset() { env = 0; }
    inline float process (float x)
    {
        const float a = std::abs (x);
        env += (a > env ? ca : cr) * (a - env);
        return env;
    }
};

// Lissage 1 pole d'un parametre (par echantillon)
struct Smoother
{
    float v = 0, target = 0, c = 0.01f;
    void setTime (float ms, float fs) { c = 1.0f - std::exp (-1.0f / (0.001f * std::max (0.1f, ms) * fs)); }
    void snap (float t) { v = target = t; }
    inline float next() { v += c * (target - v); return v; }
};
} // namespace satlab
