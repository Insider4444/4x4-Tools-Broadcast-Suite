# Release maintenance

Only a maintainer with the necessary vendor rights should publish an end-user bundle. Develop/build using your own Adobe/NVIDIA accounts. Public CI never contains SDK credentials or releases automatically.

## Prepare

1. Pin compatible SDK/feature versions in `dependencies.lock.json`. Obtain all four model families and retain vendor licenses/notices. Do not include denied-entitlement or evaluation-only components.
2. Review the supplied software, product-specific, model and bundled-library terms. For this release, the 2025 supplied AI product terms contain the compatible-application distribution grant; the supplied model licenses govern their respective files. Official current references: [software agreement](https://www.nvidia.com/en-us/agreements/enterprise-software/nvidia-software-license-agreement/), [AI terms](https://www.nvidia.com/en-us/agreements/enterprise-software/product-specific-terms-for-ai-products/), [Open Model License](https://www.nvidia.com/en-us/agreements/enterprise-software/nvidia-open-model-license/). Supplied versions remain in the package. Do not assume future versions have identical terms.
3. Run the build, relevant integration checks and actual host acceptance appropriate to the release. Update the validation status honestly. Do not publish restricted vendor benchmarking/regression data or licensed sample media.
4. Keep version identifiers aligned in CMake, resources, source About text, NSIS metadata, package scripts, release URLs and docs. Use a fresh tag for changed published contents.

## Package

Run `scripts/package-release.ps1` with the official NSIS compiler. It creates an explicit runtime/model allowlist, content-addressed compressed parts under GitHub's per-asset size limit, authenticated encrypted envelopes, an embedded manifest and a branded Setup executable.

The random 64-byte packaging key is generated in ignored `build/release-key.bin` and embedded in Setup. It is **not** an NGC credential. Never commit it; never claim it makes client-side files secret. Retain it privately when rebuilding the same unpublished candidate. Never change an already published tag's payloads silently.

The legacy `dist/release` tree is an installer-managed support inventory, not the normal-user public download set. The local legacy offline bundles, root SDK archives, `.env`, models and SDKs are never upload inputs. Required license PDFs are retained within the selected installed runtimes; their full text is also presented by Setup before acceptance.

## Audit and verify

Run `scripts/audit-public.py` for the source and `scripts/audit-release.py` for the release output. Inspect both inventories. Confirm no Adobe/NVIDIA headers, SDK tools, static libraries, private INIs or account files enter the package. Validate encrypted staging for the detected architecture against the exact embedded manifest, and compare every extracted file's digest.

Run the crypto corruption/wrong-key fixture and installer rollback/uninstall fixture. Check Setup product/company/copyright metadata and its GPU gate. Verify checksums again after upload using GitHub's asset digests. Describe the completed and pending acceptance work accurately. A regular community release must not be presented as proof of broad production validation or vendor certification.

## Publish

Publish only reviewed original source plus docs/artwork under MIT. For the main public release, upload only `dist/community-0.2.0/public/4x4Tools-Setup.exe` and `SHA256SUMS.txt` at the audited source commit. Use beginner-focused notes with the single Setup link, known limitations and own-account build guide. Data assets are ancillary application payloads, not standalone SDK downloads.

After publication, check README links, release visibility/assets and source CI. Test a small actual download through the same release URL used by Setup. Enable private vulnerability reporting. Do not mark pending hardware/Adobe acceptance complete based on publication alone.

## One-file community packaging

`package-consumer.ps1` builds the main public download: one Setup executable and an optional checksum. It reuses the immutable `resources/runtime-catalog.json`, your matching private packaging key and your own SDK license copies. This catalog contains file metadata only; no SDKs, models or credentials are committed.

Upload new `dist/community-0.2.0/support` files under their versioned names to the catalog's support-storage release. Do not overwrite or remove existing support assets: older installers pin those hashes. Publish the normal public release from the `public` directory only, mark it Latest and link it from the website. Support assets are downloaded by Setup; users do not choose or extract them.

Run `python scripts/audit-consumer.py` and stage the exact embedded consumer manifest. A fresh independent distribution with different vendor versions or a different packaging key must create and audit its own complete runtime catalog through the full packaging workflow. Do not reuse a catalog with an unmatched key or change a published catalog silently.
