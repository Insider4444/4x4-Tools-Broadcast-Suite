# Changelog

## 0.2.1 — 6 November 2026

- Audio discovery, format setup and activation no longer load NVIDIA models or create CUDA contexts. Silent and bypassed live processing stays lightweight.
- Live voice processing prepares models on a worker, reports preparation status and keeps latency-aligned audio passing through until processed audio is ready. Prefetch and offline rendering wait for preparation instead of exporting temporary pass-through audio.
- Studio Voice uses its low-latency model during live/prefetch processing, including saved explicit HQ settings. The six-second HQ model is reserved for hosts requesting offline rendering.
- Eye Contact, the six picture effects and voice processing preserve Adobe's CUDA context. Cleanup errors are contained during teardown.
- Closing one picture-effect instance no longer clears another instance's cached output or releases its active model. Native AE audio also tracks the instance that owns its model.
- Video defaults to Interactive Preview with bounded model images. Source-resolution corrections preserve original detail; Studio Light transfers lighting onto source texture.
- Studio Light estimates matte-weighted, linear luminance gains instead of transferring the model's reconstructed colour or undefined background pixels. Neutral lighting preserves source colour and texture. An explicit Apple Log 1 / Rec.2020 input option converts only bounded inference images and returns the correction in the source encoding.
- New Studio Voice instances offer a single-model, centred Dialogue mode and attenuation-only input level matching. Independent stereo remains available; existing projects retain their previous channel and volume settings. Bypass preserves the original stereo recording.
- Noise/echo/combined voice cleanup exposes a real NVIDIA suppression-strength slider alongside Mix and Output Gain. Audio status distinguishes processed model output from preparation/pass-through and silent input. Unrelated Adobe sliders stay hidden.
- Native processing controls expose quality, elapsed time, GPU memory, status and cancellation. Premiere voice effects include a native editor.
- Setup downloads independent packages concurrently and shows byte-based download, verification and extraction progress, transfer speed and estimated time remaining. Interrupted downloads resume and completed packages are verified.

Validation includes 108 model-free VST setup/activation cycles, concurrent video renders with alternating host CUDA contexts, all six native AE audio adapters, exact offline/prefetch audio comparison, silent-channel isolation, installed-copy checks and installer transaction/download/crypto fixtures. These checks do not establish universal Adobe/GPU compatibility or perceptual superiority over NVIDIA's models. See [validation scope](QUALITY-REPORT.md).

Full-resolution 4K inference remains below real-time speed in local testing. Use Interactive Preview and lower Adobe playback resolution while editing. Live audio briefly passes through during model preparation; replay the start after preparation to audition it. No SDKs, downloaded models or credentials are included in the source repository.

## 0.2.0 — 2026-11-03

- Main community release: a single Setup executable and optional checksum in the public download assets.
- Public download website with one Windows download button, three-step installation guide and clear requirements.
- Setup automatically selects/downloads required components with readable progress and resume support; users do not choose or unpack model files.
- The standard DLSS-5 sixteen-tile logo is shared across the website, repository, installer and plugin bundle icon.
- Versioned new effect payloads reuse immutable runtime/model storage; existing downloads remain compatible.

This changes the release presentation and installation experience. Wider real-host/export and GPU acceptance remains open as documented in QUALITY-REPORT.md.

## 0.2.0 Preview 1 — 2026-11-03

- Seven Adobe video effects: Eye Contact, Background Remove/Blur, Studio Light, Video Denoise, Auto Frame and Detail Restore.
- Six native AE voice effects and six VST3 processors for Premiere: Noise Clean, Room Echo Clean, Voice Clean, Studio Voice Preview/HQ and Voice Restore.
- Quality-first temporal windows, matte refinement, precision-preserving frame conversion, larger-frame denoise tiles and audio host adaptation.
- 4x4Tools installer branding, early GPU/open-Adobe checks, component selection, shared runtimes and receipt-based removal.
- Public source without SDKs/models/credentials; own-account developer setup, SDK-free CI, source audit and preview documentation.
- Authenticated encrypted end-user data assets, selected-GPU downloads, resume support and offline layout.

This is a preview. Eye Contact is user-tested in AE; the expanded suite needs broader real-host/export and hardware acceptance. See [validation scope](QUALITY-REPORT.md).
