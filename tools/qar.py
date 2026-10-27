#!/usr/bin/env python3
"""qar.py - build, inspect, verify and extract QAR (.qar) application packages.

Layout (all integers little-endian), see docs/QAR-SPEC.md:
  [Header 128B][Manifest][TOC][Payload (files, 16B aligned)][Signature block]
"""
import argparse, hashlib, json, os, struct, sys
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey
from cryptography.hazmat.primitives import serialization
from cryptography.exceptions import InvalidSignature

MAGIC = b"\x89QAR\r\n\x1a\n"
VER = (1, 0)
HDR_FMT = "<8sHHII QQ QQ QQ QQ Q I 32s"          # see spec table
HDR_SIZE = 128
HASH_OFF = HDR_SIZE - 32                         # content_hash lives in last 32 bytes
assert struct.calcsize(HDR_FMT) == HDR_SIZE, struct.calcsize(HDR_FMT)

KIND = {"exec": 1, "lib": 2, "resource": 3, "data": 4}
KIND_R = {v: k for k, v in KIND.items()}
SIG_FMT = "<32s32s64s"                           # key_id, pubkey, signature
SIG_SIZE = struct.calcsize(SIG_FMT)
MAX_FILES = 4096
MAX_PATH = 255

def sha(b): return hashlib.sha256(b).digest()
def align(n, a=16): return (n + a - 1) // a * a

def key_id(pub_raw): return sha(pub_raw)

def cmd_keygen(a):
    k = Ed25519PrivateKey.generate()
    open(a.out + ".key", "wb").write(k.private_bytes(
        serialization.Encoding.Raw, serialization.PrivateFormat.Raw, serialization.NoEncryption()))
    pub = k.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    open(a.out + ".pub", "wb").write(pub)
    os.chmod(a.out + ".key", 0o600)
    print(f"wrote {a.out}.key (KEEP SECRET) and {a.out}.pub; key_id={key_id(pub).hex()}")

def check_path(p):
    if not p or p.startswith("/") or "\\" in p or ".." in p.split("/") or len(p.encode()) > MAX_PATH:
        raise SystemExit(f"invalid package path: {p!r}")

def cmd_pack(a):
    man = json.load(open(os.path.join(a.src, "manifest.json")))
    for f in ("id", "name", "version", "entry"):
        if f not in man: raise SystemExit(f"manifest missing '{f}'")
    files = []
    for root, _, names in os.walk(a.src):
        for n in sorted(names):
            full = os.path.join(root, n)
            rel = os.path.relpath(full, a.src).replace(os.sep, "/")
            if rel == "manifest.json": continue
            check_path(rel)
            kind = "exec" if rel == man["entry"] else ("lib" if rel.endswith(".so") else "resource")
            files.append((rel, kind, open(full, "rb").read(), 0o755 if kind == "exec" else 0o644))
    files.sort(key=lambda f: f[0])
    if not any(f[0] == man["entry"] for f in files): raise SystemExit("entry file not found in package")
    if len(files) > MAX_FILES: raise SystemExit("too many files")

    manifest = json.dumps(man, sort_keys=True, separators=(",", ":")).encode()
    # TOC entry: u16 path_len, u8 kind, u8 flags, u32 mode, u64 off, u64 size, sha256, path
    toc_sz = sum(struct.calcsize("<HBBIQQ32s") + len(f[0].encode()) for f in files)
    toc_off = HDR_SIZE + align(len(manifest))
    pay_off = toc_off + align(toc_sz)
    toc, payload, cur = b"", b"", 0
    for rel, kind, data, mode in files:
        pb = rel.encode()
        toc += struct.pack("<HBBIQQ32s", len(pb), KIND[kind], 0, mode, pay_off + cur, len(data), sha(data)) + pb
        payload += data + b"\0" * (align(len(data)) - len(data))
        cur += align(len(data))
    sig_off = pay_off + len(payload)
    total = sig_off + SIG_SIZE
    priv = Ed25519PrivateKey.from_private_bytes(open(a.key, "rb").read())
    pub = priv.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)

    def header(h):
        return struct.pack(HDR_FMT, MAGIC, VER[0], VER[1], 0, HDR_SIZE,
                           HDR_SIZE, len(manifest), toc_off, toc_sz, pay_off, len(payload),
                           sig_off, SIG_SIZE, total, 0, h)
    body = manifest + b"\0" * (align(len(manifest)) - len(manifest)) + \
           toc + b"\0" * (align(toc_sz) - toc_sz) + payload
    h = sha(header(b"\0" * 32)[:HASH_OFF] + b"\0" * 32 + body)     # hash with hash field zeroed
    hdr = header(h)
    sig = priv.sign(h + key_id(pub))
    open(a.out, "wb").write(hdr + body + struct.pack(SIG_FMT, key_id(pub), pub, sig))
    print(f"packed {a.out}: {len(files)} files, {total} bytes, hash={h.hex()[:16]}..., key_id={key_id(pub).hex()[:16]}...")

def parse(path):
    raw = open(path, "rb").read()
    if len(raw) < HDR_SIZE + SIG_SIZE: raise ValueError("file too small")
    f = struct.unpack(HDR_FMT, raw[:HDR_SIZE])
    (magic, vmaj, vmin, flags, hsz, m_off, m_len, t_off, t_len, p_off, p_len, s_off, s_len, total, _res, chash) = f
    if magic != MAGIC: raise ValueError("bad magic (not a .qar file)")
    if vmaj != VER[0]: raise ValueError(f"unsupported major version {vmaj}")
    if hsz != HDR_SIZE or total != len(raw): raise ValueError("size mismatch / truncated")
    if s_len != SIG_SIZE or s_off + s_len != total: raise ValueError("bad signature block bounds")
    if not (HDR_SIZE <= m_off and m_off + m_len <= t_off and t_off + t_len <= p_off and p_off + p_len <= s_off):
        raise ValueError("section bounds invalid / overlapping")
    return raw, dict(m=(m_off, m_len), t=(t_off, t_len), p=(p_off, p_len), s=(s_off, s_len), hash=chash, flags=flags)

def read_toc(raw, h):
    t_off, t_len = h["t"]; p_off, p_len = h["p"]
    out, pos, end = [], t_off, t_off + t_len
    ent = struct.calcsize("<HBBIQQ32s")
    while pos + ent <= end and len(out) < MAX_FILES:
        pl, kind, fl, mode, off, size, fh = struct.unpack("<HBBIQQ32s", raw[pos:pos + ent])
        if pl == 0: break
        path = raw[pos + ent:pos + ent + pl].decode()
        check_path(path)
        if kind not in KIND_R: raise ValueError(f"bad kind for {path}")
        if off < p_off or off + size > p_off + p_len: raise ValueError(f"{path}: data out of bounds")
        out.append(dict(path=path, kind=KIND_R[kind], mode=mode, off=off, size=size, sha=fh))
        pos += ent + pl
    return out

def verify(path, trusted):
    """Full verification. Returns (manifest, toc, key_id_hex). Raises ValueError on any failure."""
    raw, h = parse(path)
    body = raw[HDR_SIZE:h["s"][0]]
    calc = sha(raw[:HASH_OFF] + b"\0" * 32 + body)
    if calc != h["hash"]: raise ValueError("content hash mismatch (file modified)")
    kid, pub, sig = struct.unpack(SIG_FMT, raw[h["s"][0]:])
    if key_id(pub) != kid: raise ValueError("key id does not match embedded public key")
    if kid not in trusted: raise ValueError(f"signer {kid.hex()[:16]}... is not in the trust store")
    try: Ed25519PublicKey.from_public_bytes(trusted[kid]).verify(sig, h["hash"] + kid)
    except InvalidSignature: raise ValueError("signature invalid")
    toc = read_toc(raw, h)
    for e in toc:
        if sha(raw[e["off"]:e["off"] + e["size"]]) != e["sha"]: raise ValueError(f"{e['path']}: file hash mismatch")
    man = json.loads(raw[h["m"][0]:h["m"][0] + h["m"][1]])
    if not any(e["path"] == man["entry"] and e["kind"] == "exec" for e in toc): raise ValueError("entry is not an exec file")
    return man, toc, kid.hex(), raw

def load_trust(files):
    t = {}
    for p in files:
        pub = open(p, "rb").read(); t[key_id(pub)] = pub
    return t

def cmd_inspect(a):
    raw, h = parse(a.pkg)
    man = json.loads(raw[h["m"][0]:h["m"][0] + h["m"][1]])
    print(json.dumps(man, indent=2))
    for e in read_toc(raw, h): print(f"  {e['kind']:8} {e['size']:8}  {e['path']}")

def cmd_verify(a):
    try: man, toc, kid, _ = verify(a.pkg, load_trust(a.trust))
    except (ValueError, json.JSONDecodeError, struct.error) as e:
        print(f"VERIFY FAILED: {e}"); sys.exit(1)
    print(f"OK: {man['id']} {man['version']} signed by {kid[:16]}..., {len(toc)} files verified")

def cmd_extract(a):
    try: man, toc, _, raw = verify(a.pkg, load_trust(a.trust))
    except (ValueError, json.JSONDecodeError, struct.error) as e:
        print(f"VERIFY FAILED: {e}"); sys.exit(1)
    for e in toc:
        dst = os.path.join(a.dest, e["path"]); os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, "wb").write(raw[e["off"]:e["off"] + e["size"]]); os.chmod(dst, e["mode"])
    print(f"extracted {len(toc)} files to {a.dest}")

if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__); s = p.add_subparsers(required=True)
    x = s.add_parser("keygen"); x.add_argument("out"); x.set_defaults(f=cmd_keygen)
    x = s.add_parser("pack"); x.add_argument("src"); x.add_argument("out"); x.add_argument("--key", required=True); x.set_defaults(f=cmd_pack)
    x = s.add_parser("inspect"); x.add_argument("pkg"); x.set_defaults(f=cmd_inspect)
    x = s.add_parser("verify"); x.add_argument("pkg"); x.add_argument("--trust", nargs="+", required=True); x.set_defaults(f=cmd_verify)
    x = s.add_parser("extract"); x.add_argument("pkg"); x.add_argument("dest"); x.add_argument("--trust", nargs="+", required=True); x.set_defaults(f=cmd_extract)
    a = p.parse_args(); a.f(a)
