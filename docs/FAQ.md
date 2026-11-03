# Questions and answers

## What is NVIDIA Broadcast, and how is this related?

Broadcast is NVIDIA's webcam/microphone application. This independent project uses related NVIDIA Maxine SDK capabilities inside Adobe editors, so recorded material can be processed as part of an edit. It does not embed or modify the Broadcast app.

## Is this an official NVIDIA or Adobe product?

No. 4x4Tools publishes the integration. NVIDIA supplies the underlying learned models/runtime; Adobe supplies the host applications and SDK. No endorsement or certification is claimed.

## Do ordinary users need a developer account?

No. The official installer supplies the permitted runtime/model subset for its application. Developers compiling from source must obtain their own Adobe/NVIDIA downloads and credentials.

## Are the models open source? Does MIT cover everything?

MIT covers 4x4Tools source. NVIDIA runtime/model and other third-party terms are separate and are retained in the installer. A public source repository does not relicense those components.

## Are encrypted models inaccessible to developers?

No. A working local installer carries the key needed to decrypt its payloads, and inference needs readable runtime files. Encryption packages and authenticates release data; it cannot guarantee secrecy or override licenses. SDK downloads still belong in each developer's private workspace.

## Is it better than NVIDIA Broadcast?

No controlled comparison establishes that. The project focuses on Adobe integration, deterministic render windows, alpha handling and audio adaptation. It uses NVIDIA's models and does not claim superior model accuracy or speed.

## Does it run in real time?

That depends on the GPU, frame size, model, quality mode and stacked effects. Highest Quality prioritizes processing behavior over speed. Studio Voice HQ analyzes six-second blocks and is an offline workflow. Use Faster Preview while editing where appropriate.

## Why no Speaker Focus / echo cancellation / TrueHDR?

Speaker Focus needs a separate entitlement and is not implemented here. Room Echo Clean removes room reverberation; it is not acoustic echo cancellation using a playback reference. Frame generation, TrueHDR and virtual device routing are separate integrations and are outside this preview.

## Can I use AMD, Intel, GTX, macOS or Windows Arm?

This preview is Windows x64 with the explicitly supported NVIDIA Tensor Core model families. Those other platforms are unsupported. `_woa` SDK downloads target Windows on Arm and are not interchangeable with x64 packages.

## Why are the downloads so large?

NVIDIA inference libraries and models are large, especially Studio Voice. Setup selects your GPU's models instead of downloading all families. Component selection can avoid unrelated audio/video data.

## Can I contribute without Adobe or NVIDIA SDK access?

Yes. Documentation, installer fixtures, source hygiene checks and SDK-free core tests do not need vendor accounts. Full native plugin builds and GPU integration require your own SDKs/models. See [CONTRIBUTING](../CONTRIBUTING.md).
