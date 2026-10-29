#!/usr/bin/env python3
"""mkcabundle.py <out.c>: embed the build host's trusted CA bundle (PEM) as C data for the SDK's TLS client."""
import sys, os
cands = []
try:
    import certifi; cands.append(certifi.where())
except ImportError: pass
cands += ["/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt"]
src = next((p for p in cands if os.path.exists(p)), None)
if not src: sys.exit("no CA bundle found on this machine (install ca-certificates or python3-certifi)")
pem = open(src, "rb").read().strip() + b"\n\0"
with open(sys.argv[1], "w") as f:
    f.write(f"/* generated from {src} */\nconst unsigned char qar_ca_bundle_pem[] = {{\n")
    for i in range(0, len(pem), 24): f.write(",".join(str(b) for b in pem[i:i+24]) + ",\n")
    f.write(f"}};\nconst unsigned qar_ca_bundle_len = {len(pem)};\n")
print(f"CA bundle: {src}, {pem.count(b'BEGIN CERTIFICATE')} certificates")
