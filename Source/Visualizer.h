#pragma once
// EPICES / SatLab - fond de lave et de feu : bulles de lave (metaballs) qui montent et fusionnent, flammes dont la hauteur suit
// le spectre, braises qui s'envolent. Plus ca sature, plus c'est chaud (rouge sombre -> orange -> jaune). Derriere les panneaux translucides.
#include "PluginProcessor.h"
#include "ClayTheme.h"

class VisualizerOverlay : public juce::Component, private juce::Timer
{
public:
    explicit VisualizerOverlay (SatLabProcessor& p);
    void paint (juce::Graphics&) override;
    void update();                    // un pas d'animation (timer ; utilisable pour les captures)
private:
    void timerCallback() override { if (isVisible()) { update(); repaint(); } }

    struct Blob { float homeX, homeY, ax, ay, fx, fy, phase, size; };
    struct Ember { float x, y, vx, vy, life, maxLife, size, phase; };

    void spawnEmber (float x, float y, float power);
    void renderField();

    static constexpr int kOrder = 11, kN = 1 << kOrder, kBands = 32, kBlobs = 8, kCell = 6;
    SatLabProcessor& proc;
    juce::dsp::FFT fft { kOrder };
    std::array<float, kN> window {};
    std::array<float, kBands> level {}, flame {};
    float energy = 0.0f, sat = 0.0f, intensity = 0.0f, pulse = 0.0f, bassFast = 0.0f, bassSlow = 0.0f;
    float time = 0.0f;
    int frame = 0, cooldown = 0, gw = 0, gh = 0;
    juce::Random rng { 20260101 };
    std::array<Blob, kBlobs> blobs;
    std::vector<Ember> embers;
    juce::Image field;
};
