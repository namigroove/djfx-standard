#include "PluginProcessor.h"
#include "PluginEditor.h"

NamiDJFXProcessor::NamiDJFXProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool NamiDJFXProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()) && out == l.getMainInputChannelSet();
}

void NamiDJFXProcessor::prepareToPlay (double sr, int)
{
    engine.prepare (sr);
    std::fill (std::begin (midiHeld), std::end (midiHeld), 0);
    lastLatency = -1;
}

void NamiDJFXProcessor::handleAsyncUpdate()
{
    setLatencySamples (pendingLatency.load());
    updateHostDisplay (ChangeDetails().withLatencyChanged (true));
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
    if (nCh >= 2)
        engine.process (L, buffer.getWritePointer (1), n, params, host, mask);
    else
    {
        std::vector<float> tmp ((size_t) n);
        std::copy (L, L + n, tmp.begin());
        engine.process (L, tmp.data(), n, params, host, mask);
    }
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
