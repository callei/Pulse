#include "PluginProcessor.h"

PulseProcessor::PulseProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PulseState", P::createLayout()),
      seed(juce::Random::getSystemRandom().nextInt() | 1u)
{
    for (int i = 0; i < P::Count; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue(P::def(i).id);
    engine.setSeed(seed);
}

void PulseProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.setSeed(seed);
    engine.prepare(sampleRate, samplesPerBlock);
    setLatencySamples(engine.getLatencySamples());
}

void PulseProcessor::reset() { engine.reset(); }

bool PulseProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void PulseProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear(c, 0, buffer.getNumSamples());

    P::Values v;
    for (size_t i = 0; i < v.size(); ++i) v[i] = raw[i]->load(std::memory_order_relaxed);
    engine.process(buffer, v);
}

juce::AudioProcessorEditor* PulseProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void PulseProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("seed", (juce::int64) seed, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void PulseProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml(*xml);
            if (state.hasProperty("seed")) seed = (uint32_t) (juce::int64) state["seed"];
            apvts.replaceState(state);
            engine.setSeed(seed);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PulseProcessor(); }
