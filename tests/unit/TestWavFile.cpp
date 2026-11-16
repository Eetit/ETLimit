#include <catch2/catch_test_macros.hpp>

#include "WavFile.h"

#include <cstdio>
#include <filesystem>

TEST_CASE ("WAV float32 write/read round trip is bit-exact", "[wav]")
{
    etlimit::test::WavData d;
    d.sampleRate = 96000.0;
    d.channels = { { 0.0f, 1.0f, -1.0f, 1.0e-30f, 0.123456789f }, { -0.5f, 0.25f, 3.0f, -7.5f, 0.0f } };

    const auto path = (std::filesystem::temp_directory_path() / "etlimit_wav_roundtrip.wav").string();
    std::string err;
    REQUIRE (etlimit::test::writeWavFloat32 (path, d, err));

    etlimit::test::WavData r;
    REQUIRE (etlimit::test::readWav (path, r, err));
    std::remove (path.c_str());

    REQUIRE (r.sampleRate == 96000.0);
    REQUIRE (r.channels == d.channels);
}
