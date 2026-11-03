# 4x4Tools Broadcast Suite 0.2.0

**Download one file: [4x4Tools-Setup.exe](https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/releases/latest/download/4x4Tools-Setup.exe).**

Save/close Adobe apps, run Setup, review the terms and choose video tools, voice tools, or both. Setup checks your NVIDIA GPU and automatically downloads, verifies and installs the matching application libraries/models. **No extra files to choose, SDKs to install or developer accounts to sign in to.**

[Download website](https://tools-4x4-broadcast.triplebream4.chatgpt.site/) · [User guide](https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/blob/main/docs/USER-GUIDE.md) · [Get help](https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/issues/new/choose)

## What's included

**Picture:** Eye Contact, Background Remove, Background Blur, Studio Light, Video Denoise, Auto Frame and Detail Restore.

**Voice:** Noise Clean, Room Echo Clean, Voice Clean, Studio Voice Preview, Studio Voice HQ and Voice Restore. All six are native After Effects effects and VST3 processors for Premiere.

Find the effects under **4x4Tools**. In Premiere, scan voice processors once using Audio Plug-In Manager. In AE they are native effects.

## Installation, simplified

The assets below contain **the installer and an optional SHA-256 checksum**. GitHub also generates source-code archives for developers; ordinary users do not need them. Initial setup needs internet and several gigabytes for NVIDIA's processing libraries/models. Setup handles those downloads and can resume interrupted transfers. Media processing then runs locally.

Files in older/support-storage releases are used internally by Setup. You do not download or unpack them manually. The visible branding, Windows icon and reusable publisher mark now share the standard DLSS-5 sixteen-tile 4x4-Tools logo.

## Initial community release status

This is the main public **0.2.0 community release**, with explicit testing limits. Eye Contact has been user-tested in AE. The expanded suite has native callback/GPU integration checks and SDK-free CI, but wider real Adobe timeline/export and GPU-family acceptance is still needed. A regular GitHub release does not mean NVIDIA/Adobe certification or a real-time guarantee.

Use Windows x64, supported NVIDIA Tensor Core hardware and a current CUDA-12.8-capable driver. Physical validation used an RTX 5070 (12 GB). Models are provided for the supported Turing/Ampere/Ada/Blackwell families; other cards need community testing.

Known limits: SDR model processing; AE Eye Contact supports 8/16 bpc, not 32 bpc; Detail Restore keeps the existing Adobe frame dimensions; Studio Voice HQ is an offline path and Studio Voice can alter timing/phase. The installer is unsigned. See the [validation scope](https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/blob/main/QUALITY-REPORT.md).

## Developer boundary

Original 4x4Tools source is MIT. Developers supply their own Adobe/NVIDIA SDKs, models and credentials. None are in source control. End-user runtime/model components retain their licenses and notices; encrypted packaging does not promise confidentiality or change those rights.
