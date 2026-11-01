# CAYENNE — laboratoire de saturation (VST3 / AU / Standalone)

Plugin audio de saturation avec **25 modèles de circuits**, **4 slots** utilisables en série ou en parallèle,
et un contrôle très fin de la **fréquence**, du **timbre**, de la **chaleur** et de la **harshness**.
Un analyseur intégré (courbe de transfert dynamique + spectre d'un signal de test) montre en direct ce que fait
chaque réglage.

## Nouveautés (v2) — EPICES

- **Page MOD** (onglet à droite) : 4 LFO (sinus, triangle, dents de scie, carré, Sample&Hold, aléatoire lissé ; fréquence libre
  ou **synchronisée au tempo** du DAW ; phase ; unipolaire/bipolaire), 1 **suiveur d'enveloppe**, 2 **macros** et une
  **matrice de 8 routes** (source → cible → quantité positive ou négative) vers presque tous les réglages
  (Drive, Bias, Character, Focus, Warmth, Harshness, EQ, Mix, Level, Pan, Width… des 4 slots, + Input/Output/Dry-Wet/Width et la vitesse des LFO).
- **Clic droit sur n'importe quel bouton** → « Modulate with… » crée la route tout seul ; l'anneau creme autour du bouton montre la modulation en direct.
- **Pan et Width** par slot + **Width global** (autopan, élargissement stéréo… modulables).
- **Fond de lave et de feu** (bulles de lave qui montent et fusionnent, flammes dont la hauteur suit le spectre, braises qui s'envolent ; plus ça ssature, plus c'est chaud), réactif au son et à la saturation, derrière les panneaux translucides (bouton « Lightning » en bas à droite pour les couper).
- Design sombre (gris anthracite, chrome, vert acide) avec la marque EPICES. 4 nouveaux presets de sound design (Wobble Tape, Auto-Pan Fuzz, Envelope Opener, Macro Destroyer).

## 1. Obtenir le plugin compilé (sans rien installer)

1. Crée un dépôt GitHub (privé ou public) et pousse-y **tout ce dossier**.
2. Onglet **Actions** → le workflow *Build CAYENNE* se lance tout seul (ou « Run workflow »).
3. Quand c'est vert, télécharge les artefacts : `CAYENNE-Windows` (VST3 + .exe), `CAYENNE-macOS` (VST3 + AU + app,
   Intel et Apple Silicon), `CAYENNE-Linux`.
4. Copie le plugin :
   - **Windows** : `CAYENNE.vst3` → `C:\Program Files\Common Files\VST3\`
   - **macOS** : `CAYENNE.vst3` → `~/Library/Audio/Plug-Ins/VST3/` ; `CAYENNE.component` → `~/Library/Audio/Plug-Ins/Components/`
     (macOS peut bloquer un binaire non signé : clic droit → Ouvrir, ou `xattr -dr com.apple.quarantine CAYENNE.vst3`)
   - **Linux** : `CAYENNE.vst3` → `~/.vst3/`
5. Rescanne les plugins dans ton DAW.

## 2. Compiler soi-même

Prérequis : CMake ≥ 3.22, un compilateur C++17 (Visual Studio 2022 / Xcode / gcc), git. JUCE est téléchargé automatiquement.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target CAYENNE_VST3 -j
# (Standalone : --target CAYENNE_Standalone, AU sur macOS : --target CAYENNE_AU)
```
Le résultat est dans `build/CAYENNE_artefacts/Release/`.
Sous Linux il faut les paquets de dev JUCE (`libasound2-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev
libxinerama-dev libxrandr-dev libxrender-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev`).

## 3. Chaîne de traitement d'un slot

```
Entrée -> [Stereo : St / Mid / Side / L / R] -> [BAND from..to : seule cette bande est saturée, le reste passe intact]
  -> FOCUS (pré-emphase EQ) -> SAG -> DRIVE / ASYM / BIAS -> **SATURATION (25 types)**
  -> HARSHNESS -> WARMTH -> DE-EMPHASIS -> blocage DC -> recombinaison avec la bande non traitée
  -> TONE EQ (low shelf / cloche / high shelf) -> Auto Gain -> LEVEL -> MIX
```
Global : Input → (oversampling 1x/2x/4x/8x/16x) → slots en **Serial** ou **Parallel** → Dry/Wet → Safety Clip → Output.

## 4. Guide des réglages

| Réglage | Effet |
|---|---|
| **Drive** | Gain envoyé dans la non-linéarité (0 à 48 dB). |
| **Bias** | Décale le point de fonctionnement : plus de 2ᵉ harmonique, grain dépendant du niveau. |
| **Asymmetry** | Drive différent sur les alternances + et − (harmoniques paires). |
| **Sag / Sag Time** | L'alimentation « s'affaisse » avec le niveau : compression douce avant la saturation (tubes, transfos). |
| **Character 1–4** | Réglages propres au type choisi (les noms changent). Voir le tableau ci-dessous. |
| **Focus Freq / Gain / Q** | EQ en cloche **avant** la saturation : choisis quelle zone du spectre sature le plus. |
| **De-Emphasis** | Annule (0–100 %) le Focus **après** la saturation : on sature une bande sans changer la balance tonale. |
| **Warmth / Warm Freq** | Ajoute des harmoniques paires (redressement) filtrées passe-bas à la fréquence choisie : épaisseur, rondeur. |
| **Harshness / Harsh Freq** | Valeur **positive** : renforce les produits de distorsion au-dessus de la fréquence (mordant, agressivité). **Négative** : les adoucit (distorsion « lissée »). |
| **Band From / To** | Seule la bande entre les deux fréquences est saturée (multibande, croisement Linkwitz-Riley 4). |
| **Tone EQ** | Low shelf, cloche (freq/gain/Q) et high shelf **après** la saturation. |
| **Mix / Level / Auto Gain** | Parallèle par slot, niveau de sortie, compensation automatique de volume. |
| **Stereo** | Traite Stereo, Mid seul, Side seul, Left ou Right. |
| **Oversampling** | Réduit l'aliasing (recommandé 4x et plus pour les saturations dures). Ajoute de la latence, compensée par le DAW. |
| **Safety Clip** | Limiteur doux à 0 dBFS en sortie. |

**Astuces** : le potard « Test tone » (en bas) règle la fréquence/niveau du sinus de l'analyseur — utile pour voir comment le Transformer ou le
Tube Screamer réagissent aux graves vs aux aigus. Double-clic sur un potard = valeur par défaut. Clic sur le texte d'un potard = saisie au clavier (ex. `2.5k`).
12 presets sont fournis (menu *Preset*, ou liste des programmes de ton DAW).

## 5. Les 25 types de saturation

| # | Type | Réglages « Character » | Description |
|---|------|------------------------|-------------|
| 0 | **Clean (off)** | — | Passe-plat : utile pour n'utiliser que les EQ / focus du slot. |
| 1 | **Tanh** | — | Saturation douce classique, harmoniques impaires. |
| 2 | **Arctan** | — | Genou plus lent que tanh : plus de 'queue' d'harmoniques. |
| 3 | **Soft Clip (p-norm)** | Knee | Du genou tres doux au quasi-hard-clip. |
| 4 | **Digital Clip** | Knee Width | Ecretage numerique avec genou reglable (0 = brick-wall). |
| 5 | **Sine Shaper** | — | sin(x) : sature en douceur, tres propre sur les graves. |
| 6 | **Exponential** | Power | Courbe exponentielle ; Power > 0 ajoute un effet 'gate/starve'. |
| 7 | **Triode Tube** | Cutoff Softness · Grid Conduction · Miller LP · Even Bloom | Etage triode asymetrique : 2nde harmonique riche, compression douce. |
| 8 | **Push-Pull Pentode** | Class (A>B) · Knee · Output HF Roll | Etage symetrique de classe AB : impaires + un peu de crossover. |
| 9 | **Transformer** | Core Freq · Core Saturation · Hysteresis · HF Loss | Saturation du noyau : surtout dans les graves, avec boucle d'hysteresis. |
| 10 | **Tape (Hysteresis)** | Coercivity · Head Bump · HF Loss · Wow / Flutter | Modele Jiles-Atherton : vraie boucle d'hysteresis magnetique + head bump + pertes HF. |
| 11 | **Tape (Classic)** | Softness · HF Squash · Head Bump · Wow / Flutter | Version legere : compression douce, les aigus s'ecrasent quand le niveau monte. |
| 12 | **Diode Clipper** | Forward Voltage · Softness · Asymmetry · Resistance | Ecreteur a diodes (resolution implicite) : germanium > silicium > LED. |
| 13 | **Germanium Fuzz** | Fuzz · Gate / Starve · Rail Asym · Octave Bite | Fuzz Face : deux etages asymetriques, crachotement a bas niveau. |
| 14 | **Muff Fuzz** | Sustain · Diode Type · Stage 1 Hardness | Deux etages d'ecretage en cascade, sustain infini. |
| 15 | **Overdrive (TS)** | Corner Freq · Clip Softness · Clean Blend · Op-Amp LP | Style Tube Screamer : seuls les mediums/aigus sont satures, graves intacts. |
| 16 | **Rectifier** | Half-Wave · Full-Wave · Smoothness · Post LP | Redressement : ajoute l'octave (full-wave) ou des harmoniques paires (half-wave). |
| 17 | **Crossover (Class B)** | Dead Zone · Hardness · Gap Asym | Distorsion de croisement d'un ampli classe B : grain aux faibles niveaux. |
| 18 | **Bitcrusher** | Bit Depth · Downsample · Dither · Jitter | Quantification + reduction de frequence d'echantillonnage (lo-fi numerique). |
| 19 | **Harmonic Generator** | 2nd · 3rd · 4th · 5th | Polynomes de Chebyshev : chaque harmonique se dose separement. |
| 20 | **Wavefolder** | Shape (Sine>Tri) · Post LP | Repli d'onde type Buchla / Serge : harmoniques riches et metalliques. |
| 21 | **Slew (Op-Amp)** | Slew Freq · Rise/Fall Asym · Softness · Rail Clip | Limiteur de pente : sature les aigus selon leur niveau (op-amp qui peine). |
| 22 | **JFET** | Pinch-Off · Ceiling · Input LP | Loi quadratique d'un JFET : beaucoup de 2nde harmonique, coupure en negatif. |
| 23 | **Console (Xfmr+Class A)** | Core Freq · Xfmr Amount · Class-A Warmth · Slew Freq | Chaine de console : transfo d'entree + etage classe A + slew d'op-amp. |
| 24 | **Tube Amp (2 stages)** | Stage 2 Gain · Coupling HP · Grid Conduction · Power Amp Class | Preampli a 2 triodes avec condensateur de liaison + etage de puissance. |

## 6. Structure du code

- `Source/DSP/` : moteur (header-only, sans JUCE) — `Filters.h`, `SatTypes.h`, `SatSlot.h`, `SatEngine.h`
- `Source/Params.h` : ≈ 130 paramètres automatisables (4 slots × 32 + global)
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`, `Source/Presets.h`
- `Tests/ScreenshotTool.cpp` : outil de vérification (option CMake `-DSATLAB_SCREENSHOT_TOOL=ON`) : rend l'interface en PNG et teste le processeur (`--test`)
- `Tests/dsp_test.cpp` : test autonome du DSP (`g++ -O2 -std=c++17 -ISource/DSP Tests/dsp_test.cpp -o dsp_test && ./dsp_test`)

Ajouter un type de saturation : une entrée dans `SatTypes.h` (nom, labels, valeurs par défaut) + un `case` dans `SatSlot::shape()`
(+ éventuellement ses coefficients dans `updateCoeffs()`).

## 7. Licence

Le code de CAYENNE est fourni tel quel. Il utilise [JUCE](https://juce.com), sous licence AGPLv3 ou licence commerciale :
pour un usage perso c'est sans souci ; pour **distribuer** le plugin, il faut soit publier le code source sous AGPLv3, soit acheter une licence JUCE.
