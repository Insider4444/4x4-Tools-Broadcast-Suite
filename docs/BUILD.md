# Build and check

First complete [SDK setup](../SDK-SETUP.md). Full builds need your own Adobe SDK and the compatible NVIDIA core/feature headers. Running GPU checks additionally needs models and supported hardware. All commands below run from the repository root.

## Full plugin build

Use MSVC x64 C++ Build Tools, a Windows SDK, CMake 3.25+, Git and Python 3.11+. Install CMake normally, or bootstrap a project-local copy:

```powershell
python -m pip install --target .tools/python cmake==3.31.10
.\scripts\bootstrap-vst3.ps1
.\scripts\build.ps1 -WithVstValidator
```

The build helper discovers MSVC, reads your ignored SDK-path config, configures an NMake Release build, compiles and runs the CPU/host-contract tests. Supply `-AdobeSdk`, `-ArSdk`, `-VfxSdk` and `-AfxSdk` explicitly to override saved paths. CMake cache variables with the corresponding `*_SDK_ROOT` names also work.

Outputs:

```text
build/bin/4x4*.aex                  Native Adobe effects (13 modules)
build/VST3/Release/4x4Tools Audio.vst3/
build/bin/SupportCheck.exe          GPU/open-host diagnostics
build/bin/*EffectsCli.exe           Developer media-processing tools
```

Generated development INIs point to your local NVIDIA downloads. Never distribute those INIs; the installer creates fresh paths. The suite dynamically loads NVIDIA DLLs rather than embedding SDK development libraries.

## SDK-free checks

From a Visual Studio x64 developer shell:

```powershell
cmake -S . -B build/core-only -A x64 -DFOUR_CORE_ONLY=ON
cmake --build build/core-only --config Release
ctest --test-dir build/core-only -C Release --output-on-failure
python tests/installer_fixture.py
python tests/payload_crypto_fixture.py
python scripts/audit-public.py
```

`FOUR_CORE_ONLY` builds the project's core image/audio DSP and GPU policy tests without Adobe/NVIDIA SDKs. The installer fixture uses small synthetic files in isolated test directories; it does not modify actual Adobe installations. The crypto fixture needs the release packaging dependency PyCryptodome.

## Local host and GPU integration

The normal build runs four CTest groups, including Adobe-style ABI/parameter/bypass checks. They do not replace testing inside Adobe.

GPU harnesses are opt-in developer programs. `SuiteGpuTests` expects the built module directory, an appropriately licensed P6 face PPM and an output directory; `AdobeAudioGpuTests` exercises native audio callbacks. Run a harness without arguments to inspect its usage. Use media you have rights to process. Models, source media, generated comparisons and full logs stay private/ignored. Follow NVIDIA's terms for any disclosure of vendor performance/regression data.

Run Steinberg's validator against the built VST3 bundle using the validator executable produced by `-WithVstValidator`. For release acceptance, also stage the exact packaged runtime and repeat relevant host checks against the staged modules.

Manually verify a real Adobe project: apply effects, seek out of order, save/reopen, compare SDR results, render/export, scan voice processors and listen for timing/artifacts. Expand coverage to other supported cards. Keep the [preview status](../QUALITY-REPORT.md) honest.

## Build an end-user installer

Read [release maintenance](RELEASING.md) first. Download models for every supported family using your own NVIDIA account. Install the packaging requirements:

```powershell
python -m pip install -r scripts/requirements-release.txt
.\scripts\package-release.ps1 -NsisCompiler 'C:\Program Files (x86)\NSIS\makensis.exe'
```

`dist/release` contains the branded Setup executable, encrypted data assets, a public manifest, a quick-start file and SHA-256 sums. Only the allowlisted NVIDIA runtime/model subset is included. No SDK headers, Adobe tooling, account credentials or development INIs enter that output.

`package-suite.ps1`, `package_suite.py`, `package.ps1` and `package_runtime.py` are legacy/local offline packaging tools. Their large `dist` bundles are **not** the public release upload set. Use only audited `dist/release` assets for community publication.
