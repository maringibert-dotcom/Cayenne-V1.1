#pragma once
// SatLab - moteur : 4 slots en serie ou en parallele + mix global + securite de sortie
#include "SatSlot.h"
#include <array>
#include <vector>

namespace satlab
{
constexpr int kNumSlots = 4;

struct EngineParams
{
    bool  parallel = false;
    float mix = 1.0f;
    bool  safetyClip = false;
    float width = 1.0f;          // largeur stereo globale 0..2
    std::array<SlotParams, kNumSlots> slots;
};

class SatEngine
{
public:
    void prepare (float osSampleRate, int osFactor, int maxOsBlock)
    {
        for (auto& s : slots) s.prepare (osSampleRate, osFactor);
        for (auto& v : tmp)  v.assign ((size_t) maxOsBlock, 0.0f);
        for (auto& v : acc)  v.assign ((size_t) maxOsBlock, 0.0f);
        for (auto& v : dryBuf) v.assign ((size_t) maxOsBlock, 0.0f);
        mixSm.setTime (15.0f, osSampleRate);
        mixSm.snap (1.0f);
        widthSm.setTime (15.0f, osSampleRate);
        widthSm.snap (1.0f);
        first = true;
    }
    void reset() { for (auto& s : slots) s.reset(); }

    void setParams (const EngineParams& ep)
    {
        params = ep;
        mixSm.target = ep.mix;
        widthSm.target = ep.width;
        if (first) { mixSm.snap (ep.mix); widthSm.snap (ep.width); first = false; }
        for (int i = 0; i < kNumSlots; ++i) slots[(size_t) i].setParams (ep.slots[(size_t) i]);
    }

    void process (float* L, float* R, int n)
    {
        std::copy (L, L + n, dryBuf[0].begin());
        std::copy (R, R + n, dryBuf[1].begin());

        if (! params.parallel)
        {
            for (auto& s : slots) s.process (L, R, n);
        }
        else
        {
            std::fill (acc[0].begin(), acc[0].begin() + n, 0.0f);
            std::fill (acc[1].begin(), acc[1].begin() + n, 0.0f);
            bool any = false;
            for (auto& s : slots)
            {
                if (! s.isActive()) continue;
                any = true;
                std::copy (dryBuf[0].begin(), dryBuf[0].begin() + n, tmp[0].begin());
                std::copy (dryBuf[1].begin(), dryBuf[1].begin() + n, tmp[1].begin());
                s.process (tmp[0].data(), tmp[1].data(), n);
                for (int i = 0; i < n; ++i)
                {
                    acc[0][(size_t) i] += tmp[0][(size_t) i] - dryBuf[0][(size_t) i];
                    acc[1][(size_t) i] += tmp[1][(size_t) i] - dryBuf[1][(size_t) i];
                }
            }
            if (any)
                for (int i = 0; i < n; ++i)
                {
                    L[i] = dryBuf[0][(size_t) i] + acc[0][(size_t) i];
                    R[i] = dryBuf[1][(size_t) i] + acc[1][(size_t) i];
                }
        }

        for (int i = 0; i < n; ++i)
        {
            const float m = mixSm.next();
            float l = dryBuf[0][(size_t) i] + m * (L[i] - dryBuf[0][(size_t) i]);
            float r = dryBuf[1][(size_t) i] + m * (R[i] - dryBuf[1][(size_t) i]);
            const float wd = widthSm.next();
            if (wd != 1.0f) { const float mm = 0.5f * (l + r), ss = 0.5f * (l - r) * wd; l = mm + ss; r = mm - ss; }
            if (params.safetyClip) { l = softKnee (l); r = softKnee (r); }
            L[i] = l; R[i] = r;
        }
    }

private:
    static float softKnee (float x)
    {
        const float t = 0.8f, ax = std::abs (x);
        if (ax <= t) return x;
        const float y = t + (1.0f - t) * std::tanh ((ax - t) / (1.0f - t));
        return x < 0 ? -y : y;
    }

    std::array<SatSlot, kNumSlots> slots;
    EngineParams params;
    std::array<std::vector<float>, 2> tmp, acc, dryBuf;
    Smoother mixSm, widthSm;
    bool first = true;
};
} // namespace satlab
