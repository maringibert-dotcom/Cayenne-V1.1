#pragma once
#include "Params.h"
#include "Modulation.h"
#include <vector>

namespace satlab
{
struct Preset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;   // id -> valeur reelle (Hz, dB, 0..1, index...)
};

struct PB   // petit constructeur de preset
{
    Preset p;
    explicit PB (const char* n) { p.name = n; }
    PB& g (const char* id, float v) { p.values.emplace_back (id, v); return *this; }
    PB& s (int slot, int param, float v) { p.values.emplace_back (slotId (slot - 1, param), v); return *this; }
    PB& slot (int slot, int type, float driveDb) { return s (slot, P_On, 1).s (slot, P_Type, (float) type).s (slot, P_Drive, driveDb); }
    // route de modulation n (1..8) : source, cible (slot, param) ou globale, quantite
    PB& mod (int n, int src, int destIdx, float amt)
    { return g (routeId (n - 1, "src").toRawUTF8(), (float) src).g (routeId (n - 1, "dst").toRawUTF8(), (float) destIdx).g (routeId (n - 1, "amt").toRawUTF8(), amt); }
    PB& lfo (int n, float hz, int shape) { return g (lfoId (n - 1, "rate").toRawUTF8(), hz).g (lfoId (n - 1, "shape").toRawUTF8(), (float) shape); }
    PB& ch (int slot, float a, float b, float c, float d) { return s (slot, P_S1, a).s (slot, P_S2, b).s (slot, P_S3, c).s (slot, P_S4, d); }
    operator Preset() const { return p; }
};

inline const std::vector<Preset>& getPresets()
{
    static const std::vector<Preset> presets = []
    {
        std::vector<Preset> v;
        v.push_back (PB ("Init - Soft Tanh"));

        v.push_back (PB ("Tape Glue").slot (1, TapeHysteresis, 10).ch (1, 0.35f, 0.5f, 0.7f, 0.08f)
                         .s (1, P_Warm, 0.2f).s (1, P_HsF, 10000).s (1, P_HsG, -2.0f).s (1, P_Auto, 1));

        v.push_back (PB ("Tube Warmth").slot (1, Triode, 14).ch (1, 0.5f, 0.4f, 1.0f, 0.4f)
                         .s (1, P_Warm, 0.4f).s (1, P_WarmF, 500).s (1, P_LsF, 100).s (1, P_LsG, 2.5f)
                         .s (1, P_Harsh, -0.4f).s (1, P_Sag, 0.3f).s (1, P_Auto, 1));

        v.push_back (PB ("Console Bus").slot (1, Console, 9).ch (1, 0.3f, 0.4f, 0.4f, 0.8f).s (1, P_Auto, 1)
                         .slot (2, Tanh, 3).s (2, P_Sag, 0.2f).s (2, P_Auto, 1));

        v.push_back (PB ("Fuzz Face").slot (1, GermaniumFuzz, 18).ch (1, 0.6f, 0.35f, 0.5f, 0.1f)
                         .s (1, P_FFreq, 800).s (1, P_FGain, 6).s (1, P_FQ, 0.8f)
                         .s (1, P_Harsh, -0.3f).s (1, P_Out, -6).s (1, P_Auto, 1));

        v.push_back (PB ("Screamer + Push-Pull").slot (1, Overdrive, 20).ch (1, 0.65f, 0.6f, 1.0f, 0.8f).s (1, P_Auto, 1)
                         .slot (2, Pentode, 6).ch (2, 0.2f, 0.5f, 0.9f, 0).s (2, P_Auto, 1));

        v.push_back (PB ("Parallel Grit").g ("routing", 1)
                         .slot (1, DiodeClipper, 26).ch (1, 0.35f, 0.4f, 0, 0.3f).s (1, P_Lo, 1000).s (1, P_Mix, 0.5f).s (1, P_Harsh, -0.2f)
                         .slot (2, Triode, 12).ch (2, 0.5f, 0.4f, 1.0f, 0.4f).s (2, P_Hi, 300).s (2, P_Warm, 0.3f));

        v.push_back (PB ("Lo-Fi Radio").slot (1, Bitcrusher, 6).ch (1, 0.35f, 0.35f, 0.5f, 0)
                         .s (1, P_LsF, 300).s (1, P_LsG, -9).s (1, P_MidF, 1500).s (1, P_MidG, 6).s (1, P_MidQ, 1.2f)
                         .s (1, P_HsF, 5000).s (1, P_HsG, -10).s (1, P_Auto, 1));

        v.push_back (PB ("Wavefold Metal").slot (1, Wavefolder, 24).ch (1, 0.3f, 0.8f, 0, 0)
                         .s (1, P_Harsh, 0.5f).s (1, P_Out, -8).s (1, P_Auto, 1));

        v.push_back (PB ("Air Exciter").slot (1, Harmonics, 6).ch (1, 0.2f, 0.35f, 0.2f, 0.15f)
                         .s (1, P_Lo, 3000).s (1, P_Mix, 0.35f).s (1, P_HsF, 10000).s (1, P_HsG, 3));

        v.push_back (PB ("Bass Thickener").slot (1, Transformer, 20).ch (1, 0.4f, 0.7f, 0.3f, 1.0f)
                         .s (1, P_Lo, 40).s (1, P_Hi, 300).s (1, P_Mix, 0.6f).s (1, P_Warm, 0.3f).s (1, P_WarmF, 400)
                         .slot (2, Triode, 8).s (2, P_Lo, 60).s (2, P_Hi, 600).s (2, P_Mix, 0.4f));

        v.push_back (PB ("Mid/Side Width Saturation").slot (1, TapeClassic, 10).ch (1, 0.5f, 0.4f, 0.5f, 0)
                         .s (1, P_Stereo, 2).s (1, P_Auto, 1)
                         .slot (2, Triode, 8).ch (2, 0.5f, 0.4f, 1.0f, 0.3f).s (2, P_Stereo, 1).s (2, P_Auto, 1));

        // ---- presets de sound design (modulation)
        v.push_back (PB ("Wobble Tape (LFO)").slot (1, TapeHysteresis, 12).ch (1, 0.4f, 0.5f, 0.7f, 0.5f).s (1, P_Auto, 1)
                         .lfo (1, 0.35f, 6).lfo (2, 5.5f, 0)
                         .mod (1, SrcLfo1, destIndexForSlotParam (0, P_Drive), 0.22f)
                         .mod (2, SrcLfo1, destIndexForSlotParam (0, P_HsG), -0.25f)
                         .mod (3, SrcLfo2, destIndexForSlotParam (0, P_S4), 0.12f));

        v.push_back (PB ("Auto-Pan Fuzz (Sync)").slot (1, GermaniumFuzz, 16).ch (1, 0.6f, 0.35f, 0.5f, 0.1f).s (1, P_Out, -4).s (1, P_Auto, 1)
                         .g ("l1_sync", 1).g ("l1_div", 4).g ("l1_shape", 1)
                         .mod (1, SrcLfo1, destIndexForSlotParam (0, P_Pan), 0.9f)
                         .mod (2, SrcLfo1, destIndexForSlotParam (0, P_Drive), -0.12f));

        v.push_back (PB ("Envelope Opener").slot (1, Triode, 10).ch (1, 0.5f, 0.4f, 1.0f, 0.4f).s (1, P_Auto, 1)
                         .slot (2, Wavefolder, 6).ch (2, 0.3f, 0.5f, 0, 0).s (2, P_Mix, 0.5f).s (2, P_Auto, 1)
                         .g ("e_att", 3).g ("e_rel", 220).g ("e_gain", 14)
                         .mod (1, SrcEnv, destIndexForSlotParam (0, P_Drive), 0.35f)
                         .mod (2, SrcEnv, destIndexForSlotParam (1, P_Mix), 0.6f)
                         .mod (3, SrcEnv, destIndexForSlotParam (0, P_Warm), 0.4f));

        v.push_back (PB ("Macro Destroyer").slot (1, Overdrive, 8).ch (1, 0.65f, 0.6f, 1.0f, 0.8f).s (1, P_Auto, 1)
                         .slot (2, Bitcrusher, 0).ch (2, 0.5f, 0.5f, 0.5f, 0).s (2, P_Mix, 0.0f).s (2, P_Auto, 1)
                         .g ("mac1", 0.0f)
                         .mod (1, SrcMacro1, destIndexForSlotParam (0, P_Drive), 0.6f)
                         .mod (2, SrcMacro1, destIndexForSlotParam (1, P_Mix), 0.8f)
                         .mod (3, SrcMacro1, destIndexForSlotParam (0, P_Harsh), 0.5f)
                         .mod (4, SrcMacro1, destIndexForGlobal (3), 0.25f));
        return v;
    }();
    return presets;
}
} // namespace satlab
