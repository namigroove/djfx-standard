#include "PluginProcessor.h"
#include "PluginEditor.h"

NamiDJFXProcessor::NamiDJFXProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    // 記録の対象: 演奏中に動かす操作系(セットアップ用のBPM・出力・先読みは除く)
    for (auto* id : { &ids::fxOn, &ids::fxType, &ids::bandLow, &ids::bandMid, &ids::bandHi, &ids::fxSync, &ids::fxBeat, &ids::fxTime, &ids::fxLevel,
                      &ids::xpadTouch, &ids::xpadPos, &ids::colorType, &ids::colorKnob, &ids::colorParam, &ids::centerLock,
                      &ids::jogBackspin, &ids::jogBrake, &ids::jogReverse, &ids::jogScratch, &ids::jogLoop, &ids::jogSpeed,
                      &ids::loopLatch, &ids::jogResync })
        if (auto* prm = apvts.getParameter (id->getParamID())) { recParams.add (prm); prm->addListener (this); }
}

NamiDJFXProcessor::~NamiDJFXProcessor()
{
    cancelPendingUpdate();
    if (recording.load()) setRecording (false);
    for (auto* prm : recParams) prm->removeListener (this);
}

void NamiDJFXProcessor::setRecording (bool on)
{
    if (recording.exchange (on) == on) return;
    selfGesture = true;
    for (auto* prm : recParams)
    {
        if (on) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getValue()); }   // 今の値も1点として記録させる
        else prm->endChangeGesture();
    }
    selfGesture = false;
    endedMask = 0;
}

void NamiDJFXProcessor::parameterGestureChanged (int index, bool starting)
{
    if (starting || selfGesture || ! recording.load()) return;
    for (int k = 0; k < recParams.size(); ++k)
        if (recParams[k]->getParameterIndex() == index) { endedMask |= (1u << k); triggerAsyncUpdate(); break; }
}

bool NamiDJFXProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()) && out == l.getMainInputChannelSet();
}

void NamiDJFXProcessor::prepareToPlay (double sr, int)
{
    engine.prepare (sr);
    hop = std::max (1, (int) (sr / 100.0));
    loud.prepare (sr);
    accIn = accOut = 0.f; cntIn = cntOut = 0;
    for (auto& a : waveIn) a.store (0.f);
    for (auto& a : waveOut) a.store (0.f);
    std::fill (std::begin (midiHeld), std::end (midiHeld), 0);
    lastLatency = -1;
}

void NamiDJFXProcessor::pushPeaks (const float* L, const float* R, int n, float& acc, int& cnt, std::atomic<float>* ring, std::atomic<int>& w)
{
    for (int i = 0; i < n; ++i)
    {
        acc = std::max (acc, std::max (std::fabs (L[i]), R != nullptr ? std::fabs (R[i]) : 0.f));
        if (++cnt >= hop)
        {
            const int wi = w.load (std::memory_order_relaxed);
            ring[wi & (kWaveSize - 1)].store (acc, std::memory_order_relaxed);
            w.store (wi + 1, std::memory_order_release);
            acc = 0.f; cnt = 0;
        }
    }
}

void NamiDJFXProcessor::handleAsyncUpdate()
{
    if (pendingLatency.load() != getLatencySamples())
    {
        setLatencySamples (pendingLatency.load());
        updateHostDisplay (ChangeDetails().withLatencyChanged (true));
    }
    const int m = midiRec.exchange (-1);                       // MIDIのG1を押している間だけ記録
    if (m >= 0) setRecording (m == 1);
    // 記録中に画面のつまみを離すとジェスチャーが閉じるので、つかみ直す
    if (recording.load())
    {
        const uint32_t mask = endedMask.exchange (0);
        selfGesture = true;
        for (int k = 0; k < recParams.size(); ++k)
            if (mask & (1u << k)) { recParams[k]->beginChangeGesture(); recParams[k]->setValueNotifyingHost (recParams[k]->getValue()); }
        selfGesture = false;
    }
}

void NamiDJFXProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (nCh < 1 || n < 1) return;

    // MIDI: C1..F1 でジョグ/FXをホールド操作(36..41)
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn() && m.getNoteNumber() >= 36 && m.getNoteNumber() < 36 + 64) midiHeld[m.getNoteNumber() - 36] = 1;
        else if ((m.isNoteOff() || m.isAllNotesOff() || m.isAllSoundOff()) )
        {
            if (m.isNoteOff()) { if (m.getNoteNumber() >= 36 && m.getNoteNumber() < 100) midiHeld[m.getNoteNumber() - 36] = 0; }
            else std::fill (std::begin (midiHeld), std::end (midiHeld), 0);
        }
    }
    int mask = 0;
    if (midiHeld[0]) mask |= djfx::kMidiBackspin;
    if (midiHeld[1]) mask |= djfx::kMidiBrake;
    if (midiHeld[2]) mask |= djfx::kMidiReverse;
    if (midiHeld[3]) mask |= djfx::kMidiScratch;
    if (midiHeld[4]) mask |= djfx::kMidiLoop;
    if (midiHeld[5]) mask |= djfx::kMidiFx;
    if (midiHeld[6]) mask |= djfx::kMidiResync;       // F#1: ライブ位置へ戻す
    if ((midiHeld[7] != 0) != lastRecMidi) { lastRecMidi = midiHeld[7] != 0; midiRec = lastRecMidi ? 1 : 0; triggerAsyncUpdate(); }   // G1: 押している間、DAWへ記録

    djfx::HostInfo host;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) host.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) host.ppq = *ppq;
            host.playing = pos->getIsPlaying();
        }

    const auto params = readParams (apvts);
    lastBpm = host.bpm > 1.0 ? (float) host.bpm : params.bpmFallback;
    const int lat = djfx::Engine::latencyFor (params.jogLatencyMs, getSampleRate());
    if (lat != lastLatency) { lastLatency = lat; pendingLatency = lat; triggerAsyncUpdate(); }

    float* L = buffer.getWritePointer (0);
    pushPeaks (L, nCh >= 2 ? buffer.getReadPointer (1) : nullptr, n, accIn, cntIn, waveIn, waveWIn);
    if (nCh >= 2)
        engine.process (L, buffer.getWritePointer (1), n, params, host, mask);
    else
    {
        std::vector<float> tmp ((size_t) n);
        std::copy (L, L + n, tmp.begin());
        engine.process (L, tmp.data(), n, params, host, mask);
    }
    pushPeaks (L, nCh >= 2 ? buffer.getReadPointer (1) : nullptr, n, accOut, cntOut, waveOut, waveWOut);
    if (resetLoud.exchange (false)) loud.reset();
    loud.process (L, nCh >= 2 ? buffer.getReadPointer (1) : nullptr, n);
    lufsM = loud.momentary(); lufsS = loud.shortTerm(); lufsI = loud.integrated(); peakDb = loud.peakDb();
    jogLagMs = (float) (engine.jogLagSamples() / std::max (1.0, getSampleRate()) * 1000.0);
    double nearS = 0, farS = 0;
    if (engine.jogLoopRange (nearS, farS)) { loopNearMs = (float) (nearS * 1000.0 / getSampleRate()); loopFarMs = (float) (farS * 1000.0 / getSampleRate()); }
    else { loopNearMs = -1.f; loopFarMs = -1.f; }
    for (int c = 2; c < nCh; ++c) buffer.clear (c, 0, n);
}

juce::AudioProcessorEditor* NamiDJFXProcessor::createEditor() { return new NamiDJFXEditor (*this); }

void NamiDJFXProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}
void NamiDJFXProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NamiDJFXProcessor(); }
