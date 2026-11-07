# Frikfrak VST

Plugins audio maison (VST3 / AU, macOS Apple Silicon), pensés pour Ableton Live 12 Lite.

| Plugin | Type | Description |
|---|---|---|
| **Frikfrak Rhodes** | Instrument | Piano électrique façon Rhodes Mark I des années 70, synthétisé (pas d'échantillons), avec tone, drive, phaser, trémolo et 12 presets nu soul |
| **Frikfrak Vinyl Noise** | Instrument | Grain de disque joué au clavier : craquements et souffle tant qu'une touche est enfoncée |

## Organisation

```text
CMakeLists.txt          # récupère JUCE une fois, déclare chaque plugin
shared/dsp/             # DSP commun (lissage, DC blocker, RNG, drive, trémolo)
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

Les craquements et le souffle ne sonnent que tant qu'une touche est enfoncée (attaque 5 ms, extinction ~150 ms au relâchement). La note jouée n'a pas d'importance.

## Validation

```sh
auval -v aumu Rhdx Frik   # Rhodes
auval -v aumu Vnzx Frik   # Vinyl Noise
# pluginval : télécharger la release macOS sur https://github.com/Tracktion/pluginval/releases
pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --skip-gui-tests --validate ~/Library/Audio/Plug-Ins/VST3/"Frikfrak Rhodes.vst3"
```

## Dépannage

- Plugin absent de Live : `codesign --force --deep -s - <bundle>` sur le `.vst3` / `.component`, puis Rescan.
- AU pas à jour après un rebuild : `killall -9 AudioComponentRegistrar`, puis Rescan.
