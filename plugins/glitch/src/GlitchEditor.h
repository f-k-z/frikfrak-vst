#pragma once
// Glitch editor: one column per effect (name, pad, 1-2 knobs) on 2 rows of 6, plus Hold.
// Effect pads are momentary under the mouse; Hold toggles on click (one pointer can't hold a pad
// and Hold at once). Pads light from the processor's active mask, so MIDI notes and latches show.
#include "PluginProcessor.h"

namespace glitch {

class PadButton : public juce::Component {
public:
    PadButton(juce::RangedAudioParameter& p, bool toggle) : param(p), isToggle(toggle) { setRepaintsOnMouseActivity(true); }

    void setLit(bool on) { if (on != lit) { lit = on; repaint(); } }

    void paint(juce::Graphics& g) override {
        const auto b = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(lit ? ui::col::accent : (isMouseOver() ? ui::col::chip.brighter(0.15f) : ui::col::chip));
        g.fillRoundedRectangle(b, 8.0f);
        g.setColour(lit ? ui::col::accent.brighter(0.3f) : ui::col::rule);
        g.drawRoundedRectangle(b, 8.0f, 1.0f);
    }
    void mouseDown(const juce::MouseEvent&) override {
        param.beginChangeGesture();
        param.setValueNotifyingHost(isToggle ? (param.getValue() > 0.5f ? 0.0f : 1.0f) : 1.0f);
        if (isToggle) param.endChangeGesture();
    }
    void mouseUp(const juce::MouseEvent&) override {
        if (isToggle) return;
        param.setValueNotifyingHost(0.0f);
        param.endChangeGesture();
    }

private:
    juce::RangedAudioParameter& param;
    bool isToggle, lit = false;
};

class GlitchEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit GlitchEditor(GlitchProcessor& p) : AudioProcessorEditor(p), proc(p) {
        setLookAndFeel(&lnf);
        logo = juce::Drawable::createFromImageData(BinaryData::logo_svg, BinaryData::logo_svgSize);
        for (size_t i = 0; i < kPads.size(); ++i) {
            auto* prm = dynamic_cast<juce::RangedAudioParameter*>(p.params.getParameter(kPads[i].id));
            auto& col = columns.emplace_back();
            col.name = juce::String(kPads[i].name).toUpperCase();
            col.pad = std::make_unique<PadButton>(*prm, (int) i == kFxCount);  // the last pad is Hold
            addAndMakeVisible(*col.pad);
            for (const auto& k : kKnobs)
                if ((size_t) k.fx == i)
                    if (auto* kp = dynamic_cast<juce::RangedAudioParameter*>(p.params.getParameter(k.id))) {
                        auto& knob = *col.knobs.emplace_back(std::make_unique<ui::Knob>(p.params, *kp, kColW / 2 - 4));
                        knob.name = knob.name.fromFirstOccurrenceOf(juce::String(kPads[i].name) + " ", false, false).toUpperCase();
                        addAndMakeVisible(knob.slider);
                    }
        }
        setSize(2 * (kPad + kCardPad) + kCols * kColW, kHeaderH + 2 * kPad + 2 * kCardPad + kRows * kColH);
        startTimerHz(30);
    }
    ~GlitchEditor() override { stopTimer(); setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override {
        g.fillAll(ui::col::bg);
        const auto header = ui::drawBrand(g, getLocalBounds().removeFromTop(kHeaderH).reduced(kPad, 0), logo.get(), kHeaderH);
        g.setColour(ui::col::fgMute);
        g.setFont(ui::mono(10.0f).withExtraKerningFactor(0.15f));
        g.drawText("HOLD A PAD, PLAY THE MIX", header, juce::Justification::centredRight);
        g.setColour(ui::col::rule);
        g.fillRect(0, kHeaderH - 1, getWidth(), 1);

        const auto card = getLocalBounds().withTrimmedTop(kHeaderH).reduced(kPad).toFloat();
        g.setColour(ui::col::card);
        g.fillRoundedRectangle(card, 10.0f);
        g.setColour(ui::col::rule);
        g.drawRoundedRectangle(card.reduced(0.5f), 10.0f, 1.0f);

        for (size_t i = 0; i < columns.size(); ++i) {
            auto c = column(i);
            g.setColour(ui::col::fgDim);
            g.setFont(ui::sans(11.0f, juce::Font::bold).withExtraKerningFactor(0.08f));
            g.drawText(columns[i].name, c.removeFromTop(kNameH), juce::Justification::centred);
            const auto& knobs = columns[i].knobs;
            g.setFont(ui::mono(9.0f).withExtraKerningFactor(0.1f));
            g.setColour(ui::col::fgMute);
            for (size_t k = 0; k < knobs.size(); ++k) g.drawText(knobs[k]->name, knobArea(i, k).removeFromTop(14), juce::Justification::centred);
            if (knobs.empty()) g.drawFittedText(i + 1 == columns.size() ? "CLICK: LATCH\nPADS ON/OFF" : "", c.withTrimmedTop(kPadH + 16), juce::Justification::centredTop, 2);
        }
    }

    void resized() override {
        for (size_t i = 0; i < columns.size(); ++i) {
            columns[i].pad->setBounds(column(i).withTrimmedTop(kNameH).removeFromTop(kPadH).reduced(10, 0));
            for (size_t k = 0; k < columns[i].knobs.size(); ++k) columns[i].knobs[k]->slider.setBounds(knobArea(i, k).withTrimmedTop(14));
        }
    }

private:
    static constexpr int kHeaderH = 72, kPad = 20, kCardPad = 14, kCols = 6, kRows = 2, kColW = 124, kColH = 196, kNameH = 26, kPadH = 54;

    struct Column {
        juce::String name;
        std::unique_ptr<PadButton> pad;
        std::vector<std::unique_ptr<ui::Knob>> knobs;
    };

    void timerCallback() override {
        const auto mask = proc.activeMask.load(std::memory_order_relaxed);
        for (size_t i = 0; i < columns.size(); ++i) columns[i].pad->setLit((mask >> i) & 1u);
    }

    juce::Rectangle<int> column(size_t i) const {
        return getLocalBounds().withTrimmedTop(kHeaderH).reduced(kPad + kCardPad).withSize(kColW, kColH)
            .translated((int) (i % kCols) * kColW, (int) (i / kCols) * kColH);
    }
    juce::Rectangle<int> knobArea(size_t i, size_t k) const {  // 1 knob centred, 2 knobs side by side
        auto area = column(i).withTrimmedTop(kNameH + kPadH + 10).reduced(4, 0);
        const auto n = columns[i].knobs.size();
        const int w = area.getWidth() / 2;
        return n == 1 ? area.withSizeKeepingCentre(w, area.getHeight()) : area.withWidth(w).translated((int) k * w, 0);
    }

    GlitchProcessor& proc;
    ui::LookAndFeel lnf;  // first: outlives the knobs
    std::unique_ptr<juce::Drawable> logo;
    std::vector<Column> columns;
};

}  // namespace glitch
