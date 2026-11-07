# CLAUDE.md

Plugins audio JUCE (VST3 + AU), macOS Apple Silicon uniquement, cible Ableton Live 12 Lite. Voir [README.md](README.md) pour l'usage côté utilisateur.

## Commandes

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release   # 1er configure : télécharge JUCE 9.0.5 (FetchContent)
cmake --build build -j                      # installe aussi dans ~/Library/Audio/Plug-Ins/{VST3,Components}
ctest --test-dir build                      # tests DSP (exécutable dsp_tests)
auval -v aumu Rhdx Frik                     # valider l'AU Rhodes
auval -v aumu Vnzx Frik                     # valider l'AU Vinyl Noise
auval -v aumu Knkx Frik                     # valider l'AU Kankyo
auval -v aumf Gltx Frik                     # valider l'AU Glitch (effet audio + MIDI)
```

Après un rebuild : relancer Live (un plugin déjà chargé reste en mémoire), et si l'AU n'est pas à jour : `killall -9 AudioComponentRegistrar`.

## Architecture

- `CMakeLists.txt` racine : récupère JUCE, définit `frikfrak_plugin_setup()` (includes, flags, linkage communs), ajoute chaque plugin, déclare `dsp_tests`.
- `shared/dsp/Effects.h` : DSP commun header-only (`Smoother`, `DcBlocker`, `Rng`, drive, trémolo, `Tone`, `Phaser`), namespace `dsp`. `shared/dsp/Space.h` : `Chorus` (Juno) et `Reverb` (FDN 8 lignes, mix 0 = bypass exact).
- `plugins/<nom>/` : `CMakeLists.txt` (`juce_add_plugin` + `frikfrak_plugin_setup`) et `src/`.
  - `src/dsp/*.h` : DSP **pur C++, sans JUCE** (testable hors plugin).
  - `src/PluginProcessor.*` : glue JUCE (APVTS, bus, état). UI : `ui::Editor` partagé.
- `shared/ui/FrikfrakEditor.h` : `ui::Knob` (potard lié à un paramètre ; libellés pour les `AudioParameterChoice`) et `ui::drawBrand` (en-tête) réutilisables, plus l'éditeur commun (palette du site fgillet, logo, un potard rotatif par paramètre, automatiquement). Unité affichée = texte entre parenthèses du nom de paramètre (`"Volume (dB)"`) ; plage 0..1 → affichée en %.
- Presets : `shared/presets/PresetTable.h` (`ParamSpec` id/bornes/défaut/skew, `Preset<N>`, C++ pur) et `shared/plugin/PresetHost.h` (layout APVTS, programmes hôte, index sauvé dans l'état). Chaque plugin à presets a son `src/Presets.h` (Rhodes, Kankyo) ; ordre des specs = ordre des potards (8 par rangée). Ajouter un preset = une ligne ; un paramètre = une spec + une colonne dans chaque preset. Le preset 0 (« Init ») doit égaler les défauts (testé).
- Kankyo : niveaux des presets normalisés par un test qui rend chaque preset sur toute la chaîne ; après un changement de DSP qui touche le niveau, ajuster les colonnes Volume.
- `shared/assets/logo.svg` : embarqué via `juce_add_binary_data` (`BinaryData::logo_svg`).
- Glitch (`plugins/glitch/`) : seul **effet audio** (entrée stéréo, `IS_SYNTH FALSE`, entrée MIDI, AU de type `aumf`), latence 0. DSP pur dans `src/dsp/` : `BeatClock` (tempo hôte + horloge libre), `Pads` (pads/Hold/notes 36–47), `Engage` (fondu 8 ms, tampon `History`, Hermite), `TimeFx`/`ColorFx`/`ThrowFx`, `Chain` (ordre fixe, bypass exact, plafond doux). Paramètres dans `src/Params.h`, interface dédiée `src/GlitchEditor.h`. Un effet désengagé doit rendre son entrée bit à bit (testé) ; les fondus passent par `dsp::blend`.
- `tests/dsp_tests.cpp` : tests de propriétés sur le DSP pur (macro `CHECK`, pas `assert`, pour tourner en Release).
- `docs/plans/` : plans de conception.

## Règles

- Le DSP reste dans `src/dsp/` ou `shared/dsp/` sans dépendance JUCE ; toute nouvelle logique DSP a un test dans `tests/dsp_tests.cpp` (et l'include path ajouté à `dsp_tests` si nouveau plugin).
- Ne jamais changer `PLUGIN_MANUFACTURER_CODE` (`Frik`), un `PLUGIN_CODE` existant ni un ID de paramètre : ça casse les sets Live sauvegardés.
- Nouveau plugin : copier `plugins/vinyl-noise/`, `PLUGIN_CODE` unique (4 caractères, une seule majuscule), `add_subdirectory(plugins/<nom>)` à la racine.
- Temps réel : pas d'allocation, de lock ni d'I/O dans `processBlock` ; `juce::ScopedNoDenormals` en tête ; lisser les paramètres (`dsp::Smoother`).
- Si un plugin introduit de la latence, la déclarer via `setLatencySamples()` dans `prepareToPlay`.
- Git : commits signés **f-k-z** uniquement (identité locale au dépôt, déjà configurée). Aucun trailer `Co-Authored-By`, aucune mention de Claude dans les commits ni les PR.
- C++20, arm64, macOS 11+. Documentation et README en français, commentaires de code en anglais.
