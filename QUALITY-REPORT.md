# Release status and validation scope

**0.2.1 · 6 November 2026.** This stability build adds GPU-free audio discovery, background voice-model preparation, context preservation for all picture effects, instance-aware resource teardown, processing controls and source-texture relighting. See the [patch notes and validation limits](docs/LOCAL-PATCH-0.2.1.md). Broader acceptance in real Adobe timelines, exports and additional GPU families remains open.

The development machine is Windows x64 with an RTX 5070 (12 GB). The suite was compiled against Adobe SDK 25.6 using MSVC. Other model families have not been physically tested; including their vendor models does not establish compatibility on every card.

## Checks and their scope

- SDK-free checks cover this project's frame conversion, alpha/padding, image/audio resampling and GPU eligibility policy.
- Adobe-style callback harnesses exercise metadata, entry points, parameter setup, bypass, SmartFX context and supported frame formats.
- Local GPU integration exercises cover independent render windows, module coexistence, denoise tiles, mono/stereo audio and buffer boundaries.
- Native AE audio callback/windowing exercises and Steinberg's host-interface validator have run; the VST3 development build passed 282 interface checks.
- A synthetic installer fixture covers deployment, upgrade, rollback, uninstall, integrity/path rejection and preservation of unrelated or modified files.
- Release verification covers payload hashes, encrypted-envelope interoperability and selected-architecture staging; the release notes record completed package checks.

Private model outputs, licensed media and NVIDIA performance/regression data are not published. Integration checks are not model accuracy scores or NVIDIA certification.

## Quality choices

Highest Quality requests neighboring frames explicitly and resets history between independent render windows. Matte refinement works on uncertain boundaries and combines source alpha correctly. Denoising uses overlapping tiles for larger frames. Exact proxy comparisons preserve untouched high-precision source values.

Voice processors use independent channel state, band-limited resampling, reported latency and a padded final block. Native AE audio uses fixed windows with pre-roll. Studio Voice HQ is an offline path with six-second model blocks. NVIDIA supplies the learned models; these choices concern host integration.

## Remaining acceptance work

| Area | Still needed |
| --- | --- |
| Adobe UI/export | Expanded-suite playback, seeking, caching, project reopen, exports and Media Encoder handoff in real projects |
| Host versions | Premiere/AE 2026 are current targets; broader version acceptance remains open |
| Hardware | Turing, Ampere, Ada, mobile/lower-VRAM cards and mixed/multiple-GPU systems |
| Perceptual quality | Matte hair/occlusion/motion cases, gaze naturalness, listening tests and controlled reference comparisons |
| Installation | Clean-machine end-to-end setup/uninstall with ordinary Adobe installations |

Harnesses and filesystem fixtures do not establish completion of these items. Automated inspection of the new installer UI has not been completed.

## Known limitations

Video models use SDR proxies. The new adapters accept AE 8/16/32 bpc and Premiere 8/32-bit BGRA, but they are not HDR/log-aware. Eye Contact supports AE 8/16 bpc; its AE 32-bpc adapter is not implemented. Use SDR for this release.

Eye Contact/Auto Frame target a single person. Detail Restore retains Adobe frame dimensions. Studio Voice can change timing and waveform phase; nominal latency compensation cannot make parallel dry/wet mixing phase-exact. Begin with 100% wet.

Highest Quality can be slow, especially Studio Light, which rebuilds its internally stateful handle for independent requests. No real-time guarantee or superiority over NVIDIA Broadcast is established.

Speaker Focus, acoustic echo cancellation, frame generation and TrueHDR are not implemented. Wider host acceptance is needed before claiming broad production validation.
