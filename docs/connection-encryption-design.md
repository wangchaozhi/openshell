# Connection-file encryption — design proposal

Status: **not yet implemented**. AGENTS.md lists "保存的 connections 文件可选 GPG / age
加密，方便 Git 同步" as a target. This document is the design sketch so the next
person doesn't start from zero.

## Constraints

- The current on-disk model is one JSON file per connection in
  `<AppData>/OpenShell/connections/<id>.json`. Passwords / passphrases are
  already stored in the OS keychain via `CredentialStore` (`OPENSHELL_USE_KEYCHAIN`),
  so the JSON itself only contains host, user, group, notes, port-forward specs,
  jump-host metadata, etc.
- The goal is to make the JSON safe to commit to Git or sync via Dropbox/iCloud
  without leaking infrastructure layout (hosts, usernames, jump topology).
- The keychain-stored secrets do **not** need to be re-encrypted on disk — they
  live in the keychain. The encryption layer only protects the JSON metadata.

## Candidate backends

| Backend | Pros | Cons |
| --- | --- | --- |
| **GPG subprocess** | Reuses user's existing GPG keyring + gpg-agent. No new linked dep. ASCII-armored output diffs cleanly in Git. | Requires `gpg` in PATH. One subprocess per file load → slow with many connections (mitigated by gpg-agent caching). |
| **libsodium (FetchContent)** | Real AEAD (XChaCha20-Poly1305) + Argon2id KDF. In-process, fast. | Adds a non-trivial dep. libsodium uses autotools natively; needs a CMake wrapper fork. |
| **mbedTLS reuse** | Already pulled in on Apple/Android for libssh2 (`OpenShellSources.cmake`). | Not pulled in on Linux/Windows desktop builds where libssh2 uses system OpenSSL. Conditional codepath needed. |
| **age (CLI)** | Modern, age format is small + Git-friendly. | Adds an external CLI dependency rarely preinstalled. |

Recommended: **GPG subprocess** for v1. Lowest dependency footprint, leverages
infrastructure the target user (developer doing Git sync) almost certainly
already has, and the per-file subprocess cost is acceptable because
ConnectionCatalog::reload() is only called on startup / after edits.

## Sketch of the integration

1. Add fields to `SettingsStore`:
   - `connectionEncryption` enum: `None` (default) / `Gpg`.
   - `gpgRecipient` string: key fingerprint or email used as `gpg --recipient`.
2. Add `src/CryptoStore.{h,cpp}` with a backend-agnostic interface:
   ```cpp
   namespace CryptoStore {
       bool isEncrypted(const QByteArray &bytes);              // sniff magic
       QByteArray encrypt(const QByteArray &plain, QString *error);
       QByteArray decrypt(const QByteArray &cipher, QString *error);
   }
   ```
3. In `ConnectionCatalog::saveToFile`, after building the JSON `QByteArray`, if
   encryption is enabled call `CryptoStore::encrypt`. The output file should
   still end in `.json` (so file naming + UUID keys stay invariant); the
   `isEncrypted` sniff handles dispatch on read.
4. In `ConnectionCatalog::loadFromFile`, sniff the file. If encrypted, run
   `CryptoStore::decrypt`. Fall back to plaintext if not.
5. UI: add a "Connection sync" pane in the settings dialog with backend selector
   + recipient field + a "Re-encrypt all" button (calls `saveToFile` on every
   profile to round-trip plaintext → cipher or vice versa).

## Open questions

- Master-password caching: if backend is "passphrase" rather than "GPG key",
  where do we cache the unlocked DEK? Probably in `CredentialStore` under a
  reserved key. Time-bounded auto-lock? Out of scope for v1.
- Cross-device portability: GPG key sync is the user's problem (typically via
  YubiKey / a shared keyring). Document that explicitly.
- Migration: a `migrate-encryption` CLI subcommand that rewrites every file in
  `connections/` with the new backend.

## Out of scope (deferred)

- Encrypting the password / passphrase fields stored in the OS keychain — they
  are already protected by OS ACLs.
- A separate per-connection key (right now: one key encrypts all files; rotating
  it means rewriting all files, which is fine).
