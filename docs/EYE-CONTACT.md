# 4x4 Eye Contact

Windows x64 development preview of a native Eye Contact effect for Adobe
Premiere and After Effects, using NVIDIA AR SDK 1.2.0.0.

## Current build

- Effect name: **4x4 Eye Contact**, category **4x4Tools**.
- Controls: enable, correction strength, eye-region size and split comparison.
- Local GPU inference; no video is uploaded.
- Models included for Turing SM75, Ampere SM86, Ada SM89 and Blackwell SM120
  (NVIDIA maps SM120 to its SM100 model packages).
- Installer checks NVIDIA CUDA support before installing plugin files. It
  rejects unsupported architectures, GTX 16-series cards without Tensor Cores,
  and drivers reporting a CUDA API older than 12.8.
- Adobe apps must be closed before installation or removal. Setup does not
  close applications or save projects automatically.

This is the first **Eye Contact** milestone. Voice cleanup, Studio Voice,
background effects and relighting are not implemented in this build. Their
core SDK downloads are present in the workspace for subsequent work.

## Install

1. Save your work and close Premiere, After Effects and Media Encoder.
2. Run `dist/4x4EyeContact-0.1.0-dev.1-Setup.exe`.
3. Open Adobe and search the Effects panel for **4x4 Eye Contact**.
4. Apply the effect to a single-subject talking-head clip. Begin with Strength
   100 and Eye Region Size 3. Use Compare for original left/corrected right.

The native effect is installed under:

`C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools\EyeContact`

NVIDIA runtimes, models, notices and the uninstaller are installed under:

`C:\Program Files\4x4Tools\EyeContact`

Use Windows Installed Apps or `Uninstall.exe` in that directory to remove it.
This development installer is unsigned.

## Validation and limits

The build has automated checks for Adobe entry points, PiPL resource metadata,
parameter controls, exact bypass, alpha preservation, row padding, negative
strides and unsupported GPU policies. Hardware checks additionally process a
real face through the compiled `.aex` using AE-style and Premiere-style host
callbacks. The RTX 5070 is the only GPU physically tested so far.

Those callback tests are not a substitute for acceptance tests in the actual
Premiere and After Effects UI. Host discovery, timeline playback, rendering
and uninstall/upgrade flows still require testing on installed Adobe hosts.
The user has since confirmed Eye Contact working in the actual After Effects
UI. Premiere playback/export and broader workflow acceptance remain separate.

Native formats in this preview are AE ARGB 8/16 bpc and Premiere BGRA 8/32-bit
float. AE 32-bpc SmartFX is not yet implemented. The NVIDIA model consumes an
8-bit SDR BGR proxy. Only its RGB corrections are applied back; unchanged
pixels retain their original precision and alpha. This is not a native HDR
gaze model. Start with SDR, opaque talking-head footage.

Temporal filtering and randomized look-away are disabled to make frame
requests independent of timeline order. This can trade temporal smoothness
for deterministic scrubbing. Multi-person selection is not exposed. Real-time
playback in Adobe depends on resolution, effect stacking and CPU/GPU load.

## Build

Requires MSVC x64, Windows SDK, CMake 3.24+, local Adobe effect SDK and NVIDIA
AR SDK plus Eye Contact features. See `SDK-SETUP.md` for download details.

Project-local CMake bootstrap:

```powershell
python -m pip install --target .tools/python cmake==3.31.10 ninja==1.13.0
```

Copy `.env.example` to `.env` and configure `NGC_CLI_API_KEY` there for feature
downloads. `.env` and its local variants are ignored by Git. The download
helper reads the key without printing it or including it in build outputs.

```powershell
.\scripts\install-eye-contact-models.ps1 -Gpu 120
.\scripts\build.ps1
```

The build helper reads the Adobe SDK location from your local configuration.
Pass `-AdobeSdk` and `-ArSdk` to use other local paths. See `SDK-SETUP.md`.
It creates the development runtime configuration in `build/bin/4x4EyeContact.ini`.

To include all supported model architectures, also run the feature helper for
`-Gpu 75`, `-Gpu 86` and `-Gpu 89`. Package with:

```powershell
.\scripts\package.ps1 -NsisCompiler 'C:\Program Files (x86)\NSIS\makensis.exe'
```

The packaging script uses an explicit runtime allowlist and creates a SHA-256
manifest. It excludes `.env`, SDK headers, source archives and private
development paths from the installer.
