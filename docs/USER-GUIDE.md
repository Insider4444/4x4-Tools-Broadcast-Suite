# Install and use the suite

You need a licensed Adobe installation and supported NVIDIA hardware. The official release does not require NVIDIA Broadcast, developer accounts, SDKs or an NGC key.

## Requirements

- Windows x64 and a current NVIDIA driver exposing CUDA driver API 12.8 or newer.
- Tensor Core SM75, SM86, SM89 or SM120 hardware. RTX 20/30/40/50 are the consumer targets. GTX 16-series lacks Tensor Cores and is rejected.
- Premiere Pro / After Effects. This preview targets 2026 installations; other versions need acceptance testing.
- Several gigabytes for downloads, temporary extraction and the installed runtime. Leave at least 25 GB free as a practical starting point; the actual need depends on components and GPU family.

RTX 5070 (12 GB) is the physical validation machine. Other supported, mobile and lower-VRAM cards still need testing. Technical detection does not expand NVIDIA's license grants. Community Model License components are intended here for local use on an RTX/GeForce RTX PC or workstation within their grants; server/kiosk/multi-user uses need separate review. A technically detected T4 is not a promise of consumer-release suitability.

## Installation

1. Download **4x4Tools-Setup.exe** from the [official community release](https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/releases/tag/v0.2.1). GitHub's Source code ZIP cannot install effects.
2. Save projects and close Premiere, After Effects and Media Encoder. Setup checks GPU support and open Adobe apps before installing.
3. Review the preview and third-party terms. Choose video, voice, or both.
4. Setup downloads the selected encrypted runtime/model data from GitHub, checks hashes/authentication and installs matching models. No NVIDIA sign-in occurs. Interrupted downloads can resume on retry.
5. Open Adobe. The first model initialization may take time.

This preview installer is not code-signed. Published SHA-256 sums help verify release files; they are not a publisher signing certificate. The visible branding and copyright footer belong to 4x4Tools.

### Do I need to download anything else?

No. Download **4x4Tools-Setup.exe**, run it and let Setup handle the required application files. The optional checksum verifies the installer; it is not another installation step. Do not download the source-code archive to install the effects.

The separate Setup-managed support storage contain files with a `.4x4data` extension. These are encrypted application download packages used internally by Setup. You do not choose, open or extract them. They are not an Adobe plugin format.

Initial installation needs internet access. Interrupted downloads resume automatically on retry. Once installed, media processing runs locally without those downloads. Online files are cached under `%ProgramData%\4x4Tools\Downloads\<version>`; the cache can be removed after successful installation to reclaim space.

## Premiere Pro

Find video effects in **Effects → 4x4Tools**, apply one to a clip and adjust Effect Controls. Start with **Interactive Preview**; choose **Highest Quality (Render)** for final output. Background Remove creates transparency: place a replacement on a lower video track. Inspect Show Mask around hair and occlusions.

For voice, open Premiere's **Audio Plug-In Manager** in audio preferences, scan VST3 plug-ins and enable the processors from vendor **4x4Tools**. Add them in supported audio effect slots. The six processors share **4x4Tools Audio.vst3** and provide a native editor with processing status, Mix, Gain and Bypass.

Start with Voice Clean for combined noise/room cleanup. Models load in the background when live audio first plays; the editor shows the current loading phase. Audio passes through with host delay compensation until the model is ready, then blends into processed audio. Replay the beginning to audition it after preparation. Studio Voice HQ uses the low-latency model during live/prefetch processing, including older saved HQ choices. Its six-second HQ model is used when the host explicitly requests offline rendering; hosts that never request offline mode use the preview model. Native AE HQ remains an explicit render choice. Premiere receives the reported processing latency. Studio Voice can alter timing/phase, so begin at Mix 100% and compare using whole-effect bypass.

New Studio Voice instances use **Voice Channels → Dialogue (centred voice)**: one model processes the combined left/right dialogue and sends the same processed voice to both outputs. This avoids two independently regenerated versions of one speaker and reduces model work. Use **Independent stereo** when the channels contain different speakers or stereo ambience that must remain separate. Dialogue combines the channels; opposite-phase stereo recordings may cancel. Bypass retains the original stereo recording.

Noise Clean, Room Echo Clean and Voice Clean have a **Suppression (0–100%)** slider controlling NVIDIA's processing strength. **Mix** blends original/processed audio; **Output Gain** adjusts volume. These controls appear in Premiere's effect editor and as native AE sliders. Studio Voice and Voice Restore retain Mix and Output Gain; unsupported model-strength controls are hidden. The status changes from preparation/pass-through to **Processed audio · NVIDIA effect applied** only after a model has processed a block. Replay the start after preparation to compare the whole clip.

**Match input level** attenuates an enhanced voice toward the original recording's level without boosting quiet model output or noise. Output Gain remains available for deliberate level changes. Older projects keep independent channels and their previous level until you enable these controls. Changing Voice Channels requests a host component reload; stop playback before changing it.

### Studio Light and Apple Log

If the effect receives **Apple Log 1 / Rec.2020** footage before its technical LUT or grade, select **Studio Light → Input Encoding → Apple Log (Rec.2020)**. Place the Log-to-display conversion after Studio Light. The model receives a smaller Rec.709 proxy; the lighting correction is applied to the original pixels in linear light, then returned in Apple Log. It does not apply your final LUT or grade.

If Premiere already converts the clip to SDR before the effect, keep **SDR (Rec.709)**. Select the encoding of the pixels reaching the effect, rather than the camera's recording setting. Apple Log 2, S-Log3, HLG/PQ and arbitrary linear/wide-gamut working spaces are not covered by this control.

Studio Light transfers a smooth neutral luminance correction onto the original subject, preserving source texture and colour. Warmth adds colour only when you adjust it; it defaults to zero. Use Show confidence matte to inspect the subject selection.

## After Effects

Search Effects & Presets for **4x4** or use the **4x4Tools** category. Apply video effects to footage layers. All six voice tools are native AE audio effects; AE does not need VST3 hosting.

Place a replacement layer below Background Remove. Use SDR, with the explicit Studio Light Apple Log exception described above. The six new video adapters support 8/16/32-bpc SmartFX; **Eye Contact supports AE 8/16 bpc only**.

Native AE audio uses fixed windows with pre-roll. Mix, gain and microphone-character settings are static in this preview. Render and audition a representative section first.

## Pick the right control

| Need | Starting point |
| --- | --- |
| More natural gaze | One visible face near the camera direction; review facial motion |
| Hair/edge artifacts | Show Mask; conservative edge/feather adjustments |
| Less aggressive video cleanup | Video Denoise → Preserve Fine Detail |
| Stronger cleanup | Video Denoise → Heavy Noise |
| Lighting direction | Studio Light → azimuth/elevation, then exposure/warmth |
| Faster editing | Interactive Preview; review Highest Quality before final render |
| Telephone-like speech | Voice Restore; predicted detail cannot recover an exact original |

Detail Restore does not resize an Adobe sequence/composition. Auto Frame targets a single person; inspect cuts, entrances and occlusions. Speech processors may damage non-speech material.

## Locations and removal

| Content | Standard location |
| --- | --- |
| Shared runtime/notices | `C:\Program Files\4x4Tools\BroadcastSuite` |
| Adobe video | `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools\BroadcastSuite` |
| AE native audio | Detected `Adobe After Effects <year>\Support Files\Plug-ins\4x4Tools\BroadcastSuiteAudio` folders |
| VST3 voice | `C:\Program Files\Common Files\VST3\4x4Tools Audio.vst3` |

Automatic AE discovery uses standard Program Files locations. Custom locations may need experienced manual deployment. Rerun Setup after adding an AE version. To change component selection, uninstall the previous suite first.

Remove **4x4Tools Broadcast Suite** through Windows Installed Apps. The uninstaller preserves unrelated/modified files. Existing standalone 4x4 Eye Contact is retained, has its own uninstaller, and is not duplicated by the suite.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Setup rejects hardware | Windows x64, supported Tensor Core architecture and current driver |
| Setup reports open Adobe apps | Save/close Premiere, AE and Media Encoder, then retry |
| Interrupted download | Rerun Setup to resume automatically |
| Integrity failure | Run Setup again to retry the required download |
| Premiere voice effects missing | Scan/enable VST3 in Audio Plug-In Manager |
| AE audio missing in a newly installed AE version | Close Adobe and rerun Setup with the same components |
| Eye Contact unavailable at AE 32 bpc | Use a supported 8/16-bpc workflow |
| Slow/out-of-memory rendering | Use Interactive Preview, lower Adobe playback resolution and fewer stacked effects; full 4K inference is not guaranteed in real time |

Report persistent problems using the [issue forms](https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/issues/new/choose). Include exact steps and Adobe/GPU/driver versions. Never attach `.env`, tokens, SDK archives or private footage.
