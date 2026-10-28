# Downloading apps in QAR-OS (design + what exists today)

## Status
| Piece | State |
|---|---|
| Signed `.qar` packages, in-kernel verification | **working** (boot test proves accept/reject) |
| Signed store index (`index.json` + `index.sig`) and builder (`tools/mkstore.py`) | **working**, produced by CI as the `qar-store` artifact |
| Network stack (e1000 driver, ARP, IPv4, ICMP, UDP, DHCP, DNS, TCP client) | **working** (see README, Networking) |
| TLS (needed for HTTPS) | not built |
| `qpm` client, `pkgd` install service, `/apps` storage | not built (needs VFS, IPC, network) |

The network now works, so an app can fetch files over plain HTTP. Missing for real downloads: TLS (HTTPS), a
file system to store the package, and the `qpm`/`pkgd` programs. Today apps still arrive as boot modules in the ISO.

## Flow once the pieces exist
1. `qpm update`: GET `<store>/index.json` and `index.sig`; check the Ed25519 signature against the
   store key in the OS key store.
2. `qpm install <id>`: GET the `.qar` from the index entry; check `size` and `sha256` from the index.
3. Run the normal `.qar` verification (signer must be trusted, every file hash must match).
4. Show the requested capabilities; on approval, `pkgd` stores it under `/apps/<id>/<version>/`
   and records the grant in `/system/pkgdb`.
5. Updates require the same signer and a version that does not decrease.

## Hosting
CI already builds the store folder (`build/store/`). Any static host works: GitHub Pages, a release asset
folder, or your own server. Set `--base-url` in `scripts/build_apps.sh` to its address. Serve it over HTTPS
once TLS exists; the signatures already protect integrity, so even plain HTTP cannot swap in a modified app.

## Index format (v1)
```json
{"format": 1, "apps": [{"id":"org.qar.hello","name":"Hello","version":"1.0.0","file":"hello.qar",
  "size":1234,"sha256":"<hex of the whole .qar>","capabilities":["console"],"url":"https://host/store/hello.qar"}]}
```
`index.sig` is the raw 64-byte Ed25519 signature over the exact bytes of `index.json`.
