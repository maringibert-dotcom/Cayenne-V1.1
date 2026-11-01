// Outil de verification :
//   SatLabShot out.png [slot 0-3] [preset index]   -> rend l'editeur dans un PNG
//   SatLabShot --test                              -> teste le processeur (tous presets x oversampling x tailles de blocs)
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "Modulation.h"
#include <random>


static void setP (SatLabProcessor& proc, const juce::String& id, float v)
{
    if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    else std::printf ("PARAM INCONNU %s\n", id.toRawUTF8());
}

static int runModTest()
{
    using namespace satlab;
    int failures = 0;
    auto run = [&] (SatLabProcessor& proc, int seconds, std::vector<float>* rmsPerBlock, double* panL, double* panR)
    {
        const int bs = 480; juce::MidiBuffer midi; int t = 0;
        double sl = 0, sr_ = 0;
        for (int b = 0; b < seconds * 100; ++b)
        {
            juce::AudioBuffer<float> buf (2, bs);
            for (int i = 0; i < bs; ++i) { const float v = 0.3f * std::sin (0.05f * (float) (t++)); buf.setSample (0, i, v); buf.setSample (1, i, v); }
            proc.processBlock (buf, midi);
            double e = 0; for (int i = 0; i < bs; ++i) { e += buf.getSample (0, i) * buf.getSample (0, i); sl += buf.getSample (0, i) * buf.getSample (0, i); sr_ += buf.getSample (1, i) * buf.getSample (1, i); }
            if (rmsPerBlock) rmsPerBlock->push_back ((float) std::sqrt (e / bs));
        }
        if (panL) *panL = sl; if (panR) *panR = sr_;
    };
    auto fresh = [&] (SatLabProcessor& proc)
    {
        proc.prepareToPlay (48000.0, 480);
        setP (proc, "os", 0); setP (proc, "s1_type", (float) Tanh); setP (proc, "s1_on", 1);
        setP (proc, "s1_drive", 6.0f); setP (proc, "s1_out", 0.0f);
    };

    { // 1) LFO -> Output du slot : le niveau doit osciller
        SatLabProcessor proc; fresh (proc);
        setP (proc, "l1_rate", 4.0f);
        setP (proc, "r1_src", (float) SrcLfo1);
        setP (proc, "r1_dst", (float) destIndexForSlotParam (0, P_Out));
        setP (proc, "r1_amt", 0.5f);
        std::vector<float> r; run (proc, 2, &r, nullptr, nullptr);
        float mn = 1e9f, mx = 0; for (size_t i = 20; i < r.size(); ++i) { mn = std::min (mn, r[i]); mx = std::max (mx, r[i]); }
        std::printf ("LFO->Out : rms min %.4f max %.4f (ratio %.1f)\n", mn, mx, mx / std::max (mn, 1e-9f));
        if (mx / std::max (mn, 1e-9f) < 3.0f) { ++failures; std::printf ("  ECHEC: la modulation n'agit pas\n"); }
        // vitesse : 4 Hz -> 8 passages par seconde sur 2 s ; comptons les maxima locaux
        int peaks = 0; for (size_t i = 21; i + 1 < r.size(); ++i) if (r[i] > r[i-1] && r[i] >= r[i+1] && r[i] > 0.5f * (mn + mx)) ++peaks;
        std::printf ("  maxima ~ %d (attendu ~8)\n", peaks);
        if (peaks < 6 || peaks > 10) { ++failures; std::printf ("  ECHEC: mauvaise frequence\n"); }
    }
    { // 2) pan a gauche : le canal droit doit s'eteindre
        SatLabProcessor proc; fresh (proc);
        setP (proc, "s1_pan", -1.0f);
        double l, r; run (proc, 1, nullptr, &l, &r);
        std::printf ("Pan -100 : L %.3f R %.5f\n", l, r);
        if (! (r < 1e-6 * l + 1e-9)) { ++failures; std::printf ("  ECHEC pan\n"); }
    }
    { // 3) largeur 0 -> mono (L == R) ; entree mono => inchange
        SatLabProcessor proc; fresh (proc);
        setP (proc, "width", 0.0f);
        juce::AudioBuffer<float> buf (2, 480); juce::MidiBuffer midi;
        for (int k = 0; k < 20; ++k)
        {
            for (int i = 0; i < 480; ++i) { buf.setSample (0, i, 0.3f * std::sin (0.05f * (float) (k*480+i))); buf.setSample (1, i, 0.3f * std::sin (0.031f * (float) (k*480+i))); }
            proc.processBlock (buf, midi);
        }
        float d = 0; for (int i = 0; i < 480; ++i) d = std::max (d, std::abs (buf.getSample (0, i) - buf.getSample (1, i)));
        std::printf ("Width 0 : |L-R| max = %.6f\n", d);
        if (d > 1e-4f) { ++failures; std::printf ("  ECHEC width\n"); }
    }
    { // 4) stress : toutes les routes aleatoires, tous les OS : pas de NaN, pas d'explosion
        std::mt19937 rng (3);
        for (int os = 0; os < 5; ++os)
        {
            SatLabProcessor proc; fresh (proc);
            setP (proc, "os", (float) os);
            for (int s = 2; s <= 4; ++s) setP (proc, "s" + juce::String (s) + "_on", 1);
            const int nd = (int) modDests().size();
            for (int i = 0; i < kNumLfos; ++i) { setP (proc, lfoId (i, "rate"), 0.5f + 30.0f * (float) (rng() % 100) / 100.0f); setP (proc, lfoId (i, "shape"), (float) (rng() % 7)); }
            for (int r = 0; r < kNumRoutes; ++r)
            {
                setP (proc, routeId (r, "src"), (float) (1 + rng() % 7));
                setP (proc, routeId (r, "dst"), (float) (1 + rng() % (nd - 1)));
                setP (proc, routeId (r, "amt"), -1.0f + 2.0f * (float) (rng() % 101) / 100.0f);
            }
            setP (proc, "mac1", 0.8f);
            double l, r; std::vector<float> rr; run (proc, 3, &rr, &l, &r);
            float mxv = 0; bool ok = true; for (float v : rr) { if (! std::isfinite (v)) ok = false; mxv = std::max (mxv, v); }
            std::printf ("Stress mod OS%d : %s, rms max %.3f\n", os, ok ? "ok" : "NaN!", mxv);
            if (! ok || mxv > 200.0f) ++failures;
        }
    }
    return failures;
}

static int runProcessorTest()
{
    int failures = 0;
    std::mt19937 rng (7); std::uniform_real_distribution<float> U (-1.0f, 1.0f);
    const int blockSizes[] = { 1, 37, 64, 480, 512, 2048 };    // 2048 > taille preparee (512) : doit etre gere
    for (int mono = 0; mono < 2; ++mono)
    {
        SatLabProcessor proc;
        juce::AudioProcessor::BusesLayout layout;
        layout.getMainInputChannelSet() = layout.getMainOutputChannelSet() = mono ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        proc.setBusesLayout (layout);
        proc.prepareToPlay (48000.0, 512);
        for (int pr = 0; pr < proc.getNumPrograms(); ++pr)
        {
            proc.setCurrentProgram (pr);
            for (int os = 0; os < 5; ++os)
            {
                if (auto* p = proc.apvts.getParameter ("os")) p->setValueNotifyingHost (p->convertTo0to1 ((float) os));
                float peak = 0, rmsIn = 0, rmsOut = 0; bool finite = true; int count = 0;
                for (int bs : blockSizes)
                {
                    juce::AudioBuffer<float> buf (mono ? 1 : 2, bs); juce::MidiBuffer midi;
                    for (int rep = 0; rep < std::max (2, 4096 / bs); ++rep)
                    {
                        for (int c = 0; c < buf.getNumChannels(); ++c)
                            for (int i = 0; i < bs; ++i)
                                buf.setSample (c, i, 0.35f * std::sin (0.07f * (float) (count + i)) + 0.05f * U (rng));
                        count += bs;
                        double si = 0; for (int i = 0; i < bs; ++i) si += buf.getSample (0, i) * buf.getSample (0, i);
                        proc.processBlock (buf, midi);
                        double so = 0;
                        for (int c = 0; c < buf.getNumChannels(); ++c)
                            for (int i = 0; i < bs; ++i)
                            {
                                const float v = buf.getSample (c, i);
                                if (! std::isfinite (v)) finite = false;
                                peak = std::max (peak, std::abs (v));
                                if (c == 0) so += v * v;
                            }
                        rmsIn += (float) si; rmsOut += (float) so;
                    }
                }
                const bool ok = finite && peak < 16.0f;
                if (! ok) ++failures;
                if (os == 2 || ! ok)
                    std::printf ("%s preset %2d %-28s os=%d latency=%d  peak=%.2f gain=%+.1f dB %s\n", mono ? "mono  " : "stereo", pr,
                                 proc.getProgramName (pr).toRawUTF8(), os, proc.getLatencySamples(), peak,
                                 10.0 * std::log10 ((rmsOut + 1e-12) / (rmsIn + 1e-12)), ok ? "" : "  <-- PROBLEME");
            }
        }
    }
    std::printf (failures == 0 ? "PROCESSOR TEST OK\n" : "PROCESSOR TEST FAILED (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 1 && juce::String (argv[1]) == "--modtest") return runModTest();
    if (argc > 1 && juce::String (argv[1]) == "--test") return runProcessorTest();

    SatLabProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    const int slot = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
    if (argc > 3) proc.setCurrentProgram (juce::String (argv[3]).getIntValue());
    {   // un peu d'audio pour animer les modulateurs
        juce::MidiBuffer midi; int t = 0;
        for (int b = 0; b < (argc > 4 ? 0 : 40); ++b)
        {
            juce::AudioBuffer<float> buf (2, 512);
            for (int i = 0; i < 512; ++i) { const float env = 0.6f + 0.4f * std::sin (0.0006f * (float) t); const float v = env * (0.45f * std::sin (0.05f * (float) t) + 0.15f * std::sin (0.31f * (float) t) + 0.08f * std::sin (0.9f * (float) t)); ++t; buf.setSample (0, i, v); buf.setSample (1, i, v); }
            proc.processBlock (buf, midi);
        }
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    if (auto* e = dynamic_cast<SatLabEditor*> (ed.get())) e->snapshotRefresh (slot);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (argc > 1 ? argv[1] : "shot.png");
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
    return 0;
}
