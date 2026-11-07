---
title: "feat: Presets nu soul pour Frikfrak Rhodes (+ Tone, Phaser, menu de presets)"
type: feat
status: completed
date: 2026-11-07
---

# feat: Presets nu soul pour Frikfrak Rhodes

## Summary

Ajouter au Rhodes deux paramètres (**Tone** = brillance, **Phaser** = phasing lent façon Phase 90 / Small Stone) et une douzaine de presets d'usine dans l'univers nu soul (D'Angelo, Soulquarians, J Dilla / Slum Village, Badu, Common, Glasper), sélectionnables depuis un menu dans l'interface et depuis la liste de programmes de l'hôte.

---

## Problem Frame

Avec six paramètres (Bark, Decay, Volume, Drive, Tremolo Rate/Depth), les sons « disque nu soul » se ressemblent : il manque le Rhodes feutré/sombre (Dilla, ballades) et le phasing lent typique des 70s réutilisé par les Soulquarians. Et sans presets, l'utilisateur part toujours du même son. Objectif : plug and play, on choisit « Smoke Room » dans un menu et on joue.

---

## Requirements

- R1. Paramètre **Tone** (0..1) : 1 = son actuel (transparent), 0 = Rhodes feutré/sombre.
- R2. Paramètre **Phaser** (0..1) : 0 = bypass exact, 1 = phasing profond. Vitesse fixe lente (pas de 3e paramètre).
- R3. 12 presets : un « Init » (valeurs par défaut) + 11 sons nu soul nommés, chacun réglant les 8 paramètres.
- R4. Menu déroulant de presets dans l'en-tête de l'interface partagée, affiché seulement si le plugin a plus d'un programme (Vinyl Noise : pas de menu).
- R5. Presets aussi exposés comme programmes de l'hôte (`getNumPrograms`/`setCurrentProgram`) ; changer de preset est enregistrable/annulable comme un changement de paramètres.
- R6. Sets Live existants : ils se rechargent avec Tone/Phaser à leur valeur par défaut, donc le son ne change pas.

---

## Key Technical Decisions

- **Tone = passe-bas 2 pôles post-drive**, coupure exponentielle 1,2 kHz → 20 kHz ; défaut 1.0 = quasi transparent (R6). Un passe-bas (pas un tilt) parce que la référence est le Rhodes filtré/échantillonné (Dilla : sampling + filtrage, non documenté par la recherche, choix à l'oreille).
- **Phaser = 4 étages passe-tout 1er ordre** modulés par LFO ~0,5 Hz (plage « Rhodes phasing » 0,5–2 Hz, seule donnée trouvée, faible fiabilité), mix sec/mouillé = paramètre. Canal R avec LFO décalé pour un léger stéréo, cohérent avec le trémolo existant. Vitesse fixe : l'utilisateur a demandé 2 paramètres, pas 3.
- **Ordre de la chaîne** : voix → drive → tone → phaser → trémolo → volume/plafond. Le tone filtre les harmoniques du drive (comme un ampli feutré) ; le phaser avant le trémolo comme pédale → ampli suitcase.
- **Presets = table C++ pure** (`nom` + valeurs par ID de paramètre) dans un header sans JUCE, testable dans `dsp_tests`. Les bornes des paramètres sont aussi définies dans ce header pour que `createLayout()` et les tests partagent la même source.
- **Programme courant** stocké comme propriété de l'état APVTS (sauvé avec le set) ; le menu reflète ce programme. Pas de marqueur « modifié » quand on tourne un bouton après avoir choisi un preset.
- **Menu dans `ui::Editor`** : `juce::ComboBox` stylé avec la palette, à droite de l'en-tête à la place de l'accroche (l'accroche reste pour le Vinyl Noise).

---

## Presets

Interprétations à l'oreille, pas des réglages documentés (voir Sources). Valeurs de départ, ajustables à l'écoute.

| Preset | Inspiration | Bark | Decay | Vol dB | Drive | Trem Hz / Depth | Tone | Phaser |
|---|---|---|---|---|---|---|---|---|
| Init | Défaut actuel | 0.50 | 1.00 | -6 | 0.20 | 4.5 / 0.30 | 1.00 | 0.00 |
| Smoke Room | D'Angelo, *Untitled (How Does It Feel)* | 0.35 | 1.30 | -6 | 0.30 | 3.0 / 0.15 | 0.45 | 0.00 |
| Chicken Grease | D'Angelo, *Chicken Grease* | 0.60 | 0.70 | -7 | 0.45 | 4.5 / 0.00 | 0.60 | 0.10 |
| Brown Sugar | D'Angelo, *Brown Sugar* (1995) | 0.30 | 1.00 | -6 | 0.15 | 4.5 / 0.25 | 0.75 | 0.00 |
| Mama's Phase | Erykah Badu, *Mama's Gun* | 0.45 | 1.20 | -6 | 0.25 | 4.5 / 0.00 | 0.60 | 0.55 |
| Water for Chocolate | Common, *Like Water for Chocolate* | 0.30 | 1.00 | -6 | 0.40 | 2.5 / 0.10 | 0.50 | 0.00 |
| Detroit Dust | J Dilla / Slum Village, *Fan-Tas-Tic Vol. 2* | 0.40 | 0.50 | -7 | 0.60 | 4.5 / 0.00 | 0.25 | 0.00 |
| Black Radio | Robert Glasper, *Black Radio* | 0.35 | 1.60 | -6 | 0.10 | 3.0 / 0.20 | 0.90 | 0.20 |
| Suitcase Pan | Rhodes Suitcase stéréo 70s | 0.30 | 1.00 | -6 | 0.15 | 5.5 / 0.70 | 0.80 | 0.00 |
| Velvet Bark | Comping funk tapé fort | 0.90 | 1.00 | -8 | 0.35 | 4.5 / 0.00 | 0.70 | 0.00 |
| Slow Swirl | Rhodes 70s + phaser | 0.45 | 1.50 | -6 | 0.20 | 4.5 / 0.00 | 0.70 | 0.90 |
| Felt Dream | Slow jam tardif, très sombre | 0.20 | 1.80 | -5 | 0.10 | 2.0 / 0.15 | 0.20 | 0.00 |

---

## Implementation Units

### U1. Tone et Phaser dans le DSP partagé

**Goal:** Deux blocs DSP purs, sans JUCE, testés.
**Requirements:** R1, R2
**Dependencies:** aucune
**Files:** `shared/dsp/Effects.h`, `tests/dsp_tests.cpp`
**Approach:** `Tone` (2 × 1-pôle ou biquad LP, coupure lissée par `Smoother` pour éviter le zipper) et `Phaser` (4 passe-tout, LFO interne, mix lissé). `prepare(sr)` + `process(l, r, amount)` comme `Tremolo`.
**Patterns to follow:** `dsp::Tremolo`, `dsp::Smoother`, `dsp::drive` dans `shared/dsp/Effects.h`.
**Test scenarios:**
- Tone = 1 : sinus 1 kHz et 5 kHz atténués de moins de 0,5 dB.
- Tone = 0 : sinus 5 kHz atténué d'au moins 12 dB, 200 Hz de moins de 3 dB.
- Phaser = 0 : sortie identique à l'entrée (bypass exact).
- Phaser = 1 sur bruit/sinus balayé : l'énergie à une fréquence donnée varie dans le temps (notches qui bougent) ; à 1 kHz, l'écart max/min du RMS par fenêtre sur 4 s dépasse 6 dB.
- Valeurs extrêmes sur entrée pleine échelle : sortie finie et bornée (|y| ≤ 2).
**Verification:** `dsp_tests` passe.

### U2. Paramètres et chaîne du Rhodes

**Goal:** Ajouter `tone` et `phaser` à l'APVTS et les brancher dans `processBlock`.
**Requirements:** R1, R2, R6
**Dependencies:** U1
**Files:** `plugins/rhodes/src/PluginProcessor.cpp`, `plugins/rhodes/src/PluginProcessor.h`
**Approach:** IDs `tone` (défaut 1.0) et `phaser` (défaut 0.0), version de `ParameterID` 1. Insertion entre drive et trémolo. L'éditeur affiche automatiquement 8 potards (largeur ≈ 900 px).
**Test scenarios:** Test expectation: none -- glue JUCE ; le comportement DSP est couvert par U1, la non-régression par `auval`.
**Verification:** `auval -v aumu Rhdx Frik` réussit ; un set existant se recharge sans changement audible.

### U3. Table de presets et programmes hôte

**Goal:** 12 presets en C++ pur, exposés comme programmes.
**Requirements:** R3, R5
**Dependencies:** U2
**Files:** `plugins/rhodes/src/Presets.h` (nouveau), `plugins/rhodes/src/PluginProcessor.cpp`, `plugins/rhodes/src/PluginProcessor.h`, `tests/dsp_tests.cpp`
**Approach:** `Presets.h` déclare les specs de paramètres (id, min, max, défaut) et le tableau de presets (nom + valeurs). `createLayout()` lit les specs. `setCurrentProgram(i)` applique chaque valeur via `setValueNotifyingHost` (normalisée) et enregistre l'index dans l'état APVTS ; `getCurrentProgram` le relit.
**Test scenarios:**
- Entre 10 et 14 presets, noms uniques et non vides.
- Chaque preset définit une valeur pour chaque spec, dans [min, max].
- Le preset « Init » est égal aux défauts des specs.
**Verification:** tests verts ; dans Live, la liste de programmes du plugin (AU) affiche les 12 noms.

### U4. Menu de presets dans l'interface

**Goal:** `ComboBox` dans l'en-tête de `ui::Editor` quand `getNumPrograms() > 1`.
**Requirements:** R4
**Dependencies:** U3
**Files:** `shared/ui/FrikfrakEditor.h`
**Approach:** Remplit le menu depuis `getProgramName(i)`, sélection = `getCurrentProgram()`, `onChange` → `setCurrentProgram`. Les potards suivent via les attachments. Couleurs de la palette (fond `chip`, texte `fg`, flèche `accent`) via le LookAndFeel. Sinon : accroche affichée comme aujourd'hui.
**Test scenarios:** Test expectation: none -- UI ; vérification par rendu PNG (harnais hors dépôt déjà utilisé) et dans Live.
**Verification:** le rendu montre le menu aligné à droite de l'en-tête ; choisir un preset fait bouger les 8 potards ; Vinyl Noise inchangé.

### U5. Documentation

**Goal:** README (paramètres Tone/Phaser, liste des presets), `CLAUDE.md` (presets dans `Presets.h`).
**Requirements:** R3
**Dependencies:** U3
**Files:** `README.md`, `CLAUDE.md`
**Test scenarios:** Test expectation: none -- documentation.

---

## Scope Boundaries

- Pas de sauvegarde de presets utilisateur : les presets d'appareil de Live font ce travail.
- Pas de vitesse de phaser réglable, pas de Leslie/chorus.
- Pas de presets pour Vinyl Noise.

---

## Risks

- **Fidélité des presets** : aucune source ne documente les réglages réels (voir Sources) ; ce sont des interprétations à valider à l'oreille contre les disques.
- **Programmes VST3 dans Live** : Live affiche mal les programmes VST3, d'où le menu dans l'interface (R4) comme chemin principal.
- **Coût CPU** : négligeable (4 passe-tout + 2 pôles par canal) ; à Phaser = 0 le bloc est court-circuité.

---

## Sources & Research

- Recherche web (snippets, pas de pages lues) : vibrato Suitcase = modulation d'amplitude, autopan stéréo à partir de 1969 — [Wikipedia Rhodes piano](https://en.wikipedia.org/wiki/Rhodes_piano), [fenderrhodes.com effects](https://www.fenderrhodes.com/history/effects.html).
- Rhodes sur *Untitled (How Does It Feel)*, Electric Lady 1999 — [Wikipedia](https://en.wikipedia.org/wiki/Untitled_(How_Does_It_Feel)).
- Plage de phasing Rhodes 0,5–2 Hz : guide générique, faible fiabilité.
- Non trouvés : chaîne d'effets de *Voodoo* (Russ Elevado), réglages Badu/Dilla/Glasper. Les mappings genre → paramètres sont des choix à l'oreille.
