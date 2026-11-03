# 0.2.0 Preview 1 — 4x4Tools Broadcast Suite

Bring NVIDIA-accelerated picture and voice tools into Premiere Pro and After Effects: seven video effects and six voice processors, grouped under 4x4Tools.

## Install

Download **[4x4Tools-Setup.exe](https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/releases/download/v0.2.0-preview.1/4x4Tools-Setup.exe)**. Save/close Adobe apps, run Setup, review the terms and choose video, voice or both. Setup checks supported NVIDIA hardware first, then downloads the runtime and matching GPU models. **Normal users need no SDK, NVIDIA developer account or NGC key.**

Video effects appear under **4x4Tools**. In Premiere, scan/enable VST3 processors in Audio Plug-In Manager. AE receives native voice effects. Read the [user guide](https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/blob/main/docs/USER-GUIDE.md) for installation, offline layout and troubleshooting.

## Included

**Picture:** Eye Contact, Background Remove, Background Blur, Studio Light, Video Denoise, Auto Frame and Detail Restore.

**Voice:** Noise Clean, Room Echo Clean, Voice Clean, Studio Voice Preview, Studio Voice HQ and Voice Restore. All six are native AE effects and VST3 processors for Premiere.

Highest Quality is the default. Studio Voice HQ is intended for rendered work, using six-second model blocks. Background Remove makes transparency for your own lower-track/layer replacement.

## What the assets mean

- **4x4Tools-Setup.exe:** the 4x4Tools-branded installer; start here.
- **`.4x4data` files:** authenticated encrypted application payloads. Online Setup fetches only the required components/model family. For offline setup, place the assets in `Data` beside Setup.
- **SHA256SUMS.txt / release-manifest.json:** release-file checksums and installed-file inventory.
- **Source code ZIP/tar:** developer source only. These are not installers and contain no SDKs, models or credentials.

No Adobe SDK, NVIDIA development headers/tools, account files, NGC token or private `.env` is included. NVIDIA runtimes/models keep their licenses and notices. Encryption is packaging, not DRM or a promise that installed files cannot be extracted.

## Preview scope

Windows x64, a supported NVIDIA Tensor Core model family and a current CUDA-12.8-capable driver are required. Physical validation used an RTX 5070 (12 GB). Turing/Ampere/Ada/Blackwell models are supplied, but other cards still need community acceptance testing.

Eye Contact has been user-tested in After Effects. The expanded suite needs broader real Adobe timeline/export testing. CPU/host-interface checks, installer transaction fixtures, encrypted-envelope corruption/roundtrip checks and interrupted-download recovery have been exercised. Release payloads are hashed and staged before publication. See the [full validation scope](https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/blob/main/QUALITY-REPORT.md).

Known limits: SDR model processing; Eye Contact in AE supports 8/16 bpc, not 32 bpc; Detail Restore keeps Adobe frame dimensions; Studio Voice can change timing/phase; Highest Quality is not a real-time guarantee. Speaker Focus, acoustic echo cancellation, frame generation and TrueHDR are excluded. This preview is not code-signed and is not certified or endorsed by NVIDIA/Adobe.

## Build and contribute

Developers bring their own Adobe/NVIDIA SDKs, models and credentials. Follow [SDK setup](https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/blob/main/SDK-SETUP.md) and [build instructions](https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/blob/main/docs/BUILD.md). Original 4x4Tools source is MIT; third-party terms are separate.

Share reproducible host/GPU reports through the issue forms. Never attach tokens, SDK archives, models or media you do not have rights to publish.
