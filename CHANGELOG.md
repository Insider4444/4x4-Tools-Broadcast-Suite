# Changelog

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
