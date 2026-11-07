# Frikfrak VST

Plugins audio maison (VST3 / AU, macOS Apple Silicon), pensés pour Ableton Live 12 Lite.

| Plugin | Type | Description |
|---|---|---|
| **Frikfrak Rhodes** | Instrument | Piano électrique façon Rhodes Mark I des années 70, synthétisé (pas d'échantillons), avec tone, drive, phaser, trémolo et 12 presets nu soul |
| **Frikfrak Vinyl Noise** | Instrument | Grain de disque joué au clavier : craquements et souffle tant qu'une touche est enfoncée |
| **Frikfrak Kankyo** | Instrument | Pads ambient japonais (kankyō ongaku) : couches analogique + FM + souffle, chorus, grande réverb, 36 presets |

## Organisation

```text
CMakeLists.txt          # récupère JUCE une fois, déclare chaque plugin
shared/dsp/             # DSP commun (lissage, DC blocker, RNG, drive, trémolo, tone, phaser, chorus, réverb)
shared/presets/         # tables de paramètres et de presets (C++ pur)
shared/plugin/          # glue JUCE des presets (paramètres, programmes, état)
shared/ui/              # interface commune à tous les plugins (+ shared/assets/logo.svg)
plugins/<nom>/          # un dossier par plugin : CMakeLists.txt + src/ (+ src/dsp/ pur C++)
tests/dsp_tests.cpp     # tests du DSP pur C++ (sans JUCE)
```

Ajouter un plugin : créer `plugins/<nom>/` sur le modèle de `vinyl-noise`, avec un `PLUGIN_CODE` unique (4 caractères, une seule majuscule), puis `add_subdirectory(plugins/<nom>)` dans le `CMakeLists.txt` racine.

## Build

Prérequis : Command Line Tools (Xcode complet inutile) et CMake (`brew install cmake`). JUCE 9.0.5 est téléchargé automatiquement au premier configure.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build        # tests DSP
```

Le build installe tous les plugins dans `~/Library/Audio/Plug-Ins/VST3/` et `~/Library/Audio/Plug-Ins/Components/`.

## Dans Ableton Live 12 Lite

1. Settings → Plug-Ins : activer **Use Audio Units v2** et **Use VST3 Plug-In System Folders**.
2. Cliquer **Rescan** (Option-clic pour un rescan complet).
3. Browser → Plug-ins → *Frikfrak* : *Frikfrak Rhodes* sur une piste MIDI, *Frikfrak Vinyl Noise* sur une autre piste MIDI.
4. Après un rebuild, **quitter et relancer Live** : un plugin déjà chargé garde l'ancien code en mémoire, même après un Rescan.

## Paramètres

**Frikfrak Rhodes**

| Paramètre | Effet |
|---|---|
| Bark | Saturation du micro : plus haut = plus de « grognement » quand on joue fort |
| Decay | Durée de résonance des notes (prise en compte à la note suivante) |
| Tone | Brillance : 100 % = son d'origine, vers 0 % = Rhodes feutré et sombre |
| Drive | Saturation douce globale |
| Phaser | Phasing lent façon Phase 90 / Small Stone (vitesse fixe ~0,5 Hz) |
| Tremolo Rate / Depth | Trémolo d'ampli, légèrement stéréo |
| Volume | Niveau de sortie |

Presets (menu en haut à droite de l'interface, aussi dans la liste de programmes de l'hôte) : interprétations à l'oreille de sons nu soul, pas des réglages documentés.

| Preset | Inspiration |
|---|---|
| Init | Réglages par défaut |
| Smoke Room | D'Angelo, *Untitled (How Does It Feel)* |
| Chicken Grease | D'Angelo, *Voodoo* |
| Brown Sugar | D'Angelo, *Brown Sugar* |
| Mama's Phase | Erykah Badu, *Mama's Gun* |
| Water for Chocolate | Common, *Like Water for Chocolate* |
| Detroit Dust | J Dilla / Slum Village |
| Black Radio | Robert Glasper, *Black Radio* |
| Suitcase Pan | Rhodes Suitcase stéréo des années 70 |
| Velvet Bark | Comping funk joué fort |
| Slow Swirl | Rhodes + phaser profond |
| Felt Dream | Slow jam très sombre |

**Frikfrak Vinyl Noise**

| Paramètre | Effet |
|---|---|
| Crackle | Craquements et petits pops |
| Hiss | Souffle |
| Age | Assombrit le bruit (16 kHz → 6 kHz) |

Les craquements et le souffle ne sonnent que tant qu'une touche est enfoncée (attaque 5 ms, extinction ~150 ms au relâchement).

- **Note** : hauteur du bruit. Do central (C4) = son de référence ; une octave plus haut = craquements et souffle deux fois plus aigus, une octave plus bas = disque plus sourd.
- **Vélocité** : volume du vinyle.
- Plusieurs touches tenues : la dernière jouée donne la hauteur.

**Frikfrak Kankyo**

| Paramètre | Effet |
|---|---|
| Blend | Couche analogique (0 %) ↔ couche FM verre/cloches (100 %) |
| Shape | Forme d'onde analogique : triangle → dent de scie → carré |
| Detune | Désaccord des 3 oscillateurs (jusqu'à ±25 cents), élargit la stéréo |
| Color | Filtre passe-bas, sombre → brillant |
| Ratio | Rapport FM (1 = rond, 3.5 = cloche tubulaire, 5.19 = verre, 7 / 14 = métallique) |
| Bright | Quantité de FM (brillance des cloches) |
| Air | Souffle (bruit filtré), façon shō ou vent |
| Motion | LFO lent sur le filtre et la largeur d'impulsion |
| Attack / Decay / Sustain / Release | Enveloppe (attaque jusqu'à 8 s, release jusqu'à 12 s) ; Sustain bas = son frappé (maillets, gouttes) |
| Chorus | Chorus stéréo façon Juno |
| Space / Size | Mix et longueur de la réverb (1,5 s → 10 s) |
| Volume | Niveau de sortie |

36 presets en 6 familles, interprétations d'ambiance (pas des réglages documentés) :

| Famille | Presets |
|---|---|
| Pads chauds | Asagiri, Kumo, Hidamari, Yuki, Tegami, Ensoku |
| Verre et cloches (FM, lignée DX7 de *Green* de Yoshimura) | Garasu, Kori, Suisho, Fuurin, Tsurara, Yoake |
| Souffle et shō | Sho, Kaze, Iki, Asa no Ki, Yuuge, Kasumi |
| Drones sombres | Yoru, Fukai, Tsuki, Mori, Oku, Zen |
| Maillets et eau (Midori Takada, Satoshi Ashikawa) | Mizu, Ame, Shizuku, Marimba Kage, Vibe Asa, Take |
| Évolutifs et hybrides | Nagare, Tsuyu, Shio, Niwa, Umi |

## Validation

```sh
auval -v aumu Rhdx Frik   # Rhodes
auval -v aumu Vnzx Frik   # Vinyl Noise
auval -v aumu Knkx Frik   # Kankyo
# pluginval : télécharger la release macOS sur https://github.com/Tracktion/pluginval/releases
pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --skip-gui-tests --validate ~/Library/Audio/Plug-Ins/VST3/"Frikfrak Rhodes.vst3"
```

## Dépannage

- Plugin absent de Live : `codesign --force --deep -s - <bundle>` sur le `.vst3` / `.component`, puis Rescan.
- AU pas à jour après un rebuild : `killall -9 AudioComponentRegistrar`, puis Rescan.
