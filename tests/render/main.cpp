// etlimit_render: run a WAV file through the DSP engine exactly as a host
// would (block by block), and write the result.
//
// Usage:
//   etlimit_render IN.wav OUT.wav [options]
//     --input-gain DB        input gain in dB (default 0)
//     --bypass 0|1           engine bypass (default 0)
//     --block N              fixed host block size (default 512)
//     --random-blocks SEED   random block sizes in [1, max-block] instead
//     --max-block N          block size announced in prepare (default 2048)
//     --change-at N KEY VAL  change a setting when sample N is reached
//                            (applied at the first block starting at or after N;
//                            blocks are split so a change lands exactly on N)
//
// Prints a one-line JSON summary (latency, non-finite count) to stdout.

#include "WavFile.h"

#include <etlimit/Engine.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace
{
struct Change
{
    long long at;
    std::string key;
    double value;
};

bool applySetting (etlimit::EngineSettings& s, const std::string& key, double v)
{
    if (key == "input-gain")
        s.inputGainDb = static_cast<float> (v);
    else if (key == "bypass")
        s.bypass = v >= 0.5;
    else
        return false;
    return true;
}

int fail (const std::string& msg)
{
    std::fprintf (stderr, "etlimit_render: %s\n", msg.c_str());
    return 1;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 3)
        return fail ("usage: etlimit_render IN.wav OUT.wav [options]");

    const std::string inPath = argv[1], outPath = argv[2];
    etlimit::EngineSettings settings;
    int block = 512, maxBlock = 2048;
    bool randomBlocks = false;
    unsigned seed = 0;
    std::vector<Change> changes;

    for (int i = 3; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto need = [&] (int n) { return i + n < argc; };
        if (a == "--block" && need (1))
            block = std::atoi (argv[++i]);
        else if (a == "--max-block" && need (1))
            maxBlock = std::atoi (argv[++i]);
        else if (a == "--random-blocks" && need (1))
        {
            randomBlocks = true;
            seed = static_cast<unsigned> (std::strtoul (argv[++i], nullptr, 10));
        }
        else if (a == "--change-at" && need (3))
        {
            Change c { std::atoll (argv[i + 1]), argv[i + 2], std::atof (argv[i + 3]) };
            i += 3;
            changes.push_back (c);
        }
        else if (a.rfind ("--", 0) == 0 && need (1) && applySetting (settings, a.substr (2), std::atof (argv[i + 1])))
            ++i;
        else
            return fail ("unknown or incomplete option " + a);
    }
    if (block < 1 || maxBlock < 1)
        return fail ("block sizes must be >= 1");

    etlimit::test::WavData wav;
    std::string err;
    if (! etlimit::test::readWav (inPath, wav, err))
        return fail (err);
    if (wav.numChannels() < 1 || wav.numChannels() > etlimit::Engine::maxChannels)
        return fail ("only mono and stereo files are supported");

    std::sort (changes.begin(), changes.end(), [] (const Change& x, const Change& y) { return x.at < y.at; });

    etlimit::Engine engine;
    engine.prepare (wav.sampleRate, maxBlock, wav.numChannels());

    std::mt19937 rng (seed);
    std::uniform_int_distribution<int> dist (1, maxBlock);

    const long long total = wav.numSamples();
    size_t nextChange = 0;
    long long pos = 0;
    std::vector<float*> ptrs (static_cast<size_t> (wav.numChannels()));

    while (pos < total)
    {
        while (nextChange < changes.size() && changes[nextChange].at <= pos)
        {
            if (! applySetting (settings, changes[nextChange].key, changes[nextChange].value))
                return fail ("unknown setting " + changes[nextChange].key);
            ++nextChange;
        }
        engine.setSettings (settings);

        long long n = randomBlocks ? dist (rng) : block;
        n = std::min (n, total - pos);
        if (nextChange < changes.size())
            n = std::min (n, changes[nextChange].at - pos);

        for (size_t ch = 0; ch < ptrs.size(); ++ch)
            ptrs[ch] = wav.channels[ch].data() + pos;
        engine.process (ptrs.data(), wav.numChannels(), static_cast<int> (n));
        pos += n;
    }

    if (! etlimit::test::writeWavFloat32 (outPath, wav, err))
        return fail (err);

    std::printf ("{\"latency_samples\": %d, \"non_finite_inputs\": %llu, \"sample_rate\": %.1f}\n",
                 engine.getLatencySamples(),
                 static_cast<unsigned long long> (engine.getNonFiniteInputCount()),
                 wav.sampleRate);
    return 0;
}
