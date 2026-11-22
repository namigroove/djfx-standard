#include "PluginEditor.h"
#if NAMIDJFX_HAS_FONT
 #include "BinaryData.h"
#endif
using namespace juce;

static float textWidth (const Font& f, const String& s)
{
    GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.f, 0.f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// ------------------------------------------------------------------ LookAndFeel
NamiLnF::NamiLnF()
{
#if NAMIDJFX_HAS_FONT
    face = Typeface::createSystemTypefaceFor (BinaryData::UIFont_ttf, (size_t) BinaryData::UIFont_ttfSize);
    faceBold = Typeface::createSystemTypefaceFor (BinaryData::UIFontBold_ttf, (size_t) BinaryData::UIFontBold_ttfSize);
    embedded = face != nullptr;
#endif
    setColour (Label::textColourId, ui::text);
}

Font NamiLnF::font (float scale) const
{
    if (face != nullptr) return Font (FontOptions (face).withHeight (13.f * scale));
    return Font (FontOptions (Font::getDefaultMonospacedFontName(), 11.f * scale, Font::plain));
}
Font NamiLnF::fontBold (float scale) const
{
    if (faceBold != nullptr) return Font (FontOptions (faceBold).withHeight (13.f * scale));
    return Font (FontOptions (Font::getDefaultMonospacedFontName(), 11.f * scale, Font::bold));
}

// 小さなパッド。点灯中は縁と文字に色が付く。"led"なら左にランプ、"round"なら丸い大ボタン。
void NamiLnF::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hover, bool down)
{
    const Colour accent ((uint32) (int) b.getProperties().getWithDefault ("accent", (int) ui::teal.getARGB()));
    const bool on = b.getToggleState();
    const bool round = (bool) b.getProperties().getWithDefault ("round", false);
    auto r = b.getLocalBounds().toFloat();

    if (round)
    {
        const float d = std::min (r.getWidth(), r.getHeight()) - 2.f;
        auto c = r.getCentre();
        g.setColour (ui::well);   g.fillEllipse (c.x - d / 2 - 3, c.y - d / 2 - 3, d + 6, d + 6);
        g.setColour (ui::padEdge); g.drawEllipse (c.x - d / 2 - 3, c.y - d / 2 - 3, d + 6, d + 6, 1.f);
        g.setColour (on ? accent : ui::line);
        g.fillEllipse (c.x - d / 2, c.y - d / 2, d, d);                 // リングのLED
        const float di = d - 7.f;
        g.setColour (down ? ui::panel : ui::padOff);
        g.fillEllipse (c.x - di / 2, c.y - di / 2, di, di);
        g.setColour (ui::padEdge);
        g.drawEllipse (c.x - di / 2, c.y - di / 2, di, di, 1.f);
        return;
    }

    r = r.reduced (0.5f);
    g.setColour (down ? ui::panel : ui::padOff);
    g.fillRoundedRectangle (r, 2.f);
    if (on) { g.setColour (accent.withAlpha (0.14f)); g.fillRoundedRectangle (r, 2.f); }
    g.setColour (on ? accent : (hover ? ui::dim : ui::padEdge));
    g.drawRoundedRectangle (r, 2.f, 1.f);
    if ((bool) b.getProperties().getWithDefault ("led", false))
    {
        const float cy = r.getCentreY();
        g.setColour (on ? accent : Colour (0xff202c47));
        g.fillEllipse (r.getX() + 7.f, cy - 2.5f, 5.f, 5.f);
    }
}

void NamiLnF::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const Colour accent ((uint32) (int) b.getProperties().getWithDefault ("accent", (int) ui::teal.getARGB()));
    const bool on = b.getToggleState();
    const bool round = (bool) b.getProperties().getWithDefault ("round", false);
    g.setFont (font (round ? 0.8f : 0.9f));
    g.setColour (on ? (round ? ui::text : accent) : ui::text.withAlpha (0.82f));
    auto area = b.getLocalBounds().reduced (2, 0);
    if ((bool) b.getProperties().getWithDefault ("led", false)) area.removeFromLeft (10);
    g.drawText (b.getButtonText(), area, Justification::centred, false);
}

// 滑り止めの刻みが付いた金属キャップのノブ。ポインタは白。
void NamiLnF::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s)
{
    const auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float d = std::min (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float rOuter = d * 0.5f - 1.f;
    const float rBody = rOuter - 7.f;
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const bool dimmed = s.getAlpha() < 0.9f;

    for (int i = 0; i <= 10; ++i)   // 外周の目盛り(シルク)
    {
        const float a = a0 + (a1 - a0) * (float) i / 10.f;
        const bool mid = bipolar && i == 5;
        const float r0 = rOuter - (mid ? 6.f : (i % 5 == 0 ? 4.5f : 3.f)), r1 = rOuter;
        g.setColour (mid ? ui::amber : ui::dim.withAlpha (0.85f));
        g.drawLine (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0, c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, 1.f);
    }
    g.setColour (ui::well);                                  // 取り付け穴の影
    g.fillEllipse (c.x - rBody - 1.5f, c.y - rBody - 1.5f, (rBody + 1.5f) * 2, (rBody + 1.5f) * 2);
    g.setColour (Colour (0xff1b2745));                       // キャップ側面
    g.fillEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2);
    for (int i = 0; i < 28; ++i)                             // 刻み
    {
        const float a = MathConstants<float>::twoPi * (float) i / 28.f;
        g.setColour (ui::line);
        g.drawLine (c.x + std::sin (a) * (rBody - 3.2f), c.y - std::cos (a) * (rBody - 3.2f),
                    c.x + std::sin (a) * rBody, c.y - std::cos (a) * rBody, 1.4f);
    }
    g.setColour (ui::padEdge);
    g.drawEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2, 1.f);
    const float rTop = rBody - 4.f;                          // キャップの天面
    g.setColour (Colour (0xff111a31));
    g.fillEllipse (c.x - rTop, c.y - rTop, rTop * 2, rTop * 2);
    g.setColour (Colour (0xff26355a));
    g.drawEllipse (c.x - rTop, c.y - rTop, rTop * 2, rTop * 2, 1.f);
    const float a = a0 + pos * (a1 - a0);
    g.setColour (dimmed ? ui::dim : ui::text);
    g.drawLine (c.x + std::sin (a) * (rTop * 0.2f), c.y - std::cos (a) * (rTop * 0.2f),
                c.x + std::sin (a) * (rTop - 1.5f), c.y - std::cos (a) * (rTop - 1.5f), 2.f);
}

// ------------------------------------------------------------------ Knob
Knob::Knob (AudioProcessorValueTreeState& s, const ParameterID& id, const String& n, std::function<String()> vt, bool bipolar, bool stepped)
    : name (n), valueText (std::move (vt))
{
    slider.setSliderStyle (Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    slider.getProperties().set ("bipolar", bipolar);
    slider.getProperties().set ("stepped", stepped);
    slider.setVelocityBasedMode (false);
    if (bipolar) slider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (slider);
    att = std::make_unique<SliderParameterAttachment> (*s.getParameter (id.getParamID()), slider);
}
void Knob::resized()
{
    const int d = std::min (getWidth(), getHeight() - 28);
    slider.setBounds ((getWidth() - d) / 2, 0, d, d);
}
void Knob::paint (Graphics& g)
{
    auto& l = dynamic_cast<NamiLnF&> (getLookAndFeel());
    g.setFont (l.font (0.9f));
    g.setColour (ui::amber);
    g.drawText (valueText ? valueText() : String(), 0, getHeight() - 28, getWidth(), 14, Justification::centred, false);
    g.setColour (ui::text.withAlpha (0.85f));
    g.drawText (name, 0, getHeight() - 14, getWidth(), 14, Justification::centred, false);
}

// ------------------------------------------------------------------ ChoiceGroup
ChoiceGroup::ChoiceGroup (AudioProcessorValueTreeState& s, const ParameterID& id, StringArray labels, int c, int g, bool led)
    : apvts (s), pid (id.getParamID()), cols (c), gap (g)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new TextButton (labels[i]));
        b->getProperties().set ("led", led);
        b->onClick = [this, i]
        {
            auto* p = apvts.getParameter (pid);
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) i));
            p->endChangeGesture();
        };
        addAndMakeVisible (b);
    }
    startTimerHz (30);
}
void ChoiceGroup::resized()
{
    const int n = buttons.size(), rows = (n + cols - 1) / cols;
    const int w = (getWidth() - gap * (cols - 1)) / cols, h = (getHeight() - gap * (rows - 1)) / rows;
    for (int i = 0; i < n; ++i)
        buttons[i]->setBounds ((i % cols) * (w + gap), (i / cols) * (h + gap), w, h);
}
void ChoiceGroup::timerCallback()
{
    const int cur = (int) apvts.getRawParameterValue (pid)->load();
    for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == cur, dontSendNotification);
}

// ------------------------------------------------------------------ X-PAD
XPad::XPad (AudioProcessorValueTreeState& s)
    : pos (*s.getParameter (ids::xpadPos.getParamID()), [this] (float v) { x = v; repaint(); }),
      touch (*s.getParameter (ids::xpadTouch.getParamID()), [this] (float v) { touching = v > 0.5f; repaint(); })
{
    pos.sendInitialUpdate(); touch.sendInitialUpdate();
}
void XPad::setFrom (const MouseEvent& e) { pos.setValueAsPartOfGesture (jlimit (0.f, 1.f, e.position.x / (float) getWidth())); }
void XPad::mouseDown (const MouseEvent& e) { pos.beginGesture(); touch.beginGesture(); touch.setValueAsPartOfGesture (1.f); setFrom (e); }
void XPad::mouseDrag (const MouseEvent& e) { setFrom (e); }
void XPad::mouseUp (const MouseEvent&) { touch.setValueAsPartOfGesture (0.f); touch.endGesture(); pos.endGesture(); }
void XPad::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ui::well);
    g.fillRoundedRectangle (r, 3.f);
    if (touching) { g.setColour (ui::amber.withAlpha (0.12f)); g.fillRoundedRectangle (r, 3.f); }
    g.setColour (Colour (0xff1c2a47));                          // 面のドット格子
    for (int ix = 1; ix < 16; ++ix)
        for (int iy = 1; iy < 7; ++iy)
            g.fillRect (r.getWidth() * (float) ix / 16.f - 0.5f, r.getHeight() * (float) iy / 7.f - 0.5f, 1.5f, 1.5f);
    g.setColour (touching ? ui::amber : ui::padEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.f, 1.f);
    const float px = x * r.getWidth();
    g.setColour (touching ? ui::amber : ui::text.withAlpha (0.8f));
    g.fillRect (px - 1.f, 3.f, 2.f, r.getHeight() - 6.f);
    auto& l = dynamic_cast<NamiLnF&> (getLookAndFeel());
    g.setFont (l.font (0.85f)); g.setColour (ui::dim);
    g.drawText ("X-PAD", 8, getHeight() - 18, 80, 14, Justification::left, false);
}

// ------------------------------------------------------------------ JOG
JogWheel::JogWheel (AudioProcessorValueTreeState& s)
    : speed (*s.getParameter (ids::jogSpeed.getParamID()), [] (float) {}),
      touch (*s.getParameter (ids::jogScratch.getParamID()), [] (float) {})
{
    startTimerHz (60);
}
float JogWheel::angleAt (Point<float> p) const
{
    auto c = getLocalBounds().toFloat().getCentre();
    return std::atan2 (p.y - c.y, p.x - c.x);
}
void JogWheel::mouseDown (const MouseEvent& e)
{
    dragging = true; touch.beginGesture(); speed.beginGesture();
    touch.setValueAsPartOfGesture (1.f);
    lastAngle = curAngle = angleAt (e.position); speedSmoothed = 0.f;
    lastTime = Time::getMillisecondCounterHiRes();
}
void JogWheel::mouseDrag (const MouseEvent& e) { curAngle = angleAt (e.position); }
void JogWheel::mouseUp (const MouseEvent&)
{
    dragging = false;
    speed.setValueAsPartOfGesture (0.f); touch.setValueAsPartOfGesture (0.f);
    speed.endGesture(); touch.endGesture();
}
void JogWheel::timerCallback()
{
    if (! dragging) return;
    const double now = Time::getMillisecondCounterHiRes();
    const float dt = (float) jmax (0.001, (now - lastTime) * 0.001);
    float d = curAngle - lastAngle;
    while (d > MathConstants<float>::pi) d -= MathConstants<float>::twoPi;
    while (d < -MathConstants<float>::pi) d += MathConstants<float>::twoPi;
    shown += d;
    const float target = jlimit (-4.f, 4.f, d / MathConstants<float>::twoPi / dt / 0.5556f);   // 33 1/3 rpm = 等速
    speedSmoothed += (target - speedSmoothed) * 0.5f;
    speed.setValueAsPartOfGesture (speedSmoothed);
    lastAngle = curAngle; lastTime = now;
    repaint();
}
void JogWheel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.f);
    const auto c = r.getCentre();
    const float R = std::min (r.getWidth(), r.getHeight()) * 0.5f;
    auto circle = [&] (float rad) { return Rectangle<float> (c.x - rad, c.y - rad, rad * 2, rad * 2); };

    g.setColour (ui::well);    g.fillEllipse (circle (R));                       // 外側のベゼル
    g.setColour (ui::padEdge); g.drawEllipse (circle (R), 1.f);
    g.setColour (Colour (0xff0a1226)); g.fillEllipse (circle (R - 5.f));         // ジョグ面
    g.setColour (dragging ? ui::amber.withAlpha (0.8f) : Colour (0xff1c2a47));
    g.drawEllipse (circle (R - 5.f), 1.f);

    for (int i = 0; i < 40; ++i)                                                  // 回るくぼみ
    {
        const float a = shown + MathConstants<float>::twoPi * (float) i / 40.f;
        const float rr = R - 17.f;
        const Rectangle<float> dot (c.x + std::cos (a) * rr - 2.4f, c.y + std::sin (a) * rr - 2.4f, 4.8f, 4.8f);
        g.setColour (Colour (0xff060b1a)); g.fillEllipse (dot);
        g.setColour (i == 0 ? ui::amber : Colour (0xff22314f)); g.drawEllipse (dot, 1.f);
    }
    const float rp = R * 0.66f;                                                   // 内側のプラッター
    g.setColour (Colour (0xff0d1529)); g.fillEllipse (circle (rp));
    g.setColour (ui::padEdge);         g.drawEllipse (circle (rp), 1.f);

    const float ra = rp - 9.f;                                                    // 左右で色が違うアーク(回転に追従)
    for (int side = 0; side < 2; ++side)
    {
        Path arc;
        const float start = shown + (side ? -MathConstants<float>::halfPi : MathConstants<float>::halfPi) + 0.35f;
        arc.addCentredArc (c.x, c.y, ra, ra, 0.f, start, start + MathConstants<float>::pi - 0.7f, true);
        g.setColour ((side ? ui::amber : ui::teal).withAlpha (dragging ? 1.f : 0.8f));
        g.strokePath (arc, PathStrokeType (5.f, PathStrokeType::curved, PathStrokeType::butt));
    }
    const float rc = ra - 10.f;
    g.setColour (ui::panel); g.fillEllipse (circle (rc));
    g.setColour (ui::padEdge); g.drawEllipse (circle (rc), 1.f);
    auto& l = dynamic_cast<NamiLnF&> (getLookAndFeel());
    g.setFont (l.fontBold (0.95f)); g.setColour (ui::text);
    g.drawText ("SCRATCH", Rectangle<int> ((int) (c.x - rc), (int) (c.y - 14), (int) (rc * 2), 14), Justification::centred, false);
    g.setFont (l.font (0.85f)); g.setColour (ui::dim);
    g.drawText ("DRAG", Rectangle<int> ((int) (c.x - rc), (int) (c.y + 2), (int) (rc * 2), 14), Justification::centred, false);
}

// ------------------------------------------------------------------ 液晶表示
FxDisplay::FxDisplay (NamiDJFXProcessor& p) : proc (p) { startTimerHz (20); }
void FxDisplay::paint (Graphics& g)
{
    auto& l = dynamic_cast<NamiLnF&> (getLookAndFeel());
    auto f = [&] (const ParameterID& id) { return proc.apvts.getRawParameterValue (id.getParamID())->load(); };
    const int type = jlimit (0, djfx::kNumFx - 1, (int) f (ids::fxType));
    const bool sync = f (ids::fxSync) > 0.5f, touch = f (ids::xpadTouch) > 0.5f, on = f (ids::fxOn) > 0.5f || touch;
    const float norm = touch ? f (ids::xpadPos) : f (ids::fxBeat);
    const float bpm = proc.lastBpm.load();
    const auto list = djfx::beatListFor (type);
    const double beats = std::fabs (list.v[djfx::beatIndex (type, norm)]);
    const double ms = sync ? beats * 60000.0 / bpm : (touch ? (double) djfx::expMap (jlimit (0.f, 1.f, f (ids::xpadPos)), 10.f, 4000.f) : (double) f (ids::fxTime));

    auto r = getLocalBounds().toFloat();
    g.setColour (ui::well);    g.fillRoundedRectangle (r, 3.f);
    g.setColour (Colour (0xff02050d)); g.drawRoundedRectangle (r.reduced (0.5f), 3.f, 1.f);
    g.setColour (ui::padEdge); g.drawRoundedRectangle (r.reduced (1.5f), 2.f, 1.f);

    const int x0 = 10, w = getWidth() - 20;
    g.setFont (l.font (0.85f));
    g.setColour (ui::dim);
    g.drawText ("BEAT FX", x0, 6, 80, 14, Justification::left, false);
    g.setColour (ui::amber);
    g.drawText (String (bpm, 1) + " BPM", x0, 6, w, 14, Justification::right, false);

    g.setFont (l.fontBold (1.55f));
    g.setColour (on ? ui::teal : ui::teal.withAlpha (0.45f));
    g.drawText (djfx::fxName (type), x0, 22, w, 26, Justification::centred, false);

    g.setFont (l.font (0.9f)); g.setColour (ui::text.withAlpha (0.9f));
    const String beatTxt = type == djfx::kReverb ? String (djfx::beatLabel (type, norm)) : String (djfx::beatLabel (type, norm));
    g.drawText ((sync ? "BEAT " : "TIME ") + (sync ? beatTxt : String ((int) ms) + "ms"), x0, 50, w / 2, 14, Justification::left, false);
    g.drawText (sync ? String ((int) ms) + "ms" : String ("FREE"), x0, 50, w, 14, Justification::right, false);

    const float level = f (ids::fxLevel);                                           // レベルのセグメント表示
    const int segs = 16, segW = (w - (segs - 1) * 2) / segs;
    const int lit = (int) std::lround (level * segs);
    for (int i = 0; i < segs; ++i)
    {
        g.setColour (i < lit ? (i >= segs - 3 ? ui::amber : ui::teal) : Colour (0xff111c36));
        g.fillRect (x0 + i * (segW + 2), 68, segW, 7);
    }
}

// ------------------------------------------------------------------ Editor
static String pct (float v) { return String ((int) std::lround (v * 100.f)) + "%"; }

TextButton* NamiDJFXEditor::addToggle (const ParameterID& id, const String& label, Colour accent)
{
    auto* b = toggles.add (new TextButton (label));
    b->setClickingTogglesState (true);
    b->getProperties().set ("accent", (int) accent.getARGB());
    btnAtts.add (new ButtonParameterAttachment (*proc.apvts.getParameter (id.getParamID()), *b));
    addAndMakeVisible (b);
    return b;
}
HoldButton* NamiDJFXEditor::addHold (const ParameterID& id, const String& label)
{
    auto* b = holds.add (new HoldButton());
    b->setButtonText (label);
    const String pid = id.getParamID();
    b->onHold = [this, pid] (bool down)
    {
        auto* p = proc.apvts.getParameter (pid);
        if (down) p->beginChangeGesture();
        p->setValueNotifyingHost (down ? 1.f : 0.f);
        if (! down) p->endChangeGesture();
    };
    addAndMakeVisible (b);
    return b;
}

NamiDJFXEditor::NamiDJFXEditor (NamiDJFXProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    StringArray fxNames, colNames;
    for (int i = 0; i < djfx::kNumFx; ++i) fxNames.add (djfx::fxName (i));
    for (int i = 0; i < djfx::kNumColor; ++i) colNames.add (djfx::colorName (i));

    fxTypes = std::make_unique<ChoiceGroup> (s, ids::fxType, fxNames, 1, 2, true);
    fxBands = std::make_unique<ChoiceGroup> (s, ids::fxBand, StringArray { "ALL", "LOW", "MID", "HI" }, 4, 4);
    colorTypes = std::make_unique<ChoiceGroup> (s, ids::colorType, colNames, 1, 2, true);
    latency = std::make_unique<ChoiceGroup> (s, ids::jogLatency, StringArray { "0", "60", "125", "250", "500" }, 5, 3);
    xpad = std::make_unique<XPad> (s);
    jog = std::make_unique<JogWheel> (s);
    lcd = std::make_unique<FxDisplay> (proc);
    for (auto* c : { (Component*) fxTypes.get(), (Component*) fxBands.get(), (Component*) colorTypes.get(), (Component*) latency.get(),
                     (Component*) xpad.get(), (Component*) jog.get(), (Component*) lcd.get() })
        addAndMakeVisible (c);

    bOn = addToggle (ids::fxOn, "ON/OFF", ui::amber);
    bOn->getProperties().set ("round", true);
    bSync = addToggle (ids::fxSync, "BPM SYNC");
    bLock = addToggle (ids::centerLock, "CENTER LOCK", ui::amber);
    hBack = addHold (ids::jogBackspin, "BACKSPIN");
    hBrake = addHold (ids::jogBrake, "BRAKE");
    hRev = addHold (ids::jogReverse, "REVERSE");
    hLoop = addHold (ids::jogLoop, "LOOP");
    for (auto* h : { hBack, hBrake, hRev, hLoop }) h->getProperties().set ("accent", (int) ui::amber.getARGB());

    auto typeNow = [this] { return (int) raw (ids::fxType); };
    kBeat = std::make_unique<Knob> (s, ids::fxBeat, "BEAT", [this, typeNow] { return String (djfx::beatLabel (typeNow(), raw (ids::fxBeat))); }, false, true);
    kBeat->slider.stepCount = [this, typeNow] { return typeNow() == djfx::kReverb ? 0 : djfx::beatListFor (typeNow()).n; };
    kTime = std::make_unique<Knob> (s, ids::fxTime, "TIME", [this] { return String ((int) raw (ids::fxTime)) + "ms"; });
    kLevel = std::make_unique<Knob> (s, ids::fxLevel, "LEVEL/DEPTH", [this] { return pct (raw (ids::fxLevel)); });
    kColor = std::make_unique<Knob> (s, ids::colorKnob, "COLOR", [this]
    {
        const float c = raw (ids::colorKnob);
        return std::fabs (c) < 0.02f ? String ("CENTER") : String (c < 0 ? "L " : "R ") + String ((int) std::lround (std::fabs (c) * 100.f));
    }, true);
    kParam = std::make_unique<Knob> (s, ids::colorParam, "PARAMETER", [this] { return pct (raw (ids::colorParam)); });
    kBs = std::make_unique<Knob> (s, ids::bsTime, "SPIN TIME", [this] { return String ((int) raw (ids::bsTime)) + "ms"; });
    kRate = std::make_unique<Knob> (s, ids::bsRate, "SPIN RATE", [this] { return "x" + String (raw (ids::bsRate), 1); });
    kBrake = std::make_unique<Knob> (s, ids::brakeTime, "BRAKE TIME", [this] { return String ((int) raw (ids::brakeTime)) + "ms"; });
    kLoop = std::make_unique<Knob> (s, ids::loopBeat, "LOOP BEAT", [this] { return String (djfx::beatLabel (djfx::kEcho, raw (ids::loopBeat))); }, false, true);
    kLoop->slider.stepCount = [] { return djfx::beatListFor (djfx::kEcho).n; };
    kBpm = std::make_unique<Knob> (s, ids::bpm, "BPM (NO HOST)", [this] { return String ((int) raw (ids::bpm)); });
    kOut = std::make_unique<Knob> (s, ids::outGain, "OUT", [this] { return String (raw (ids::outGain), 1) + "dB"; });
    for (auto* k : { kBeat.get(), kTime.get(), kLevel.get(), kColor.get(), kParam.get(), kBs.get(), kRate.get(), kBrake.get(), kLoop.get(), kBpm.get(), kOut.get() })
        addAndMakeVisible (k);

    setSize (1000, 640);
    startTimerHz (30);
}
NamiDJFXEditor::~NamiDJFXEditor() { setLookAndFeel (nullptr); }

void NamiDJFXEditor::timerCallback()
{
    const bool sync = raw (ids::fxSync) > 0.5f;
    kBeat->slider.setAlpha (sync ? 1.f : 0.35f);
    kTime->slider.setAlpha (sync ? 0.35f : 1.f);
    for (auto* k : { kBeat.get(), kTime.get(), kLevel.get(), kColor.get(), kParam.get(), kBs.get(), kRate.get(), kBrake.get(), kLoop.get(), kBpm.get(), kOut.get() })
        k->repaint();
    hBack->setToggleState (raw (ids::jogBackspin) > 0.5f, dontSendNotification);
    hBrake->setToggleState (raw (ids::jogBrake) > 0.5f, dontSendNotification);
    hRev->setToggleState (raw (ids::jogReverse) > 0.5f, dontSendNotification);
    hLoop->setToggleState (raw (ids::jogLoop) > 0.5f, dontSendNotification);
}

// 面板の金属テクスチャ(粒状のノイズ + 横方向のヘアライン)。毎回同じ模様になるよう乱数は固定。
void NamiDJFXEditor::buildFace (int w, int h)
{
    faceImage = Image (Image::RGB, w, h, false);
    Image::BitmapData bd (faceImage, Image::BitmapData::writeOnly);
    Random rnd (20261122);
    const float br = ui::face.getFloatRed() * 255.f, bg_ = ui::face.getFloatGreen() * 255.f, bb = ui::face.getFloatBlue() * 255.f;
    float rowBias = 0.f;
    for (int y = 0; y < h; ++y)
    {
        rowBias = rowBias * 0.55f + (rnd.nextFloat() - 0.5f) * 3.2f;
        for (int x = 0; x < w; ++x)
        {
            const float n = (rnd.nextFloat() - 0.5f) * 3.6f + rowBias + std::sin ((float) x * 0.012f + (float) y * 0.05f) * 0.9f;
            auto cl = [] (float v) { return (uint8) jlimit (0.f, 255.f, v); };
            bd.setPixelColour (x, y, Colour (cl (br + n), cl (bg_ + n * 1.05f), cl (bb + n * 1.2f)));
        }
    }
}

void NamiDJFXEditor::sectionHeader (Graphics& g, Rectangle<int> r, const String& title)
{
    g.setFont (lnf.fontBold (0.95f));
    const int tw = (int) textWidth (lnf.fontBold (0.95f), title);
    const int cx = r.getCentreX(), y = r.getY() + 12;
    g.setColour (ui::text);
    g.drawText (title, cx - tw / 2 - 4, r.getY() + 5, tw + 8, 14, Justification::centred, false);
    for (int side = 0; side < 2; ++side)                      // 見出しの両側に刻印の線
    {
        const int x0 = side ? cx + tw / 2 + 12 : r.getX() + 6;
        const int x1 = side ? r.getRight() - 6 : cx - tw / 2 - 12;
        g.setColour (ui::line);         g.drawHorizontalLine (y, (float) x0, (float) x1);
        g.setColour (Colour (0x22ffffff)); g.drawHorizontalLine (y + 1, (float) x0, (float) x1);
    }
}

void NamiDJFXEditor::paint (Graphics& g)
{
    if (! faceImage.isValid() || faceImage.getWidth() != getWidth() || faceImage.getHeight() != getHeight())
        buildFace (getWidth(), getHeight());
    g.drawImageAt (faceImage, 0, 0);

    // 上の帯(タイトル)と下の帯
    g.setColour (ui::bg.withAlpha (0.55f)); g.fillRect (0, 0, getWidth(), 38); g.fillRect (0, getHeight() - 36, getWidth(), 36);
    g.setColour (ui::line); g.drawHorizontalLine (38, 0.f, (float) getWidth()); g.drawHorizontalLine (getHeight() - 36, 0.f, (float) getWidth());
    g.setColour (Colour (0x22ffffff)); g.drawHorizontalLine (39, 0.f, (float) getWidth());

    g.setColour (ui::text);
    g.setFont (lnf.fontBold (1.5f));
    g.drawText ("DJFX Standard", 40, 5, 320, 28, Justification::left, false);
    g.setFont (lnf.font (0.85f)); g.setColour (ui::dim);
    g.drawText ("MIDI  C1 BACKSPIN / C#1 BRAKE / D1 REVERSE / D#1 SCRATCH / E1 LOOP / F1 BEAT FX", 330, 12, 630, 14, Justification::right, false);
    g.drawText ("BEAT FX x14   SOUND COLOR FX x6   JOG", 40, getHeight() - 26, 500, 14, Justification::left, false);
    g.drawText ("DJFX-STD", getWidth() - 140, getHeight() - 26, 100, 14, Justification::right, false);

    for (int dx : { 424, 644 })                                       // 区画の刻印線
    {
        g.setColour (Colour (0xff070c1c)); g.drawVerticalLine (dx, 50.f, 592.f);
        g.setColour (Colour (0x28ffffff)); g.drawVerticalLine (dx + 1, 50.f, 592.f);
    }
    sectionHeader (g, boxA, "BEAT FX");
    sectionHeader (g, boxB, "SOUND COLOR FX");
    sectionHeader (g, boxC, "JOG");

    g.setFont (lnf.font (0.85f)); g.setColour (ui::dim);
    g.drawText ("BAND", boxA.getX() + 212, boxA.getY() + 310, 80, 14, Justification::left, false);
    g.drawText ("LOOKAHEAD ms", boxC.getX() + 8, boxC.getY() + 406, 160, 14, Justification::left, false);

    for (auto pt : { Point<float> (18.f, 19.f), Point<float> ((float) getWidth() - 18.f, 19.f),
                     Point<float> (18.f, (float) getHeight() - 18.f), Point<float> ((float) getWidth() - 18.f, (float) getHeight() - 18.f) })
    {
        g.setColour (ui::well);   g.fillEllipse (pt.x - 7.f, pt.y - 7.f, 14.f, 14.f);
        g.setColour (Colour (0xff223052)); g.fillEllipse (pt.x - 5.f, pt.y - 5.f, 10.f, 10.f);
        g.setColour (ui::padEdge); g.drawEllipse (pt.x - 5.f, pt.y - 5.f, 10.f, 10.f, 1.f);
        g.setColour (ui::well);   g.drawLine (pt.x - 3.5f, pt.y + 1.5f, pt.x + 3.5f, pt.y - 1.5f, 1.4f);
    }
}

void NamiDJFXEditor::resized()
{
    boxA = { 8, 46, 412, 546 };
    boxB = { 430, 46, 208, 546 };
    boxC = { 650, 46, 342, 546 };
    auto at = [] (Rectangle<int> b, int x, int y, int w, int h) { return Rectangle<int> (b.getX() + x, b.getY() + y, w, h); };

    // BEAT FX
    fxTypes->setBounds (at (boxA, 8, 30, 192, 14 * 22 + 13 * 2));
    lcd->setBounds (at (boxA, 212, 30, 192, 84));
    xpad->setBounds (at (boxA, 212, 122, 192, 100));
    bOn->setBounds (at (boxA, 212 + 64, 234, 64, 64));
    fxBands->setBounds (at (boxA, 212, 326, 192, 26));
    bSync->setBounds (at (boxA, 212, 360, 192, 26));
    kBeat->setBounds (at (boxA, 8, 400, 124, 128));
    kTime->setBounds (at (boxA, 144, 400, 124, 128));
    kLevel->setBounds (at (boxA, 280, 400, 124, 128));

    // SOUND COLOR FX
    colorTypes->setBounds (at (boxB, 6, 30, 196, 7 * 24 + 6 * 2));
    kColor->setBounds (at (boxB, 26, 224, 156, 150));
    kParam->setBounds (at (boxB, 52, 384, 104, 112));
    bLock->setBounds (at (boxB, 6, 508, 196, 28));

    // JOG
    jog->setBounds (at (boxC, 61, 28, 220, 220));
    hBack->setBounds (at (boxC, 6, 262, 80, 34));
    hBrake->setBounds (at (boxC, 90, 262, 80, 34));
    hRev->setBounds (at (boxC, 174, 262, 80, 34));
    hLoop->setBounds (at (boxC, 258, 262, 78, 34));
    kBs->setBounds (at (boxC, 6, 308, 80, 96 - 4));
    kRate->setBounds (at (boxC, 90, 308, 80, 96 - 4));
    kBrake->setBounds (at (boxC, 174, 308, 80, 96 - 4));
    kLoop->setBounds (at (boxC, 258, 308, 78, 96 - 4));
    latency->setBounds (at (boxC, 6, 424, 330, 26));
    kBpm->setBounds (at (boxC, 50, 456, 110, 86));
    kOut->setBounds (at (boxC, 182, 456, 110, 86));
}
