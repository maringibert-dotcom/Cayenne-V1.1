#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "Modulation.h"

class SatLabProcessor : public juce::AudioProcessor
{
public:
    SatLabProcessor();
    ~SatLabProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CAYENNE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    satlab::ParamCache params;
    satlab::ModParamCache modParams;

    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f };   // crete, pour les vu-metres

    // lecture seule pour l'interface : decalage de modulation par destination (anneaux) et valeur des sources
    std::atomic<float> uiDestOff[satlab::ModEngine::kMaxDest] {};
    std::atomic<float> uiSrc[satlab::NumSources] {};
    std::atomic<float> uiLfoPhase[satlab::kNumLfos] {};

    // tampon circulaire du signal de sortie (mono) pour le visualiseur
    static constexpr int kVizSize = 4096;
    std::array<std::atomic<float>, kVizSize> vizBufL {}, vizBufR {};
    std::atomic<int> vizHead { 0 };
    double currentSampleRate() const { return sr > 0.0 ? sr : 48000.0; }
    static juce::AudioProcessorValueTreeState::ParameterLayout buildLayout();

private:
    static constexpr int kMaxFactorLog2 = 4;      // 16x
    void applyPreset (int index);

    std::array<satlab::SatEngine, kMaxFactorLog2 + 1> engines;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, kMaxFactorLog2> oversamplers;  // 2x, 4x, 8x, 16x
    satlab::ModEngine modEngine;
    float lastInG = 1.0f, lastOutG = 1.0f;
    juce::AudioBuffer<float> work;
    int  maxBlock = 512;
    double sr = 44100.0;
    int  currentOs = -1;
    int  currentProgram = 0;
    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SatLabProcessor)
};
