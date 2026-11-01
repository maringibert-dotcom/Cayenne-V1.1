#pragma once
// EPICES / SatLab - page MOD : 4 LFO, enveloppe, macros, matrice de modulation
#include "PluginEditor.h"

//==============================================================================
class LfoScope : public juce::Component, private juce::Timer
{
public:
    LfoScope (SatLabProcessor& p, int lfoIndex);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    SatLabProcessor& proc;
    int idx;
    std::vector<float> history;
    int head = 0;
};

//==============================================================================
class LfoCard : public juce::Component
{
public:
    LfoCard (SatLabProcessor& p, int lfoIndex);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void updateSyncVisibility();
    SatLabProcessor& proc;
    int idx;
    juce::ComboBox shapeBox, divBox;
    Knob rate, phase;
    juce::ToggleButton sync { "Sync" }, uni { "Unipolar" };
    LfoScope scope;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shapeAtt, divAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAtt, uniAtt;
    std::unique_ptr<juce::ParameterAttachment> syncWatch;
};

//==============================================================================
class EnvMeter : public juce::Component, private juce::Timer
{
public:
    explicit EnvMeter (SatLabProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    SatLabProcessor& proc;
};

//==============================================================================
class RouteRow : public juce::Component
{
public:
    RouteRow (SatLabProcessor& p, int routeIndex);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void showTargetMenu();
    SatLabProcessor& proc;
    int idx;
    juce::ComboBox srcBox;
    juce::TextButton dstButton;
    juce::Slider amount;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAtt;
    std::unique_ptr<juce::ParameterAttachment> dstAtt;
};

//==============================================================================
class ModPanel : public juce::Component
{
public:
    explicit ModPanel (SatLabProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    SatLabProcessor& proc;
    std::array<std::unique_ptr<LfoCard>, satlab::kNumLfos> lfos;
    Knob envAtt, envRel, envSens, macro1, macro2;
    EnvMeter envMeter;
    std::array<std::unique_ptr<RouteRow>, satlab::kNumRoutes> rows;
    juce::Rectangle<int> envRect, macroRect, helpRect, matrixRect;
};
