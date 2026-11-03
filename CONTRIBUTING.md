# Contributing

Start with [SDK setup](SDK-SETUP.md) and [build instructions](docs/BUILD.md). You can work on docs, packaging fixtures and SDK-free tests without vendor downloads.

Keep changes focused. Explain the user-visible behavior, the reason for the change and the checks you ran. Keep stable Adobe match names and VST3 class IDs compatible with saved projects. Add tests for meaningful bugs/boundaries; do not add tests that merely restate an implementation.

Before a pull request:

1. Run the relevant core, host-contract or installer checks.
2. Run `python scripts/audit-public.py` against the tracked/staged files.
3. Check that no SDKs, models, vendor sample media, private paths, build outputs or credentials enter the diff.
4. Describe actual Adobe/GPU testing precisely. Do not equate a callback harness with a real Adobe export.

Use your own Adobe/NVIDIA accounts. Do not upload `.env`, NGC config, tokens, account-gated SDK packages, extracted release runtimes or models. Do not send a maintainer credentials to reproduce a bug. Model/benchmark disclosures must comply with the vendor terms.

Bug reports should include Adobe version, Windows version, GPU/driver, effect/settings, color depth and exact reproduction steps. Attach media only when you have permission to publish it. Sensitive issues belong in [private security reporting](SECURITY.md).

By contributing, you agree to license your original contribution under this repository's MIT license. Third-party components keep their own terms; do not copy SDK sample implementation into the project without checking and retaining its required notices.
