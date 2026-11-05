# Local native patch 0.2.1

This patch is installed on the development machine for Adobe acceptance testing. It has not replaced the public 0.2.0 installer.

## Editing controls

- The six picture effects added in 0.2.0 now default to **Interactive Preview**. Existing projects keep their saved quality choice. Choose **Highest Quality (Render)** before final output when appropriate.
- **Processing → Status and Quality…** opens a native panel with the current processing phase, elapsed time, GPU memory usage and cancellation. NVIDIA calls that expose no internal percentage show an indeterminate state. Cancellation takes effect after the current model call returns.
- Premiere's six VST3 voice effects have a native editor with Mix, Output Gain, Bypass and processing status. Studio Voice Preview also exposes Microphone Character.
- New Studio Voice HQ instances default to **Auto**: the low-latency model during editing, the HQ model when the host explicitly requests offline processing. Hosts that do not request offline mode require choosing HQ explicitly. Existing saved instances preserve HQ. HQ uses six-second model blocks.
- Native After Effects Studio Voice HQ provides an explicit Preview/HQ choice.

## Changes

Audio bypass and zero wet mix skip inference while maintaining host latency compensation. Deactivated VST processors release their model allocations; video and native AE audio sequence teardown also release shared model state. Bypass alone does not unload a model.

Video and voice processing use an explicitly retained CUDA primary context and restore Adobe's previous context. Vendor cleanup exceptions are contained. These changes address unsafe lifetime/context handling; they do not establish that every reported Premiere crash has been reproduced or eliminated.

Video output caching reuses identical requests and checks all input pixels, effect settings and temporal inputs. The cache is bounded CPU memory, contains no files, and is cleared on teardown. Interactive model images are bounded in size. SIMD conversion/resizing and correction-only resizing reduce CPU work. Studio Light transfers broad illumination onto source texture instead of replacing fine detail with model reconstruction.

## Validation and limits

The local build passed five core/Adobe-contract/UI tests. GPU checks covered all six new picture adapters with a portrait 4K source, alternating host CUDA contexts, preservation of the host context and exact cached-frame replay. All six voice models processed the supplied stereo clip; native AE audio checks covered split requests, output bounds and silent-channel isolation. Audio buffer partitioning and exact bypass were checked at 44.1, 48 and 96 kHz.

Steinberg's validator completed with exit code zero. It reported rejection of unsupported fractional or out-of-range sample rates; the adapter accepts integer rates from 8 to 192 kHz. The installed Program Files copies and bundled runtimes were also exercised. The native panel was inspected on Windows, including cancellation feedback.

Full-resolution 4K inference is still below real-time playback on the development machine. Cache-hit speed is not new-frame inference speed. Premiere project playback, long renders, repeated activation, project reopen and export acceptance remain necessary. No NVIDIA/Adobe certification or model-quality superiority is claimed.

## Local installation

Developers with their own configured SDKs can build the source and validate an existing owned suite installation using `scripts/install-local-patch.ps1`. Add `-Apply` in an administrator PowerShell to install the tested local binaries. Close Adobe first. The script verifies current ownership hashes, backs up all replaced files, updates uninstall receipts, and restores the old files on installation failure. Its backups and all test media stay under the ignored build directory. It does not install SDKs or fetch credentials.
