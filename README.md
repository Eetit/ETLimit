# ETLimit

A mastering limiter VST3 plugin (C++ / JUCE), built for transparency and
reliable true-peak control. Work in progress, being built in stages, each one
measured before the next starts.

| Stage | Content | Status |
|---|---|---|
| 1 | Project architecture, VST3 shell, test infrastructure, validated reference meter | **done** |
| 2 | Basic transparent limiter (guaranteed sample-peak ceiling) | next |
| 3 | Lookahead and gain-reduction envelope | |
| 4 | True-peak detection and limiting | |
| 5 | Oversampling and anti-aliasing | |
| 6 | Advanced release, stereo linking | |
| 7 | Metering and loudness (BS.1770) | |
| 8 | Professional interface | |
| 9 | Automated DSP testing and optimisation | |
| 10 | Final validation and release build | |

Latest QA results: [docs/QA_REPORT.md](docs/QA_REPORT.md).
Design: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
How measurements are made: [docs/MEASUREMENT.md](docs/MEASUREMENT.md).

## Getting the plugin without building it

Every push is built on GitHub's Windows machines. Open the repository's
**Actions** tab, click the newest green run, and download the
`ETLimit-VST3-Windows` artifact. Unzip it and copy the `ETLimit.vst3` folder to
`C:\Program Files\Common Files\VST3\`.

## Building on Windows

1. Install **Visual Studio 2022 Community** with the "Desktop development with
   C++" workload (this includes CMake).
2. Install **Git for Windows**.
3. Open the "x64 Native Tools Command Prompt for VS 2022" and run:

   ```bat
   git clone https://github.com/Eetit/ETLimit.git
   cd ETLimit
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release --parallel
   ```

   The first configure downloads JUCE and Catch2 (a few minutes).
4. The plugin is at `build\plugin\ETLimit_artefacts\Release\VST3\ETLimit.vst3`.
5. Optional, to run the tests: install Python 3, then
   `python -m pip install -r tests/reference/requirements.txt` and
   `ctest --test-dir build -C Release --output-on-failure`.

You can also open the folder directly in Visual Studio ("Open a local folder");
it understands CMake projects.

## Repository layout

```
core/      the DSP engine: plain C++, no JUCE, fully unit-testable
plugin/    JUCE wrapper: parameters, host communication, editor
tests/
  unit/        C++ unit tests (Catch2)
  render/      etlimit_render: WAV -> engine -> WAV, used by the Python tests
  reference/   independent Python measurements (true peak, loudness, ...)
docs/      architecture, measurement method, QA report
```

## Licensing note

JUCE is dual-licensed (AGPLv3 or commercial). A closed-source commercial
release requires an appropriate JUCE licence. Check JUCE's current terms
before selling the plugin.
