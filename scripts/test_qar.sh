#!/usr/bin/env bash
# Round-trip test for the .qar toolchain: pack, verify, reject tampering and unknown signers.
set -euo pipefail
T="$(dirname "$0")/../tools/qar.py"; W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT
mkdir -p "$W/app/res"; printf 'bin' > "$W/app/main"; printf 'x' > "$W/app/res/a.txt"
echo '{"id":"org.qar.t","name":"T","version":"1","entry":"main"}' > "$W/app/manifest.json"
python3 "$T" keygen "$W/good" >/dev/null; python3 "$T" keygen "$W/other" >/dev/null
python3 "$T" pack "$W/app" "$W/t.qar" --key "$W/good.key" >/dev/null
python3 "$T" verify "$W/t.qar" --trust "$W/good.pub" >/dev/null && echo "PASS: valid package verifies"
! python3 "$T" verify "$W/t.qar" --trust "$W/other.pub" >/dev/null && echo "PASS: unknown signer rejected"
python3 - "$W/t.qar" <<'PY'
import sys,struct; p=sys.argv[1]; b=bytearray(open(p,'rb').read())
b[struct.unpack_from("<Q",b,52)[0]+1]^=1; open(p+'.bad','wb').write(b)
PY
! python3 "$T" verify "$W/t.qar.bad" --trust "$W/good.pub" >/dev/null && echo "PASS: tampered payload rejected"
