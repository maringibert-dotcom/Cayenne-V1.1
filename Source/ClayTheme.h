#pragma once
// EPICES - theme issu du logo (orange + noir chaud) : palette, polices embarquees, helpers de dessin, LookAndFeel
#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
// ---- palette tiree du logo EPICES : orange vif + noir chaud, rien d'autre (plus une creme pour la modulation)
const juce::Colour brand    { 0xffF2541B };      // fond orange du logo
const juce::Colour ink      { 0xff1E1B16 };      // noir chaud du logo
const juce::Colour sandTop  = ink;               // version "negatif" du logo : fond noir chaud, marque orange
const juce::Colour sandBot  = ink;
const juce::Colour slab     { 0xff2A2520 };      // panneaux un cran au-dessus du fond
const juce::Colour slabHi   { 0xff403930 };
const juce::Colour slabLo   { 0xff191613 };
const juce::Colour shade    { 0xff000000 };
const juce::Colour accent   = brand;             // anneaux, valeurs, ON
const juce::Colour accentHi { 0xffFF8A4D };
const juce::Colour glaze    { 0xffF6E7D0 };      // creme (modulation)
const juce::Colour glazeHi  { 0xffFFF4E4 };
const juce::Colour text     { 0xffF6E9DA };      // texte sur panneaux
const juce::Colour dim      { 0xffA89A88 };      // texte secondaire sur panneaux
const juce::Colour cream    { 0xffF6E9DA };
const juce::Colour fired    { 0xff141210 };      // fenetres
const juce::Colour firedLine{ 0xff3A342B };
const juce::Colour onBrand  = brand;             // libelles poses directement sur le fond
const juce::Colour onBrandDim { 0xff5A2610 };

// noms historiques
const juce::Colour bg = sandBot, panel = slab, panelHi = slabHi, edge = shade, accent2 = glaze;

juce::Font font (float height, bool bold = false);
const juce::Image& noiseTile();

void drawBackground (juce::Graphics&, juce::Rectangle<int> bounds);
void drawSlab (juce::Graphics&, juce::Rectangle<float>, float radius, float shadowAmount = 1.0f);          // plaque noire
void drawWell (juce::Graphics&, juce::Rectangle<float>, float radius, juce::Colour fill);                  // creux
void drawBlob (juce::Graphics&, juce::Rectangle<float> bounds, int seed, juce::Colour light, juce::Colour dark, float shadow = 1.0f, float wobble = 0.0f);
void drawLogo (juce::Graphics&, juce::Rectangle<float> bounds);   // la "caisse" a deux haut-parleurs du logo (ratio 1.83:1)
void drawRings (juce::Graphics&, juce::Point<float> centre, float outerR, bool dot, juce::Colour ring, juce::Colour inner);
}

//==============================================================================
class ClayLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ClayLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
};
