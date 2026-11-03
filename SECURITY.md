# Security and credentials

Do not put NGC keys, GitHub tokens, `.env`, vendor account files, SDK archives or private footage in public issues. Use this repository's **Security → Report a vulnerability** private reporting option for a credential leak, unsafe installer behavior or a security bug. If private reporting is unavailable, report a minimal non-sensitive description and request a private channel; never post the secret.

For an exposed key, revoke/rotate it in the issuing service. Removing a file from the latest commit does not remove it from history, caches or forks. The project cannot revoke someone else's account credentials.

Source auditing checks tracked/staged paths and content, and separately compares against local `.env` values without printing them. It is a defense in depth, not a guarantee that every possible secret format is recognized. Maintainers must review the publication inventory as well.

Release payloads use SHA-256 values embedded in Setup and authenticated encryption. Setup includes its decryption key; this protects packaging/integrity and does not provide model confidentiality. A compromised publisher/release can replace both assets and checksums, so only use the intended repository. This preview is not code-signed.

The preview has no formal support SLA. Reports about the current preview are prioritized; vendor runtime vulnerabilities should also be reported through the vendor's official security channel.
