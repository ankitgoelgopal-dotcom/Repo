#!/usr/bin/env python3
"""mkstore.py - build a signed app-store index for QAR-OS (what the OS package manager `qpm` will download).

  mkstore.py build  <dir with .qar files> <out dir> --key store.key --base-url https://host/store
  mkstore.py verify <out dir> --pub store.pub

Produces  index.json  (apps: id, name, version, file, size, sha256, capabilities, url)
          index.sig   (Ed25519 signature over the exact bytes of index.json)
The client checks index.sig, then downloads each .qar, checks size+sha256 from the index,
and finally runs the normal .qar verification (signer must be trusted).
"""
import argparse, hashlib, json, os, shutil, sys
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey
from cryptography.exceptions import InvalidSignature
sys.path.insert(0, os.path.dirname(__file__))
import qar

def build(a):
    os.makedirs(a.out, exist_ok=True); apps = []
    for n in sorted(os.listdir(a.src)):
        if not n.endswith(".qar") or n.startswith(("tampered", "untrusted")): continue   # skip security-demo packages
        raw, h = qar.parse(os.path.join(a.src, n))
        man = json.loads(raw[h["m"][0]:h["m"][0] + h["m"][1]])
        shutil.copy(os.path.join(a.src, n), os.path.join(a.out, n))
        apps.append(dict(id=man["id"], name=man["name"], version=man["version"], file=n, size=len(raw),
                         sha256=hashlib.sha256(raw).hexdigest(), capabilities=man.get("capabilities", []),
                         url=a.base_url.rstrip("/") + "/" + n))
    idx = json.dumps({"format": 1, "apps": apps}, sort_keys=True, indent=1).encode()
    open(os.path.join(a.out, "index.json"), "wb").write(idx)
    key = Ed25519PrivateKey.from_private_bytes(open(a.key, "rb").read())
    open(os.path.join(a.out, "index.sig"), "wb").write(key.sign(idx))
    print(f"store index: {len(apps)} apps -> {a.out}")

def verify(a):
    idx = open(os.path.join(a.out, "index.json"), "rb").read(); sig = open(os.path.join(a.out, "index.sig"), "rb").read()
    try: Ed25519PublicKey.from_public_bytes(open(a.pub, "rb").read()).verify(sig, idx)
    except InvalidSignature: print("INDEX SIGNATURE INVALID"); sys.exit(1)
    for app in json.loads(idx)["apps"]:
        raw = open(os.path.join(a.out, app["file"]), "rb").read()
        if len(raw) != app["size"] or hashlib.sha256(raw).hexdigest() != app["sha256"]: print("MISMATCH", app["id"]); sys.exit(1)
    print("store OK")

if __name__ == "__main__":
    p = argparse.ArgumentParser(); s = p.add_subparsers(required=True)
    x = s.add_parser("build"); x.add_argument("src"); x.add_argument("out"); x.add_argument("--key", required=True); x.add_argument("--base-url", default="https://example.invalid/store"); x.set_defaults(f=build)
    x = s.add_parser("verify"); x.add_argument("out"); x.add_argument("--pub", required=True); x.set_defaults(f=verify)
    a = p.parse_args(); a.f(a)
