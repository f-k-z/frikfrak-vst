#pragma once
// Shared editor for every Frikfrak plugin: dark card, one rotary knob per parameter, logo header.
// Palette from the fgillet site (_vars.scss). Fonts are macOS system fallbacks (no font embedding).
#include <BinaryData.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace ui {

namespace col {
inline const juce::Colour accent { 0xFFFECD67 }, bg { 0xFF1C1C1C }, card { 0xFF252525 }, chip { 0xFF3E3E3E },
    knob { 0xFF222222 }, fg { 0xFFFFFFFF }, fgDim { 0xFFC7C7C7 }, fgMute { 0xFF777777 }, rule { 0x14FFFFFF };
}

inline juce::Font mono(float h) { return juce::FontOptions("Menlo", h, juce::Font::plain); }
inline juce::Font sans(float h, int style = juce::Font::plain) { return juce::FontOptions("Helvetica Neue", h, style); }
inline juce::Font serif(float h) { return juce::FontOptions("Didot", h, juce::Font::plain); }  // Playfair stand-in

class LookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) override {
        const auto b = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(4.0f);
        const float r = std::min(b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float angle = start + pos * (end - start);
        const juce::PathStrokeType stroke(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track, value;
        track.addCentredArc(c.x, c.y, r - 2, r - 2, 0, start, end, true);
        value.addCentredArc(c.x, c.y, r - 2, r - 2, 0, start, angle, true);
        g.setColour(col::chip);
        g.strokePath(track, stroke);
        g.setColour(s.isMouseOverOrDragging() ? col::accent.brighter(0.15f) : col::accent);
        g.strokePath(value, stroke);

        const float kr = r - 10;
        g.setColour(col::knob);
        g.fillEllipse(c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour(col::rule);
        g.drawEllipse(c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);

        juce::Path needle;
        needle.startNewSubPath(c.getPointOnCircumference(kr * 0.35f, angle));
        needle.lineTo(c.getPointOnCircumference(kr * 0.8f, angle));
        g.setColour(col::fg);
        g.strokePath(needle, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font getLabelFont(juce::Label&) override { return mono(12.0f); }  // knob value boxes
    juce::Font getComboBoxFont(juce::ComboBox&) override { return sans(14.0f); }
    juce::Font getPopupMenuFont() override { return sans(14.0f); }
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override {
        label.setBounds(12, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont(getComboBoxFont(box));
    }
};

class Editor : public juce::AudioProcessorEditor {
public:
    Editor(juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& apvts, juce::String taglineText)
        : AudioProcessorEditor(p), tagline(std::move(taglineText)) {
        setLookAndFeel(&lnf);
        logo = juce::Drawable::createFromImageData(BinaryData::logo_svg, BinaryData::logo_svgSize);

        for (auto* param : p.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(param)) {
                auto& k = *knobs.emplace_back(std::make_unique<Knob>(apvts, *rp));
                addAndMakeVisible(k.slider);
            }
        if (p.getNumPrograms() > 1) {  // factory presets menu
            for (int i = 0; i < p.getNumPrograms(); ++i) presets.addItem(p.getProgramName(i), i + 1);
            presets.setSelectedId(p.getCurrentProgram() + 1, juce::dontSendNotification);
            presets.onChange = [this, &p] { p.setCurrentProgram(presets.getSelectedId() - 1); };
            presets.setColour(juce::ComboBox::backgroundColourId, col::chip);
            presets.setColour(juce::ComboBox::textColourId, col::fg);
            presets.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
            presets.setColour(juce::ComboBox::arrowColourId, col::accent);
            presets.setColour(juce::ComboBox::focusedOutlineColourId, col::accent);
            lnf.setColour(juce::PopupMenu::backgroundColourId, col::card);
            lnf.setColour(juce::PopupMenu::textColourId, col::fg);
            lnf.setColour(juce::PopupMenu::highlightedBackgroundColourId, col::accent);
            lnf.setColour(juce::PopupMenu::highlightedTextColourId, col::bg);
            addAndMakeVisible(presets);
        }
        setSize(2 * (kPad + kCardPad) + (int) knobs.size() * kCellW, kHeaderH + 2 * kPad + 2 * kCardPad + kCellH);
    }
    ~Editor() override { setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override {
        g.fillAll(col::bg);

        // Header: logo, brand, product name, tagline
        auto header = getLocalBounds().removeFromTop(kHeaderH).reduced(kPad, 0);
        if (logo) logo->drawWithin(g, header.removeFromLeft(34).withSizeKeepingCentre(34, 36).toFloat(), juce::RectanglePlacement::centred, 1.0f);
        header.removeFromLeft(14);
        const auto name = juce::String(JucePlugin_Name).fromFirstOccurrenceOf("Frikfrak ", false, false);
        g.setColour(col::accent);
        g.setFont(mono(10.0f).withExtraKerningFactor(0.25f));
        g.drawText("FRIKFRAK", header.withTrimmedBottom(kHeaderH / 2 + 2), juce::Justification::bottomLeft);
        g.setColour(col::fg);
        g.setFont(serif(26.0f));
        g.drawText(name, header.withTrimmedTop(kHeaderH / 2 - 4), juce::Justification::topLeft);
        if (presets.isVisible()) {
            g.setColour(col::fgMute);
            g.setFont(mono(10.0f).withExtraKerningFactor(0.25f));
            g.drawText("PRESET", presets.getBounds().translated(-70, 0).withWidth(60), juce::Justification::centredRight);
        } else {
            g.setColour(col::fgMute);
            g.setFont(mono(10.0f).withExtraKerningFactor(0.15f));
            g.drawText(tagline.toUpperCase(), header, juce::Justification::centredRight);
        }
        g.setColour(col::rule);
        g.fillRect(0, kHeaderH - 1, getWidth(), 1);

        // Card with one cell per knob, parameter name above each knob
        const auto card = cardBounds().toFloat();
        g.setColour(col::card);
        g.fillRoundedRectangle(card, 10.0f);
        g.setColour(col::rule);
        g.drawRoundedRectangle(card.reduced(0.5f), 10.0f, 1.0f);
        g.setFont(sans(11.0f, juce::Font::bold).withExtraKerningFactor(0.08f));
        g.setColour(col::fgDim);
        for (size_t i = 0; i < knobs.size(); ++i)
            g.drawFittedText(knobs[i]->name.toUpperCase(), cell(i).removeFromTop(kNameH), juce::Justification::centredBottom, 2);
    }

    void resized() override {
        presets.setBounds(getLocalBounds().removeFromTop(kHeaderH).reduced(kPad, 0).removeFromRight(210).withSizeKeepingCentre(210, 32));
        for (size_t i = 0; i < knobs.size(); ++i) knobs[i]->slider.setBounds(cell(i).withTrimmedTop(kNameH + 6));
    }

private:
    static constexpr int kHeaderH = 72, kPad = 20, kCardPad = 14, kCellW = 104, kNameH = 28, kCellH = kNameH + 6 + 78 + 22;

    struct Knob {
        Knob(juce::AudioProcessorValueTreeState& apvts, juce::RangedAudioParameter& p)
            : name(p.getName(40).upToFirstOccurrenceOf(" (", false, false)), attachment(apvts, p.getParameterID(), slider) {
            const auto unitText = p.getName(40).fromFirstOccurrenceOf("(", false, false).upToFirstOccurrenceOf(")", false, false);  // "Volume (dB)" -> dB
            // Set on the slider itself: its text box is built before the editor's LookAndFeel applies.
            slider.setColour(juce::Slider::textBoxTextColourId, col::fgDim);
            slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setRepaintsOnMouseActivity(true);  // hover brightens the arc
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, kCellW - 8, 22);
            slider.setDoubleClickReturnValue(true, p.convertFrom0to1(p.getDefaultValue()));
            const auto range = p.getNormalisableRange();
            const bool unit = range.start == 0.0f && range.end == 1.0f;  // 0..1 params read as percent
            slider.textFromValueFunction = [unit, unitText](double v) {
                if (unit) return juce::String(juce::roundToInt(v * 100)) + " %";
                return (juce::String(v, std::abs(v) < 10 ? 2 : 1) + " " + unitText).trimEnd();
            };
            slider.updateText();
        }
        juce::String name;
        juce::Slider slider;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;  // after slider: destroyed first
    };

    juce::Rectangle<int> cardBounds() const { return getLocalBounds().withTrimmedTop(kHeaderH).reduced(kPad); }
    juce::Rectangle<int> cell(size_t i) const {
        return cardBounds().reduced(kCardPad).withWidth(kCellW).translated((int) i * kCellW, 0).withHeight(kCellH);
    }

    LookAndFeel lnf;  // first: outlives the knobs
    juce::String tagline;
    std::unique_ptr<juce::Drawable> logo;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::ComboBox presets;
};

}  // namespace ui
