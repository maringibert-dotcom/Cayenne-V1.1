#pragma once
// SatLab - un "slot" de saturation complet (header-only, sans JUCE).
//
// Chaine d'un slot (par canal) :
//   [mode stereo L/R/M/S] -> [Range : bande traitee, le reste passe intact]
//   -> Focus (pre-emphase) -> Sag (compression de l'alim) -> Drive/Asym/Bias
//   -> SATURATION (25 types)
//   -> Harshness (balance HF des produits de distorsion)
//   -> Warmth (harmoniques paires dans les graves-mediums)
//   -> De-emphasis -> bloqueur DC -> recombinaison avec la partie non traitee
//   -> EQ Tone (low shelf / cloche / high shelf) -> Auto gain -> Output -> Mix
#include "Filters.h"
#include "SatTypes.h"
#include <vector>
#include <array>
#include <cstdint>

namespace satlab
{
struct SlotParams
{
    bool  on = false;
    int   type = Tanh;
    float rangeLo = 20.0f, rangeHi = 20000.0f;
    int   stereoMode = 0;              // 0 Stereo, 1 Mid, 2 Side, 3 Left, 4 Right
    float driveDb = 12.0f, bias = 0.0f, asym = 0.0f;
    float focusFreq = 1000.0f, focusGainDb = 0.0f, focusQ = 1.0f, deemph = 0.0f;
    float sag = 0.0f, sagTimeMs = 80.0f;
    float s[4] = { 0.5f, 0.5f, 0.5f, 0.5f };
    float warmth = 0.0f, warmFreq = 800.0f;
    float harsh = 0.0f, harshFreq = 3000.0f;
    float lsFreq = 120.0f, lsGainDb = 0.0f;
    float midFreq = 1000.0f, midGainDb = 0.0f, midQ = 1.0f;
    float hsFreq = 8000.0f, hsGainDb = 0.0f;
    float mix = 1.0f, outDb = 0.0f;
    bool  autoGain = false;
    float pan = 0.0f, width = 1.0f;       // pan (balance) -1..1 ; largeur stereo 0..2
};

//------------------------------------------------------------------------------
// Modele d'hysteresis magnetique de Jiles-Atherton (integration RK2 en H)
struct Hysteresis
{
    float M = 0, Hprev = 0;
    float Ms = 1.0f, a = 0.22f, alpha = 1.6e-3f, k = 0.3f, c = 0.17f;

    static float langevin (float x)
    {
        const float ax = std::abs (x);
        if (ax < 0.1f) { const float x2 = x * x; return x * (1.0f / 3.0f - x2 / 45.0f + 2.0f * x2 * x2 / 945.0f); }
        if (ax > 15.0f) return (x > 0 ? 1.0f : -1.0f) - 1.0f / x;
        return 1.0f / std::tanh (x) - 1.0f / x;
    }
    static float langevinD (float x)
    {
        const float ax = std::abs (x);
        if (ax < 0.1f) { const float x2 = x * x; return 1.0f / 3.0f - x2 / 15.0f + 2.0f * x2 * x2 / 189.0f; }
        if (ax > 15.0f) return 1.0f / (x * x);
        const float t = std::tanh (x);
        return 1.0f / (x * x) - (1.0f - t * t) / (t * t);
    }
    float dMdH (float Mv, float H, float delta) const
    {
        const float Q = (H + alpha * Mv) / a;
        const float diff = Ms * langevin (Q) - Mv;
        const float deltaM = ((delta > 0) == (diff > 0)) ? 1.0f : 0.0f;
        const float Lp = langevinD (Q);
        float den = (1.0f - c) * delta * k - alpha * diff;
        if (std::abs (den) < 1e-6f) den = (den < 0 ? -1e-6f : 1e-6f);
        const float f1 = (1.0f - c) * deltaM * diff / den;
        const float f2 = Ms / a * c * Lp;
        const float f3 = 1.0f - Ms / a * alpha * c * Lp;
        return (f1 + f2) / f3;
    }
    void reset() { M = 0; Hprev = 0; }
    float process (float H)
    {
        const float dH = H - Hprev;
        if (dH == 0.0f) return M;
        const float delta = dH > 0 ? 1.0f : -1.0f;
        const float k1 = dMdH (M, Hprev, delta);
        const float k2 = dMdH (M + 0.5f * dH * k1, 0.5f * (Hprev + H), delta);
        M += dH * k2;
        M = clampf (M, -1.2f * Ms, 1.2f * Ms);
        if (! std::isfinite (M)) M = 0;
        Hprev = H;
        return M;
    }
};

//------------------------------------------------------------------------------
class SatSlot
{
public:
    void prepare (float osSampleRate, int osFactor_)
    {
        fs = osSampleRate; osFactor = std::max (1, osFactor_);
        const int dl = (int) std::ceil (0.006f * fs) + 8;
        for (auto& c : ch) { c.delay.assign ((size_t) dl, 0.0f); }
        delayMask = dl;
        agCoef = 1.0f - std::exp (-1.0f / (0.25f * fs));
        slewScale = 1.0f / fs;
        hystFsK = fs / 60.0f;
        for (auto* sm : { &smDrive, &smBias, &smAsym, &smMix, &smOut, &smAg, &smPan, &smWidth, &smS[0], &smS[1], &smS[2], &smS[3] })
            sm->setTime (12.0f, fs);
        smAg.setTime (60.0f, fs);
        reset();
        dirty = true;
    }

    void reset()
    {
        for (auto& c : ch)
        {
            c.hp.reset(); c.lp.reset();
            c.focus.reset(); c.deemph.reset(); c.ls.reset(); c.mid.reset(); c.hs.reset();
            c.harshHp.reset(); c.warmHp.reset(); c.warmLp.reset(); c.sagEnv.reset(); c.dc.reset();
            c.pA.reset(); c.pB.reset(); c.sA.reset(); c.sB.reset(); c.tapeEnv.reset();
            c.hyst.reset(); c.v = 0; c.v1 = 0; c.xPrev = 0; c.tapeLp = 0;
            c.hold = 0; c.phase = 1.0f; c.rng = 22222u + (uint32_t) (&c - ch);
            std::fill (c.delay.begin(), c.delay.end(), 0.0f);
            c.wpos = 0;
        }
        wowPhase = 0; flutPhase = 0; driftState = 0;
        agIn = agOut = 1e-6f;
    }

    // A appeler une fois par bloc (peu couteux)
    void setParams (const SlotParams& np)
    {
        p = np;
        smDrive.target = dbToLin (p.driveDb);
        smBias.target = p.bias;
        smAsym.target = p.asym;
        smMix.target = p.mix;
        smOut.target = dbToLin (p.outDb);
        smPan.target = p.pan; smWidth.target = p.width;
        for (int i = 0; i < 4; ++i) smS[i].target = p.s[i];
        if (! initialised) { snapSmoothers(); initialised = true; }
        if (coeffSignature (p) != lastSig) { lastSig = coeffSignature (p); dirty = true; }
    }

    bool isActive() const { return p.on; }

private:
    using Sig = std::array<float, 24>;
    static Sig coeffSignature (const SlotParams& q)
    {
        return { (float) q.type, (float) q.stereoMode, q.rangeLo, q.rangeHi, q.focusFreq, q.focusGainDb, q.focusQ, q.deemph,
                 q.sagTimeMs, q.warmFreq, q.harshFreq, q.lsFreq, q.lsGainDb, q.midFreq, q.midGainDb, q.midQ, q.hsFreq, q.hsGainDb, 0, 0, 0, 0, 0, 0 };
    }
    Sig lastSig {};

public:

    // Traite L/R en place (n echantillons a la frequence suréchantillonnée).
    void process (float* L, float* R, int n)
    {
        if (! p.on) return;
        int pos = 0;
        while (pos < n)
        {
            const int len = std::min (kChunk, n - pos);
            updateCoeffs();
            processChunk (L + pos, R + pos, len);
            pos += len;
        }
    }

private:
    static constexpr int kChunk = 16;

    struct Chan
    {
        LR4 hp, lp;
        Svf focus, deemph, ls, mid, hs, harshHp, sA, sB;
        OnePole warmHp, warmLp, pA, pB;
        EnvFollower sagEnv, tapeEnv;
        DcBlocker dc;
        Hysteresis hyst;
        float v = 0, v1 = 0, xPrev = 0, tapeLp = 0, hold = 0, phase = 1.0f;
        uint32_t rng = 1;
        std::vector<float> delay;
        int wpos = 0;
    };

    SlotParams p;
    Chan ch[2];
    float fs = 48000.0f;
    int   osFactor = 1;
    bool  dirty = true, initialised = false;

    Smoother smDrive, smBias, smAsym, smMix, smOut, smAg, smPan, smWidth, smS[4];
    float agIn = 1e-6f, agOut = 1e-6f, agCoef = 0.001f;
    float wowPhase = 0, flutPhase = 0, driftState = 0;
    int   delayMask = 1;
    float slewScale = 1.0f, hystFsK = 800.0f;

    // coefficients derives (recalcules tous les 16 echantillons)
    bool  rangeOn = false;
    float d0 = 0, d1 = 0, d2 = 0, d3 = 0, d4 = 0, d5 = 0, d6 = 0, d7 = 0;
    bool  postLpOn = false;
    float holdInc = 1.0f, bitsL = 512.0f, dither = 0, jitter = 0;
    float wowDepth = 0;
    float diodeP[5] = { 0 }, diodeN[5] = { 0 };   // vf, vs, ivs, cr, vmax
    bool  procCh[2] = { true, true };

    void snapSmoothers()
    {
        smDrive.snap (smDrive.target); smBias.snap (smBias.target); smAsym.snap (smAsym.target);
        smMix.snap (smMix.target); smOut.snap (smOut.target); smPan.snap (smPan.target); smWidth.snap (smWidth.target);
        for (int i = 0; i < 4; ++i) smS[i].snap (smS[i].target);
        smAg.snap (1.0f);
    }

    static float pnorm (float x, float k, float ik)
    {
        const float ax = std::abs (x);
        return x / std::pow (1.0f + std::pow (ax, k), ik);
    }
    static float atanN (float x) { return 0.63661977f * std::atan (1.57079633f * x); }
    static float expClip (float x, float vf)
    {
        const float ax = std::abs (x);
        const float y = vf * (1.0f - std::exp (-ax / vf));
        return x < 0 ? -y : y;
    }
    static float triode (float u, float cp, float cn, float bloom)
    {
        float y = u >= 0 ? cp * std::tanh (u / cp) : cn * std::tanh (u / cn);
        if (bloom > 0.0f) { const float t = std::tanh (u); y += bloom * 0.3f * t * t; }
        return y;
    }
    static float classDead (float u, float dz)
    {
        if (dz < 1e-4f) return u;
        return u - dz * std::tanh (u / dz);
    }
    static float rnd01 (uint32_t& s) { s = s * 1664525u + 1013904223u; return (float) (s >> 8) * (1.0f / 16777216.0f); }

    void setDiode (float* d, float vf, float vs, float r)
    {
        d[0] = vf; d[1] = vs; d[2] = 1.0f / vs;
        d[3] = r / std::sinh (std::min (vf / vs, 40.0f));
        d[4] = vf * 1.5f;
    }

    void updateCoeffs()
    {
        if (dirty)
        {
            dirty = false;
            // Range
            const float hiMax = std::min (19500.0f, 0.45f * fs);
            const bool hpOn = p.rangeLo > 24.0f, lpOn = p.rangeHi < hiMax;
            rangeOn = hpOn || lpOn;
            procCh[0] = procCh[1] = true;
            switch (p.stereoMode) { case 1: procCh[1] = false; break; case 2: procCh[0] = false; break;
                                    case 3: procCh[1] = false; break; case 4: procCh[0] = false; break; default: break; }
            for (auto& c : ch)
            {
                if (hpOn) c.hp.setHighpass (p.rangeLo, fs); else { c.hp.a.bypass = true; c.hp.b.bypass = true; }
                if (lpOn) c.lp.setLowpass (p.rangeHi, fs);  else { c.lp.a.bypass = true; c.lp.b.bypass = true; }
                c.focus.setBell (p.focusFreq, p.focusQ, p.focusGainDb, fs);
                c.deemph.setBell (p.focusFreq, p.focusQ, -p.focusGainDb * p.deemph, fs);
                c.ls.setLowShelf (p.lsFreq, 0.7071f, p.lsGainDb, fs);
                c.mid.setBell (p.midFreq, p.midQ, p.midGainDb, fs);
                c.hs.setHighShelf (p.hsFreq, 0.7071f, p.hsGainDb, fs);
                c.harshHp.setHighpass (p.harshFreq, 0.7071f, fs);
                c.warmHp.setFreq (25.0f, fs);
                c.warmLp.setFreq (p.warmFreq, fs);
                c.sagEnv.setTimes (0.5f, p.sagTimeMs, fs);
                c.dc.prepare (fs, 8.0f);
            }
        }

        // coefficients dependants des reglages specifiques (suivent le lissage)
        const float a = smS[0].v, b = smS[1].v, c = smS[2].v, d = smS[3].v;
        const float nyq = 0.45f * fs;
        auto freqMap = [&] (float v, float lo, float hi) { return std::min (lo * std::pow (hi / lo, v), nyq); };
        postLpOn = false;
        switch (p.type)
        {
            case PNorm:       d0 = 1.5f + 14.0f * a; d1 = 1.0f / d0; break;
            case DigitalClip: d0 = std::max (0.002f, a); break;
            case Exponential: d0 = 1.0f + 2.0f * a; break;
            case Triode:
                d0 = 0.8f + 1.7f * a; d1 = 1.0f - 0.7f * b; d2 = d;
                for (auto& s : ch) s.pA.setFreq (freqMap (c, 1000.0f, 30000.0f), fs);
                postLpOn = c < 0.98f; break;
            case Pentode:
                d0 = 0.35f * a; d1 = 1.5f + 8.0f * b; d2 = 1.0f / d1;
                for (auto& s : ch) s.pA.setFreq (freqMap (c, 1000.0f, 30000.0f), fs);
                postLpOn = c < 0.98f; break;
            case Transformer:
                for (auto& s : ch) { s.pA.setFreq (freqMap (a, 20.0f, 400.0f), fs); s.pB.setFreq (freqMap (d, 1000.0f, 30000.0f), fs); }
                d0 = 1.0f + 5.0f * b; d1 = c; postLpOn = d < 0.98f; break;
            case TapeHysteresis:
            case TapeClassic:
            {
                const float bump = (p.type == TapeHysteresis) ? b : c;
                const float bumpHz = freqMap (bump, 30.0f, 240.0f);
                for (auto& s : ch)
                {
                    s.sA.setBell (bumpHz, 1.1f, 2.5f, fs);
                    if (p.type == TapeHysteresis) s.sB.setLowpass (freqMap (c, 1000.0f, 24000.0f), 0.7071f, fs);
                }
                const float flut = d;
                wowDepth = flut;
                if (p.type == TapeHysteresis)
                {
                    const float k = 0.02f + 0.9f * a * a + 0.05f * a;
                    for (auto& s : ch) { s.hyst.k = k; }
                }
                d0 = a; d1 = b;
                break;
            }
            case DiodeClipper:
            {
                const float vf = 0.2f + 1.1f * a;
                const float vs = vf * (0.03f + 0.25f * b);
                const float r  = vf * (0.1f + 1.4f * d);
                setDiode (diodeP, vf, vs, r);
                const float vfN = vf * (1.0f + c);
                setDiode (diodeN, vfN, vfN * (0.03f + 0.25f * b), vfN * (0.1f + 1.4f * d));
                break;
            }
            case GermaniumFuzz: d0 = 1.0f + 20.0f * a; d1 = 0.25f * b; d2 = 0.4f * c; d3 = d; break;
            case MuffFuzz:      d0 = 1.0f + 25.0f * a; d1 = 0.3f + 0.9f * b; d2 = d1 * (0.5f + c); break;
            case Overdrive:
                for (auto& s : ch) { s.pA.setFreq (freqMap (a, 100.0f, 2000.0f), fs); s.pB.setFreq (freqMap (d, 1000.0f, 20000.0f), fs); }
                d0 = 1.5f + 6.0f * (1.0f - b); d1 = 1.0f / d0; d2 = c; postLpOn = d < 0.98f; break;
            case Rectifier:
                for (auto& s : ch) s.pA.setFreq (freqMap (d, 500.0f, 20000.0f), fs);
                d0 = a; d1 = b; d2 = 0.003f + 0.4f * c * c; postLpOn = d < 0.98f; break;
            case ClassB: d0 = 0.5f * a; d1 = 1.5f + 6.0f * b; d2 = c; break;
            case Bitcrusher:
            {
                const float bits = 2.0f + 14.0f * a;
                bitsL = std::pow (2.0f, bits - 1.0f);
                const float N = 1.0f + 63.0f * b * b;
                holdInc = 1.0f / (N * (float) osFactor);
                dither = c; jitter = d; break;
            }
            case Harmonics: d0 = a * 0.5f; d1 = b * 0.5f; d2 = c * 0.5f; d3 = d * 0.5f; break;
            case Wavefolder:
                d0 = a;
                for (auto& s : ch) s.pA.setFreq (freqMap (b, 1000.0f, 20000.0f), fs);
                postLpOn = b < 0.98f; break;
            case SlewLimiter:
                d0 = 2.0f * kPi * std::min (200.0f * std::pow (200.0f, a), 40000.0f) / fs;   // pas max/echantillon (amplitude 1)
                d1 = b; d2 = c; d3 = d; break;
            case Jfet:
                d0 = 3.0f * std::pow (0.1f, a); d1 = 1.5f * (1.0f - 0.8f * b);
                for (auto& s : ch) s.pA.setFreq (freqMap (c, 1000.0f, 30000.0f), fs);
                postLpOn = c < 0.98f; break;
            case Console:
                for (auto& s : ch) s.pA.setFreq (freqMap (a, 20.0f, 400.0f), fs);
                d0 = 1.0f + 5.0f * b; d1 = 0.5f * c;
                d2 = 2.0f * kPi * std::min (2000.0f * std::pow (20.0f, d), 40000.0f) / fs; break;
            case TubeAmp:
                for (auto& s : ch) s.pA.setFreq (freqMap (b, 20.0f, 500.0f), fs);
                d0 = 1.0f + 15.0f * a; d1 = 1.0f - 0.7f * c; d2 = 0.25f * d; break;
            default: break;
        }
    }

    //--------------------------------------------------------------------------
    inline float shape (Chan& c, float z, float lfoDelay)
    {
        float y = z;
        switch (p.type)
        {
            case Clean: break;
            case Tanh: y = std::tanh (z); break;
            case Arctan: y = atanN (z); break;
            case PNorm: y = pnorm (z, d0, d1); break;
            case DigitalClip:
            {
                const float w = d0, az = std::abs (z);
                float m;
                if (az <= 1.0f - w) m = az;
                else if (az < 1.0f + w) { const float t = az - (1.0f - w); m = az - t * t / (4.0f * w); }
                else m = 1.0f;
                y = z < 0 ? -m : m; break;
            }
            case SineShaper: y = std::sin (clampf (z, -1.57079633f, 1.57079633f)); break;
            case Exponential:
            {
                const float az = std::abs (z);
                const float m = 1.0f - std::exp (-std::pow (az, d0));
                y = z < 0 ? -m : m; break;
            }
            case Triode:
            {
                const float u = postLpOn ? c.pA.lp (z) : z;
                y = triode (u, d1, d0, d2); break;
            }
            case Pentode:
            {
                const float y0 = classDead (z, d0);
                y = pnorm (y0, d1, d2);
                if (postLpOn) y = c.pA.lp (y);
                break;
            }
            case Transformer: y = xfmr (c, z, d0, d1);
                if (postLpOn) y = c.pB.lp (y);
                break;
            case TapeHysteresis:
            {
                y = c.hyst.process (z) * 1.6f;      // x1.6 : compense en partie la perte de gain petit-signal
                y = wowFlutter (c, y, lfoDelay);
                y = c.sA.process (y);
                y = c.sB.process (y);
                break;
            }
            case TapeClassic:
            {
                float t = (1.0f - d0) * std::tanh (z) + d0 * atanN (z);
                const float env = c.tapeEnv.process (t);
                const float fc = std::min (0.45f * fs, 18000.0f) * (1.0f - d1 * std::min (env, 1.0f) * 0.9f);
                const float co = 1.0f - std::exp (-2.0f * kPi * fc / fs);
                c.tapeLp += co * (t - c.tapeLp);
                y = wowFlutter (c, c.tapeLp, lfoDelay);
                y = c.sA.process (y);
                break;
            }
            case DiodeClipper: y = diode (c, z); break;
            case GermaniumFuzz:
            {
                const float b0 = d2;
                float a = std::tanh (z + b0) - std::tanh (b0);
                const float a2 = a * a;
                a *= a2 / (a2 + d1 * d1 + 1e-12f);
                const float u = a * d0;
                y = u > 0 ? std::tanh (u * 0.7f) / 0.7f : std::tanh (u * 1.4f) / 1.4f;
                y += d3 * 0.6f * std::abs (y);
                break;
            }
            case MuffFuzz:
            {
                const float A = expClip (z, d2);
                y = expClip (A * d0, d1) / d1; break;
            }
            case Overdrive:
            {
                const float hp = c.pA.hp (z);
                const float lo = z - hp;
                float cl = pnorm (hp, d0, d1);
                if (postLpOn) cl = c.pB.lp (cl);
                y = d2 * lo + cl; break;
            }
            case Rectifier:
            {
                const float sabs = std::sqrt (z * z + d2 * d2) - d2;
                const float relu = 0.5f * (z + sabs);
                const float w0 = std::max (0.0f, 1.0f - d0 - d1);
                y = w0 * z + d0 * relu * 1.0f + d1 * sabs;
                y = 2.0f * std::tanh (0.5f * y);
                if (postLpOn) y = c.pA.lp (y);
                break;
            }
            case ClassB:
            {
                const float dz = z > 0 ? d0 * (1.0f + d2) : d0 * (1.0f - d2);
                float y0 = z;
                if (dz > 1e-4f)
                {
                    const float az = std::abs (z);
                    const float pa = std::pow (az, d1), pd = std::pow (dz, d1);
                    y0 = z * pa / (pa + pd + 1e-20f);
                }
                y = std::tanh (y0); break;
            }
            case Bitcrusher:
            {
                c.phase += holdInc * (jitter > 0 ? 1.0f + jitter * (rnd01 (c.rng) - 0.5f) * 2.0f : 1.0f);
                if (c.phase >= 1.0f)
                {
                    c.phase -= 1.0f; if (c.phase > 1.0f) c.phase = 0;
                    const float u = clampf (z, -1.0f, 1.0f);
                    const float nz = dither > 0 ? (rnd01 (c.rng) - rnd01 (c.rng)) * dither : 0.0f;
                    c.hold = std::floor ((u + nz / bitsL) * bitsL + 0.5f) / bitsL;
                }
                y = c.hold; break;
            }
            case Harmonics:
            {
                const float x = std::tanh (z), x2 = x * x, x3 = x2 * x, x4 = x2 * x2, x5 = x4 * x;
                y = x + d0 * (2.0f * x2) + d1 * (4.0f * x3) + d2 * (8.0f * x4 - 8.0f * x2) + d3 * (16.0f * x5 - 20.0f * x3);
                break;
            }
            case Wavefolder:
            {
                const float s = std::sin (z * 1.57079633f);
                float t = z + 1.0f; t -= 4.0f * std::floor (t * 0.25f);
                const float tri = 1.0f - std::abs (t - 2.0f);
                y = (1.0f - d0) * s + d0 * tri;
                if (postLpOn) y = c.pA.lp (y);
                break;
            }
            case SlewLimiter:
            {
                const float dd = z - c.v1;
                float lim = d0 * (dd > 0 ? (1.0f + d1) : (1.0f - 0.9f * d1));
                lim = std::max (lim, 1e-6f);
                const float hard = clampf (dd, -lim, lim);
                const float soft = lim * std::tanh (dd / lim);
                c.v1 += (1.0f - d2) * hard + d2 * soft;
                y = c.v1;
                if (d3 > 0.001f) { const float r = 0.5f + 4.0f * (1.0f - d3); y = r * std::tanh (y / r); }
                break;
            }
            case Jfet:
            {
                const float u = postLpOn ? c.pA.lp (z) : z;
                const float vp = d0;
                const float yy = u >= -vp ? u + u * u / (2.0f * vp) : -0.5f * vp;
                y = d1 * std::tanh (yy / d1); break;
            }
            case Console:
            {
                const float t = xfmr (c, z, d0, 0.2f);
                const float b0 = d1;
                const float ya = std::tanh (t + b0) - std::tanh (b0);
                const float dd = ya - c.v1;
                c.v1 += d2 * std::tanh (dd / d2);
                y = c.v1; break;
            }
            case TubeAmp:
            {
                float s1 = triode (z, 0.8f, 1.6f, 0.2f);
                s1 = c.pA.hp (s1);
                const float s2 = triode (s1 * d0, d1, 1.4f, 0.15f);
                y = classDead (s2, d2); break;
            }
            default: break;
        }
        return y;
    }

    inline float xfmr (Chan& c, float z, float kc, float hyst)
    {
        const float lp = c.pA.lp (z);
        const float hf = z - lp;
        const float dl = lp - c.xPrev; c.xPrev = lp;
        const float flux = lp + hyst * 0.3f * std::tanh (dl * hystFsK);
        return hf / (1.0f + 0.15f * std::abs (hf)) + std::tanh (kc * flux) / kc;
    }

    inline float diode (Chan& c, float z)
    {
        const float* d = z >= 0 ? diodeP : diodeN;
        float v = clampf (c.v, -d[4], d[4]);
        for (int i = 0; i < 4; ++i)
        {
            const float e = std::exp (clampf (v * d[2], -40.0f, 40.0f)), ie = 1.0f / e;
            const float sh = 0.5f * (e - ie), ch_ = 0.5f * (e + ie);
            const float f = z - v - d[3] * sh;
            const float fp = -1.0f - d[3] * d[2] * ch_;
            v = clampf (v - f / fp, -d[4], d[4]);
        }
        c.v = v;
        return v;
    }

    inline float wowFlutter (Chan& c, float x, float lfoDelay)
    {
        const int size = (int) c.delay.size();
        c.delay[(size_t) c.wpos] = x;
        float out = x;
        if (wowDepth > 0.001f)
        {
            const float rp = (float) c.wpos - lfoDelay;
            const float rpw = rp < 0 ? rp + (float) size : rp;
            const int i0 = (int) rpw; const float fr = rpw - (float) i0;
            const int i1 = (i0 + 1) % size;
            out = c.delay[(size_t) (i0 % size)] * (1.0f - fr) + c.delay[(size_t) i1] * fr;
        }
        c.wpos = (c.wpos + 1) % size;
        return out;
    }

    //--------------------------------------------------------------------------
    void processChunk (float* L, float* R, int n)
    {
        const bool tape = (p.type == TapeHysteresis || p.type == TapeClassic);
        const bool useSag = p.sag > 0.001f;
        const bool useHarsh = std::abs (p.harsh) > 0.001f;
        const bool useWarm = p.warmth > 0.001f;
        const float harshK = p.harsh >= 0 ? p.harsh * 3.0f : p.harsh;
        const float asymK = 0.7f;

        for (int i = 0; i < n; ++i)
        {
            const float drive = smDrive.next(), bias = smBias.next(), asym = smAsym.next();
            const float mix = smMix.next(), outg = smOut.next();
            for (auto& s : smS) s.next();
            const float agGain = p.autoGain ? smAg.next() : 1.0f;

            // LFO wow/flutter (partage entre canaux)
            float lfoDelay = 0;
            if (tape && wowDepth > 0.001f)
            {
                wowPhase += 0.6f / fs; if (wowPhase > 1.0f) wowPhase -= 1.0f;
                flutPhase += 7.3f / fs; if (flutPhase > 1.0f) flutPhase -= 1.0f;
                const float m = std::sin (2.0f * kPi * wowPhase) * 0.5f + std::sin (2.0f * kPi * flutPhase) * 0.1f;
                lfoDelay = (0.00004f + wowDepth * 0.0006f * (1.0f + m)) * fs;   // pas de retard fixe : pas de peigne avec le dry
            }

            float in[2] = { L[i], R[i] };
            float x[2] = { in[0], in[1] };
            if (p.stereoMode == 1 || p.stereoMode == 2)
            {
                x[0] = 0.5f * (in[0] + in[1]);      // Mid
                x[1] = 0.5f * (in[0] - in[1]);      // Side
            }
            const float dry[2] = { x[0], x[1] };
            float wet[2] = { x[0], x[1] };

            for (int k = 0; k < 2; ++k)
            {
                if (! procCh[k]) continue;
                Chan& c = ch[k];
                float src = x[k], rest = 0.0f;
                if (rangeOn)
                {
                    const float bp = c.lp.process (c.hp.process (x[k]));
                    rest = x[k] - bp; src = bp;
                }
                const float f = c.focus.process (src);
                float z0 = f * drive;
                float eff = 1.0f;
                if (useSag)
                {
                    const float e = c.sagEnv.process (z0);
                    eff = 1.0f - 0.8f * p.sag * (e / (1.0f + e));
                }
                float z = z0 * eff;
                z *= (z > 0 ? 1.0f + asymK * asym : 1.0f - asymK * asym);
                z += bias * 1.2f;

                float y = shape (c, z, lfoDelay);

                if (useHarsh)
                {
                    const float r = y - z;
                    y += harshK * c.harshHp.process (r);
                }
                if (useWarm)
                {
                    const float ev = c.warmLp.lp (c.warmHp.hp (std::abs (y)));
                    y += p.warmth * 0.8f * ev;
                }
                y = c.deemph.process (y);
                y = c.dc.process (y);
                y = sanitize (clampf (y, -64.0f, 64.0f));
                wet[k] = rest + y;
            }

            // EQ de tonalite sur le signal traite
            for (int k = 0; k < 2; ++k)
            {
                if (! procCh[k]) continue;
                Chan& c = ch[k];
                wet[k] = c.hs.process (c.mid.process (c.ls.process (wet[k])));
            }

            // auto gain (mesure sur les canaux traites)
            if (p.autoGain)
            {
                float pi = 0, po = 0; int cnt = 0;
                for (int k = 0; k < 2; ++k) if (procCh[k]) { pi += dry[k] * dry[k]; po += wet[k] * wet[k]; ++cnt; }
                if (cnt) { agIn += agCoef * (pi / cnt - agIn); agOut += agCoef * (po / cnt - agOut); }
                if ((i & 7) == 0)
                    smAg.target = clampf (std::sqrt ((agIn + 1e-9f) / (agOut + 1e-9f)), 0.03f, 30.0f);
            }

            float o[2];
            for (int k = 0; k < 2; ++k)
            {
                if (! procCh[k]) { o[k] = dry[k]; continue; }
                const float w = wet[k] * agGain * outg;
                o[k] = dry[k] + mix * (w - dry[k]);
            }
            float outL, outR;
            if (p.stereoMode == 1 || p.stereoMode == 2) { outL = o[0] + o[1]; outR = o[0] - o[1]; }
            else { outL = o[0]; outR = o[1]; }

            // largeur (M/S) puis pan (balance : le canal oppose est attenue, centre = unite)
            const float wd = smWidth.next(), pn = smPan.next();
            if (wd != 1.0f)
            {
                const float m = 0.5f * (outL + outR), sd = 0.5f * (outL - outR) * wd;
                outL = m + sd; outR = m - sd;
            }
            if (pn != 0.0f)
            {
                outL *= pn > 0.0f ? 1.0f - pn : 1.0f;
                outR *= pn < 0.0f ? 1.0f + pn : 1.0f;
            }
            L[i] = outL; R[i] = outR;
        }
    }
};
} // namespace satlab
