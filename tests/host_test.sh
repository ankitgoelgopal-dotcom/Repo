#!/usr/bin/env bash
# Cross-checks the in-OS verifier (C) against the reference packer (Python), on the host.
set -euo pipefail
cd "$(dirname "$0")/.."; W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT
E=third_party/ed25519/src
gcc -O1 -Wall -o "$W/hv" tests/host_verify.c kernel/qar.c kernel/sha256.c $E/verify.c $E/ge.c $E/fe.c $E/sc.c $E/sha512.c
[ "$("$W/hv" -)" = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ] && echo "PASS: sha256 test vector"
T=tools/qar.py
mkdir -p "$W/app"; printf '\x7fELFxxxx' > "$W/app/main"
echo '{"id":"org.qar.t","name":"T","version":"1","entry":"main","capabilities":["gfx.window","net.connect"]}' > "$W/app/manifest.json"
python3 $T keygen "$W/a" >/dev/null; python3 $T keygen "$W/b" >/dev/null
python3 $T pack "$W/app" "$W/t.qar" --key "$W/a.key" >/dev/null
"$W/hv" "$W/t.qar" "$W/a.pub" | grep -q ACCEPT && echo "PASS: C verifier accepts Python-signed package"
{ "$W/hv" "$W/t.qar" "$W/b.pub" || true; } | grep -q "untrusted" && echo "PASS: C verifier rejects unknown signer"
python3 - "$W/t.qar" <<'PY'
import sys,struct; p=sys.argv[1]; b=bytearray(open(p,'rb').read()); b[struct.unpack_from("<Q",b,52)[0]+1]^=1; open(p+'.bad','wb').write(b)
PY
{ "$W/hv" "$W/t.qar.bad" "$W/a.pub" || true; } | grep -q REJECT && echo "PASS: C verifier rejects tampered package"
