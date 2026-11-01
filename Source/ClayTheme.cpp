#include "ClayTheme.h"
#include "BinaryData.h"

// Theme EPICES : orange + noir chaud du logo, anneaux concentriques, typo grasse
namespace ui
{
static juce::Typeface::Ptr faceRegular()
{
    static juce::Typeface::Ptr f = juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoVariable_ttf, BinaryData::ArchivoVariable_ttfSize);
    return f;
}
static juce::Typeface::Ptr faceBold()
{
    static juce::Typeface::Ptr f = juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoBlackRegular_ttf, BinaryData::ArchivoBlackRegular_ttfSize);
    return f;
}

juce::Font font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (bold ? faceBold() : faceRegular()).withHeight (height));
}

const juce::Image& noiseTile()
{
    static juce::Image img = []
    {
        juce::Image im (juce::Image::ARGB, 160, 160, true);
        juce::Image::BitmapData bd (im, juce::Image::BitmapData::writeOnly);
        juce::Random rnd (4242);
        for (int y = 0; y < 160; ++y)
            for (int x = 0; x < 160; ++x)
            {
                const float n = rnd.nextFloat();
                const bool light = rnd.nextBool();
                bd.setPixelColour (x, y, light ? juce::Colour (0xffFFFFFF).withAlpha (n * n * 0.045f)
                                               : juce::Colour (0xff000000).withAlpha (n * n * 0.10f));
            }
        return im;
    }();
    return img;
}

static void fillNoise (juce::Graphics& g, const juce::Path& p, float alpha)
{
    g.saveState();
    g.reduceClipRegion (p.getBounds().getSmallestIntegerContainer().expanded (1));
    g.setTiledImageFill (noiseTile(), 0, 0, alpha);
    g.fillPath (p);
    g.restoreState();
}

void drawBackground (juce::Graphics& g, juce::Rectangle<int> b)
{
    g.setColour (ink);                         // aplat, comme le logo
    g.fillRect (b);
}

void drawSlab (juce::Graphics& g, juce::Rectangle<float> r, float rad, float)
{
    g.setColour (slab.withAlpha (0.95f));      // la lave transparait a peine
    g.fillRoundedRectangle (r, std::min (rad, 14.0f));
}

void drawWell (juce::Graphics& g, juce::Rectangle<float> r, float rad, juce::Colour fill)
{
    g.setColour (fill.darker (0.35f));
    g.fillRoundedRectangle (r, rad);
    g.setColour (firedLine);
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
}

// disque plat (le "cone" du haut-parleur)
void drawBlob (juce::Graphics& g, juce::Rectangle<float> b, int, juce::Colour, juce::Colour dark, float, float)
{
    g.setColour (dark);
    g.fillEllipse (b);
}

// anneau epais + disque interieur (+ point central), exactement le motif du logo
void drawRings (juce::Graphics& g, juce::Point<float> c, float R, bool dot, juce::Colour ring, juce::Colour inner)
{
    g.setColour (ring);
    g.fillEllipse (c.x - R, c.y - R, 2.0f * R, 2.0f * R);
    g.setColour (inner);
    const float r = R * 0.70f;
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
    if (dot)
    {
        g.setColour (ring);
        const float d = R * 0.22f;
        g.fillEllipse (c.x - d, c.y - d, 2.0f * d, 2.0f * d);
    }
}

void drawLogo (juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour (brand);
    g.fillRoundedRectangle (b, b.getHeight() * 0.36f);   // caisse en pilule du logo, en orange sur fond noir
    const float R = b.getHeight() * 0.333f;
    drawRings (g, { b.getX() + b.getWidth() * 0.296f, b.getCentreY() }, R, false, ink, brand);
    drawRings (g, { b.getX() + b.getWidth() * 0.704f, b.getCentreY() }, R, true,  ink, brand);
}
} // namespace ui

//==============================================================================
ClayLookAndFeel::ClayLookAndFeel()
{
    setDefaultSansSerifTypefaceName ({});
    setColour (juce::ResizableWindow::backgroundColourId, ui::ink);
    setColour (juce::Slider::textBoxTextColourId, ui::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, ui::accent.withAlpha (0.35f));
    setColour (juce::Label::textColourId, ui::dim);
    setColour (juce::Label::textWhenEditingColourId, ui::text);
    setColour (juce::TextEditor::textColourId, ui::text);
    setColour (juce::TextEditor::backgroundColourId, ui::fired);
    setColour (juce::TextEditor::highlightColourId, ui::accent.withAlpha (0.35f));
    setColour (juce::TextEditor::outlineColourId, ui::firedLine);
    setColour (juce::CaretComponent::caretColourId, ui::accent);
    setColour (juce::ComboBox::backgroundColourId, ui::slabLo);
    setColour (juce::ComboBox::textColourId, ui::text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, ui::accent);
    setColour (juce::PopupMenu::backgroundColourId, ui::slab);
    setColour (juce::PopupMenu::textColourId, ui::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::brand);
    setColour (juce::PopupMenu::highlightedTextColourId, ui::ink);
    setColour (juce::PopupMenu::headerTextColourId, ui::brand);
    setColour (juce::TextButton::buttonColourId, ui::slab);
    setColour (juce::TextButton::buttonOnColourId, ui::brand);
    setColour (juce::TextButton::textColourOffId, ui::text);
    setColour (juce::TextButton::textColourOnId, ui::ink);
}

juce::Font ClayLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return ui::font (14.0f, true); }
juce::Font ClayLookAndFeel::getComboBoxFont (juce::ComboBox&)         { return ui::font (14.0f, true); }
juce::Font ClayLookAndFeel::getPopupMenuFont()                        { return ui::font (14.5f); }
juce::Font ClayLookAndFeel::getLabelFont (juce::Label&)               { return ui::font (12.0f); }

void ClayLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);
    g.setColour (ui::slab);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (ui::brand);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.5f);
}

void ClayLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ringR = r - 3.0f;
    const float bodyR = ringR - 9.0f;

    const float ang = a0 + pos * (a1 - a0);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? a0 + (a1 - a0) * (float) s.valueToProportionOfLength (0.0) : a0;
    const auto stroke = juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, a0, a1, true);
    g.setColour (ui::slabHi);
    g.strokePath (track, stroke);

    // tube neon : halo + coeur chaud ; l'intensite vibre (buzz) et monte quand on tourne le bouton
    const float neon = (float) s.getProperties().getWithDefault ("neon", 1.0);
    const float charge = (float) s.getProperties().getWithDefault ("charge", 0.0);
    juce::Path val;
    val.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, std::min (from, ang), std::max (from, ang), true);
    const juce::Colour vc = s.isEnabled() ? ui::accent : ui::dim;
    if (s.isEnabled())
    {
        const float k = juce::jlimit (0.0f, 1.6f, neon);
        g.setColour (vc.withAlpha (juce::jmin (1.0f, 0.18f * k + 0.14f * charge)));
        g.strokePath (val, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (vc.interpolatedWith (ui::accentHi, juce::jlimit (0.0f, 1.0f, 0.5f * charge)).withAlpha (juce::jlimit (0.0f, 1.0f, 0.62f + 0.38f * k)));
        g.strokePath (val, stroke);
    }
    else { g.setColour (vc); g.strokePath (val, stroke); }

    const float mod = (float) s.getProperties().getWithDefault ("mod", 0.0);
    const bool hasMod = (bool) s.getProperties().getWithDefault ("hasMod", false);
    if (hasMod && std::abs (mod) > 0.0005f)
    {
        const float aM = a0 + juce::jlimit (0.0f, 1.0f, pos + mod) * (a1 - a0);
        juce::Path m;
        m.addCentredArc (c.x, c.y, ringR - 5.0f, ringR - 5.0f, 0.0f, std::min (ang, aM), std::max (ang, aM), true);
        g.setColour (ui::glaze);
        g.strokePath (m, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const juce::Point<float> d (c.x + (ringR - 5.0f) * std::sin (aM), c.y - (ringR - 5.0f) * std::cos (aM));
        g.fillEllipse (d.x - 2.8f, d.y - 2.8f, 5.6f, 5.6f);
    }
    else if (hasMod)
    {
        g.setColour (ui::glaze);
        g.fillEllipse (c.x - 1.8f, c.y - ringR + 3.2f - 1.8f + 0.0f, 0.0f, 0.0f);
        const juce::Point<float> d (c.x + (ringR - 5.0f) * std::sin (ang), c.y - (ringR - 5.0f) * std::cos (ang));
        g.fillEllipse (d.x - 1.6f, d.y - 1.6f, 3.2f, 3.2f);
    }

    // le "cone" : disque noir plus clair + point orange comme le centre du haut-parleur du logo
    ui::drawBlob (g, juce::Rectangle<float> (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f), 0, ui::cream, ui::slabHi.darker (0.25f), 0.0f);
    const float pr = bodyR * 0.62f;
    g.setColour (s.isEnabled() ? ui::brand : ui::dim);
    g.fillEllipse (c.x + pr * std::sin (ang) - 3.4f, c.y - pr * std::cos (ang) - 3.4f, 6.8f, 6.8f);
}

void ClayLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                                        juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, s); return; }
    const float cy = (float) y + (float) h * 0.5f;
    const float x0 = (float) x + 6.0f, x1 = (float) x + (float) w - 6.0f;
    g.setColour (ui::slabHi);
    g.fillRoundedRectangle (x0, cy - 1.5f, x1 - x0, 3.0f, 1.5f);

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroX = bipolar ? (float) s.getPositionOfValue (0.0) : x0;
    const float a = std::min (zeroX, sliderPos), b = std::max (zeroX, sliderPos);
    if (b - a > 1.0f)
    {
        g.setColour (s.findColour (juce::Slider::trackColourId));
        g.fillRoundedRectangle (a, cy - 1.5f, b - a, 3.0f, 1.5f);
    }
    if (bipolar) { g.setColour (ui::dim); g.fillRect (zeroX - 0.5f, cy - 5.0f, 1.0f, 10.0f); }
    g.setColour (ui::cream);
    g.fillEllipse (sliderPos - 6.0f, cy - 6.0f, 12.0f, 12.0f);
}

void ClayLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hl, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    g.setColour (hl ? ui::slabHi.brighter (0.15f) : ui::slabHi);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (on ? ui::brand : ui::slabHi);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.5f);
    const juce::Rectangle<float> led (r.getX() + 11.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setColour (on ? ui::accent : ui::slabHi.brighter (0.2f));
    g.fillEllipse (led);
    g.setColour (on ? ui::text : ui::dim);
    g.setFont (ui::font (13.0f, true));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (28.0f).withTrimmedRight (6.0f), juce::Justification::centredLeft);
}

void ClayLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hl, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    if (on)
    {
        g.setColour (ui::brand);                 // actif = orange plein, texte noir
        g.fillRoundedRectangle (r, 8.0f);
    }
    else
    {
        g.setColour (down ? ui::slabLo : (hl ? ui::slabHi.brighter (0.15f) : ui::slabHi));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (ui::firedLine);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    }
}

void ClayLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (1.0f);
    g.setColour (ui::slabHi);
    g.fillRoundedRectangle (r, 8.0f);
    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (ui::brand);
    g.fillPath (arrow);
}
