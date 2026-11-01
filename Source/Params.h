#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/SatEngine.h"

namespace satlab
{
//==============================================================================
// Parametres d'un slot (identiques pour les 4 slots) : id = "s<N>_<suffixe>"
enum SlotParamIndex
{
    P_On, P_Type, P_Stereo, P_Lo, P_Hi,
    P_Drive, P_Bias, P_Asym,
    P_FFreq, P_FGain, P_FQ, P_Deemph,
    P_Sag, P_SagT,
    P_S1, P_S2, P_S3, P_S4,
    P_Warm, P_WarmF, P_Harsh, P_HarshF,
    P_LsF, P_LsG, P_MidF, P_MidG, P_MidQ, P_HsF, P_HsG,
    P_Mix, P_Out, P_Auto,
    P_Pan, P_Width,
    P_Count
};

enum class Kind { Float, Choice, Bool };
enum class Fmt { Plain, Percent, Bipolar, Hz, dB, Q, Ms };

struct PDef
{
    const char* id; const char* name; Kind kind;
    float mn, mx, def, centre; Fmt fmt;
};

inline const PDef& slotDef (int i)
{
    static const PDef defs[P_Count] =
    {
        { "on",     "On",            Kind::Bool,   0, 1, 0, 0, Fmt::Plain },
        { "type",   "Type",          Kind::Choice, 0, 0, 1, 0, Fmt::Plain },
        { "stereo", "Stereo Mode",   Kind::Choice, 0, 0, 0, 0, Fmt::Plain },
        { "lo",     "Range Low",     Kind::Float,  20.0f, 20000.0f, 20.0f, 700.0f, Fmt::Hz },
        { "hi",     "Range High",    Kind::Float,  20.0f, 20000.0f, 20000.0f, 2500.0f, Fmt::Hz },
        { "drive",  "Drive",         Kind::Float,  0.0f, 48.0f, 12.0f, 0, Fmt::dB },
        { "bias",   "Bias",          Kind::Float, -1.0f, 1.0f, 0.0f, 0, Fmt::Bipolar },
        { "asym",   "Asymmetry",     Kind::Float, -1.0f, 1.0f, 0.0f, 0, Fmt::Bipolar },
        { "ffreq",  "Focus Freq",    Kind::Float,  20.0f, 20000.0f, 1000.0f, 1000.0f, Fmt::Hz },
        { "fgain",  "Focus Gain",    Kind::Float, -18.0f, 18.0f, 0.0f, 0, Fmt::dB },
        { "fq",     "Focus Q",       Kind::Float,  0.3f, 8.0f, 1.0f, 1.0f, Fmt::Q },
        { "deemph", "De-Emphasis",   Kind::Float,  0.0f, 1.0f, 0.0f, 0, Fmt::Percent },
        { "sag",    "Sag",           Kind::Float,  0.0f, 1.0f, 0.0f, 0, Fmt::Percent },
        { "sagt",   "Sag Time",      Kind::Float,  5.0f, 500.0f, 80.0f, 80.0f, Fmt::Ms },
        { "c1",     "Character 1",   Kind::Float,  0.0f, 1.0f, 0.5f, 0, Fmt::Percent },
        { "c2",     "Character 2",   Kind::Float,  0.0f, 1.0f, 0.5f, 0, Fmt::Percent },
        { "c3",     "Character 3",   Kind::Float,  0.0f, 1.0f, 0.5f, 0, Fmt::Percent },
        { "c4",     "Character 4",   Kind::Float,  0.0f, 1.0f, 0.5f, 0, Fmt::Percent },
        { "warm",   "Warmth",        Kind::Float,  0.0f, 1.0f, 0.0f, 0, Fmt::Percent },
        { "warmf",  "Warmth Freq",   Kind::Float,  100.0f, 8000.0f, 800.0f, 800.0f, Fmt::Hz },
        { "harsh",  "Harshness",     Kind::Float, -1.0f, 1.0f, 0.0f, 0, Fmt::Bipolar },
        { "harshf", "Harsh Freq",    Kind::Float,  500.0f, 15000.0f, 3000.0f, 3000.0f, Fmt::Hz },
        { "lsf",    "Low Shelf Freq",Kind::Float,  30.0f, 1000.0f, 120.0f, 120.0f, Fmt::Hz },
        { "lsg",    "Low Shelf Gain",Kind::Float, -18.0f, 18.0f, 0.0f, 0, Fmt::dB },
        { "mf",     "Mid Freq",      Kind::Float,  100.0f, 10000.0f, 1000.0f, 1000.0f, Fmt::Hz },
        { "mg",     "Mid Gain",      Kind::Float, -18.0f, 18.0f, 0.0f, 0, Fmt::dB },
        { "mq",     "Mid Q",         Kind::Float,  0.2f, 10.0f, 1.0f, 1.0f, Fmt::Q },
        { "hsf",    "High Shelf Freq",Kind::Float, 1000.0f, 20000.0f, 8000.0f, 8000.0f, Fmt::Hz },
        { "hsg",    "High Shelf Gain",Kind::Float,-18.0f, 18.0f, 0.0f, 0, Fmt::dB },
        { "mix",    "Mix",           Kind::Float,  0.0f, 1.0f, 1.0f, 0, Fmt::Percent },
        { "out",    "Output",        Kind::Float, -24.0f, 24.0f, 0.0f, 0, Fmt::dB },
        { "auto",   "Auto Gain",     Kind::Bool,   0, 1, 0, 0, Fmt::Plain },
        { "pan",    "Pan",           Kind::Float, -1.0f, 1.0f, 0.0f, 0, Fmt::Bipolar },
        { "width",  "Width",         Kind::Float,  0.0f, 2.0f, 1.0f, 0, Fmt::Percent },
    };
    return defs[i];
}

inline juce::String slotId (int slot, int p) { return "s" + juce::String (slot + 1) + "_" + slotDef (p).id; }

inline juce::StringArray stereoModeNames() { return { "Stereo", "Mid", "Side", "Left", "Right" }; }
inline juce::StringArray osNames()         { return { "Off", "2x", "4x", "8x", "16x" }; }
inline juce::StringArray routingNames()    { return { "Serial", "Parallel" };  }

inline juce::StringArray typeNames()
{
    juce::StringArray a;
    for (int i = 0; i < NumSatTypes; ++i) a.add (getTypeInfo (i).name);
    return a;
}

//==============================================================================
inline juce::String formatValue (Fmt f, float v)
{
    switch (f)
    {
        case Fmt::Hz:      return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String ((int) std::round (v)) + " Hz";
        case Fmt::dB:      return (v > 0.04f ? "+" : "") + juce::String (v, 1) + " dB";
        case Fmt::Percent: return juce::String ((int) std::round (v * 100.0f)) + " %";
        case Fmt::Bipolar: return (v > 0.004f ? "+" : "") + juce::String ((int) std::round (v * 100.0f)) + " %";
        case Fmt::Q:       return juce::String (v, 2);
        case Fmt::Ms:      return juce::String ((int) std::round (v)) + " ms";
        default:           return juce::String (v, 2);
    }
}

inline float parseValue (Fmt f, const juce::String& text, float fallback)
{
    auto t = text.trim().toLowerCase();
    float v = t.getFloatValue();
    if (f == Fmt::Hz && t.contains ("k")) v *= 1000.0f;
    if ((f == Fmt::Percent || f == Fmt::Bipolar) ) v *= 0.01f;
    if (! t.containsAnyOf ("0123456789")) return fallback;
    return v;
}

inline std::unique_ptr<juce::RangedAudioParameter> makeParam (const juce::String& id, const juce::String& name, const PDef& d,
                                                              int choiceDefault = -1, const juce::StringArray* choices = nullptr)
{
    using namespace juce;
    const ParameterID pid { id, 1 };
    switch (d.kind)
    {
        case Kind::Bool:   return std::make_unique<AudioParameterBool> (pid, name, d.def > 0.5f);
        case Kind::Choice: return std::make_unique<AudioParameterChoice> (pid, name, *choices, choiceDefault >= 0 ? choiceDefault : (int) d.def);
        case Kind::Float:
        {
            NormalisableRange<float> r (d.mn, d.mx);
            if (d.centre > 0.0f) r.setSkewForCentre (d.centre);
            const Fmt fmt = d.fmt; const float fb = d.def;
            auto attr = AudioParameterFloatAttributes()
                .withStringFromValueFunction ([fmt] (float v, int) { return formatValue (fmt, v); })
                .withValueFromStringFunction ([fmt, fb] (const String& s) { return parseValue (fmt, s, fb); });
            return std::make_unique<AudioParameterFloat> (pid, name, r, d.def, attr);
        }
    }
    return nullptr;
}

//==============================================================================
inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto global = std::make_unique<AudioProcessorParameterGroup> ("global", "Global", "|");
    auto gdb = [] (float lo, float hi) { PDef d { "", "", Kind::Float, lo, hi, 0.0f, 0, Fmt::dB }; return d; };
    global->addChild (makeParam ("in",  "Input",  gdb (-24.0f, 24.0f)));
    global->addChild (makeParam ("out", "Output", gdb (-24.0f, 24.0f)));
    { PDef d { "", "", Kind::Float, 0, 1, 1.0f, 0, Fmt::Percent }; global->addChild (makeParam ("mix", "Dry/Wet", d)); }
    { auto c = osNames();      PDef d { "", "", Kind::Choice, 0, 0, 2, 0, Fmt::Plain }; global->addChild (makeParam ("os", "Oversampling", d, 2, &c)); }
    { auto c = routingNames(); PDef d { "", "", Kind::Choice, 0, 0, 0, 0, Fmt::Plain }; global->addChild (makeParam ("routing", "Routing", d, 0, &c)); }
    { PDef d { "", "", Kind::Bool, 0, 1, 0, 0, Fmt::Plain }; global->addChild (makeParam ("safety", "Safety Clip", d)); }
    { PDef d { "", "", Kind::Float, 0, 2, 1.0f, 0, Fmt::Percent }; global->addChild (makeParam ("width", "Width", d)); }
    layout.add (std::move (global));

    const int defaultTypes[kNumSlots] = { Tanh, TapeHysteresis, Triode, Transformer };
    auto types = typeNames(); auto stereo = stereoModeNames();
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto grp = std::make_unique<AudioProcessorParameterGroup> ("slot" + String (s + 1), "Slot " + String (s + 1), "|");
        for (int p = 0; p < P_Count; ++p)
        {
            PDef d = slotDef (p);
            if (p == P_On) d.def = (s == 0) ? 1.0f : 0.0f;
            const String name = "S" + String (s + 1) + " " + d.name;
            if (p == P_Type)        grp->addChild (makeParam (slotId (s, p), name, d, defaultTypes[s], &types));
            else if (p == P_Stereo) grp->addChild (makeParam (slotId (s, p), name, d, 0, &stereo));
            else                    grp->addChild (makeParam (slotId (s, p), name, d));
        }
        layout.add (std::move (grp));
    }
    return layout;
}

//==============================================================================
// Lecture des parametres -> structures DSP (pointeurs caches : aucun acces par chaine dans le thread audio)
struct ParamCache
{
    std::atomic<float>* slot[kNumSlots][P_Count] {};
    std::atomic<float> *in = nullptr, *out = nullptr, *mix = nullptr, *os = nullptr, *routing = nullptr, *safety = nullptr, *width = nullptr;
    juce::NormalisableRange<float> slotRange[kNumSlots][P_Count];
    juce::NormalisableRange<float> inRange, outRange, mixRange, widthRange;

    void build (juce::AudioProcessorValueTreeState& apvts)
    {
        for (int s = 0; s < kNumSlots; ++s)
            for (int p = 0; p < P_Count; ++p)
            {
                slot[s][p] = apvts.getRawParameterValue (slotId (s, p));
                slotRange[s][p] = apvts.getParameterRange (slotId (s, p));
            }
        width = apvts.getRawParameterValue ("width");
        inRange = apvts.getParameterRange ("in");   outRange = apvts.getParameterRange ("out");
        mixRange = apvts.getParameterRange ("mix"); widthRange = apvts.getParameterRange ("width");
        in = apvts.getRawParameterValue ("in");   out = apvts.getRawParameterValue ("out");
        mix = apvts.getRawParameterValue ("mix"); os = apvts.getRawParameterValue ("os");
        routing = apvts.getRawParameterValue ("routing"); safety = apvts.getRawParameterValue ("safety");
    }
};

// valeur d'un parametre + decalage de modulation (en espace normalise 0..1)
inline float modded (const juce::NormalisableRange<float>& r, float v, float off)
{
    if (off == 0.0f) return v;
    return r.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, r.convertTo0to1 (v) + off));
}

// slotOff : tableau [P_Count] de decalages normalises pour ce slot (ou nullptr)
inline SlotParams readSlotParams (const ParamCache& pc, int slot, const float* slotOff = nullptr)
{
    auto get = [&] (int p)
    {
        const float v = pc.slot[slot][p]->load();
        return slotOff != nullptr ? modded (pc.slotRange[slot][p], v, slotOff[p]) : v;
    };
    SlotParams sp;
    sp.on = get (P_On) > 0.5f;
    sp.type = (int) std::round (get (P_Type));
    sp.stereoMode = (int) std::round (get (P_Stereo));
    sp.rangeLo = get (P_Lo); sp.rangeHi = std::max (get (P_Hi), sp.rangeLo * 1.05f);
    sp.driveDb = get (P_Drive); sp.bias = get (P_Bias); sp.asym = get (P_Asym);
    sp.focusFreq = get (P_FFreq); sp.focusGainDb = get (P_FGain); sp.focusQ = get (P_FQ); sp.deemph = get (P_Deemph);
    sp.sag = get (P_Sag); sp.sagTimeMs = get (P_SagT);
    for (int i = 0; i < 4; ++i) sp.s[i] = get (P_S1 + i);
    sp.warmth = get (P_Warm); sp.warmFreq = get (P_WarmF);
    sp.harsh = get (P_Harsh); sp.harshFreq = get (P_HarshF);
    sp.lsFreq = get (P_LsF); sp.lsGainDb = get (P_LsG);
    sp.midFreq = get (P_MidF); sp.midGainDb = get (P_MidG); sp.midQ = get (P_MidQ);
    sp.hsFreq = get (P_HsF); sp.hsGainDb = get (P_HsG);
    sp.mix = get (P_Mix); sp.outDb = get (P_Out); sp.autoGain = get (P_Auto) > 0.5f;
    sp.pan = get (P_Pan); sp.width = get (P_Width);
    return sp;
}
} // namespace satlab
