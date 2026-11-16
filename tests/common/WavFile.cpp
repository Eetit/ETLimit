#include "WavFile.h"

#include <cstdint>
#include <cstring>
#include <fstream>

namespace etlimit::test
{
namespace
{
uint16_t rd16 (const unsigned char* p) { return static_cast<uint16_t> (p[0] | (p[1] << 8)); }
uint32_t rd32 (const unsigned char* p)
{
    return static_cast<uint32_t> (p[0]) | (static_cast<uint32_t> (p[1]) << 8) | (static_cast<uint32_t> (p[2]) << 16)
           | (static_cast<uint32_t> (p[3]) << 24);
}
void wr16 (std::ofstream& f, uint16_t v)
{
    const unsigned char b[2] = { static_cast<unsigned char> (v & 0xff), static_cast<unsigned char> (v >> 8) };
    f.write (reinterpret_cast<const char*> (b), 2);
}
void wr32 (std::ofstream& f, uint32_t v)
{
    const unsigned char b[4] = { static_cast<unsigned char> (v & 0xff), static_cast<unsigned char> ((v >> 8) & 0xff),
                                 static_cast<unsigned char> ((v >> 16) & 0xff), static_cast<unsigned char> (v >> 24) };
    f.write (reinterpret_cast<const char*> (b), 4);
}
} // namespace

bool readWav (const std::string& path, WavData& out, std::string& error)
{
    std::ifstream f (path, std::ios::binary);
    if (! f)
    {
        error = "cannot open " + path;
        return false;
    }
    std::vector<unsigned char> bytes ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
    if (bytes.size() < 12 || std::memcmp (bytes.data(), "RIFF", 4) != 0 || std::memcmp (bytes.data() + 8, "WAVE", 4) != 0)
    {
        error = "not a RIFF/WAVE file";
        return false;
    }

    uint16_t format = 0, numCh = 0, bits = 0;
    uint32_t rate = 0;
    const unsigned char* data = nullptr;
    size_t dataSize = 0;

    size_t pos = 12;
    while (pos + 8 <= bytes.size())
    {
        const unsigned char* ck = bytes.data() + pos;
        const uint32_t ckSize = rd32 (ck + 4);
        const size_t body = pos + 8;
        const size_t avail = bytes.size() - body;
        const size_t len = ckSize <= avail ? ckSize : avail;

        if (std::memcmp (ck, "fmt ", 4) == 0 && len >= 16)
        {
            format = rd16 (ck + 8);
            numCh = rd16 (ck + 10);
            rate = rd32 (ck + 12);
            bits = rd16 (ck + 22);
            if (format == 0xFFFE && len >= 26)
                format = rd16 (ck + 8 + 24); // first two bytes of the SubFormat GUID
        }
        else if (std::memcmp (ck, "data", 4) == 0)
        {
            data = bytes.data() + body;
            dataSize = len;
        }
        pos = body + ckSize + (ckSize & 1u);
    }

    if (data == nullptr || numCh == 0 || rate == 0)
    {
        error = "missing fmt or data chunk";
        return false;
    }

    const bool isFloat = format == 3;
    const bool isPcm = format == 1;
    if (! ((isFloat && (bits == 32 || bits == 64)) || (isPcm && (bits == 16 || bits == 24 || bits == 32))))
    {
        error = "unsupported sample format";
        return false;
    }

    const size_t bytesPerSample = bits / 8u;
    const size_t frames = dataSize / (bytesPerSample * numCh);
    out.sampleRate = rate;
    out.channels.assign (numCh, std::vector<float> (frames));

    for (size_t i = 0; i < frames; ++i)
    {
        for (size_t ch = 0; ch < numCh; ++ch)
        {
            const unsigned char* p = data + (i * numCh + ch) * bytesPerSample;
            double v = 0.0;
            if (isFloat && bits == 32)
            {
                float fv;
                std::memcpy (&fv, p, 4);
                v = fv;
            }
            else if (isFloat)
            {
                std::memcpy (&v, p, 8);
            }
            else if (bits == 16)
                v = static_cast<int16_t> (rd16 (p)) / 32768.0;
            else if (bits == 24)
            {
                int32_t s = static_cast<int32_t> (p[0] | (p[1] << 8) | (p[2] << 16));
                if (s & 0x800000)
                    s -= 0x1000000;
                v = s / 8388608.0;
            }
            else
                v = static_cast<int32_t> (rd32 (p)) / 2147483648.0;

            out.channels[ch][i] = static_cast<float> (v);
        }
    }
    return true;
}

bool writeWavFloat32 (const std::string& path, const WavData& d, std::string& error)
{
    std::ofstream f (path, std::ios::binary);
    if (! f)
    {
        error = "cannot write " + path;
        return false;
    }
    const auto nch = static_cast<uint32_t> (d.numChannels());
    const auto frames = static_cast<uint32_t> (d.numSamples());
    const uint32_t dataBytes = frames * nch * 4u;

    f.write ("RIFF", 4);
    wr32 (f, 4u + (8u + 16u) + (8u + 4u) + (8u + dataBytes));
    f.write ("WAVE", 4);
    f.write ("fmt ", 4);
    wr32 (f, 16);
    wr16 (f, 3); // IEEE float
    wr16 (f, static_cast<uint16_t> (nch));
    wr32 (f, static_cast<uint32_t> (d.sampleRate));
    wr32 (f, static_cast<uint32_t> (d.sampleRate) * nch * 4u);
    wr16 (f, static_cast<uint16_t> (nch * 4u));
    wr16 (f, 32);
    f.write ("fact", 4);
    wr32 (f, 4);
    wr32 (f, frames);
    f.write ("data", 4);
    wr32 (f, dataBytes);

    std::vector<float> interleaved (static_cast<size_t> (frames) * nch);
    for (uint32_t i = 0; i < frames; ++i)
        for (uint32_t ch = 0; ch < nch; ++ch)
            interleaved[i * nch + ch] = d.channels[ch][i];
    f.write (reinterpret_cast<const char*> (interleaved.data()), static_cast<std::streamsize> (interleaved.size() * 4));

    if (! f)
    {
        error = "write failed";
        return false;
    }
    return true;
}

} // namespace etlimit::test
