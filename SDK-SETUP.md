# Bring your own SDKs

This guide is for developers. Editors using the official [4x4Tools release](https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/releases/tag/v0.2.0-preview.1) do not need developer accounts or SDK downloads.

No Adobe SDK, NVIDIA SDK, models, account downloads or credentials are committed here. Obtain them from the vendors under your own accounts and terms. Do not submit SDKs or unpacked release payloads with a contribution.

## 1. Adobe effect SDK

Request the [After Effects SDK through Adobe's developer portal](https://developer.adobe.com/after-effects/). This project uses the native C++ effect API and Premiere compatibility headers, not a UXP panel.

Select a directory containing `Headers/AE_Effect.h`, `Headers/PrSDKPixelFormat.h`, `Util/entry.h` and `Resources/PiPLtool.exe`. An SDK root with `Examples/Headers` also works. The current build used SDK 25.6; other releases need validation. Adobe application licenses are separate.

## 2. NVIDIA core packages — Windows x64

Sign in to [NGC](https://catalog.ngc.nvidia.com/) with your own account and accept the required terms. Download and extract these compatible core releases:

| Package | Version | Official source |
| --- | --- | --- |
| AR SDK Core | `1.2.0.0_windows` | [AR core](https://catalog.ngc.nvidia.com/orgs/nvidia/maxine/resources/ar_sdk_core) |
| VFX SDK Core | `1.3.0.0_windows` | [VFX core](https://catalog.ngc.nvidia.com/orgs/nvidia/maxine/resources/vfx_sdk_core) |
| Windows Audio Effects | `3.0.0` (x64 core build 3.0.0.51) | [AFX collection](https://catalog.ngc.nvidia.com/orgs/nvidia/maxine/collections/maxine_windows_audio_effects_sdk_collection) |

With NVIDIA's NGC CLI configured for your own account:

```powershell
ngc registry resource download-version nvidia/maxine/ar_sdk_core:1.2.0.0_windows
ngc registry resource download-version nvidia/maxine/vfx_sdk_core:1.3.0.0_windows
ngc registry resource download-version nvidia/maxine/maxine_windows_audio_effects_sdk:3.0.0
```

**`_woa` means Windows on Arm and cannot run this x64 plugin.** Use Version History and the platform selector for Windows/x86-64. If only Arm packages appear or access is denied, resolve access with NVIDIA. Do not substitute mismatched core/feature versions.

Default ignored directories:

```text
ARSDK_windows_1.2.0.0/ARSDK/
VFXSDK_windows_1.3.0.0/VideoFX/
2026-08-24_NVIDIA_AFX_SDK_Win_Core_v3.0.0.51_x64/
third_party/adobe-ae-sdk/
```

Configure different locations with:

```powershell
.\scripts\configure-sdks.ps1 `
  -AdobeSdk 'C:\Developer\Adobe\AfterEffectsSDK\Examples' `
  -ArSdk 'C:\Developer\NVIDIA\ARSDK' `
  -VfxSdk 'C:\Developer\NVIDIA\VideoFX' `
  -AfxSdk 'C:\Developer\NVIDIA\AudioFX'
```

Explicit parameters override environment variables (`AE_SDK_ROOT`, `AR_SDK_ROOT`, `VFX_SDK_ROOT`, `AFX_SDK_ROOT`), then ignored `sdk-paths.local.json`, then defaults.

## 3. Feature libraries and models

The core packages alone cannot run inference. Copy `.env.example` to `.env` and set **your own** `NGC_CLI_API_KEY` locally. Never paste tokens into issues, screenshots or commits. Helpers load the key without printing it and restore the prior process environment afterward.

| Helper `-Gpu` | AFX family | AR/VFX model suffix |
| --- | --- | --- |
| `75` | Turing | `75` |
| `86` | Ampere | `86` |
| `89` | Ada | `89` |
| `120` | Blackwell | `100` (NVIDIA mapping for these SDKs) |

For Blackwell:

```powershell
.\scripts\install-eye-contact-models.ps1 -Gpu 120
.\scripts\install-suite-models.ps1 -Kind Video -Gpu 120
.\scripts\install-suite-models.ps1 -Kind Audio -Gpu 120
```

For a multi-family release, repeat Eye Contact for all four GPU values and use `-AllArchitectures` on the video/audio helper. Downloads are large. Preserve the vendor layout, feature headers and licenses.

| Area | Features used |
| --- | --- |
| Eye Contact | `nvargazeredirection`, `nvarlandmarkdetection`, `nvarfaceboxdetection` (gaze installer resolves dependencies) |
| Video | `nvvfxgreenscreen`, `nvvfxbackgroundblur`, `nvvfxdenoising`, `nvvfxrelighting`, `nvvfxupscale`, `nvvfxvideosuperres` |
| Audio | `denoiser-48k`, `dereverb-48k`, `dereverb_denoiser-48k`, `studio_voice_high_quality-48k`, `studio_voice_low_latency-48k`, `superres-16k_to_48k`, plus low-latency microphone profiles |

Speaker Focus is excluded: it needs a separate NVIDIA entitlement and is not a registered effect here. `-IncludeSpeakerFocus` only requests an optional download; it does not implement a plugin or bypass access restrictions. Do not work around entitlement failures.

Official guides: [AR](https://docs.nvidia.com/maxine/ar/latest/WindowsARSDK/InstalltheARSDK.html), [VFX](https://docs.nvidia.com/maxine/vfx/latest/WindowsVFXSDK/InstalltheVFXSDK.html), [AFX](https://docs.nvidia.com/maxine/afx/latest/WindowsAFXSDK/InstallTheAFXSDK.html).

## 4. Open development tools

Install MSVC x64 C++ Build Tools, a Windows SDK, CMake 3.25+, Git and Python 3.11+. Use the [official NSIS compiler](https://nsis.sourceforge.io/Download) to build installers; tools are not vendored here.

Fetch the public MIT-licensed [Steinberg VST3 SDK](https://github.com/steinbergmedia/vst3sdk) with `.\scripts\bootstrap-vst3.ps1`. Revisions and vendor versions are pinned in [dependencies.lock.json](dependencies.lock.json). Continue with [building and testing](docs/BUILD.md).
