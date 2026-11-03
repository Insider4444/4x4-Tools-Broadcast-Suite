# Release maintenance

Only a maintainer with the necessary vendor rights should publish an end-user bundle. Develop/build using your own Adobe/NVIDIA accounts. Public CI never contains SDK credentials or releases automatically.

## Prepare

1. Pin compatible SDK/feature versions in `dependencies.lock.json`. Obtain all four model families and retain vendor licenses/notices. Do not include denied-entitlement or evaluation-only components.
2. Review the supplied software, product-specific, model and bundled-library terms. For this preview, the 2025 supplied AI product terms contain the compatible-application distribution grant; the supplied model licenses govern their respective files. Official current references: [software agreement](https://www.nvidia.com/en-us/agreements/enterprise-software/nvidia-software-license-agreement/), [AI terms](https://www.nvidia.com/en-us/agreements/enterprise-software/product-specific-terms-for-ai-products/), [Open Model License](https://www.nvidia.com/en-us/agreements/enterprise-software/nvidia-open-model-license/). Supplied versions remain in the package. Do not assume future versions have identical terms.
3. Run the build, relevant integration checks and actual host acceptance appropriate to the release. Update the preview status honestly. Do not publish restricted vendor benchmarking/regression data or licensed sample media.
4. Keep version identifiers aligned in CMake, resources, source About text, NSIS metadata, package scripts, release URLs and docs. Use a fresh tag for changed published contents.

## Package

Run `scripts/package-release.ps1` with the official NSIS compiler. It creates an explicit runtime/model allowlist, content-addressed compressed parts under GitHub's per-asset size limit, authenticated encrypted envelopes, an embedded manifest and a branded Setup executable.

The random 64-byte packaging key is generated in ignored `build/release-key.bin` and embedded in Setup. It is **not** an NGC credential. Never commit it; never claim it makes client-side files secret. Retain it privately when rebuilding the same unpublished candidate. Never change an already published tag's payloads silently.

`dist/release` is the only community upload directory. The local legacy offline bundles, root SDK archives, `.env`, models and SDKs are never upload inputs. Required license PDFs are retained within the selected installed runtimes; their full text is also presented by Setup before acceptance.

## Audit and verify

Run `scripts/audit-public.py` for the source and `scripts/audit-release.py` for the release output. Inspect both inventories. Confirm no Adobe/NVIDIA headers, SDK tools, static libraries, private INIs or account files enter the package. Validate encrypted staging for the detected architecture against the exact embedded manifest, and compare every extracted file's digest.

Run the crypto corruption/wrong-key fixture and installer rollback/uninstall fixture. Check Setup product/company/copyright metadata and its GPU gate. Verify checksums again after upload using GitHub's asset digests. Keep the release a prerelease until the acceptance scope supports a stable designation.

## Publish

Publish only reviewed original source plus docs/artwork under MIT. Create a GitHub prerelease at the audited source commit, upload the exact `dist/release` assets and use beginner-focused notes with the Setup link, known limitations and own-account build guide. Data assets are ancillary application payloads, not standalone SDK downloads.

After publication, check README links, release visibility/assets and source CI. Test a small actual download through the same release URL used by Setup. Enable private vulnerability reporting. Do not mark pending hardware/Adobe acceptance complete based on publication alone.
