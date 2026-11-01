#pragma once
// SatLab - liste des types de saturation + labels des 4 reglages specifiques ("S1..S4")
// Les labels (et valeurs par defaut) sont utilises par l'interface : les 4 potards
// "Character" changent de nom selon le type choisi.
namespace satlab
{
enum SatType
{
    Clean = 0, Tanh, Arctan, PNorm, DigitalClip, SineShaper, Exponential,
    Triode, Pentode, Transformer, TapeHysteresis, TapeClassic,
    DiodeClipper, GermaniumFuzz, MuffFuzz, Overdrive, Rectifier, ClassB,
    Bitcrusher, Harmonics, Wavefolder, SlewLimiter, Jfet, Console, TubeAmp,
    NumSatTypes
};

struct TypeInfo
{
    const char* name;
    const char* label[4];   // nullptr = reglage inutilise pour ce type
    float       def[4];
    const char* hint;
};

inline const TypeInfo& getTypeInfo (int t)
{
    static const TypeInfo table[NumSatTypes] =
    {
        { "Clean (off)",           { nullptr, nullptr, nullptr, nullptr }, { 0, 0, 0, 0 }, "Passe-plat : utile pour n'utiliser que les EQ / focus du slot." },
        { "Tanh",                  { nullptr, nullptr, nullptr, nullptr }, { 0, 0, 0, 0 }, "Saturation douce classique, harmoniques impaires." },
        { "Arctan",                { nullptr, nullptr, nullptr, nullptr }, { 0, 0, 0, 0 }, "Genou plus lent que tanh : plus de 'queue' d'harmoniques." },
        { "Soft Clip (p-norm)",    { "Knee", nullptr, nullptr, nullptr },  { 0.3f, 0, 0, 0 }, "Du genou tres doux au quasi-hard-clip." },
        { "Digital Clip",          { "Knee Width", nullptr, nullptr, nullptr }, { 0.15f, 0, 0, 0 }, "Ecretage numerique avec genou reglable (0 = brick-wall)." },
        { "Sine Shaper",           { nullptr, nullptr, nullptr, nullptr }, { 0, 0, 0, 0 }, "sin(x) : sature en douceur, tres propre sur les graves." },
        { "Exponential",           { "Power", nullptr, nullptr, nullptr }, { 0.0f, 0, 0, 0 }, "Courbe exponentielle ; Power > 0 ajoute un effet 'gate/starve'." },
        { "Triode Tube",           { "Cutoff Soft", "Grid Cond.", "Miller LP", "Even Bloom" }, { 0.5f, 0.4f, 1.0f, 0.3f }, "Etage triode asymetrique : 2nde harmonique riche, compression douce." },
        { "Push-Pull Pentode",     { "Class A>B", "Knee", "Out HF Roll", nullptr }, { 0.2f, 0.5f, 1.0f, 0 }, "Etage symetrique de classe AB : impaires + un peu de crossover." },
        { "Transformer",           { "Core Freq", "Core Sat", "Hysteresis", "HF Loss" }, { 0.35f, 0.5f, 0.3f, 1.0f }, "Saturation du noyau : surtout dans les graves, avec boucle d'hysteresis." },
        { "Tape (Hysteresis)",     { "Coercivity", "Head Bump", "HF Loss", "Flutter" }, { 0.35f, 0.5f, 0.75f, 0.0f }, "Modele Jiles-Atherton : vraie boucle d'hysteresis magnetique + head bump + pertes HF." },
        { "Tape (Classic)",        { "Softness", "HF Squash", "Head Bump", "Flutter" }, { 0.5f, 0.4f, 0.5f, 0.0f }, "Version legere : compression douce, les aigus s'ecrasent quand le niveau monte." },
        { "Diode Clipper",         { "Fwd Voltage", "Softness", "Asymmetry", "Resistance" }, { 0.35f, 0.4f, 0.0f, 0.3f }, "Ecreteur a diodes (resolution implicite) : germanium > silicium > LED." },
        { "Germanium Fuzz",        { "Fuzz", "Gate/Starve", "Rail Asym", "Octave Bite" }, { 0.5f, 0.3f, 0.4f, 0.0f }, "Fuzz Face : deux etages asymetriques, crachotement a bas niveau." },
        { "Muff Fuzz",             { "Sustain", "Diode Type", "Hardness 1", nullptr }, { 0.5f, 0.4f, 0.5f, 0 }, "Deux etages d'ecretage en cascade, sustain infini." },
        { "Overdrive (TS)",        { "Corner Freq", "Softness", "Clean Blend", "Op-Amp LP" }, { 0.65f, 0.6f, 1.0f, 0.8f }, "Style Tube Screamer : seuls les mediums/aigus sont satures, graves intacts." },
        { "Rectifier",             { "Half-Wave", "Full-Wave", "Smoothness", "Post LP" }, { 0.5f, 0.0f, 0.3f, 1.0f }, "Redressement : ajoute l'octave (full-wave) ou des harmoniques paires (half-wave)." },
        { "Crossover (Class B)",   { "Dead Zone", "Hardness", "Gap Asym", nullptr }, { 0.3f, 0.4f, 0.0f, 0 }, "Distorsion de croisement d'un ampli classe B : grain aux faibles niveaux." },
        { "Bitcrusher",            { "Bit Depth", "Downsample", "Dither", "Jitter" }, { 0.6f, 0.0f, 0.3f, 0.0f }, "Quantification + reduction de frequence d'echantillonnage (lo-fi numerique)." },
        { "Harmonic Generator",    { "2nd", "3rd", "4th", "5th" }, { 0.4f, 0.3f, 0.15f, 0.1f }, "Polynomes de Chebyshev : chaque harmonique se dose separement." },
        { "Wavefolder",            { "Shape", "Post LP", nullptr, nullptr }, { 0.0f, 1.0f, 0, 0 }, "Repli d'onde type Buchla / Serge : harmoniques riches et metalliques." },
        { "Slew (Op-Amp)",         { "Slew Freq", "Rise/Fall", "Softness", "Rail Clip" }, { 0.6f, 0.0f, 0.5f, 0.0f }, "Limiteur de pente : sature les aigus selon leur niveau (op-amp qui peine)." },
        { "JFET",                  { "Pinch-Off", "Ceiling", "Input LP", nullptr }, { 0.5f, 0.4f, 1.0f, 0 }, "Loi quadratique d'un JFET : beaucoup de 2nde harmonique, coupure en negatif." },
        { "Console (Xfmr+Class A)",{ "Core Freq", "Xfmr Amt", "A Warmth", "Slew Freq" }, { 0.3f, 0.4f, 0.4f, 0.8f }, "Chaine de console : transfo d'entree + etage classe A + slew d'op-amp." },
        { "Tube Amp (2 stages)",   { "S2 Gain", "Coupling HP", "Grid Cond.", "Amp Class" }, { 0.4f, 0.3f, 0.4f, 0.15f }, "Preampli a 2 triodes avec condensateur de liaison + etage de puissance." },
    };
    return table[t < 0 ? 0 : (t >= NumSatTypes ? NumSatTypes - 1 : t)];
}
} // namespace satlab
