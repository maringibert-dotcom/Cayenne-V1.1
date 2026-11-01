#pragma once
// SatLab - systeme de modulation : 4 LFO, 1 suiveur d'enveloppe, 2 macros, matrice de 8 routes
#include "Params.h"
#include <cmath>
#include <cstdint>

namespace satlab
{
constexpr int kNumLfos   = 4;
constexpr int kNumRoutes = 8;
constexpr int kNumMacros = 2;

//==============================================================================
// Sources : 0 = aucune, 1..4 = LFO, 5 = enveloppe, 6..7 = macros
enum ModSource { SrcNone = 0, SrcLfo1, SrcLfo2, SrcLfo3, SrcLfo4, SrcEnv, SrcMacro1, SrcMacro2, NumSources };

inline juce::StringArray modSourceNames()
{
    return { "None", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "Envelope", "Macro 1", "Macro 2" };
}
inline juce::StringArray lfoShapeNames()
{
    return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Sample & Hold", "Smooth Random" };
}
inline juce::StringArray lfoDivNames()
{
    return { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 bar", "2 bars", "4 bars" };
}
inline double lfoDivBeats (int i)
{
    static const double b[] = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0, 2.0, 4.0, 8.0, 16.0 };
    return b[juce::jlimit (0, 10, i)];
}

//==============================================================================
// Destinations
struct ModDest
{
    enum Kind { Slot, Global, LfoRate } kind;
    int slot;      // 0..3 pour Slot, sinon -1
    int param;     // SlotParamIndex pour Slot ; 0..3 (in,out,mix,width) pour Global ; 0..3 (lfo) pour LfoRate
    juce::String label;
    juce::String group;
};

inline const int* modSlotParams (int& count)
{
    static const int list[] = {
        P_Drive, P_Bias, P_Asym,
        P_S1, P_S2, P_S3, P_S4,
        P_FFreq, P_FGain, P_FQ,
        P_Warm, P_WarmF, P_Harsh, P_HarshF, P_Sag, P_Deemph,
        P_Lo, P_Hi,
        P_LsG, P_MidF, P_MidG, P_HsG,
        P_Mix, P_Out, P_Pan, P_Width
    };
    count = (int) (sizeof (list) / sizeof (list[0]));
    return list;
}

// index 0 = "None" ; ensuite slots 1..4, globaux, vitesses des LFO
inline const std::vector<ModDest>& modDests()
{
    static const std::vector<ModDest> d = []
    {
        std::vector<ModDest> v;
        v.push_back ({ ModDest::Global, -1, -1, "None", "" });
        int n = 0; const int* l = modSlotParams (n);
        for (int s = 0; s < kNumSlots; ++s)
            for (int k = 0; k < n; ++k)
                v.push_back ({ ModDest::Slot, s, l[k], "S" + juce::String (s + 1) + " " + slotDef (l[k]).name, "Slot " + juce::String (s + 1) });
        const char* gn[] = { "Input", "Output", "Dry/Wet", "Width" };
        for (int g = 0; g < 4; ++g)
            v.push_back ({ ModDest::Global, -1, g, juce::String ("Global ") + gn[g], "Global" });
        for (int i = 0; i < kNumLfos; ++i)
            v.push_back ({ ModDest::LfoRate, -1, i, "LFO " + juce::String (i + 1) + " Rate", "Modulators" });
        return v;
    }();
    return d;
}

inline juce::StringArray modDestNames()
{
    juce::StringArray a;
    for (auto& d : modDests()) a.add (d.label);
    return a;
}

// index de destination pour un parametre de slot / global (0 si non modulable)
inline int destIndexForSlotParam (int slot, int param)
{
    const auto& d = modDests();
    for (size_t i = 1; i < d.size(); ++i)
        if (d[i].kind == ModDest::Slot && d[i].slot == slot && d[i].param == param) return (int) i;
    return 0;
}
inline int destIndexForLfoRate (int lfo)
{
    const auto& d = modDests();
    for (size_t i = 1; i < d.size(); ++i)
        if (d[i].kind == ModDest::LfoRate && d[i].param == lfo) return (int) i;
    return 0;
}
inline int destIndexForGlobal (int g)
{
    const auto& d = modDests();
    for (size_t i = 1; i < d.size(); ++i)
        if (d[i].kind == ModDest::Global && d[i].param == g) return (int) i;
    return 0;
}

//==============================================================================
inline juce::String lfoId (int i, const char* s) { return "l" + juce::String (i + 1) + "_" + s; }
inline juce::String routeId (int i, const char* s) { return "r" + juce::String (i + 1) + "_" + s; }

inline void addModParams (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    using namespace juce;
    auto grp = std::make_unique<AudioProcessorParameterGroup> ("mod", "Modulation", "|");
    auto shapes = lfoShapeNames(); auto divs = lfoDivNames(); auto srcs = modSourceNames(); auto dsts = modDestNames();

    for (int i = 0; i < kNumLfos; ++i)
    {
        const String n = "LFO" + String (i + 1) + " ";
        {
          const ParameterID pid { lfoId (i, "rate"), 1 };
          NormalisableRange<float> r (0.02f, 40.0f); r.setSkewForCentre (1.5f);
          auto attr = AudioParameterFloatAttributes()
              .withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 2 : 1) + " Hz"; })
              .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); });
          grp->addChild (std::make_unique<AudioParameterFloat> (pid, n + "Rate", r, 1.0f, attr)); }
        { PDef d { "", "", Kind::Bool, 0, 1, 0, 0, Fmt::Plain }; grp->addChild (makeParam (lfoId (i, "sync"), n + "Sync", d)); }
        { PDef d { "", "", Kind::Choice, 0, 0, 6, 0, Fmt::Plain }; grp->addChild (makeParam (lfoId (i, "div"), n + "Division", d, 6, &divs)); }
        { PDef d { "", "", Kind::Choice, 0, 0, 0, 0, Fmt::Plain }; grp->addChild (makeParam (lfoId (i, "shape"), n + "Shape", d, 0, &shapes)); }
        { PDef d { "", "", Kind::Float, 0, 1, 0, 0, Fmt::Percent }; grp->addChild (makeParam (lfoId (i, "phase"), n + "Phase", d)); }
        { PDef d { "", "", Kind::Bool, 0, 1, 0, 0, Fmt::Plain }; grp->addChild (makeParam (lfoId (i, "uni"), n + "Unipolar", d)); }
    }
    { PDef d { "", "", Kind::Float, 0.1f, 300.0f, 5.0f, 20.0f, Fmt::Ms }; grp->addChild (makeParam ("e_att", "Env Attack", d)); }
    { PDef d { "", "", Kind::Float, 5.0f, 2000.0f, 150.0f, 200.0f, Fmt::Ms }; grp->addChild (makeParam ("e_rel", "Env Release", d)); }
    { PDef d { "", "", Kind::Float, 0.0f, 36.0f, 12.0f, 0, Fmt::dB }; grp->addChild (makeParam ("e_gain", "Env Sensitivity", d)); }
    for (int m = 0; m < kNumMacros; ++m)
    { PDef d { "", "", Kind::Float, 0, 1, 0, 0, Fmt::Percent }; grp->addChild (makeParam ("mac" + String (m + 1), "Macro " + String (m + 1), d)); }

    for (int r = 0; r < kNumRoutes; ++r)
    {
        const String n = "Mod " + String (r + 1) + " ";
        { PDef d { "", "", Kind::Choice, 0, 0, 0, 0, Fmt::Plain }; grp->addChild (makeParam (routeId (r, "src"), n + "Source", d, 0, &srcs)); }
        { PDef d { "", "", Kind::Choice, 0, 0, 0, 0, Fmt::Plain }; grp->addChild (makeParam (routeId (r, "dst"), n + "Target", d, 0, &dsts)); }
        { PDef d { "", "", Kind::Float, -1, 1, 0, 0, Fmt::Bipolar }; grp->addChild (makeParam (routeId (r, "amt"), n + "Amount", d)); }
    }
    layout.add (std::move (grp));
}

// ---- aides d'edition de la matrice (utilisees par l'interface)
inline void setParamReal (juce::AudioProcessorValueTreeState& a, const juce::String& id, float v)
{
    if (auto* p = a.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); }
}
inline int getParamIndex (juce::AudioProcessorValueTreeState& a, const juce::String& id)
{
    return a.getRawParameterValue (id) != nullptr ? (int) std::round (a.getRawParameterValue (id)->load()) : 0;
}
// attribue (ou renforce) une route source -> destination
inline void assignModRoute (juce::AudioProcessorValueTreeState& a, int src, int dest)
{
    int slotFree = -1;
    for (int r = 0; r < kNumRoutes; ++r)
    {
        const int s = getParamIndex (a, routeId (r, "src")), d = getParamIndex (a, routeId (r, "dst"));
        if (s == src && d == dest) { setParamReal (a, routeId (r, "amt"), 0.5f); return; }
        if (slotFree < 0 && (s == 0 || d == 0)) slotFree = r;
    }
    if (slotFree < 0) slotFree = kNumRoutes - 1;
    setParamReal (a, routeId (slotFree, "src"), (float) src);
    setParamReal (a, routeId (slotFree, "dst"), (float) dest);
    setParamReal (a, routeId (slotFree, "amt"), 0.5f);
}
inline void clearModRoutes (juce::AudioProcessorValueTreeState& a, int dest)
{
    for (int r = 0; r < kNumRoutes; ++r)
        if (getParamIndex (a, routeId (r, "dst")) == dest)
        {
            setParamReal (a, routeId (r, "src"), 0.0f);
            setParamReal (a, routeId (r, "dst"), 0.0f);
            setParamReal (a, routeId (r, "amt"), 0.0f);
        }
}

//==============================================================================
struct ModParamCache
{
    std::atomic<float> *lRate[kNumLfos] {}, *lSync[kNumLfos] {}, *lDiv[kNumLfos] {}, *lShape[kNumLfos] {}, *lPhase[kNumLfos] {}, *lUni[kNumLfos] {};
    juce::NormalisableRange<float> lRateRange[kNumLfos];
    std::atomic<float> *eAtt = nullptr, *eRel = nullptr, *eGain = nullptr, *mac[kNumMacros] {};
    std::atomic<float> *rSrc[kNumRoutes] {}, *rDst[kNumRoutes] {}, *rAmt[kNumRoutes] {};

    void build (juce::AudioProcessorValueTreeState& a)
    {
        for (int i = 0; i < kNumLfos; ++i)
        {
            lRate[i] = a.getRawParameterValue (lfoId (i, "rate"));   lRateRange[i] = a.getParameterRange (lfoId (i, "rate"));
            lSync[i] = a.getRawParameterValue (lfoId (i, "sync"));   lDiv[i] = a.getRawParameterValue (lfoId (i, "div"));
            lShape[i] = a.getRawParameterValue (lfoId (i, "shape")); lPhase[i] = a.getRawParameterValue (lfoId (i, "phase"));
            lUni[i] = a.getRawParameterValue (lfoId (i, "uni"));
        }
        eAtt = a.getRawParameterValue ("e_att"); eRel = a.getRawParameterValue ("e_rel"); eGain = a.getRawParameterValue ("e_gain");
        for (int m = 0; m < kNumMacros; ++m) mac[m] = a.getRawParameterValue ("mac" + juce::String (m + 1));
        for (int r = 0; r < kNumRoutes; ++r)
        {
            rSrc[r] = a.getRawParameterValue (routeId (r, "src"));
            rDst[r] = a.getRawParameterValue (routeId (r, "dst"));
            rAmt[r] = a.getRawParameterValue (routeId (r, "amt"));
        }
    }
};

//==============================================================================
// Moteur de modulation : appele une fois par sous-bloc (cadence de base, pas sur-echantillonnee)
class ModEngine
{
public:
    static constexpr int kMaxDest = 1 + kNumSlots * 32 + 16;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& l : lfo) l = {};
        for (int i = 0; i < kNumLfos; ++i) { lfo[i].rng = 0x9E3779B9u * (uint32_t) (i + 1); lfo[i].heldA = nextRand (lfo[i]); lfo[i].heldB = nextRand (lfo[i]); }
        env = 0.0f;
        clearOffsets();
        for (auto& v : src) v = 0.0f;
    }

    // offsets normalises (additionnes a la valeur normalisee du parametre)
    float slotOff[kNumSlots][P_Count] {};
    float globOff[4] {};          // in, out, mix, width
    float lfoRateOff[kNumLfos] {};
    float srcValue[NumSources] {};
    float lfoPhase[kNumLfos] {};     // phase courante 0..1 (affichage)   // valeurs courantes des sources (pour l'affichage)
    float destOff[kMaxDest] {};      // decalage par index de destination (pour les anneaux de l'interface)
    int   numDests = 0;

    void update (const ModParamCache& pc, const float* inL, const float* inR, int n,
                 bool playing, double ppq, double bpm)
    {
        const double dt = (double) n / sr;
        if (bpm <= 1.0) bpm = 120.0;

        // ---- LFO
        for (int i = 0; i < kNumLfos; ++i)
        {
            auto& s = lfo[i];
            const bool sync = pc.lSync[i]->load() > 0.5f;
            const int shape = (int) std::round (pc.lShape[i]->load());
            const double phaseOff = pc.lPhase[i]->load();
            double rate = modded (pc.lRateRange[i], pc.lRate[i]->load(), lfoRateOff[i]);

            double acc;
            if (sync)
            {
                const double beats = lfoDivBeats ((int) std::round (pc.lDiv[i]->load()));
                if (playing && ppq >= 0.0) { acc = ppq / beats; s.accSync = acc; }
                else { s.accSync += dt * (bpm / 60.0) / beats; acc = s.accSync; }
            }
            else { s.accFree += dt * rate; acc = s.accFree; }

            acc += phaseOff;
            const double cyc = std::floor (acc);
            const double ph = acc - cyc;
            if ((int64_t) cyc != s.lastCycle)
            {
                s.lastCycle = (int64_t) cyc;
                s.heldA = s.heldB; s.heldB = nextRand (s);
            }

            float v = 0.0f;
            const float p = (float) ph;
            switch (shape)
            {
                case 0: v = std::sin (2.0f * kPi * p); break;
                case 1: v = p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f); break;
                case 2: v = 2.0f * p - 1.0f; break;
                case 3: v = 1.0f - 2.0f * p; break;
                case 4: v = p < 0.5f ? 1.0f : -1.0f; break;
                case 5: v = s.heldA; break;
                default: { const float t = 0.5f - 0.5f * std::cos (kPi * p); v = s.heldA + (s.heldB - s.heldA) * t; } break;
            }
            if (pc.lUni[i]->load() > 0.5f) v = 0.5f * v + 0.5f;
            srcValue[SrcLfo1 + i] = v;
            lfoPhase[i] = (float) ph;
        }

        // ---- enveloppe
        {
            const float g = dbToLin (pc.eGain->load());
            const float aC = std::exp (-1.0f / (0.001f * pc.eAtt->load() * (float) sr + 1.0f));
            const float rC = std::exp (-1.0f / (0.001f * pc.eRel->load() * (float) sr + 1.0f));
            float e = env;
            for (int i = 0; i < n; ++i)
            {
                const float x = std::max (std::abs (inL[i]), std::abs (inR[i])) * g;
                const float c = x > e ? aC : rC;
                e = x + c * (e - x);
            }
            env = sanitize (e);
            srcValue[SrcEnv] = juce::jlimit (0.0f, 1.0f, env);
        }
        srcValue[SrcMacro1] = pc.mac[0]->load();
        srcValue[SrcMacro2] = pc.mac[1]->load();
        srcValue[SrcNone] = 0.0f;

        // ---- matrice
        clearOffsets();
        const auto& dests = modDests();
        numDests = (int) dests.size();
        for (int r = 0; r < kNumRoutes; ++r)
        {
            const int sIdx = (int) std::round (pc.rSrc[r]->load());
            const int dIdx = (int) std::round (pc.rDst[r]->load());
            const float amt = pc.rAmt[r]->load();
            if (sIdx <= 0 || sIdx >= NumSources || dIdx <= 0 || dIdx >= numDests || amt == 0.0f) continue;
            const float o = amt * srcValue[sIdx];
            const auto& d = dests[(size_t) dIdx];
            destOff[dIdx] += o;
            switch (d.kind)
            {
                case ModDest::Slot:    slotOff[d.slot][d.param] += o; break;
                case ModDest::Global:  globOff[d.param] += o; break;
                case ModDest::LfoRate: lfoRateOff[d.param] += o; break;
            }
        }
    }

private:
    struct Lfo
    {
        double accFree = 0.0, accSync = 0.0;
        int64_t lastCycle = -1000000;
        float heldA = 0.0f, heldB = 0.0f;
        uint32_t rng = 12345u;
    };
    static float nextRand (Lfo& l)
    {
        l.rng ^= l.rng << 13; l.rng ^= l.rng >> 17; l.rng ^= l.rng << 5;
        return (float) (l.rng & 0xFFFFFF) / (float) 0x7FFFFF - 1.0f;
    }
    void clearOffsets()
    {
        for (auto& s : slotOff) for (auto& v : s) v = 0.0f;
        for (auto& v : globOff) v = 0.0f;
        for (auto& v : lfoRateOff) v = 0.0f;
        for (auto& v : destOff) v = 0.0f;
    }

    double sr = 44100.0;
    Lfo lfo[kNumLfos];
    float env = 0.0f;
    float src[NumSources] {};
};
} // namespace satlab
