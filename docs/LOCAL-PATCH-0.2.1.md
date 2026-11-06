# Local native patch 0.2.1

This document describes the 0.2.1 stability build and its validation scope.

## Editing controls

- The six picture effects added in 0.2.0 now default to **Interactive Preview**. Existing projects keep their saved quality choice. Choose **Highest Quality (Render)** before final output when appropriate.
- **Processing → Status and Quality…** opens a native panel with the current processing phase, elapsed time, GPU memory usage and cancellation. NVIDIA calls that expose no internal percentage show an indeterminate state. Cancellation takes effect after the current model call returns.
- Premiere's six VST3 voice effects have a native editor with Mix, Output Gain, Bypass and processing status. Studio Voice Preview also exposes Microphone Character.
- Studio Voice HQ uses the low-latency model during live/prefetch processing, including saved instances with explicit HQ selected. The six-second HQ model is reserved for explicit offline processing. Hosts that never request offline mode use the preview model.
- Native After Effects Studio Voice HQ provides an explicit Preview/HQ choice.

## Changes

Audio bypass and zero wet mix skip inference while maintaining host latency compensation. Deactivated VST processors release their model allocations; video and native AE audio sequence teardown also release shared model state. Bypass alone does not unload a model.

The VST3 binary revision **0.2.1.4** keeps plugin discovery, format setup and activation independent of NVIDIA runtimes, models and CUDA. Latency is computed from the shipped model contract and checked against the loaded model. `setProcessing` acknowledges the host without loading, destroying or allocating a model.

During live playback, the first nonzero input to an enabled voice effect starts model preparation on an owned worker thread. Audio passes through with the advertised delay while preparation runs, then blends into processed audio after the model's delay line fills. The editor shows preparation status. Replay the beginning after preparation if you need to preview its processed sound. Silence, bypass and zero Mix do not trigger loading. Offline rendering and host prefetch wait for preparation and report loading errors; they never save the temporary live pass-through because a model is still loading. Auto HQ uses its preview model for realtime/prefetch and its HQ model for explicit offline processing.

Model preparation honors cancellation between channels and after the current NVIDIA load call returns. Deactivation joins the worker before allowing plugin unload, including when a vendor call is still finishing. Threads are never detached from the plugin DLL.

Video and voice processing use an explicitly retained CUDA primary context and restore Adobe's previous context. Vendor cleanup exceptions are contained. These changes address unsafe lifetime/context handling; they do not establish that every reported Premiere crash has been reproduced or eliminated.

Video output caching reuses identical requests and checks all input pixels, effect settings and temporal inputs. The cache is bounded CPU memory and contains no files. Sequence teardown clears only that instance's cache and releases a model only when the closing instance last used it. Interactive model images target a 1280×720 box, respecting model minimum dimensions. SIMD conversion/resizing and correction-only resizing reduce CPU work. Studio Light transfers broad illumination onto source texture instead of replacing fine detail with model reconstruction. Eye Contact now also retains its processing CUDA context and restores the host's current context.

## Validation and limits

Revision 0.2.1.4 responds to local feedback that Studio Light warmed and softened a subject on Log footage. It uses matte-weighted linear luminance, ignores NVIDIA's invalid output background, and transfers neutral lighting onto the original source texture. **Input Encoding → Apple Log (Rec.2020)** supports Apple Log 1 before a later technical LUT/grade; the default remains SDR for already converted footage. Soft edges and Mix blend illumination in linear light. See the [workflow instructions](USER-GUIDE.md#studio-light-and-apple-log).

Studio Voice adds Dialogue mode and input-level matching. New instances use one centred voice model; older saved states keep independent channels and their original level. The new CPU checks cover saved-state migration, neutral/soft-edge Log exposure, extended-range pixel preservation and level-matching behavior. Local perceptual acceptance of this revision is still pending; it is not yet an approved stable publication.

The local build passed six core/Adobe-contract/UI/startup tests. The VST startup regression exercises all six processors through 108 format/activation cycles with an unavailable runtime, including silence and sample-exact delayed bypass. The previous binary fails this same host-setup test. GPU checks covered all six new picture adapters with a portrait 4K source, alternating host CUDA contexts, preservation of the host context and exact cached-frame replay. All six voice models processed the supplied stereo clip; native AE audio checks covered split requests, output bounds and silent-channel isolation. Audio buffer partitioning and exact bypass were checked at 44.1, 48 and 96 kHz.

Steinberg's validator completed with exit code zero. It reported rejection of unsupported fractional or out-of-range sample rates; the adapter accepts integer rates from 8 to 192 kHz. The installed Program Files copies and bundled runtimes were also exercised. The native panel was inspected on Windows, including cancellation feedback.

Full-resolution 4K inference is still below real-time playback on the development machine. Cache-hit speed is not new-frame inference speed. Premiere project playback, long renders, repeated activation, project reopen and export acceptance remain necessary. No NVIDIA/Adobe certification or model-quality superiority is claimed.

## Local installation

Developers with their own configured SDKs can build the source and validate an existing owned suite installation using `scripts/install-local-patch.ps1`. Add `-Apply` in an administrator PowerShell to install the tested local binaries. Close Adobe first. The script verifies current ownership hashes, backs up all replaced files, updates uninstall receipts, and restores the old files on installation failure. Its backups and all test media stay under the ignored build directory. It does not install SDKs or fetch credentials.
