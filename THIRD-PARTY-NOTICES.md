# Third-party components

4x4Tools Broadcast Suite integrates NVIDIA Maxine SDKs and models:

- Augmented Reality SDK 1.2.0.0: Gaze Redirection, Face Landmark Detection and
  Face Box Detection.
- Video Effects SDK 1.3.0.0: AI Green Screen, Background Blur, Denoising,
  Relighting, Upscale and Video Super Resolution.
- Windows Audio Effects SDK 3.0.0: Denoiser v2, Dereverb, combined
  Dereverb/Denoiser, Studio Voice HQ/LL, microphone profiles and Super Resolution.

NVIDIA's software, model and bundled-library licenses are retained under
`runtime/ARSDK`, `runtime/VideoFX` and `runtime/AudioFX`, including per-feature
license directories and third-party notices. Those terms apply to their
respective components. SDK headers, static development libraries, source
archives and download credentials are excluded from the installer.

The MIT license at the repository root applies to 4x4Tools source, not NVIDIA
or Adobe components. The source repository contains no Adobe/NVIDIA SDKs,
feature packages or models. Developers must obtain them using their own
accounts. End-user release payloads contain only the allowlisted runtime/model
subset, compiled effects and notices. Encryption does not change any license.

Models under the NVIDIA Open Model License carry the required attribution:
“Licensed by NVIDIA Corporation under the NVIDIA Open Model License”. Other
models retain their supplied NVIDIA Community Model License. The applicable
agreement is retained beside each feature, with a consolidated Notice.txt.

The release installer presents the supplied license text and requires
acceptance before deployment. Its limited local RTX workstation use must stay
within the applicable NVIDIA grants; enterprise/server/kiosk use may require
additional rights. No denied-entitlement feature is distributed.

The VST3 integration uses the official Steinberg VST3 SDK 3.8.1. Its MIT license
is included as `Notices/Steinberg-VST3-LICENSE.txt`. VST is a trademark of
Steinberg Media Technologies GmbH. The dependency revision is recorded in
`dependencies.lock.json`.

NVIDIA trademarks belong to NVIDIA Corporation. Adobe, Premiere and After
Effects trademarks belong to Adobe. 4x4Tools is the publisher of this plugin
suite; no endorsement by NVIDIA, Adobe or Steinberg is claimed.

Developer verification uses NVIDIA's public AR/VFX/AFX samples. Their sample
data and comparisons remain development artifacts and are not part of the
installer. The sample-data license files remain in the downloaded sample
repositories under `third_party`.

Official SDK information:
https://developer.nvidia.com/topics/ai/generative-ai/ai-for-media
