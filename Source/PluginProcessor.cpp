#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace satlab;

SatLabProcessor::SatLabProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", buildLayout())
{
    params.build (apvts);
    modParams.build (apvts);
}

juce::AudioProcessorValueTreeState::ParameterLayout SatLabProcessor::buildLayout()
{
    auto layout = createLayout();
    addModParams (layout);
    return layout;
}

bool SatLabProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void SatLabProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = std::max (samplesPerBlock, 32);
    work.setSize (2, maxBlock, false, true, false);

    for (int i = 0; i <= kMaxFactorLog2; ++i)
    {
        const int factor = 1 << i;
        engines[(size_t) i].prepare ((float) (sr * factor), factor, maxBlock * factor);
        if (i > 0)
        {
            auto os = std::make_unique<juce::dsp::Oversampling<float>> (
                2, (size_t) i, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
            os->initProcessing ((size_t) maxBlock);
            oversamplers[(size_t) i - 1] = std::move (os);
        }
    }
    modEngine.prepare (sr);
    lastInG = lastOutG = -1.0f;
    currentOs = -1;
    prepared = true;
}

void SatLabProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (! prepared) return;

    const int total = buffer.getNumSamples();
    const int numCh = std::min (buffer.getNumChannels(), 2);
    for (int c = numCh; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, total);   // canaux en trop : silence
    if (numCh < 1) return;

    // ---- parametres globaux
    const int osIdx = juce::jlimit (0, kMaxFactorLog2, (int) std::round (params.os->load()));
    if (osIdx != currentOs)
    {
        currentOs = osIdx;
        engines[(size_t) osIdx].reset();
        if (osIdx > 0) oversamplers[(size_t) osIdx - 1]->reset();
        const int lat = osIdx > 0 ? (int) std::round (oversamplers[(size_t) osIdx - 1]->getLatencyInSamples()) : 0;
        setLatencySamples (lat);
    }

    // ---- transport (pour les LFO synchronises)
    bool playing = false; double ppq = -1.0, bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppq = *q;
            if (auto t = pos->getBpm()) bpm = *t;
        }

    auto& engine = engines[(size_t) osIdx];
    constexpr int kSub = 32;                       // la modulation est evaluee tous les 32 echantillons
    float peakIn = 0.0f, peakOut = 0.0f;
    const double beatsPerSample = bpm / 60.0 / sr;
    const double rawIn = params.in->load(), rawOut = params.out->load();

    for (int start = 0; start < total; start += kSub)
    {
        const int n = std::min (kSub, total - start);
        float* wl = work.getWritePointer (0);
        float* wr = work.getWritePointer (1);
        const float* inL = buffer.getReadPointer (0) + start;
        const float* inR = buffer.getReadPointer (numCh > 1 ? 1 : 0) + start;

        // modulation (sur le signal d'entree)
        modEngine.update (modParams, inL, inR, n, playing, ppq >= 0.0 ? ppq + beatsPerSample * start : -1.0, bpm);

        EngineParams ep;
        ep.parallel   = params.routing->load() > 0.5f;
        ep.mix        = modded (params.mixRange, params.mix->load(), modEngine.globOff[2]);
        ep.width      = modded (params.widthRange, params.width->load(), modEngine.globOff[3]);
        ep.safetyClip = params.safety->load() > 0.5f;
        for (int s = 0; s < kNumSlots; ++s) ep.slots[(size_t) s] = readSlotParams (params, s, modEngine.slotOff[s]);
        engine.setParams (ep);

        const float inG  = dbToLin ((float) modded (params.inRange,  (float) rawIn,  modEngine.globOff[0]));
        const float outG = dbToLin ((float) modded (params.outRange, (float) rawOut, modEngine.globOff[1]));
        if (lastInG < 0.0f) { lastInG = inG; lastOutG = outG; }
        const float dIn = (inG - lastInG) / (float) n, dOut = (outG - lastOutG) / (float) n;

        float g = lastInG;
        for (int i = 0; i < n; ++i)
        {
            g += dIn;
            wl[i] = inL[i] * g; wr[i] = inR[i] * g;
            peakIn = std::max (peakIn, std::max (std::abs (wl[i]), std::abs (wr[i])));
        }
        lastInG = inG;

        if (osIdx == 0)
        {
            engine.process (wl, wr, n);
        }
        else
        {
            juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, 0, (size_t) n);
            auto& os = *oversamplers[(size_t) osIdx - 1];
            auto up = os.processSamplesUp (block);
            engine.process (up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples());
            os.processSamplesDown (block);
        }

        float* outL = buffer.getWritePointer (0) + start;
        float* outR = numCh > 1 ? buffer.getWritePointer (1) + start : nullptr;
        float go = lastOutG;
        for (int i = 0; i < n; ++i)
        {
            go += dOut;
            const float l = sanitize (wl[i] * go), r = sanitize (wr[i] * go);
            peakOut = std::max (peakOut, std::max (std::abs (l), std::abs (r)));
            { const int hd = vizHead.load (std::memory_order_relaxed); vizBufL[(size_t) hd].store (l, std::memory_order_relaxed); vizBufR[(size_t) hd].store (outR ? r : l, std::memory_order_relaxed); vizHead.store ((hd + 1) & (kVizSize - 1), std::memory_order_release); }
            outL[i] = l;
            if (outR) outR[i] = r;
        }
        lastOutG = outG;
    }

    // publication pour l'interface (anneaux de modulation)
    for (int i = 0; i < modEngine.numDests && i < ModEngine::kMaxDest; ++i) uiDestOff[i].store (modEngine.destOff[i]);
    for (int i = 0; i < NumSources; ++i) uiSrc[i].store (modEngine.srcValue[i]);
    for (int i = 0; i < kNumLfos; ++i) uiLfoPhase[i].store (modEngine.lfoPhase[i]);

    // vu-metres avec relachement
    const float decay = std::exp (-(float) total / (0.25f * (float) sr));
    inLevel.store (std::max (peakIn, inLevel.load() * decay));
    outLevel.store (std::max (peakOut, outLevel.load() * decay));
}

//==============================================================================
int SatLabProcessor::getNumPrograms() { return (int) getPresets().size(); }
const juce::String SatLabProcessor::getProgramName (int i)
{
    auto& p = getPresets();
    return (i >= 0 && i < (int) p.size()) ? p[(size_t) i].name : juce::String();
}
void SatLabProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms()) return;
    currentProgram = index;
    applyPreset (index);
}

void SatLabProcessor::applyPreset (int index)
{
    // 1) tout remettre aux valeurs par defaut, 2) appliquer les valeurs du preset
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& e : getPresets()[(size_t) index].values)
        if (auto* rp = apvts.getParameter (e.first))
            rp->setValueNotifyingHost (rp->convertTo0to1 (e.second));
}

void SatLabProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void SatLabProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            currentProgram = xml->getIntAttribute ("program", 0);
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
        }
}

juce::AudioProcessorEditor* SatLabProcessor::createEditor() { return new SatLabEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SatLabProcessor(); }
