#include "PluginEditor.h"
#include "Presets.h"
#include "ModPanel.h"
#include "Visualizer.h"

using namespace satlab;

//==============================================================================
// Knob
//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& text)
    : apvtsRef (apvts), rnd (paramId.hashCode64())
{
    igniteDelay = rnd.nextInt (16);
    buzzPhase = rnd.nextFloat() * 6.28f;
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setInterceptsMouseClicks (false, false);
    label.setColour (juce::Label::textColourId, ui::dim);
    label.setFont (ui::font (10.5f, true));
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setColour (juce::Slider::textBoxTextColourId, ui::text);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setName (paramId);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 15);
    slider.setMouseDragSensitivity (260);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
    slider.onValueChange = [this] { charge = 1.0f; };
    startTimerHz (24);
    if (auto* p = apvts.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::linkMod (const std::atomic<float>* src, int destIndex)
{
    modSrc = src; destIdx = destIndex;
    slider.getProperties().set ("hasMod", true);
    slider.onRightClick = [this]
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Modulate with");
        const auto names = modSourceNames();
        for (int i = 1; i < names.size(); ++i) m.addItem (i, names[i]);
        m.addSeparator();
        m.addItem (100, "Remove modulation");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
        {
            if (r >= 1 && r < 100) assignModRoute (apvtsRef, r, destIdx);
            else if (r == 100) clearModRoutes (apvtsRef, destIdx);
        });
    };
}

void Knob::timerCallback()
{
    if (! isShowing() && ignition >= 1.0f) return;    // pas de calcul pour les pages cachees
    const float m = modSrc != nullptr ? modSrc->load() : 0.0f;
    bool dirty = false;
    if (std::abs (m - lastMod) > 0.0004f)
    {
        lastMod = m;
        slider.getProperties().set ("mod", m);
        dirty = true;
    }

    // neon : allumage par a-coups, puis leger buzz permanent avec de rares sautes
    float b = 1.0f;
    if (igniteDelay > 0) { --igniteDelay; b = 0.12f; }
    else if (ignition < 1.0f) { ignition = std::min (1.0f, ignition + 0.045f); b = rnd.nextFloat() < ignition ? 1.0f : 0.25f; }
    else
    {
        buzzPhase += 1.9f + 0.4f * rnd.nextFloat();
        b = 0.94f + 0.06f * std::sin (buzzPhase) + 0.03f * (rnd.nextFloat() - 0.5f);
        if (dipTicks > 0) { --dipTicks; b = 0.62f; }
        else if (rnd.nextFloat() < 0.012f) dipTicks = 1 + rnd.nextInt (2);
    }
    charge *= 0.86f;
    if (charge < 0.01f) charge = 0.0f;
    b *= 1.0f + 0.4f * charge;

    if (std::abs (b - lastNeon) > 0.012f || charge > 0.0f)
    {
        lastNeon = b;
        slider.getProperties().set ("neon", b);
        slider.getProperties().set ("charge", charge);
        dirty = true;
    }
    if (dirty) slider.repaint();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

//==============================================================================
// Analyseur
//==============================================================================
AnalyzerComponent::AnalyzerComponent (SatLabProcessor& p, int slotIndex, TestTone& t)
    : proc (p), slot (slotIndex), tone (t)
{
    startTimerHz (10);
}

std::vector<float> AnalyzerComponent::signature (const SlotParams& s, float f, float l)
{
    std::vector<float> v { (float) s.type, (float) s.stereoMode, s.rangeLo, s.rangeHi, s.driveDb, s.bias, s.asym,
                           s.focusFreq, s.focusGainDb, s.focusQ, s.deemph, s.sag, s.sagTimeMs,
                           s.s[0], s.s[1], s.s[2], s.s[3], s.warmth, s.warmFreq, s.harsh, s.harshFreq,
                           s.lsFreq, s.lsGainDb, s.midFreq, s.midGainDb, s.midQ, s.hsFreq, s.hsGainDb, f, l };
    return v;
}

void AnalyzerComponent::timerCallback()
{
    if (! isShowing()) return;
    const auto sp = readSlotParams (proc.params, slot);
    const auto sig = signature (sp, tone.freq.load(), tone.levelDb.load());
    if (sig != lastSig) { lastSig = sig; compute (sp); repaint(); }
}

void AnalyzerComponent::refresh()
{
    const auto sp = readSlotParams (proc.params, slot);
    lastSig = signature (sp, tone.freq.load(), tone.levelDb.load());
    compute (sp);
    repaint();
}

void AnalyzerComponent::compute (const SlotParams& spIn)
{
    SlotParams sp = spIn;
    sp.on = true; sp.mix = 1.0f; sp.outDb = 0.0f; sp.autoGain = false;

    const int os = 4;
    fs = 192000.0f;
    const int kbin = std::max (2, (int) std::lround (tone.freq.load() * (float) N / fs));
    f0 = (float) kbin * fs / (float) N;
    amp = dbToLin (tone.levelDb.load());

    auto s = std::make_unique<SatSlot>();
    s->prepare (fs, os);
    s->setParams (sp);

    const int warm = 16384, total = warm + N;
    std::vector<float> L ((size_t) total), R ((size_t) total);
    for (int i = 0; i < total; ++i)
        L[(size_t) i] = R[(size_t) i] = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * (double) kbin * (double) i / (double) N);
    for (int pos = 0; pos < total; pos += 1024)
    {
        const int n = std::min (1024, total - pos);
        s->process (L.data() + pos, R.data() + pos, n);
    }
    const auto& out = (sp.stereoMode == 4) ? R : L;

    // courbe de transfert : 2 periodes
    const int count = std::min (N, (int) (2.0f * (float) N / (float) kbin));
    xs.resize ((size_t) count); ys.resize ((size_t) count);
    float peak = 1.0f;
    for (int i = 0; i < count; ++i)
    {
        const int idx = total - count + i;
        xs[(size_t) i] = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * (double) kbin * (double) idx / (double) N);
        ys[(size_t) i] = out[(size_t) idx];
        peak = std::max (peak, std::abs (ys[(size_t) i]));
    }
    yScale = peak * 1.05f;

    // spectre (sinus aligne sur un bin : pas besoin de fenetre)
    juce::dsp::FFT fft (15);
    std::vector<float> data ((size_t) N * 2, 0.0f);
    for (int i = 0; i < N; ++i) data[(size_t) i] = out[(size_t) (warm + i)];
    fft.performFrequencyOnlyForwardTransform (data.data());
    specDb.resize ((size_t) N / 2);
    for (int k = 0; k < N / 2; ++k)
        specDb[(size_t) k] = 20.0f * std::log10 (2.0f * data[(size_t) k] / (float) N + 1e-9f);

    const float fund = 2.0f * data[(size_t) kbin] / (float) N + 1e-9f;
    double sumSq = 0.0;
    for (int h = 2; h * f0 < 20000.0f && h * kbin < N / 2; ++h)
    {
        const float a = 2.0f * data[(size_t) (h * kbin)] / (float) N;
        sumSq += (double) a * a;
        if (h <= 9) harmDb[h - 2] = 20.0f * std::log10 (a / fund + 1e-9f);
    }
    thdPct = 100.0f * (float) std::sqrt (sumSq) / fund;
    valid = true;
}

void AnalyzerComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    ui::drawWell (g, b, 12.0f, ui::fired.brighter (0.12f));
    if (! valid) return;

    auto area = b.reduced (10.0f);
    auto tf = area.removeFromTop (area.getHeight() * 0.46f);
    area.removeFromTop (14.0f);
    auto txt = area.removeFromBottom (36.0f);
    auto sp = area;

    g.setFont (ui::font (11.0f));
    g.setColour (ui::dim);
    g.drawText ("TRANSFER (dynamic)", tf.removeFromTop (12.0f), juce::Justification::left);
    tf.removeFromTop (2.0f);

    // ---- transfert
    {
        g.setColour (ui::firedLine);
        g.drawRect (tf, 1.0f);
        g.drawLine (tf.getX(), tf.getCentreY(), tf.getRight(), tf.getCentreY(), 0.6f);
        g.drawLine (tf.getCentreX(), tf.getY(), tf.getCentreX(), tf.getBottom(), 0.6f);

        const auto sparams = readSlotParams (proc.params, slot);
        const float gLin = dbToLin (sparams.driveDb);
        auto mapPt = [&] (float x, float y)
        {
            return juce::Point<float> (tf.getCentreX() + (x / amp) * tf.getWidth() * 0.5f,
                                       tf.getCentreY() - (y / yScale) * tf.getHeight() * 0.5f);
        };
        // ligne de reference : gain lineaire = drive
        g.saveState();
        g.reduceClipRegion (tf.toNearestInt());
        g.setColour (ui::cream.withAlpha (0.35f));
        g.drawLine (juce::Line<float> (mapPt (-amp, -amp * gLin), mapPt (amp, amp * gLin)), 1.0f);
        juce::Path p;
        for (size_t i = 0; i < xs.size(); ++i)
        {
            const auto pt = mapPt (xs[i], ys[i]);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        g.setColour (ui::accentHi);
        g.strokePath (p, juce::PathStrokeType (1.8f));
        g.restoreState();
    }

    // ---- spectre
    {
        g.setColour (ui::dim);
        g.drawText ("SPECTRUM  (test tone " + juce::String ((int) std::round (f0)) + " Hz, " + juce::String (tone.levelDb.load(), 0) + " dBFS)",
                    sp.removeFromTop (12.0f), juce::Justification::left);
        sp.removeFromTop (2.0f);
        const float dbMin = -110.0f, dbMax = 6.0f;
        auto yOf = [&] (float db) { return sp.getBottom() - (juce::jlimit (dbMin, dbMax, db) - dbMin) / (dbMax - dbMin) * sp.getHeight(); };
        g.setColour (ui::firedLine);
        g.drawRect (sp, 1.0f);
        for (float db : { -20.0f, -40.0f, -60.0f, -80.0f, -100.0f })
            g.drawLine (sp.getX(), yOf (db), sp.getRight(), yOf (db), 0.5f);
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = sp.getX() + std::log10 (f / 20.0f) / 3.0f * sp.getWidth();
            g.drawLine (x, sp.getY(), x, sp.getBottom(), 0.5f);
            g.setColour (ui::dim);
            g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) x + 2, (int) sp.getBottom() - 13, 30, 12, juce::Justification::left);
            g.setColour (ui::firedLine);
        }

        const float binHz = fs / (float) N;
        const int W = (int) sp.getWidth();
        juce::Path line;
        for (int i = 0; i < W; ++i)
        {
            const float fLo = 20.0f * std::pow (1000.0f, (float) i / (float) W);
            const float fHi = 20.0f * std::pow (1000.0f, (float) (i + 1) / (float) W);
            int kLo = (int) std::floor (fLo / binHz), kHi = (int) std::ceil (fHi / binHz);
            kLo = juce::jlimit (1, N / 2 - 1, kLo); kHi = juce::jlimit (kLo, N / 2 - 1, kHi);
            float m = -200.0f;
            for (int k = kLo; k <= kHi; ++k) m = std::max (m, specDb[(size_t) k]);
            const float x = sp.getX() + (float) i, y = yOf (m);
            if (i == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
        }
        juce::Path fill (line);
        fill.lineTo (sp.getRight(), sp.getBottom()); fill.lineTo (sp.getX(), sp.getBottom()); fill.closeSubPath();
        g.setColour (ui::accentHi.withAlpha (0.22f));
        g.fillPath (fill);
        g.setColour (ui::accentHi);
        g.strokePath (line, juce::PathStrokeType (1.3f));

        // reperes des harmoniques (paires = creme, impaires = orange)
        for (int h = 1; h <= 16 && h * f0 < 20000.0f; ++h)
        {
            const float x = sp.getX() + std::log10 (h * f0 / 20.0f) / 3.0f * sp.getWidth();
            g.setColour ((h % 2 == 0 ? ui::glazeHi : ui::accentHi).withAlpha (0.9f));
            g.fillRect (x - 1.0f, sp.getY() + 1.0f, 2.0f, 4.0f);
            if (h <= 6) { g.setFont (ui::font (10.0f)); g.drawText (juce::String (h), (int) x - 6, (int) sp.getY() + 5, 12, 11, juce::Justification::centred); }
        }
    }

    // ---- lecture numerique
    g.setFont (ui::font (12.0f));
    g.setColour (ui::cream);
    juce::String l1 = "THD " + juce::String (thdPct, thdPct < 10 ? 2 : 1) + " %";
    juce::String l2;
    for (int h = 2; h <= 7; ++h) l2 << "H" << h << " " << (harmDb[h - 2] < -99.0f ? juce::String ("<-99") : juce::String ((int) std::round (harmDb[h - 2]))) << "   ";
    g.drawText (l1, txt.removeFromTop (17.0f), juce::Justification::left);
    g.setColour (ui::dim);
    g.drawText (l2, txt, juce::Justification::left);
}

//==============================================================================
// Panneau d'un slot
//==============================================================================
static juce::String knobTitle (int p)
{
    switch (p)
    {
        case P_Drive: return "DRIVE";      case P_Bias: return "BIAS";       case P_Asym: return "ASYMMETRY";
        case P_Sag: return "SAG";          case P_SagT: return "SAG TIME";
        case P_FFreq: return "FREQ";       case P_FGain: return "GAIN";      case P_FQ: return "Q";       case P_Deemph: return "DE-EMPH";
        case P_Warm: return "WARMTH";      case P_WarmF: return "WARM FREQ"; case P_Harsh: return "HARSHNESS"; case P_HarshF: return "HARSH FREQ";
        case P_LsF: return "LOW FREQ";     case P_LsG: return "LOW GAIN";    case P_MidF: return "MID FREQ"; case P_MidG: return "MID GAIN";
        case P_MidQ: return "MID Q";       case P_HsF: return "HIGH FREQ";   case P_HsG: return "HIGH GAIN";
        case P_Lo: return "FROM";          case P_Hi: return "TO";
        case P_Pan: return "PAN";          case P_Width: return "WIDTH";
        case P_Mix: return "MIX";          case P_Out: return "LEVEL";
        default: return "";
    }
}

SlotPanel::SlotPanel (SatLabProcessor& p, int slotIndex, TestTone& t)
    : proc (p), slot (slotIndex), analyzer (p, slotIndex, t)
{
    auto& apvts = proc.apvts;
    for (int i = 0; i < P_Count; ++i)
        if (slotDef (i).kind == Kind::Float)
        {
            knobs[(size_t) i] = std::make_unique<Knob> (apvts, slotId (slot, i), knobTitle (i));
            addAndMakeVisible (*knobs[(size_t) i]);
            if (const int di = destIndexForSlotParam (slot, i); di > 0) knobs[(size_t) i]->linkMod (&proc.uiDestOff[di], di);
        }

    addAndMakeVisible (typeButton);
    typeButton.onClick = [this] { showTypeMenu(); };

    stereoBox.addItemList (stereoModeNames(), 1);
    addAndMakeVisible (stereoBox);
    stereoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, slotId (slot, P_Stereo), stereoBox);

    addAndMakeVisible (autoGain);
    autoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, slotId (slot, P_Auto), autoGain);

    addAndMakeVisible (analyzer);

    typeAtt = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter (slotId (slot, P_Type)),
                                                            [this] (float v) { onTypeChanged ((int) std::round (v)); });
    typeAtt->sendInitialUpdate();
}

void SlotPanel::onTypeChanged (int type)
{
    currentType = juce::jlimit (0, (int) NumSatTypes - 1, type);
    const auto& info = getTypeInfo (currentType);
    typeButton.setButtonText (info.name);
    for (int i = 0; i < 4; ++i)
    {
        auto& k = knobs[(size_t) (P_S1 + i)];
        if (info.label[i] != nullptr) { k->setText (juce::String (info.label[i]).toUpperCase()); k->setVisible (true); }
        else k->setVisible (false);
    }
    repaint();
}

void SlotPanel::showTypeMenu()
{
    juce::PopupMenu m;
    auto add = [&] (const char* heading, int first, int last)
    {
        m.addSectionHeader (heading);
        for (int t = first; t <= last; ++t) m.addItem (t + 1, getTypeInfo (t).name, true, t == currentType);
    };
    add ("Basic", Clean, Clean);
    add ("Waveshapers", Tanh, Exponential);
    add ("Analog gear", Triode, TapeClassic);
    add ("Pedals & circuits", DiodeClipper, ClassB);
    add ("Digital & creative", Bitcrusher, Jfet);
    add ("Full chains", Console, TubeAmp);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (typeButton).withMinimumWidth (typeButton.getWidth()),
                     [this] (int r)
                     {
                         if (r <= 0) return;
                         const int t = r - 1;
                         if (auto* prm = proc.apvts.getParameter (slotId (slot, P_Type)))
                         {
                             prm->beginChangeGesture();
                             prm->setValueNotifyingHost (prm->convertTo0to1 ((float) t));
                             prm->endChangeGesture();
                         }
                         resetCharacterDefaults (t);
                     });
}

void SlotPanel::resetCharacterDefaults (int type)
{
    const auto& info = getTypeInfo (type);
    for (int i = 0; i < 4; ++i)
        if (auto* prm = proc.apvts.getParameter (slotId (slot, P_S1 + i)))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (info.def[i]));
            prm->endChangeGesture();
        }
}

void SlotPanel::place (juce::Rectangle<int> r, std::initializer_list<int> params)
{
    int x = r.getX() + 12;
    for (int p : params)
    {
        knobs[(size_t) p]->setBounds (x, r.getY() + 28, 71, 130);
        x += 71;
    }
}

void SlotPanel::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (316);
    r.removeFromLeft (14);

    typeButton.setBounds (left.getX(), left.getY() + 18, left.getWidth(), 34);
    hintRect = juce::Rectangle<int> (left.getX() + 4, left.getY() + 58, left.getWidth() - 8, 46);
    analyzer.setBounds (left.withTrimmedTop (112).withHeight (3 * 168 + 2 * 10 - 112));   // meme bas que la 3e rangee de blocs

    sections.clear();
    const int x0 = r.getX(), kw = 71, secH = 168, gap = 10;
    auto secWidth = [&] (int n) { return n * kw + 24; };
    auto add = [&] (int x, int y, int n, const juce::String& title) { sections.push_back ({ { x, y, secWidth (n), secH }, title }); return sections.back().r; };

    int y = 0;
    auto s1 = add (x0, y, 5, "DRIVE  /  BIAS  /  DYNAMICS");
    place (s1, { P_Drive, P_Bias, P_Asym, P_Sag, P_SagT });
    auto s2 = add (s1.getRight() + gap, y, 4, "CHARACTER");
    place (s2, { P_S1, P_S2, P_S3, P_S4 });
    auto s2b = add (s2.getRight() + gap, y, 2, "PAN / WIDTH");
    place (s2b, { P_Pan, P_Width });

    y += secH + gap;
    auto s3 = add (x0, y, 4, "FOCUS  (PRE-EMPHASIS)");
    place (s3, { P_FFreq, P_FGain, P_FQ, P_Deemph });
    auto s4 = add (s3.getRight() + gap, y, 4, "TIMBRE  (WARMTH / HARSHNESS)");
    place (s4, { P_Warm, P_WarmF, P_Harsh, P_HarshF });
    auto s5 = add (s4.getRight() + gap, y, 2, "BAND");
    place (s5, { P_Lo, P_Hi });

    y += secH + gap;
    auto s6 = add (x0, y, 7, "TONE EQ  (POST)");
    place (s6, { P_LsF, P_LsG, P_MidF, P_MidG, P_MidQ, P_HsF, P_HsG });
    auto s7 = add (s6.getRight() + gap, y, 2, "OUTPUT");
    place (s7, { P_Mix, P_Out });
    juce::Rectangle<int> s8 { s7.getRight() + gap, y, r.getRight() - (s7.getRight() + gap), secH };
    sections.push_back ({ s8, "STEREO MODE" });
    stereoBox.setBounds (s8.getX() + 14, s8.getY() + 40, s8.getWidth() - 28, 32);
    autoGain.setBounds (s8.getX() + 14, s8.getY() + 88, s8.getWidth() - 28, 32);
}

void SlotPanel::paint (juce::Graphics& g)
{
    for (auto& s : sections)
    {
        auto rf = s.r.toFloat();
        ui::drawSlab (g, rf, 16.0f);
        g.setColour (ui::brand);
        g.setFont (ui::font (11.0f, true).withExtraKerningFactor (0.06f));
        juce::String title = s.title;
        if (title == "CHARACTER") title += "   -   " + juce::String (getTypeInfo (currentType).name).toUpperCase();
        g.drawText (title, s.r.getX() + 16, s.r.getY() + 9, s.r.getWidth() - 24, 14, juce::Justification::left);
        if (s.title == "CHARACTER" && getTypeInfo (currentType).label[0] == nullptr)
        {
            g.setColour (ui::dim);
            g.drawFittedText ("This circuit has no extra controls.\nShape it with Drive, Bias, Asymmetry, Focus, Warmth and Harshness.",
                              s.r.reduced (16, 36), juce::Justification::centred, 3);
        }
    }
    g.setColour (ui::onBrand);
    g.setFont (ui::font (11.0f, true));
    g.drawText ("SATURATION TYPE", 4, 0, 200, 14, juce::Justification::left);
    g.setColour (ui::text.withAlpha (0.85f));
    g.setFont (ui::font (13.0f));
    g.drawFittedText (getTypeInfo (currentType).hint, hintRect, juce::Justification::topLeft, 3);
}

//==============================================================================
void LevelMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    ui::drawWell (g, b, 5.0f, ui::slabLo);
    const float db = juce::Decibels::gainToDecibels (src.load(), -60.0f);
    const float f = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
    auto bar = b.reduced (2.5f);
    bar = bar.withTrimmedTop (bar.getHeight() * (1.0f - f));
    g.setColour (db > 0.0f ? juce::Colour (0xffB3261E) : (db > -9.0f ? ui::accent : ui::glaze));
    g.fillRoundedRectangle (bar, 3.0f);
}

//==============================================================================
// Editeur
//==============================================================================
SatLabEditor::SatLabEditor (SatLabProcessor& p)
    : AudioProcessorEditor (&p), proc (p), inMeter (p.inLevel), outMeter (p.outLevel)
{
    setLookAndFeel (&laf);
    viz = std::make_unique<VisualizerOverlay> (proc);
    addAndMakeVisible (*viz);                                  // premier enfant : dessine derriere tous les panneaux
    deco.fn = [this] (juce::Graphics& g) { paintBrand (g); };
    addAndMakeVisible (deco);

    inKnob  = std::make_unique<Knob> (proc.apvts, "in",  "INPUT");
    outKnob = std::make_unique<Knob> (proc.apvts, "out", "OUTPUT");
    mixKnob = std::make_unique<Knob> (proc.apvts, "mix", "DRY / WET");
    widthKnob = std::make_unique<Knob> (proc.apvts, "width", "WIDTH");
    { int i = 0; for (auto* k : { inKnob.get(), outKnob.get(), mixKnob.get(), widthKnob.get() })
      { addAndMakeVisible (*k); { const int di = destIndexForGlobal (i++); k->linkMod (&proc.uiDestOff[di], di); } } }

    auto& presets = getPresets();
    for (size_t i = 0; i < presets.size(); ++i) presetBox.addItem (presets[i].name, (int) i + 1);
    presetBox.setSelectedItemIndex (juce::jlimit (0, (int) presets.size() - 1, proc.getCurrentProgram()), juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    osBox.addItemList (osNames(), 1);       addAndMakeVisible (osBox);
    routingBox.addItemList (routingNames(), 1); addAndMakeVisible (routingBox);
    osAtt      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "os", osBox);
    routingAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "routing", routingBox);
    addAndMakeVisible (safety);
    safetyAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "safety", safety);
    addAndMakeVisible (inMeter); addAndMakeVisible (outMeter);

    for (int s = 0; s < kNumSlots; ++s)
    {
        panels[(size_t) s] = std::make_unique<SlotPanel> (proc, s, tone);
        addChildComponent (*panels[(size_t) s]);

        tabs[(size_t) s].setClickingTogglesState (false);
        tabs[(size_t) s].onClick = [this, s] { selectSlot (s); };
        addAndMakeVisible (tabs[(size_t) s]);

        onButtons[(size_t) s].setButtonText ("ON");
        addAndMakeVisible (onButtons[(size_t) s]);
        onAtt[(size_t) s] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, slotId (s, P_On), onButtons[(size_t) s]);

        tabTypeAtt[(size_t) s] = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (slotId (s, P_Type)),
            [this, s] (float v) { tabs[(size_t) s].setButtonText ("SLOT " + juce::String (s + 1) + "   " + getTypeInfo ((int) std::round (v)).name); });
        tabTypeAtt[(size_t) s]->sendInitialUpdate();
    }

    modPanel = std::make_unique<ModPanel> (proc);
    addChildComponent (*modPanel);
    modTab.onClick = [this] { selectSlot (kNumSlots); };
    addAndMakeVisible (modTab);

    auto setupSlider = [this] (juce::Slider& sl, juce::Label& lb, const juce::String& text)
    {
        sl.setSliderStyle (juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
        styleLinearSlider (sl);
        sl.setColour (juce::Slider::trackColourId, ui::brand);
        sl.setColour (juce::Slider::textBoxTextColourId, ui::text);
        addAndMakeVisible (sl);
        lb.setText (text, juce::dontSendNotification);
        lb.setColour (juce::Label::textColourId, ui::onBrand); lb.setFont (ui::font (11.0f, true));
        addAndMakeVisible (lb);
    };
    setupSlider (testFreq, testFreqLabel, "TEST TONE FREQ");
    setupSlider (testLevel, testLevelLabel, "LEVEL");
    testFreq.setRange (40.0, 10000.0, 1.0);
    testFreq.setSkewFactorFromMidPoint (800.0);
    testFreq.setTextValueSuffix (" Hz");
    testFreq.setValue (1000.0, juce::dontSendNotification);
    testFreq.onValueChange = [this] { tone.freq = (float) testFreq.getValue(); };
    testLevel.setRange (-40.0, 0.0, 0.5);
    testLevel.setTextValueSuffix (" dBFS");
    testLevel.setValue (-12.0, juce::dontSendNotification);
    testLevel.onValueChange = [this] { tone.levelDb = (float) testLevel.getValue(); };

    for (auto& pn : panels) pn->setBufferedToImage (true);     // evite de redessiner les ombres a chaque image du visualiseur
    modPanel->setBufferedToImage (true);

    vizToggle.setToggleState (true, juce::dontSendNotification);
    vizToggle.onClick = [this] { viz->setVisible (vizToggle.getToggleState()); };
    addAndMakeVisible (vizToggle);

    selectSlot (0);
    setSize (1240, 770);      // en dernier : declenche resized() une fois tous les composants crees
    startTimerHz (30);
}

SatLabEditor::~SatLabEditor() { setLookAndFeel (nullptr); }

void SatLabEditor::selectSlot (int s)
{
    selected = s;
    const bool mod = (s >= kNumSlots);
    for (int i = 0; i < kNumSlots; ++i)
    {
        panels[(size_t) i]->setVisible (i == s);
        tabs[(size_t) i].setToggleState (i == s, juce::dontSendNotification);
    }
    modPanel->setVisible (mod);
    modTab.setToggleState (mod, juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &testFreq, &testLevel, &testFreqLabel, &testLevelLabel }) c->setVisible (! mod);
}

void SatLabEditor::updateViz() { for (int i = 0; i < 160; ++i) viz->update(); }

void SatLabEditor::timerCallback()
{
    inMeter.repaint(); outMeter.repaint();
}

void SatLabEditor::paint (juce::Graphics& g)
{
    ui::drawBackground (g, getLocalBounds());
}

void SatLabEditor::paintBrand (juce::Graphics& g)
{
    // plaque noire qui porte les reglages globaux (comme la caisse du logo)
    const float sx = (float) osBox.getX() - 18.0f;
    ui::drawSlab (g, { sx, 14.0f, (float) (getWidth() - 14) - sx, 100.0f }, 14.0f);

    // plaque du pied de page (reglages du signal de test), lisible meme sur les flammes
    ui::drawSlab (g, { 14.0f, (float) vizToggle.getY() - 2.0f, (float) vizToggle.getX() - 24.0f, (float) vizToggle.getHeight() + 4.0f }, 14.0f);

    // marque : la caisse a deux haut-parleurs + "epices" en grasse minuscule + "audio" espace
    ui::drawLogo (g, { 14.0f, 24.0f, 84.0f, 46.0f });
    g.setColour (ui::brand);
    g.setFont (ui::font (44.0f, true).withExtraKerningFactor (-0.025f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc3\xa9pices")), 114, 12, 240, 52, juce::Justification::left, false);
    g.setFont (ui::font (13.0f, true).withExtraKerningFactor (0.55f));
    g.drawText ("audio", 117, 64, 90, 18, juce::Justification::left, false);
    g.setFont (ui::font (12.0f));
    g.setColour (ui::dim);
    g.drawText ("CAYENNE  -  spicy saturation", 196, 65, 200, 16, juce::Justification::left, false);

    g.setColour (ui::onBrand);
    g.setFont (ui::font (10.5f, true));
    g.drawText ("PRESET", presetBox.getX() + 4, presetBox.getY() - 15, 100, 13, juce::Justification::left);
    g.setColour (ui::dim);
    g.drawText ("OVERSAMP.", osBox.getX() + 4, osBox.getY() - 15, 100, 13, juce::Justification::left);
    g.drawText ("ROUTING", routingBox.getX() + 4, routingBox.getY() - 15, 100, 13, juce::Justification::left);
    g.drawText ("IN", inMeter.getX() - 7, inMeter.getBottom() + 2, 24, 12, juce::Justification::centred);
    g.drawText ("OUT", outMeter.getX() - 7, outMeter.getBottom() + 2, 24, 12, juce::Justification::centred);
}

void SatLabEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto header = r.removeFromTop (100);
    r.removeFromTop (6);
    auto tabRow = r.removeFromTop (42);
    r.removeFromTop (10);
    auto footer = r.removeFromBottom (34);

    auto hr = header;
    hr.removeFromRight (18);                                   // marge interieure de la plaque d'en-tete
    outMeter.setBounds (hr.removeFromRight (12).withTrimmedTop (12).withTrimmedBottom (26));
    hr.removeFromRight (16);
    inMeter.setBounds (hr.removeFromRight (12).withTrimmedTop (12).withTrimmedBottom (26));
    hr.removeFromRight (14);
    widthKnob->setBounds (hr.removeFromRight (68).withTrimmedTop (10));
    mixKnob->setBounds (hr.removeFromRight (68).withTrimmedTop (10));
    outKnob->setBounds (hr.removeFromRight (68).withTrimmedTop (10));
    inKnob->setBounds (hr.removeFromRight (68).withTrimmedTop (10));
    hr.removeFromRight (14);
    safety.setBounds (hr.removeFromRight (118).withSizeKeepingCentre (118, 32).translated (0, 18));
    hr.removeFromRight (8);
    routingBox.setBounds (hr.removeFromRight (92).withSizeKeepingCentre (92, 30).translated (0, 18));
    hr.removeFromRight (8);
    osBox.setBounds (hr.removeFromRight (72).withSizeKeepingCentre (72, 30).translated (0, 18));
    presetBox.setBounds (366, header.getY() + 49, 156, 30);

    modTab.setBounds (tabRow.removeFromRight (120));
    tabRow.removeFromRight (10);
    const int tw = (tabRow.getWidth() - 3 * 10) / 4;
    for (int i = 0; i < kNumSlots; ++i)
    {
        auto t = tabRow.removeFromLeft (tw);
        tabRow.removeFromLeft (10);
        onButtons[(size_t) i].setBounds (t.removeFromLeft (66));
        t.removeFromLeft (4);
        tabs[(size_t) i].setBounds (t);
    }

    for (auto& p : panels) p->setBounds (r);
    modPanel->setBounds (r);
    viz->setBounds (getLocalBounds());
    deco.setBounds (getLocalBounds());

    auto f = footer;
    vizToggle.setBounds (f.removeFromRight (150).withTrimmedTop (2).withTrimmedBottom (2));
    testFreqLabel.setBounds (f.removeFromLeft (140));
    testFreq.setBounds (f.removeFromLeft (330));
    f.removeFromLeft (20);
    testLevelLabel.setBounds (f.removeFromLeft (60));
    testLevel.setBounds (f.removeFromLeft (300));
}
