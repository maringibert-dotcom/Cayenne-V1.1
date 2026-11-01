#include "Visualizer.h"
#include "Params.h"

using namespace satlab;

VisualizerOverlay::VisualizerOverlay (SatLabProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    setOpaque (false);
    for (int i = 0; i < kN; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (kN - 1));
    for (int i = 0; i < kBlobs; ++i)
    {
        auto& b = blobs[(size_t) i];
        b.homeX = 0.12f + 0.76f * rng.nextFloat();
        b.homeY = 0.25f + 0.55f * rng.nextFloat();
        b.ax = 0.06f + 0.10f * rng.nextFloat();     // amplitude du mouvement (fraction de la fenetre)
        b.ay = 0.14f + 0.22f * rng.nextFloat();
        b.fx = 0.006f + 0.010f * rng.nextFloat();   // lenteur de la lampe a lave
        b.fy = 0.005f + 0.011f * rng.nextFloat();
        b.phase = rng.nextFloat() * 6.28f;
        b.size = 0.6f + 0.8f * rng.nextFloat();
    }
    startTimerHz (30);
}

void VisualizerOverlay::spawnEmber (float x, float y, float power)
{
    Ember e;
    e.x = x; e.y = y;
    e.vx = (rng.nextFloat() - 0.5f) * 0.8f;
    e.vy = -(0.8f + 2.4f * rng.nextFloat()) * (0.7f + 0.8f * power);
    e.maxLife = e.life = 40.0f + 90.0f * rng.nextFloat();
    e.size = 1.0f + 2.2f * rng.nextFloat() * (0.6f + power);
    e.phase = rng.nextFloat() * 6.28f;
    embers.push_back (e);
}

void VisualizerOverlay::update()
{
    const int head = proc.vizHead.load (std::memory_order_acquire);
    std::vector<float> data ((size_t) kN * 2, 0.0f);
    for (int i = 0; i < kN; ++i)
    {
        const size_t idx = (size_t) ((head - (kN - i) + SatLabProcessor::kVizSize * 2) & (SatLabProcessor::kVizSize - 1));
        data[(size_t) i] = 0.5f * (proc.vizBufL[idx].load (std::memory_order_relaxed) + proc.vizBufR[idx].load (std::memory_order_relaxed)) * window[(size_t) i];
    }
    fft.performFrequencyOnlyForwardTransform (data.data());

    const float binHz = (float) proc.currentSampleRate() / (float) kN;
    float maxLevel = 0.0f, hf = 0.0f, all = 1e-6f, bass = 0.0f;
    for (int b = 0; b < kBands; ++b)
    {
        const float f0 = 30.0f * std::pow (16000.0f / 30.0f, (float) b / (float) kBands);
        const float f1 = 30.0f * std::pow (16000.0f / 30.0f, (float) (b + 1) / (float) kBands);
        const int k0 = juce::jlimit (1, kN / 2 - 1, (int) std::floor (f0 / binHz));
        const int k1 = juce::jlimit (k0, kN / 2 - 1, (int) std::ceil (f1 / binHz));
        float m = 0.0f; for (int k = k0; k <= k1; ++k) m = std::max (m, data[(size_t) k]);
        const float amp = 4.0f * m / (float) kN;
        const float db = 20.0f * std::log10 (amp + 1e-9f) + 3.0f * std::log2 (std::sqrt (f0 * f1) / 1000.0f);
        const float v = juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 56.0f);
        auto& l = level[(size_t) b];
        l = v > l ? l + (v - l) * 0.6f : l * 0.9f + v * 0.1f;
        // les flammes montent vite et retombent doucement, avec un peu de crepitement
        auto& f = flame[(size_t) b];
        const float tgt = l * (0.85f + 0.3f * rng.nextFloat());
        f = tgt > f ? f + (tgt - f) * 0.55f : f * 0.93f + tgt * 0.07f;
        maxLevel = std::max (maxLevel, l);
        all += l; if (b > kBands * 2 / 3) hf += l;
        if (b < 6) bass = std::max (bass, l);
    }
    energy += (maxLevel - energy) * (maxLevel > energy ? 0.4f : 0.06f);

    float drive = 0.0f;
    for (int s = 0; s < kNumSlots; ++s)
        if (proc.params.slot[s][P_On]->load() > 0.5f)
            drive = std::max (drive, proc.params.slot[s][P_Drive]->load() / 36.0f);
    sat += (juce::jlimit (0.0f, 1.0f, 0.75f * drive + 0.9f * (hf / all)) - sat) * 0.05f;
    intensity = juce::jlimit (0.0f, 1.0f, energy * (0.5f + 1.1f * sat));
    bassFast += (bass - bassFast) * 0.5f;
    bassSlow += (bass - bassSlow) * 0.05f;
    pulse = std::max (bassFast, pulse * 0.92f);
    time += 1.0f;

    // ---- braises : naissent sous les flammes les plus hautes, en rafale sur les basses
    const float W = (float) std::max (1, getWidth()), H = (float) std::max (1, getHeight());
    if (cooldown > 0) --cooldown;
    int n = (int) (0.4f + 5.0f * intensity) + (rng.nextFloat() < 0.5f ? 1 : 0);
    if (bassFast - bassSlow > 0.10f && bassFast > 0.30f && cooldown == 0) { n += (int) (30.0f * bassFast); cooldown = 6; }
    float total = 0.0f; for (float f : flame) total += f * f + 0.002f;
    while (n-- > 0 && embers.size() < 260)
    {
        if (rng.nextFloat() < 0.3f)
        {
            const auto& b = blobs[(size_t) rng.nextInt (kBlobs)];
            spawnEmber (W * (b.homeX + b.ax * std::sin (time * b.fx + b.phase)), H * (b.homeY + b.ay * std::sin (time * b.fy + 1.7f * b.phase)), intensity);
        }
        else
        {
            float pick = rng.nextFloat() * total, acc = 0.0f; int band = 0;
            for (int i = 0; i < kBands; ++i) { acc += flame[(size_t) i] * flame[(size_t) i] + 0.002f; if (acc >= pick) { band = i; break; } }
            spawnEmber (W * ((float) band + rng.nextFloat()) / (float) kBands, H - 10.0f * rng.nextFloat() - flame[(size_t) band] * 120.0f, intensity);
        }
    }
    for (auto& e : embers)
    {
        e.phase += 0.25f;
        e.x += e.vx + std::sin (e.phase) * 0.7f * (0.5f + intensity);
        e.y += e.vy;
        e.vy *= 0.992f;
        e.life -= 1.0f;
    }
    embers.erase (std::remove_if (embers.begin(), embers.end(), [] (const Ember& e) { return e.life <= 0.0f; }), embers.end());
    ++frame;
}

// rampe de couleur du feu, posee sur le fond orange : rouge profond -> orange -> jaune -> creme
static juce::Colour fireColour (float v, float heat)
{
    v *= heat;
    const juce::Colour c0 (0xff8E1D06), c1 (0xffD43A0B), c2 (0xffFF7A2A), c3 (0xffFF9A3C), c4 (0xffFFD27A);   // ton sur ton avec le fond orange du logo
    if (v < 0.5f)  return c0.interpolatedWith (c1, juce::jlimit (0.0f, 1.0f, (v - 0.2f) / 0.3f));
    if (v < 0.9f)  return c1.interpolatedWith (c2, (v - 0.5f) / 0.4f);
    if (v < 1.35f) return c2.interpolatedWith (c3, (v - 0.9f) / 0.45f);
    return c3.interpolatedWith (c4, juce::jlimit (0.0f, 1.0f, (v - 1.35f) / 0.6f));
}

void VisualizerOverlay::renderField()
{
    const int W = getWidth(), H = getHeight();
    const int nw = std::max (8, W / kCell + 1), nh = std::max (8, H / kCell + 1);
    if (! field.isValid() || nw != gw || nh != gh) { field = juce::Image (juce::Image::ARGB, nw, nh, true); gw = nw; gh = nh; }
    juce::Image::BitmapData bd (field, juce::Image::BitmapData::writeOnly);

    const float heat = 0.85f + 0.55f * sat + 0.25f * intensity;          // la saturation chauffe la lave
    const float master = 0.45f + 0.55f * juce::jlimit (0.0f, 1.0f, intensity * 1.2f);

    // bulles de lave
    struct B { float x, y, r2; };
    std::array<B, kBlobs> bl;
    for (int i = 0; i < kBlobs; ++i)
    {
        const auto& b = blobs[(size_t) i];
        const float r = (42.0f + 46.0f * b.size) * (1.0f + 0.45f * pulse + 0.15f * intensity);
        bl[(size_t) i] = { W * (b.homeX + b.ax * std::sin (time * b.fx + b.phase)), H * (b.homeY + b.ay * std::sin (time * b.fy + 1.7f * b.phase)), r * r };
    }
    const float colW = (float) W / (float) kBands, sig = colW * 0.85f;

    for (int gy = 0; gy < nh; ++gy)
        for (int gx = 0; gx < nw; ++gx)
        {
            const float x = (float) gx * kCell, y = (float) gy * kCell;
            float v = 0.0f;
            for (const auto& b : bl) { const float dx = x - b.x, dy = y - b.y; v += b.r2 / (dx * dx + dy * dy + 900.0f); }

            // flammes : une colonne par bande, hauteur = niveau, ondulation (turbulence)
            const float ty = y + 16.0f * std::sin (x * 0.021f + time * 0.17f) + 9.0f * std::sin (x * 0.05f - time * 0.31f + y * 0.015f);
            const float up = (float) H - ty;
            const int c0 = juce::jmax (0, (int) (x / colW) - 2), c1 = juce::jmin (kBands - 1, (int) (x / colW) + 2);
            for (int b = c0; b <= c1; ++b)
            {
                const float f = flame[(size_t) b];
                if (f < 0.03f) continue;
                const float dx = x - (b + 0.5f) * colW;
                const float len = 40.0f + 240.0f * f;
                v += 1.0f * f * std::exp (-dx * dx / (2.0f * sig * sig)) * std::exp (-up / len);
            }
            // lit de braises tres bas
            v += 0.30f * std::exp (-up / 45.0f) * (0.4f + 0.6f * intensity);

            // a-plats a bords nets, comme le logo : 3 paliers (rouge profond / orange clair / jaune chaud)
            const float vt = 1.0f - std::exp (-0.85f * v) + 0.12f * sat;
            auto step = [] (float x, float edge) { return juce::jlimit (0.0f, 1.0f, (x - edge) / 0.035f + 0.5f); };
            const float s1 = step (vt, 0.42f), s2 = step (vt, 0.66f + 0.08f * (1.0f - heat * 0.6f)), s3 = step (vt, 0.95f);
            if (s1 <= 0.0f) { bd.setPixelColour (gx, gy, juce::Colours::transparentBlack); continue; }
            juce::Colour c = juce::Colour (0xff8A200A).withAlpha (0.80f * s1 * master);
            c = c.overlaidWith (juce::Colour (0xffF2541B).withAlpha (0.85f * s2 * master));
            c = c.overlaidWith (juce::Colour (0xffFF9A3C).withAlpha (0.90f * s3 * master));
            bd.setPixelColour (gx, gy, c);
        }
}

void VisualizerOverlay::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();
    if (W < 10.0f) return;
    renderField();
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (field, juce::Rectangle<float> (0.0f, 0.0f, (float) gw * kCell, (float) gh * kCell));

    // braises : ronds a-plats, creme a la naissance puis noir chaud en refroidissant
    for (const auto& e : embers)
    {
        const float t = juce::jlimit (0.0f, 1.0f, e.life / e.maxLife);
        g.setColour ((t > 0.45f ? juce::Colour (0xffFFE2B0) : juce::Colour (0xffA83210)).withAlpha (0.35f + 0.65f * std::sqrt (t)));
        g.fillEllipse (e.x - e.size * 0.7f, e.y - e.size * 0.7f, e.size * 1.4f, e.size * 1.4f);
    }
}
