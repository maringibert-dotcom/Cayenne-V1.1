#pragma once
#include "PluginProcessor.h"
#include "ClayTheme.h"
#include "Modulation.h"
class ModPanel;
class VisualizerOverlay;

//==============================================================================
//==============================================================================
// Reglage du signal de test de l'analyseur (partage entre les 4 slots)
struct TestTone
{
    std::atomic<float> freq { 1000.0f }, levelDb { -12.0f };
};

inline void styleLinearSlider (juce::Slider& s)
{
    s.setColour (juce::Slider::textBoxTextColourId, ui::text);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxHighlightColourId, ui::accent.withAlpha (0.35f));
}

//==============================================================================
class Knob : public juce::Component, private juce::Timer
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& text);
    void linkMod (const std::atomic<float>* src, int destIndex);
    void pollMod() { ignition = 1.0f; igniteDelay = 0; timerCallback(); }
    void resized() override;
    void setText (const juce::String& t) { label.setText (t, juce::dontSendNotification); }

    struct KnobSlider : juce::Slider
    {
        std::function<void()> onRightClick;
        void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu() && onRightClick) { onRightClick(); return; } juce::Slider::mouseDown (e); }
    } slider;
    juce::Label label;
private:
    void timerCallback() override;
    juce::AudioProcessorValueTreeState& apvtsRef;
    int destIdx = 0;
    const std::atomic<float>* modSrc = nullptr;
    float lastMod = 0.0f, ignition = 0.0f, charge = 0.0f, lastNeon = -1.0f, buzzPhase = 0.0f;
    int igniteDelay = 0, dipTicks = 0;
    juce::Random rnd;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
// Courbe de transfert dynamique (x -> y, boucle d'hysteresis visible) + spectre d'un sinus de test
class AnalyzerComponent : public juce::Component, private juce::Timer
{
public:
    AnalyzerComponent (SatLabProcessor& p, int slotIndex, TestTone& t);
    void paint (juce::Graphics&) override;
    void resized() override {}
    void refresh();   // recalcule immediatement (utilise aussi pour les captures d'ecran)
private:
    void timerCallback() override;
    void compute (const satlab::SlotParams& sp);
    static std::vector<float> signature (const satlab::SlotParams& sp, float f, float l);

    SatLabProcessor& proc;
    int slot;
    TestTone& tone;
    std::vector<float> lastSig;

    static constexpr int N = 32768;
    float fs = 192000.0f, f0 = 1000.0f, amp = 0.25f;
    std::vector<float> xs, ys;          // 2 periodes
    std::vector<float> specDb;          // N/2 bins
    float harmDb[9] {};                 // H2..H9 par rapport au fondamental
    float thdPct = 0.0f;
    float yScale = 1.0f;
    bool  valid = false;
};

//==============================================================================
class SlotPanel : public juce::Component
{
public:
    SlotPanel (SatLabProcessor& p, int slotIndex, TestTone& t);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshAnalyzer() { analyzer.refresh(); }

private:
    struct Section { juce::Rectangle<int> r; juce::String title; };
    void onTypeChanged (int type);
    void showTypeMenu();
    void resetCharacterDefaults (int type);
    void place (juce::Rectangle<int> r, std::initializer_list<int> params);

    SatLabProcessor& proc;
    int slot;
    int currentType = 1;

    juce::TextButton typeButton;
    std::array<std::unique_ptr<Knob>, satlab::P_Count> knobs;
    juce::ComboBox stereoBox;
    juce::ToggleButton autoGain { "Auto Gain" };
    AnalyzerComponent analyzer;
    std::vector<Section> sections;
    juce::Rectangle<int> hintRect;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stereoAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoAtt;
    std::unique_ptr<juce::ParameterAttachment> typeAtt;
};

//==============================================================================
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (std::atomic<float>& s) : src (s) {}
    void paint (juce::Graphics&) override;
private:
    std::atomic<float>& src;
};

//==============================================================================
class SatLabEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SatLabEditor (SatLabProcessor&);
    ~SatLabEditor() override;
    void showModPage() { selectSlot (satlab::kNumSlots); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void snapshotRefresh (int slot) { selectSlot (slot); for (auto& p : panels) p->refreshAnalyzer(); pollAllKnobs (*this); updateViz(); }
    void updateViz();

private:
    static void pollAllKnobs (juce::Component& c)
    {
        for (auto* ch : c.getChildren())
        {
            if (auto* k = dynamic_cast<Knob*> (ch)) k->pollMod();
            pollAllKnobs (*ch);
        }
    }
    void timerCallback() override;
    void selectSlot (int s);
    void paintBrand (juce::Graphics&);

    // calque d'habillage (marque, plaque d'en-tete) : ajoute apres la lave pour rester net par-dessus
    struct Deco : juce::Component
    {
        std::function<void (juce::Graphics&)> fn;
        Deco() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override { if (fn) fn (g); }
    } deco;

    SatLabProcessor& proc;
    ClayLookAndFeel laf;
    TestTone tone;

    juce::ComboBox presetBox, osBox, routingBox;
    juce::ToggleButton safety { "Safety Clip" };
    std::unique_ptr<Knob> inKnob, outKnob, mixKnob, widthKnob;
    LevelMeter inMeter, outMeter;
    std::array<std::unique_ptr<SlotPanel>, satlab::kNumSlots> panels;
    std::array<juce::TextButton, satlab::kNumSlots> tabs;
    std::array<juce::ToggleButton, satlab::kNumSlots> onButtons;
    std::array<std::unique_ptr<juce::ParameterAttachment>, satlab::kNumSlots> tabTypeAtt;
    juce::Slider testFreq, testLevel;
    juce::Label testFreqLabel, testLevelLabel;
    std::unique_ptr<ModPanel> modPanel;
    std::unique_ptr<VisualizerOverlay> viz;
    juce::ToggleButton vizToggle { "Lava" };
    juce::TextButton modTab { "MOD" };
    int selected = 0;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> osAtt, routingAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> safetyAtt;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, satlab::kNumSlots> onAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SatLabEditor)
};
