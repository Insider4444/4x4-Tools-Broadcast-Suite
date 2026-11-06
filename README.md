<p align="center"><img src="docs/assets/hero.svg?v=2" alt="4x4-Tools Broadcast Suite — Your edit. Better presence. NVIDIA-accelerated picture and voice for Premiere Pro and After Effects." width="100%"></p>

<p align="center">
  <a href="https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/releases/tag/v0.2.1"><img alt="Download for Windows" src="https://img.shields.io/badge/Download-0.2.1-b8f558?style=for-the-badge&labelColor=151e29"></a>
  <a href="LICENSE"><img alt="Source license MIT" src="https://img.shields.io/badge/Source-MIT-e5eaf2?style=for-the-badge&labelColor=151e29"></a>
  <img alt="Windows x64 and NVIDIA RTX" src="https://img.shields.io/badge/Windows_x64-NVIDIA_RTX-76b900?style=for-the-badge&labelColor=151e29">
  <a href="https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/actions/workflows/checks.yml"><img alt="Source and core checks" src="https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/actions/workflows/checks.yml/badge.svg"></a>
</p>

<p align="center"><a href="#start-here">Install</a> · <a href="#the-toolkit">Explore the effects</a> · <a href="docs/USER-GUIDE.md">User guide</a> · <a href="SDK-SETUP.md">Build it yourself</a> · <a href="QUALITY-REPORT.md">Validation status</a></p>

# Your edit. Better presence.

**Eye contact, cleaner dialogue, background control and studio lighting — inside your Adobe timeline.** 4x4Tools Broadcast Suite adds seven video effects and six voice processors to Premiere Pro and After Effects, using NVIDIA Maxine on supported NVIDIA GPUs.

If you have seen NVIDIA Broadcast clean up a webcam and microphone, this project brings related SDK capabilities to **recorded clips, layers and audio inside your editor**. Apply an effect, adjust its controls and render with your project. The NVIDIA Broadcast app does not need to be installed.

> **0.2.1 stability update.** Lightweight audio discovery, background model preparation, safer GPU contexts, instance-aware teardown and processing controls. Wider Adobe export and GPU-family testing remains open; see the [validation scope](QUALITY-REPORT.md).

## Start here

**[Visit the download website](https://insider4444.github.io/4x4-Tools-Broadcast-Suite/)** · **[Download for Windows](https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/releases/latest/download/4x4Tools-Setup.exe)**

Download **one file: `4x4Tools-Setup.exe`**. Setup downloads and installs the matching NVIDIA libraries and models automatically. No other files to choose or unpack.

1. Save projects and close Premiere Pro, After Effects and Media Encoder.
2. Run Setup, review the terms and choose video tools, voice tools, or both. Setup checks GPU support first and downloads the matching runtime/models. **No SDK, NVIDIA developer account or API key is needed.**
3. Open Adobe. Find video effects under **4x4Tools**. In Premiere's **Audio Plug-In Manager**, scan VST3 effects to enable the voice tools. After Effects receives native audio effects.

Initial setup needs internet access and downloads several gigabytes, depending on the GPU and components you choose. **You do not download model files yourself.** After setup, processing runs locally. The main release contains the installer and an optional checksum; separate Setup-managed support storage are managed by Setup. See the [user guide](docs/USER-GUIDE.md#installation).

**Developing the plugin?** Clone the source, then follow [SDK setup](SDK-SETUP.md) and the [build guide](docs/BUILD.md). Bring your own Adobe/NVIDIA SDKs, models and credentials. **None of those are in this repository.**

## The toolkit

### Picture

| Effect | What you can do |
| :--- | :--- |
| **4x4 Eye Contact** | Redirect a talking subject's gaze toward the camera. |
| **4x4 Background Remove** | Create transparency without a physical green screen; composite your own background below. |
| **4x4 Background Blur** | Soften the background while keeping the subject visible. |
| **4x4 Studio Light** | Relight a person with adjustable direction, exposure and warmth. |
| **4x4 Video Denoise** | Clean video noise, with fine-detail and stronger-cleanup choices. |
| **4x4 Auto Frame** | Reframe a single subject using foreground tracking and neighboring frames. |
| **4x4 Detail Restore** | Apply NVIDIA video super resolution at the current Adobe frame dimensions. |

### Voice

| Effect | Start with it when… |
| :--- | :--- |
| **4x4 Noise Clean** | Background noise distracts from speech. |
| **4x4 Room Echo Clean** | Dialogue sounds roomy or reverberant. |
| **4x4 Voice Clean** | Speech needs both noise and room-echo cleanup. |
| **4x4 Studio Voice Preview** | You want voice enhancement while editing, with natural and six microphone characters. |
| **4x4 Studio Voice HQ** | You want offline enhancement for rendered work; this uses six-second model blocks. |
| **4x4 Voice Restore** | Narrowband speech needs estimated high-frequency detail. |

All six voice processors are native AE effects and are also in the **4x4Tools Audio VST3** bundle for Premiere. They target speech; audition music and other material carefully.

## Made for the timeline

- **Editing and rendering controls.** Video defaults to Interactive Preview. Choose Highest Quality for final output. Studio Voice uses its low-latency model during live editing and HQ when the host requests offline rendering.
- **Respect the source.** Soft masks combine with source alpha. Unchanged pixels retain their precision; denoising uses overlapping tiles for larger frames.
- **Follow the audio host.** Independent channel processing, band-limited resampling, reported latency and padded final blocks support different buffer sizes.
- **One 4x4Tools home.** Shared runtime storage, component selection, GPU checks, verified payloads and a branded installer/uninstaller.
- **Local inference.** Effects process media on the installed machine. Setup contacts GitHub for version-pinned data; it never requests an NGC key.

These are integration choices around NVIDIA's models. They do not establish better accuracy or speed than NVIDIA Broadcast. No universal real-time guarantee is made.

## Compatibility at a glance

| Area | This release |
| :--- | :--- |
| Platform | Windows x64; current NVIDIA driver exposing CUDA driver API 12.8 or newer |
| GPU model families | Tensor Core SM75, SM86, SM89 and SM120; RTX 20/30/40/50 are the consumer targets |
| Physical validation GPU | RTX 5070, 12 GB. Other supported families still need community testing. |
| Integration | Native Adobe video, native AE audio, VST3 audio for Premiere |
| Six new video effects | AE 8/16/32 bpc SmartFX; Premiere BGRA 8-bit and float32 |
| Eye Contact | AE 8/16 bpc; Premiere 8-bit/float32. **AE 32-bpc Eye Contact is not implemented.** |
| Color | SDR models; Studio Light also has an explicit Apple Log 1 / Rec.2020 input path; other Log/HDR spaces are not covered |
| Voice | Mono/stereo; Studio Voice HQ is intended for offline rendering |

GTX, AMD/Intel GPUs, Windows on Arm and macOS are unsupported. Compute capability alone does not prove every GPU configuration works. Technical detection is separate from NVIDIA licensing; see [requirements](docs/USER-GUIDE.md#requirements).

Eye Contact and Auto Frame target a single visible person. Hair, occlusions, fast motion and difficult lighting need review. Detail Restore keeps Adobe frame dimensions; CLI tools provide actual enlargement. Studio Voice may change timing/phase: begin at 100% wet. Highest Quality can be substantially slower than preview.

Speaker Focus, NVIDIA Broadcast's application UI, virtual camera routing, acoustic echo cancellation, frame generation and TrueHDR are not included. See [FAQ](docs/FAQ.md) and [validation status](QUALITY-REPORT.md).

## Open source, clear dependency boundaries

| Public source | End-user release | Private developer workspace |
| :--- | :--- | :--- |
| 4x4Tools source, adapters, tests, packaging tools, artwork and docs | Compiled effects, permitted NVIDIA runtime/model subset and required notices | Adobe/NVIDIA SDKs, feature downloads, models, `.env` and account credentials |
| MIT for 4x4Tools code | Separate NVIDIA/third-party terms apply | Obtain directly from the vendors using your own accounts |

Release payloads use authenticated encryption and pinned SHA-256 checks. **Encryption is packaging, not DRM:** Setup carries the release key and runtime files must be readable after installation. This does not promise to prevent extraction or remove rights granted by third-party licenses.

Source and release manifests are audited separately. SDK headers/tools, private download archives, credentials and sample media are excluded. [Third-party notices](THIRD-PARTY-NOTICES.md) explain the boundary.

## For contributors

Start with [CONTRIBUTING](CONTRIBUTING.md), [SDK setup](SDK-SETUP.md), [build and checks](docs/BUILD.md) and [architecture](docs/ARCHITECTURE.md). SDK-free CPU checks run in CI; public CI never uses a maintainer's NVIDIA account.

Help with reproducible Adobe timeline/export reports, other GPUs, matte edge cases, listening tests and integration fixes. Include versions and steps; publish only media you have rights to share. Use [issues](https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/issues) for bugs and [security reporting](SECURITY.md) for sensitive problems.

---

Built by **4x4Tools** · [Other 4x4Tools work](https://github.com/Insider4444/4x4-Tools-DLSS-5) · [MIT source license](LICENSE)

[Shared 4x4-Tools logo and brand assets](docs/BRANDING.md)

Powered by NVIDIA Maxine. NVIDIA, Adobe and Steinberg trademarks belong to their owners. Independent community project; no endorsement is claimed.
