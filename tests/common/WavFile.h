#pragma once

#include <string>
#include <vector>

namespace etlimit::test
{

/** Minimal WAV reader/writer for the test tools (not used in the plugin).
    Reads PCM 16/24/32-bit and IEEE float 32/64-bit, including
    WAVE_FORMAT_EXTENSIBLE. Writes 32-bit float. */
struct WavData
{
    double sampleRate = 0.0;
    std::vector<std::vector<float>> channels; // [channel][sample]

    int numChannels() const { return static_cast<int> (channels.size()); }
    int numSamples() const { return channels.empty() ? 0 : static_cast<int> (channels[0].size()); }
};

/** Returns false and fills `error` on failure. */
bool readWav (const std::string& path, WavData& out, std::string& error);
bool writeWavFloat32 (const std::string& path, const WavData& data, std::string& error);

} // namespace etlimit::test
