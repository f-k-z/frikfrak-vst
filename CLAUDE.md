# CLAUDE.md

Plugins audio JUCE (VST3 + AU), macOS Apple Silicon uniquement, cible Ableton Live 12 Lite. Voir [README.md](README.md) pour l'usage côté utilisateur.

## Commandes

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release   # 1er configure : télécharge JUCE 9.0.5 (FetchContent)
cmake --build build -j                      # installe aussi dans ~/Library/Audio/Plug-Ins/{VST3,Components}
ctest --test-dir build                      # tests DSP (exécutable dsp_tests)
auval -v aumu Rhdx Frik                     # valider l'AU Rhodes
auval -v aufx Vnzx Frik                     # valider l'AU Vinyl Noise
```

Après un rebuild, si l'AU n'est pas à jour : `killall -9 AudioComponentRegistrar`.

## Architecture

- `CMakeLists.txt` racine : récupère JUCE, définit `frikfrak_plugin_setup()` (includes, flags, linkage communs), ajoute chaque plugin, déclare `dsp_tests`.
- `shared/dsp/Effects.h` : DSP commun header-only (`Smoother`, `DcBlocker`, `Rng`, drive, trémolo), namespace `dsp`.
- `plugins/<nom>/` : `CMakeLists.txt` (`juce_add_plugin` + `frikfrak_plugin_setup`) et `src/`.
  - `src/dsp/*.h` : DSP **pur C++, sans JUCE** (testable hors plugin).
  - `src/PluginProcessor.*` : glue JUCE (APVTS, bus, état). Pas d'UI custom : `GenericAudioProcessorEditor`.
- `tests/dsp_tests.cpp` : tests de propriétés sur le DSP pur (macro `CHECK`, pas `assert`, pour tourner en Release).
- `docs/plans/` : plans de conception.

## Règles

- Le DSP reste dans `src/dsp/` ou `shared/dsp/` sans dépendance JUCE ; toute nouvelle logique DSP a un test dans `tests/dsp_tests.cpp` (et l'include path ajouté à `dsp_tests` si nouveau plugin).
- Ne jamais changer `PLUGIN_MANUFACTURER_CODE` (`Frik`), un `PLUGIN_CODE` existant ni un ID de paramètre : ça casse les sets Live sauvegardés.
- Nouveau plugin : copier `plugins/vinyl-noise/`, `PLUGIN_CODE` unique (4 caractères, une seule majuscule), `add_subdirectory(plugins/<nom>)` à la racine.
- Temps réel : pas d'allocation, de lock ni d'I/O dans `processBlock` ; `juce::ScopedNoDenormals` en tête ; lisser les paramètres (`dsp::Smoother`).
- Si un plugin introduit de la latence, la déclarer via `setLatencySamples()` dans `prepareToPlay`.
- C++20, arm64, macOS 11+. Documentation et README en français, commentaires de code en anglais.
