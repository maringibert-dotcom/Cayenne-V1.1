#include "ModPanel.h"

using namespace satlab;

//==============================================================================
LfoScope::LfoScope (SatLabProcessor& p, int lfoIndex) : proc (p), idx (lfoIndex), history (180, 0.0f)
{
    startTimerHz (45);
}

void LfoScope::timerCallback()
{
    if (! isShowing()) return;
    history[(size_t) head] = proc.uiSrc[SrcLfo1 + idx].load();
    head = (head + 1) % (int) history.size();
    repaint();
}

void LfoScope::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    ui::drawWell (g, b, 10.0f, ui::fired.brighter (0.12f));
    auto a = b.reduced (10.0f, 8.0f);
    const bool uni = proc.apvts.getRawParameterValue (lfoId (idx, "uni"))->load() > 0.5f;
    const int shape = (int) std::round (proc.apvts.getRawParameterValue (lfoId (idx, "shape"))->load());
    const float phaseOff = proc.apvts.getRawParameterValue (lfoId (idx, "phase"))->load();

    auto yOf = [&] (float v) { return uni ? a.getBottom() - v * a.getHeight() : a.getCentreY() - v * a.getHeight() * 0.5f; };
    g.setColour (ui::firedLine);
    g.drawLine (a.getX(), yOf (0.0f), a.getRight(), yOf (0.0f), 1.0f);

    juce::Path p;
    if (shape <= 4)   // formes periodiques : un cycle + point mobile
    {
        auto val = [&] (float ph)
        {
            float v = 0.0f;
            switch (shape)
            {
                case 0: v = std::sin (2.0f * juce::MathConstants<float>::pi * ph); break;
                case 1: v = ph < 0.25f ? 4.0f * ph : (ph < 0.75f ? 2.0f - 4.0f * ph : 4.0f * ph - 4.0f); break;
                case 2: v = 2.0f * ph - 1.0f; break;
                case 3: v = 1.0f - 2.0f * ph; break;
                default: v = ph < 0.5f ? 1.0f : -1.0f; break;
            }
            return uni ? 0.5f * v + 0.5f : v;
        };
        const int W = (int) a.getWidth();
        for (int i = 0; i <= W; ++i)
        {
            const float ph = std::fmod ((float) i / (float) W + phaseOff, 1.0f);
            const auto pt = juce::Point<float> (a.getX() + (float) i, yOf (val (ph)));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        g.setColour (ui::accentHi);
        g.strokePath (p, juce::PathStrokeType (2.0f));
        const float ph = proc.uiLfoPhase[idx].load();
        const float cx = a.getX() + std::fmod (ph - phaseOff + 1.0f, 1.0f) * a.getWidth();
        const float cy = yOf (proc.uiSrc[SrcLfo1 + idx].load());
        g.setColour (ui::glazeHi.withAlpha (0.35f));
        g.drawLine (cx, a.getY(), cx, a.getBottom(), 1.0f);
        g.setColour (ui::cream);   g.fillEllipse (cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
        g.setColour (ui::glazeHi); g.fillEllipse (cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);
    }
    else              // aleatoire : historique recent
    {
        const int n = (int) history.size();
        for (int i = 0; i < n; ++i)
        {
            const float v = history[(size_t) ((head + i) % n)];
            const auto pt = juce::Point<float> (a.getX() + a.getWidth() * (float) i / (float) (n - 1), yOf (v));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        g.setColour (ui::accentHi);
        g.strokePath (p, juce::PathStrokeType (2.0f));
        const float cy = yOf (proc.uiSrc[SrcLfo1 + idx].load());
        g.setColour (ui::cream);   g.fillEllipse (a.getRight() - 5.0f, cy - 5.0f, 10.0f, 10.0f);
        g.setColour (ui::glazeHi); g.fillEllipse (a.getRight() - 3.5f, cy - 3.5f, 7.0f, 7.0f);
    }
}

//==============================================================================
LfoCard::LfoCard (SatLabProcessor& p, int lfoIndex)
    : proc (p), idx (lfoIndex),
      rate (p.apvts, lfoId (lfoIndex, "rate"), "RATE"), phase (p.apvts, lfoId (lfoIndex, "phase"), "PHASE"),
      scope (p, lfoIndex)
{
    shapeBox.addItemList (lfoShapeNames(), 1);
    divBox.addItemList (lfoDivNames(), 1);
    addAndMakeVisible (shapeBox); addAndMakeVisible (divBox);
    addAndMakeVisible (rate); addAndMakeVisible (phase);
    addAndMakeVisible (sync); addAndMakeVisible (uni); addAndMakeVisible (scope);
    shapeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, lfoId (idx, "shape"), shapeBox);
    divAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, lfoId (idx, "div"), divBox);
    syncAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, lfoId (idx, "sync"), sync);
    uniAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, lfoId (idx, "uni"), uni);
    const int di = destIndexForLfoRate (idx);
    rate.linkMod (&p.uiDestOff[di], di);
    syncWatch = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter (lfoId (idx, "sync")), [this] (float) { updateSyncVisibility(); });
    syncWatch->sendInitialUpdate();
}

void LfoCard::updateSyncVisibility()
{
    const bool s = proc.apvts.getRawParameterValue (lfoId (idx, "sync"))->load() > 0.5f;
    rate.setVisible (! s);
    divBox.setVisible (s);
    repaint();
}

void LfoCard::paint (juce::Graphics& g)
{
    ui::drawSlab (g, getLocalBounds().toFloat().reduced (3.0f), 16.0f);
    g.setColour (ui::text);
    g.setFont (ui::font (12.5f, true).withExtraKerningFactor (0.08f));
    g.drawText ("LFO " + juce::String (idx + 1), 18, 12, 80, 16, juce::Justification::left);
    const bool s = proc.apvts.getRawParameterValue (lfoId (idx, "sync"))->load() > 0.5f;
    if (s)
    {
        g.setColour (ui::dim);
        g.setFont (ui::font (10.5f, true));
        g.drawText ("DIVISION", rate.getBounds().withHeight (14), juce::Justification::centred);
    }
}

void LfoCard::resized()
{
    auto r = getLocalBounds().reduced (3);
    shapeBox.setBounds (r.getRight() - 150, r.getY() + 8, 138, 28);
    scope.setBounds (r.getX() + 12, r.getY() + 44, r.getWidth() - 24, 56);
    rate.setBounds (r.getX() + 12, r.getY() + 106, 72, 100);
    divBox.setBounds (r.getX() + 12, r.getY() + 146, 76, 28);
    phase.setBounds (r.getX() + 90, r.getY() + 106, 72, 100);
    sync.setBounds (r.getX() + 168, r.getY() + 118, r.getWidth() - 178, 32);
    uni.setBounds (r.getX() + 168, r.getY() + 158, r.getWidth() - 178, 32);
}

//==============================================================================
void EnvMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    ui::drawWell (g, b, 6.0f, ui::slabLo);
    const float v = juce::jlimit (0.0f, 1.0f, proc.uiSrc[SrcEnv].load());
    auto bar = b.reduced (3.0f);
    bar = bar.withTrimmedTop (bar.getHeight() * (1.0f - v));
    g.setColour (ui::glaze);
    g.fillRoundedRectangle (bar, 3.0f);
}

//==============================================================================
RouteRow::RouteRow (SatLabProcessor& p, int routeIndex) : proc (p), idx (routeIndex)
{
    srcBox.addItemList (modSourceNames(), 1);
    addAndMakeVisible (srcBox);
    srcAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, routeId (idx, "src"), srcBox);

    addAndMakeVisible (dstButton);
    dstButton.onClick = [this] { showTargetMenu(); };
    dstAtt = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter (routeId (idx, "dst")),
        [this] (float v) { const auto& d = modDests(); dstButton.setButtonText (d[(size_t) juce::jlimit (0, (int) d.size() - 1, (int) std::round (v))].label); });
    dstAtt->sendInitialUpdate();

    amount.setSliderStyle (juce::Slider::LinearHorizontal);
    amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 22);
    styleLinearSlider (amount);
    amount.setColour (juce::Slider::trackColourId, ui::glaze);
    amount.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (amount);
    amtAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, routeId (idx, "amt"), amount);
}

void RouteRow::showTargetMenu()
{
    const auto& d = modDests();
    juce::PopupMenu root;
    root.addItem (1, "None");
    for (int s = 0; s < kNumSlots; ++s)
    {
        juce::PopupMenu sub;
        for (size_t i = 1; i < d.size(); ++i)
            if (d[i].kind == ModDest::Slot && d[i].slot == s) sub.addItem ((int) i + 1, slotDef (d[i].param).name);
        root.addSubMenu ("Slot " + juce::String (s + 1), sub);
    }
    juce::PopupMenu glob, mods;
    for (size_t i = 1; i < d.size(); ++i)
    {
        if (d[i].kind == ModDest::Global)  glob.addItem ((int) i + 1, d[i].label.fromFirstOccurrenceOf ("Global ", false, false));
        if (d[i].kind == ModDest::LfoRate) mods.addItem ((int) i + 1, d[i].label);
    }
    root.addSubMenu ("Global", glob);
    root.addSubMenu ("Modulators", mods);
    root.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (dstButton), [this] (int r)
    {
        if (r > 0) setParamReal (proc.apvts, routeId (idx, "dst"), (float) (r - 1));
    });
}

void RouteRow::paint (juce::Graphics& g)
{
    g.setColour (ui::accent);
    g.setFont (ui::font (13.0f, true));
    g.drawText (juce::String (idx + 1), 0, 0, 20, getHeight(), juce::Justification::centred);
    g.setColour (ui::dim);
    g.setFont (ui::font (11.0f, true));
    g.drawText ("to", srcBox.getRight(), 0, dstButton.getX() - srcBox.getRight(), getHeight(), juce::Justification::centred);
}

void RouteRow::resized()
{
    auto r = getLocalBounds().reduced (0, 2);
    r.removeFromLeft (24);
    srcBox.setBounds (r.removeFromLeft (112));
    r.removeFromLeft (26);
    dstButton.setBounds (r.removeFromLeft (170));
    r.removeFromLeft (10);
    amount.setBounds (r);
}

//==============================================================================
ModPanel::ModPanel (SatLabProcessor& p)
    : proc (p),
      envAtt (p.apvts, "e_att", "ATTACK"), envRel (p.apvts, "e_rel", "RELEASE"), envSens (p.apvts, "e_gain", "SENSITIVITY"),
      macro1 (p.apvts, "mac1", "MACRO 1"), macro2 (p.apvts, "mac2", "MACRO 2"), envMeter (p)
{
    for (int i = 0; i < kNumLfos; ++i) { lfos[(size_t) i] = std::make_unique<LfoCard> (p, i); addAndMakeVisible (*lfos[(size_t) i]); }
    for (auto* k : { &envAtt, &envRel, &envSens, &macro1, &macro2 }) addAndMakeVisible (*k);
    addAndMakeVisible (envMeter);
    for (int i = 0; i < kNumRoutes; ++i) { rows[(size_t) i] = std::make_unique<RouteRow> (p, i); addAndMakeVisible (*rows[(size_t) i]); }
}

void ModPanel::resized()
{
    auto r = getLocalBounds();
    const int gap = 10;
    auto row1 = r.removeFromTop (214);
    const int cw = (row1.getWidth() - 3 * gap) / 4;
    for (int i = 0; i < kNumLfos; ++i) { lfos[(size_t) i]->setBounds (row1.removeFromLeft (cw)); row1.removeFromLeft (gap); }

    r.removeFromTop (gap);
    auto row2 = r.removeFromTop (156);
    envRect = row2.removeFromLeft (3 * 74 + 56 + 24);
    row2.removeFromLeft (gap);
    macroRect = row2.removeFromLeft (2 * 74 + 24);
    row2.removeFromLeft (gap);
    helpRect = row2;

    int x = envRect.getX() + 12;
    for (auto* k : { &envAtt, &envRel, &envSens }) { k->setBounds (x, envRect.getY() + 28, 74, 124); x += 74; }
    envMeter.setBounds (envRect.getRight() - 40, envRect.getY() + 40, 18, 90);
    x = macroRect.getX() + 12;
    for (auto* k : { &macro1, &macro2 }) { k->setBounds (x, macroRect.getY() + 28, 74, 124); x += 74; }

    r.removeFromTop (gap);
    matrixRect = r;
    auto m = matrixRect.reduced (14, 0);
    m.removeFromTop (30);
    const int colW = (m.getWidth() - 24) / 2;
    auto left = m.removeFromLeft (colW); m.removeFromLeft (24);
    const int rh = std::min (36, m.getHeight() / 4);
    for (int i = 0; i < 4; ++i)
    {
        rows[(size_t) i]->setBounds (left.removeFromTop (rh));
        rows[(size_t) (i + 4)]->setBounds (m.removeFromTop (rh));
    }
}

void ModPanel::paint (juce::Graphics& g)
{
    auto title = [&] (juce::Rectangle<int> r, const juce::String& t)
    {
        ui::drawSlab (g, r.toFloat().reduced (3.0f), 16.0f);
        g.setColour (ui::brand);
        g.setFont (ui::font (11.0f, true).withExtraKerningFactor (0.06f));
        g.drawText (t, r.getX() + 18, r.getY() + 11, r.getWidth() - 30, 14, juce::Justification::left);
    };
    title (envRect, "ENVELOPE FOLLOWER");
    title (macroRect, "MACROS");
    title (helpRect, "HOW TO MODULATE");
    title (matrixRect, "MODULATION MATRIX   -   8 ROUTES");

    g.setColour (ui::text.withAlpha (0.85f));
    g.setFont (ui::font (13.0f));
    g.drawFittedText ("Right-click any knob and pick a modulator: a route is created for you (cream ring = modulation range).\n\n"
                      "Fine-tune it here: choose a source, a target and an amount (positive or negative). "
                      "Tempo Sync locks an LFO to your DAW's beat. The envelope follows the input level; macros are knobs you can automate.",
                      helpRect.reduced (22, 34), juce::Justification::topLeft, 7);
}
