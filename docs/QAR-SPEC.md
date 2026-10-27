# .qar Package Format, version 1.0

`.qar` is the only native application package type of QAR-OS. The package manager,
file-association handler and loader accept **nothing else** as a native app.
All integers are little-endian. Reference implementation: `tools/qar.py`.

## Layout
```
[ Header 128 B ][ Manifest ][ TOC ][ Payload ][ Signature block 128 B ]
   each of Manifest / TOC / Payload starts 16-byte aligned
```

## Header (128 bytes)
| Off | Size | Field | Notes |
|----:|-----:|-------|-------|
| 0   | 8  | magic | `89 51 41 52 0D 0A 1A 0A` ("\x89QAR\r\n\x1a\n") |
| 8   | 2  | version_major | 1. Loader rejects unknown majors |
| 10  | 2  | version_minor | additive changes only |
| 12  | 4  | flags | 0 in v1 |
| 16  | 4  | header_size | 128 |
| 20  | 8+8 | manifest_off, manifest_len | JSON, UTF-8, canonical (sorted keys, no spaces) |
| 36  | 8+8 | toc_off, toc_len | |
| 52  | 8+8 | payload_off, payload_len | |
| 68  | 8+8 | sig_off, sig_len | sig_len = 128, sig_off + sig_len = file_size |
| 84  | 8  | file_size | must equal actual size (truncation check) |
| 92  | 4  | reserved | 0 |
| 96  | 32 | content_hash | SHA-256 over bytes `[0, sig_off)` with this field zeroed |

## Manifest (required keys)
`id` (reverse-DNS), `name`, `version`, `entry` (path of an `exec` file).
Optional: `min_os`, `capabilities` (list, e.g. `net.connect`, `gfx.window`, `fs.user`).
Capabilities are a request only; the user/policy grants them at install (see Security).

## TOC
Repeated entries, no padding (fixed part is 56 bytes, then the path): `u16 path_len, u8 kind, u8 flags, u32 mode, u64 offset, u64 size, u8[32] sha256, path[path_len]`.
`kind`: 1 exec, 2 lib, 3 resource, 4 data. `offset` is absolute and must lie inside the payload.
Paths: relative, `/`-separated, no `..`, no leading `/`, no `\`, max 255 bytes. Max 4096 entries.

## Signature block (128 bytes)
`u8[32] key_id` (SHA-256 of public key), `u8[32] public_key` (Ed25519), `u8[64] signature`
over `content_hash || key_id`. The embedded key is informational: **trust comes only from
the OS key store**, never from the package.

## Verification order (a package that fails any step is never executed)
1. Magic, major version, `header_size`, `file_size == actual size`.
2. Section bounds: ordered, non-overlapping, inside the file.
3. Recompute `content_hash`, compare.
4. `key_id == SHA-256(public_key)`; key_id must be in the trust store; verify Ed25519 signature.
5. Parse TOC with bounds and path checks; verify every file's SHA-256.
6. Manifest valid; `entry` exists and is kind `exec`.
7. Install-time: compare requested capabilities against policy; prompt/deny.

## OS integration (design; implemented in later phases)
* **File association:** VFS lookup maps extension `.qar` + magic bytes to `pkgd`.
  Both must match; extension alone is never enough.
* **pkgd (package manager service, user space):** verify → extract to `/apps/<id>/<version>/`
  (read-only mount) → record granted capabilities in `/system/pkgdb` → handle upgrade/rollback
  (version must not decrease unless the signer explicitly allows).
* **Launch:** `exec` request goes to the loader with a capability set derived from the
  *granted* capabilities. Libraries (`kind=lib`) are resolved only from the same package or
  the signed system image.
* **Trust store:** system keys are built into the kernel image; developer keys are added
  by the user and revocable (revocation list checked at verify and at launch).

## Security notes
* Hashing the whole file prevents splice/append tricks; per-file hashes allow lazy loading with verification.
* Rollback attacks need a monotonic version check in pkgd (not part of the file format).
* The reference tool is Python; the in-OS verifier will need SHA-256 and Ed25519 implemented
  in C (planned Phase 8; use a small audited implementation, don't write crypto from scratch).
