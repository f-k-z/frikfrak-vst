# Frikfrak VST

Plugins audio maison (VST3 / AU, macOS Apple Silicon), pensés pour Ableton Live 12 Lite.

| Plugin | Type | Description |
|---|---|---|
| **Frikfrak Rhodes** | Instrument | Piano électrique façon Rhodes Mark I des années 70, synthétisé (pas d'échantillons), avec drive et trémolo |
| **Frikfrak Vinyl Noise** | Effet audio | Grain de disque : craquements, souffle, pleurage/scintillement, perte de bande passante |

## Organisation

```text
CMakeLists.txt          # récupère JUCE une fois, déclare chaque plugin
shared/dsp/             # DSP commun (lissage, DC blocker, RNG, drive, trémolo)
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
3. Browser → Plug-ins → *Frikfrak* : *Frikfrak Rhodes* sur une piste MIDI, *Frikfrak Vinyl Noise* sur n'importe quelle piste ou bus.

## Paramètres

**Frikfrak Rhodes**

| Paramètre | Effet |
|---|---|
| Bark | Saturation du micro : plus haut = plus de « grognement » quand on joue fort |
| Decay | Durée de résonance des notes (prise en compte à la note suivante) |
| Volume | Niveau de sortie |
| Drive | Saturation douce globale |
| Tremolo Rate / Depth | Trémolo d'ampli, légèrement stéréo |

**Frikfrak Vinyl Noise** (latence fixe de 1,5 ms, compensée par Live)

| Paramètre | Effet |
|---|---|
| Crackle | Craquements et petits pops |
| Hiss | Souffle |
| Wow / Flutter | Pleurage lent et scintillement de hauteur |
| Age | Perte d'aigus (16 kHz → 6 kHz) |

Les craquements et le souffle continuent même sur un signal silencieux : le disque tourne.

## Validation

```sh
auval -v aumu Rhdx Frik   # Rhodes
auval -v aufx Vnzx Frik   # Vinyl Noise
# pluginval : télécharger la release macOS sur https://github.com/Tracktion/pluginval/releases
pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --skip-gui-tests --validate ~/Library/Audio/Plug-Ins/VST3/"Frikfrak Rhodes.vst3"
```

## Dépannage

- Plugin absent de Live : `codesign --force --deep -s - <bundle>` sur le `.vst3` / `.component`, puis Rescan.
- AU pas à jour après un rebuild : `killall -9 AudioComponentRegistrar`, puis Rescan.
