// pulse_render: runs the Pulse engine offline on a WAV file (same argument style as reference/render.js).
//
//   pulse_render <in.wav> <out.wav> [Name=value ...] [--block N] [--trim] [--info]
//     Name=value  parameter by automation ID in plain units, e.g. TapeDrive=12 RetroBypass=1
//     --block N   processing block size (default 512)
//     --trim      remove the plugin latency from the output so it lines up with the input
//     --info      print latency and render speed
//   pulse_render --params   lists all parameters (id min max default)
#include <juce_audio_formats/juce_audio_formats.h>
#include "Params.h"
#include "engine/PulseEngine.h"

int main(int argc, char** argv)
{
    juce::ScopedNoDenormals noDenormals;
    juce::StringArray pos;
    P::Values v = P::defaultValues();
    int block = 512;
    bool trim = false, info = false;

    for (int i = 1; i < argc; ++i)
    {
        juce::String a(argv[i]);
        if (a == "--block" && i + 1 < argc) block = juce::String(argv[++i]).getIntValue();
        else if (a == "--trim") trim = true;
        else if (a == "--info") info = true;
        else if (a == "--params") // list every parameter: id min max default
        {
            for (int id = 0; id < P::Count; ++id)
                std::printf("%s %g %g %g\n", P::def(id).id, P::def(id).min, P::def(id).max, P::def(id).def);
            return 0;
        }
        else if (a.contains("="))
        {
            const int id = P::findByIdString(a.upToFirstOccurrenceOf("=", false, false));
            if (id < 0) { std::fprintf(stderr, "unknown param %s\n", a.toRawUTF8()); return 1; }
            v[(size_t) id] = a.fromFirstOccurrenceOf("=", false, false).getFloatValue();
        }
        else pos.add(a);
    }
    if (pos.size() < 2) { std::fprintf(stderr, "usage: pulse_render in.wav out.wav [Name=value ...] [--block N] [--trim] [--info]\n"); return 1; }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(juce::File::getCurrentWorkingDirectory().getChildFile(pos[0])));
    if (! reader) { std::fprintf(stderr, "cannot read %s\n", pos[0].toRawUTF8()); return 1; }
    const int len = (int) reader->lengthInSamples, nch = (int) juce::jmin(2u, reader->numChannels);
    const double sr = reader->sampleRate;

    PulseEngine engine;
    engine.prepare(sr, block);
    const int lat = engine.getLatencySamples();
    const int outLen = len + (trim ? lat : 0);

    juce::AudioBuffer<float> in(nch, outLen);
    in.clear();
    reader->read(&in, 0, len, 0, true, nch > 1);

    juce::AudioBuffer<float> out(nch, outLen);
    const auto t0 = juce::Time::getHighResolutionTicks();
    for (int s = 0; s < outLen; s += block)
    {
        const int n = std::min(block, outLen - s);
        juce::AudioBuffer<float> chunk(nch, n);
        for (int c = 0; c < nch; ++c) chunk.copyFrom(c, 0, in, c, s, n);
        engine.process(chunk, v);
        for (int c = 0; c < nch; ++c) out.copyFrom(c, s, chunk, c, 0, n);
    }
    const double secs = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - t0);

    juce::AudioBuffer<float> result(nch, len);
    for (int c = 0; c < nch; ++c) result.copyFrom(c, 0, out, c, trim ? lat : 0, len);

    for (int c = 0; c < nch; ++c)
        for (int i = 0; i < len; ++i)
            if (! std::isfinite(result.getSample(c, i))) { std::fprintf(stderr, "NaN/Inf in output at %d\n", i); return 2; }

    auto outFile = juce::File::getCurrentWorkingDirectory().getChildFile(pos[1]);
    outFile.getParentDirectory().createDirectory();
    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os(outFile.createOutputStream().release());
    const auto opts = juce::AudioFormatWriterOptions {}.withSampleRate(sr).withNumChannels(nch).withBitsPerSample(32)
                          .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    auto writer = wav.createWriterFor(os, opts);
    if (! writer) { std::fprintf(stderr, "cannot write %s\n", pos[1].toRawUTF8()); return 1; }
    writer->writeFromAudioSampleBuffer(result, 0, len);

    if (info)
        std::printf("latency %d samples (%.2f ms), render %.0fx realtime\n", lat, lat * 1000.0 / sr, (len / sr) / std::max(1e-9, secs));
    return 0;
}
