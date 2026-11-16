## Known limitations (stage 1)

* **No limiting yet.** Stage 1 is the plugin shell, the engine framework (input gain, bypass, latency path, safety) and the test infrastructure. The ceiling is not enforced until stage 2.
* **Steinberg's VST3 validator was not run.** pluginval's "vst3 validator" sub-test was skipped because the validator binary is not installed. It is planned for stage 10.
* **Windows results come from CI.** This report was produced on Linux. Windows build, tests and pluginval run in GitHub Actions (`.github/workflows/build.yml`); see that workflow's QA artifact for the Windows results.
* **The editor is a temporary generic one.** The real interface is stage 8.
* **Bypass crossfade is linear.** That is click-free for the time-aligned dry/wet signals measured here. Once limiting is added, it will be re-checked on material with heavy gain reduction.
